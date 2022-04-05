/*
  Red Black Trees
  (C) 1999  Andrea Arcangeli <andrea@suse.de>
  
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

  linux/include/linux/rbtree.h

  To use rbtrees you'll have to implement your own insert and search cores.
  This will avoid us to use callbacks and to drop drammatically performances.
  I know it's not the cleaner way,  but in C (not in C++) to get
  performances and genericity...

  See Documentation/rbtree.txt for documentation and samples.
*/

#ifndef	_LINUX_RBTREE_H
#define	_LINUX_RBTREE_H

#ifdef __linux__
#define _ALIGNAS(n) __attribute__((aligned(n)))
#define RB_NODE_ALIGN (sizeof(long))
#else
#ifdef _WIN32
#define _ALIGNAS(n) __declspec(align(n))
#ifdef _M_IX86
#define RB_NODE_ALIGN 4
#elif defined _M_X64
#define RB_NODE_ALIGN 8
#endif
#endif  /* _WIN32 */
#endif /* __linux__ */

#ifdef __linux__
#include <alloca.h>
#define ALLOCA(x) alloca(x)
#else
#ifdef _WIN32
#include <malloc.h>
#define ALLOCA(x) _malloca(x)
#endif
#endif

#ifdef __linux__
#if defined __cplusplus
#define container_of(ptr, type, member) ({      \
  const decltype( ((type *)0)->member ) *__mptr = (decltype( ((type *)0)->member )*)(ptr);  \
  (type *)( (char *)__mptr - offsetof(type,member) );})
#else
#define container_of(ptr, type, member) ({      \
  const typeof( ((type *)0)->member ) *__mptr = (typeof( ((type *)0)->member )*)(ptr);  \
  (type *)( (char *)__mptr - offsetof(type,member) );})
#endif
#else
#ifdef _WIN32
#define container_of(ptr, type, member) (type *)( (char *)(ptr) - offsetof(type,member) )
#endif
#endif

#include <stdint.h>

struct _ALIGNAS(RB_NODE_ALIGN) rb_node {
  uintptr_t  __rb_parent_color;
  struct rb_node *rb_right;
  struct rb_node *rb_left;
};
/* The alignment might seem pointless, but allegedly CRIS needs it */

struct rb_root {
  struct rb_node *rb_node;
};

#define rb_parent(r)   ((struct rb_node *)((r)->__rb_parent_color & ~3))

#define RB_ROOT	{ 0, }
#define	rb_entry(ptr, type, member) container_of(ptr, type, member)

#define RB_EMPTY_ROOT(root)  ((root)->rb_node == 0)

/* 'empty' nodes are nodes that are known not to be inserted in an rbtree */
#define RB_EMPTY_NODE(node)  \
	((node)->__rb_parent_color == (uintptr_t)(node))
#define RB_CLEAR_NODE(node)  \
	((node)->__rb_parent_color = (uintptr_t)(node))

#if defined __cplusplus
extern "C" {
#endif
	extern void rb_insert_color(struct rb_node *, struct rb_root *);
	extern void rb_erase(struct rb_node *, struct rb_root *);
	/* Find logical next and previous nodes in a tree */
	extern struct rb_node *rb_next(const struct rb_node *);
	extern struct rb_node *rb_prev(const struct rb_node *);
	extern struct rb_node *rb_first(const struct rb_root *);
	extern struct rb_node *rb_last(const struct rb_root *);
#if defined __cplusplus
}
#endif

/* Fast replacement of a single node without remove/rebalance/add/rebalance */
extern void rb_replace_node(struct rb_node *victim, struct rb_node *_new,
			    struct rb_root *root);

static inline void rb_link_node(struct rb_node * node, struct rb_node * parent,
				struct rb_node ** rb_link)
{
	node->__rb_parent_color = (uintptr_t)parent;
	node->rb_left = node->rb_right = 0;

	*rb_link = node;
}

#endif	/* _LINUX_RBTREE_H */
