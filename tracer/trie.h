/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Prefix trie
 *
 * Unlike the one in mainline kernel, this actually compresses
 * bytearrays.
 *
 * by Michał Lach <michal.lach@samsung.com>
 */
#ifndef _TRIE_H
#define _TRIE_H
#include <linux/list.h>

/* trie_root is meant to be castable to trie_node */
struct trie_root {
	struct list_head children;
	size_t count;
};

struct trie_node {
	struct list_head children;
	struct list_head siblings;
	struct trie_node *parent;
	char *key;
	size_t size;
	void *priv;
};

/* used later for trie_iter */
struct __trie_selem {
	struct list_head head;
	struct trie_node *ptr;
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(6,3,0)
static inline size_t list_count_nodes(struct list_head *head)
{
	struct list_head *pos;
	size_t count = 0;

	list_for_each(pos, head)
		count++;

	return count;
}
#endif

/**
 * strnpfx - Return amount of bytes which are equal in both strings at their
 * begginings
 * @lhs: left-hand-side string
 * @rhs: right-hand-side string
 * @size: minimum size of both the strings excluding trailing NUL byte
 */
static size_t strnpfx(char *lhs, char *rhs, size_t size)
{
	size_t pos = 0;

	while (lhs[pos] == rhs[pos]) {
		pos++;
		if (pos == size)
			break;
	}

	return pos;
}

/**
 * DEFINE_TRIE - Initialize struct trie_node and its fields
 * @name: name for new trie.
 *
 * Initializes .children list_head
 */
#define DEFINE_TRIE(name) \
	struct trie_root name = { {(struct list_head *) &name, \
				   (struct list_head *) &name}}

/**
 * INIT_TRIE_NODE - Initialize struct trie_node and its fields
 * @node: pointer to struct trie_node
 *
 * Zeroes out entries to use later, more of an internal function
 */
static inline void INIT_TRIE_NODE(struct trie_node *node)
{
	memset(node, 0, sizeof(struct trie_node));
	INIT_LIST_HEAD(&node->siblings);
	INIT_LIST_HEAD(&node->children);
}

/**
 * __trie_node_make - Allocate new node with passed key, size and data
 * @key: string
 * @size: size of @key excluding trailing NUL byte
 * @priv: private data
 * @parent: parent node
 * @flags: flags for kernel allocation functions
 */
static struct trie_node *__trie_node_make(char *key, size_t size, void *priv,
					struct trie_node *parent, gfp_t flags)
{
	struct trie_node *node;

	node = (struct trie_node *) kvmalloc(sizeof(struct trie_node), flags);
	if (!node)
		return NULL;

	INIT_TRIE_NODE(node);
	node->size = size;
	node->key = (char *) kvmalloc(node->size, flags);
	if (!node->key) {
		kvfree(node);
		return NULL;
	}
	node->priv = priv;
	node->parent = parent;
	memcpy(node->key, key, size);
	list_add(&node->siblings, &parent->children);

	return node;
}

/**
 * __trie_compress_node - Resize node @src by provided key
 * @src: node with compress
 * @key: string
 * @size: size of @key excluding trailing NUL byte
 * @pos: position of already matched nodes
 */
static int __trie_compress_node(struct trie_node *src, char *key, size_t size,
				size_t pos, gfp_t flags)
{
	char *buf;
	size_t newsize;

	newsize = src->size + size - pos;
	buf = (char *) kvmalloc(newsize, flags);
	if (!buf)
		return -ENOMEM;
	memcpy(buf, key + pos - src->size, newsize);
	kvfree(src->key);
	src->key = buf;
	src->size = newsize;

	return 0;
}

/**
 * __trie_rotate_node - Change key and parent of provided @src node
 * @src: node to change
 * @key: string
 * @size: size of @key excluding trailing NUL byte
 * @parent: new parent node
 * @pos: position of already matched nodes
 */
static int __trie_rotate_node(struct trie_node *src, char *key, size_t size,
			      struct trie_node *parent, size_t pos, gfp_t flags)
{
	char *buf;

	buf = (char *) kvmalloc(size, flags);
	if (!buf)
		return -ENOMEM;
	memcpy(buf, key, size);
	kvfree(src->key);
	src->key = buf;
	src->size = size;
	src->parent = parent;

	list_del(&src->siblings);
	INIT_LIST_HEAD(&src->siblings);
	list_add(&src->siblings, &parent->children);

	return 0;
}

/**
 * trie_node_lookup - Lookup node matching @key and @size from @root subtree
 * if exact match was not found, return the last partially matching node in @last
 * @root: (sub)tree node root
 * @key: string
 * @size: size of @key NUL byte
 * @last: pointer to struct trie_node * which will contain the matching node
 * or a partially matching node
 * @pos: pointer to size_t that will contain size of already matched nodes,
 * @savedpfx: pointer to user variable that will contain size of character
 * matched in the last matched node.
 *
 * Pretty much the heart of this implementation. If this function at any point
 * return something less than 0, *last will contain the last matching node
 * so this is useful for implementing most operation on this trie.
 */
static int trie_node_lookup(struct trie_root *root, char *key, size_t size,
		     struct trie_node **last, size_t *pos, size_t *savedpfx)
{
	size_t foundsize = 0, pfx = 0, keysize;
	struct trie_node *anchor, *iter, *lastentry;

	anchor = (struct trie_node *) root;

search:
	lastentry = list_last_entry(&anchor->children, struct trie_node, siblings);

	if (list_empty(&anchor->children)) {
		if (last)
			*last = anchor;
		if (pos)
			*pos = foundsize;
		if (savedpfx)
			*savedpfx = pfx;
		return -ENOENT;
	}

	list_for_each_entry(iter, &anchor->children, siblings) {
		keysize = iter->size < size ? iter->size : size;
		pfx = strnpfx(iter->key, key, keysize);

		if (!pfx && (iter == lastentry)) {
			/*
			 * If no prefixes match in any child,
			 * return the parent node.
			 * Insert then is performed on a parent.
			 */
			if (last)
				*last = anchor;
			if (pos)
				*pos = foundsize;
			if (savedpfx)
				*savedpfx = pfx;
			return -ENOENT;
		} else if (pfx == iter->size) {
			/*
			 * If length if the prefix matches length of the
			 * node, then we either found the node we looked
			 * for or we need to search in its children.
			 */
			if (last)
				*last = iter;
			if (pos)
				*pos = foundsize;
			if (savedpfx)
				*savedpfx = pfx;

			if (!strncmp(iter->key, key, keysize)
					&& iter->size == size)
				return 0;
			else {
				key += pfx;
				anchor = iter;
				foundsize += pfx;
				size -= pfx;
				pfx = 0;
				goto search;
			}
		} else if (pfx < iter->size && pfx) {
			if (last)
				*last = iter;
			if (pos)
				*pos = foundsize;
			if (savedpfx)
				*savedpfx = pfx;
			return -ENOENT;
		}
	}

	return -ENOENT;
}

/**
 * trie_lookup - Lookup a node with matching @key.
 * @root: (sub)tree node root
 * @key: key string
 * @size: size of @key excluding NUL byte
 *
 * More light-hearted version of the previous function. It expected the internal
 * trie_node_lookup to be successful, otherwise return NULL
 */
static inline struct trie_node *trie_lookup(struct trie_root *root, char *key,
					    size_t size)
{
	int ret;
	struct trie_node *last;

	ret = trie_node_lookup(root, key, size, &last, NULL, NULL);

	if (ret)
		return NULL;

	return last;
}

/**
 * trie_insert - Insert a node with passed key
 * @root: (sub)tree node root
 * @key: string
 * @size: size of @key excluding NUL byte
 * @inserted: pointer to struct trie_node * which will contain the
 * inserted node pointer
 *
 * Insert must handle various cases (which was challenging to write to say the
 * least). Because this trie compresses its keys, we need to take care
 * of all kinds of splits at different cases, which are hopefully described here
 * in an understandable manner.
 */
static int trie_insert(struct trie_root *root, char *key, size_t size, void *priv,
		       struct trie_node **inserted, gfp_t flags)
{
	int ret;
	char *buf;
	size_t pos, pfxlen, count;
	struct trie_node *last, *n, *split, *suffix;

	ret = trie_node_lookup(root, key, size, &last, &pos, &pfxlen);
	if (!ret)
		return -EEXIST;

	if (inserted)
		*inserted = last;

	if (last == (struct trie_node *) root) {
		/*
		 * This is a root node, special case if we found nothing from
		 * root.
		 */
		n = __trie_node_make(key,
				     size,
				     priv,
				     (struct trie_node *) root,
				     flags);
		if (!n)
			return -ENOMEM;
		if (inserted)
			*inserted = n;
		goto ret;
	}

	count = list_count_nodes(&last->children);
	if (!pfxlen && pos > 0) {
		if (!count && !(last->priv)) {
			/*
			 * Case 1.1.
			 * If we have matched at least one previous node,
			 * but current most matching (whose children
			 * do not match) node contains no children.
			 *
			 * Here node compression happens.
			 * If @last has no children, this means we can
			 * just concat suffix of @key with @last->key.
			 */
			ret = __trie_compress_node(last, key, size, pos, flags);
			if (inserted)
				*inserted = last;
			if (ret)
				return ret;
		} else {
			/*
			 * Case 1.2.
			 * Currently matched node has children.
			 *
			 * Simply enough, put @key suffix as a child
			 * of @last.
			 */
			n = __trie_node_make(key + pos,
					     size - pos,
					     priv,
					     last,
					     flags);
			if (inserted)
				*inserted = n;
			if (!n)
				return -ENOMEM;
		}
	} else {
		/*
		 * Here are more complex cases
		 */
		if (!count) {
			/*
			 * Case 2.1.
			 * Most matching node has a matching prefix
			 * but no children.
			 *
			 * Here we split matching node into:
			 * 1. Prefix matching for both nodes,
			 * 2 & 3. Newly inserted unique suffix and
			 * suffix from split parent node
			 */

			/*
			 * this is a suffix node, split from @last
			 */
			suffix = __trie_node_make(last->key + pfxlen,
						  last->size - pfxlen,
						  last->priv,
						  last,
						  flags);
			if (!suffix)
				return -ENOMEM;

			/*
			 * @last is our prefix node
			 */
			last->size = pfxlen;
			buf = (char *) kvmalloc(last->size, flags);
			if (!buf)
				return -ENOMEM;
			memcpy(buf, last->key, pfxlen);
			kvfree(last->key);
			last->key = buf;
			last->priv = NULL;

			/*
			 * these guards prevent empty nodes from
			 * appearing if what we insert is already just
			 * a prefix that relies now in @last
			 */
			if (!(size - pos - pfxlen)) {
				last->priv = priv;
				goto ret;
			}

			n = __trie_node_make(key + pos + pfxlen,
					     size - pos - pfxlen,
					     priv,
					     last,
					     flags);
			if (!n)
				return -ENOMEM;

			if (inserted)
				*inserted = n;
		} else {
			/*
			 * Case 2.2.
			 * Most matching node has a matching prefix but
			 * has children.
			 * Seemingly similar but some more complex stuff
			 * may happen there.
			 *
			 * We do mostly the same as above, except that
			 * we need to move @last->children to a node
			 * with a matching suffix, so a placeholder node
			 * is created with matching prefixes to @last
			 * and @key, retains siblings of @last, but its
			 * children are @last and our newly created node
			 * from @key.
			 */
			split = __trie_node_make(last->key,
						 pfxlen,
						 NULL,
						 last->parent,
						 flags);
			if (!split)
				return -ENOMEM;

			ret = __trie_rotate_node(last,
						 last->key + pfxlen,
						 last->size - pfxlen,
						 split,
						 pos,
						 flags);
			if (ret)
				return ret;

			if (!(size - pos - pfxlen)) {
				split->priv = priv;
				goto ret;
			}

			n = __trie_node_make(key + pos + pfxlen,
					     size - pos - pfxlen,
					     priv,
					     split,
					     flags);
			if (!n)
				return -ENOMEM;

			if (inserted)
				*inserted = n;
		}
	}

ret:
	return 0;
}

/**
 * trie_get_string - Allocate a full string from given node by traversing up
 * @root: upper bound node
 * @leaf: lower bound node
 * @out: output buffer for collected string
 * @size: output size of collected string
 * @flags: flags for kvmalloc()
 *
 * Node traversal begins with @leaf and goes up to nodes parent.
 * Each node visited resizes the internal buffer by sum of previous suffixes
 * sizes and by its own size. The internal buffer content is then moved forward
 * by the size of currently visited node and the current nodes string is
 * prepended. @root is a node at which the traversal is stopped.
 */
static int trie_get_string(struct trie_root *root, struct trie_node *leaf,
		    char **out, size_t *size, gfp_t flags)
{
	char *buf;

	if (!size || !out)
		return -EINVAL;

	*size = 0;
	*out = NULL;

	while (leaf != (struct trie_node *) root) {
		buf = (char *) kvmalloc(*size + leaf->size + 1, flags);
		if (!buf)
			return -ENOMEM;
		memmove(buf + leaf->size, *out, *size);
		memcpy(buf, leaf->key, leaf->size);
		kvfree(*out);
		*out = buf;
		*size += leaf->size;
		leaf = leaf->parent;
	}

	return 0;
}

/**
 * trie_get_deepest_left_node - Get deepest left node starting from @root
 * @root: (sub)tree node root
 */
static inline struct trie_node *trie_get_deepest_left_node(struct trie_root *root)
{
	struct trie_node *anchor = (struct trie_node *) root;

	while (!list_empty(&anchor->children))
		anchor = list_last_entry(&anchor->children,
					 struct trie_node,
					 siblings);

	return anchor;
}

/**
 * __trie_remove_node - Remove passed node and each one above it if they contain
 * no children
 * @root: upper bound node
 * @node: node for removal
 * @itfn: visitor function pointer
 *
 * This is recursive upwards, instead of removing each key, they are removed if
 * are no longer used, meaning if they have no children. Unfortunately if we
 * inserted a node which is a subset of its child (meaning that it is not a
 * placeholder node), it is also going to be removed. I have to do
 * something about it. @root delimiters further removal
 */
static int __trie_remove_node(struct trie_root *root, struct trie_node *node,
			      void (*itfn)(struct trie_root *, struct trie_node *))
{
	struct trie_node *parent;

	while (node != (struct trie_node *) root) {
		if (!list_empty(&node->children))
			return -EFAULT;

		if (itfn)
			itfn(root, node);

		if (list_count_nodes(&node->parent->children) == 1) {
			list_del(&node->siblings);
			parent = node->parent;
			kvfree(node->key);
			kvfree(node);
			node = parent;
		} else {
			list_del(&node->siblings);
			kvfree(node->key);
			kvfree(node);
			break;
		}
	}

	return 0;
}

/**
 * trie_remove_key - Remove node with matching key
 * @root: (sub)tree node root
 * @key: string
 * @size: string size
 *
 * Utility function, finds our desired node and removes it.
 */
static inline int trie_remove_key(struct trie_root *root, char *key, size_t size)
{
	int ret;
	struct trie_node *last;

	ret = trie_node_lookup(root, key, size, &last, NULL, NULL);
	if (ret)
		return -ENOENT;

	return __trie_remove_node(root, last, NULL);
}

/**
 * trie_purge - Remove all nodes from tree
 * @root: (sub)tree root
 * @itfn: visitor function pointer
 */
static inline void trie_purge(struct trie_root *root,
			      void (*itfn)(struct trie_root *, struct trie_node *))
{
	struct trie_node *node;

	while ((node = trie_get_deepest_left_node(root))
		&& node != (struct trie_node *) root) {
		__trie_remove_node(root, node, itfn);
	}
}

/**
 * trie_iter - Iterate over leaves in the trie
 * @root: (sub)tree root
 * @itfn: visitor function pointer
 *
 * First argument in @itfn argument is a @root and the second one is a pointer
 * to currently traversed leaf.
 */
static inline int trie_iter(struct trie_root *root,
			    void (*itfn)(struct trie_root *, struct trie_node *), gfp_t flags)
{
	struct trie_node *iter, *delim = (struct trie_node *) root;
	struct __trie_selem *elem = NULL, *siter, *tmp;
	struct list_head head;

	INIT_LIST_HEAD(&head);

push:
	list_for_each_entry(iter, &delim->children, siblings) {
		elem = (struct __trie_selem *) kvmalloc(sizeof(struct __trie_selem),
							flags);
		if (!elem)
			goto nomem;
		INIT_LIST_HEAD(&elem->head);
		elem->ptr = iter;
		list_add(&elem->head, &head);
	}

	list_for_each_entry_safe(siter, tmp, &head, head) {
		iter = siter->ptr;
		if (!list_count_nodes(&iter->children)) {
			if (itfn)
				itfn(root, iter);
			list_del(&siter->head);
			kvfree(siter);
		} else {
			delim = iter;
			list_del(&siter->head);
			kvfree(siter);
			goto push;
		}
	}

	return 0;

nomem:
	list_for_each_entry_safe(siter, tmp, &head, head) {
		list_del(&siter->head);
		kvfree(siter);
	}

	return -ENOMEM;
}

/**
 * trie_iter_val - Iterate over nodes which have assigned values
 * @root: (sub)tree root
 * @itfn: visitor function pointer
 */
static inline int trie_iter_val(struct trie_root *root,
				void (*itfn)(struct trie_root *, struct trie_node *), gfp_t flags)
{
	struct trie_node *iter, *delim = (struct trie_node *) root;
	struct __trie_selem *elem = NULL, *siter, *tmp;
	struct list_head head;

	INIT_LIST_HEAD(&head);

push:
	list_for_each_entry(iter, &delim->children, siblings) {
		elem = (struct __trie_selem *) kvmalloc(sizeof(struct __trie_selem),
							flags);
		if (!elem)
			goto nomem;
		INIT_LIST_HEAD(&elem->head);
		elem->ptr = iter;
		list_add(&elem->head, &head);
	}

	list_for_each_entry_safe(siter, tmp, &head, head) {
		iter = siter->ptr;
		if (iter->priv && itfn) {
			itfn(root, iter);
		}
		delim = iter;
		list_del(&siter->head);
		kvfree(siter);
		goto push;
	}

	return 0;

nomem:
	list_for_each_entry_safe(siter, tmp, &head, head) {
		list_del(&siter->head);
		kvfree(siter);
	}

	return -ENOMEM;
}

#endif /* _TRIE_H */
