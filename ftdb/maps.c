#include <stddef.h>
#include <stdbool.h>

#define container_of(ptr, type, member) ({			\
  	const __typeof__( ((type *)0)->member ) *__mptr = (ptr);	\
  	(type *)( (char *)__mptr - offsetof(type,member) );})

#include "rbtree.h"
#include "utils.h"
#include "maps.h"

struct ulongMap_node* ulongMap_search(const struct rb_root* ulongMap, unsigned long key) {

	struct rb_node *node = ulongMap->rb_node;

	while (node) {
		struct ulongMap_node* data = container_of(node, struct ulongMap_node, node);

		if (key<data->key) {
			node = node->rb_left;
		}
		else if (key>data->key) {
			node = node->rb_right;
		}
		else
			return data;
	}

	return 0;
}

int ulongMap_insert(struct rb_root* ulongMap, unsigned long key, unsigned long* value, unsigned long count, unsigned long alloc_size) {

	struct ulongMap_node* data = calloc(1,sizeof(struct ulongMap_node));
	data->key = key;
	data->value_list = value;
	data->value_count = count;
	data->value_alloc = alloc_size;
	struct rb_node **new = &(ulongMap->rb_node), *parent = 0;

	/* Figure out where to put new node */
	while (*new) {
		struct ulongMap_node* this = container_of(*new, struct ulongMap_node, node);

		parent = *new;
		if (data->key<this->key)
			new = &((*new)->rb_left);
		else if (data->key>this->key)
			new = &((*new)->rb_right);
		else {
			free(data->value_list);
		    free(data);
		    return 0;
		}
	}

	/* Add new node and rebalance tree. */
	rb_link_node(&data->node, parent, new);
	rb_insert_color(&data->node, ulongMap);

	return 1;
}

void ulongMap_destroy(struct rb_root* ulongMap) {

    struct rb_node * p = rb_first(ulongMap);
    while(p) {
        struct ulongMap_node* data = (struct ulongMap_node*)p;
        rb_erase(p, ulongMap);
        p = rb_next(p);
        free(data->value_list);
        free(data);
    }
}

size_t ulongMap_count(const struct rb_root* ulongMap) {

	struct rb_node * p = rb_first(ulongMap);
	size_t count = 0;
	while(p) {
		count++;
		p = rb_next(p);
	}
	return count;
}

size_t ulongMap_entry_count(const struct rb_root* ulongMap) {

	struct rb_node * p = rb_first(ulongMap);
	size_t count = 0;
	while(p) {
		struct ulongMap_node* data = (struct ulongMap_node*)p;
		count+=data->value_count;
		p = rb_next(p);
	}
	return count;
}

/*
 * key0|key1 == otherkey0|otherkey1 : return 0
 * key0|key1 < otherkey0|otherkey1 : return -1
 * key0|key1 > otherkey0|otherkey1 : return 1
 */
static int ulongPairCompare(unsigned long key0, unsigned long key1, unsigned long otherkey0, unsigned long otherkey1) {

	if (key0>otherkey0) return 1;
	if (key0<otherkey0) return -1;
	if (key1>otherkey1) return 1;
	if (key1<otherkey1) return -1;
	return 0;
}

struct ulongPairMap_node* ulongPairMap_search(const struct rb_root* ulongPairMap, unsigned long key0, unsigned long key1) {

	struct rb_node *node = ulongPairMap->rb_node;

	while (node) {
		struct ulongPairMap_node* data = container_of(node, struct ulongPairMap_node, node);

		if (ulongPairCompare(key0,key1,data->key0,data->key1)<0) {
			node = node->rb_left;
		}
		else if (ulongPairCompare(key0,key1,data->key0,data->key1)>0) {
			node = node->rb_right;
		}
		else
			return data;
	}

	return 0;
}

int ulongPairMap_insert(struct rb_root* ulongPairMap, unsigned long key0, unsigned long key1, unsigned long* value,
		unsigned long count, unsigned long alloc) {

	struct ulongPairMap_node* data = calloc(1,sizeof(struct ulongPairMap_node));
	data->key0 = key0;
	data->key1 = key0;
	data->value_list = value;
	data->value_count = count;
	data->value_alloc = alloc;
	struct rb_node **new = &(ulongPairMap->rb_node), *parent = 0;

	/* Figure out where to put new node */
	while (*new) {
		struct ulongPairMap_node* this = container_of(*new, struct ulongPairMap_node, node);

		parent = *new;
		if (ulongPairCompare(data->key0,data->key1,this->key0,this->key1)<0)
			new = &((*new)->rb_left);
		else if (ulongPairCompare(data->key0,data->key1,this->key0,this->key1)>0)
			new = &((*new)->rb_right);
		else {
			free(data->value_list);
		    free(data);
		    return 0;
		}
	}

	/* Add new node and rebalance tree. */
	rb_link_node(&data->node, parent, new);
	rb_insert_color(&data->node, ulongPairMap);

	return 1;
}

void ulongPairMap_destroy(struct rb_root* ulongPairMap) {

    struct rb_node * p = rb_first(ulongPairMap);
    while(p) {
        struct ulongPairMap_node* data = (struct ulongPairMap_node*)p;
        rb_erase(p, ulongPairMap);
        p = rb_next(p);
        free(data->value_list);
        free(data);
    }
}

size_t ulongPairMap_count(const struct rb_root* ulongPairMap) {

	struct rb_node * p = rb_first(ulongPairMap);
	size_t count = 0;
	while(p) {
		count++;
		p = rb_next(p);
	}
	return count;
}

size_t ulongPairMap_entry_count(const struct rb_root* ulongPairMap) {

	struct rb_node * p = rb_first(ulongPairMap);
	size_t count = 0;
	while(p) {
		struct ulongPairMap_node* data = (struct ulongPairMap_node*)p;
		count+=data->value_count;
		p = rb_next(p);
	}
	return count;
}

struct stringRefMap_node* stringRefMap_search(const struct rb_root* stringRefMap, const char* key) {

	struct rb_node *node = stringRefMap->rb_node;

	while (node) {
		struct stringRefMap_node* data = container_of(node, struct stringRefMap_node, node);

		if (strcmp(key,data->key)<0) {
			node = node->rb_left;
		}
		else if (strcmp(key,data->key)>0) {
			node = node->rb_right;
		}
		else
			return data;
	}

	return 0;
}

struct stringRefMap_node* stringRefMap_insert(struct rb_root* stringRefMap, const char* key, unsigned long value) {

	struct stringRefMap_node* data = calloc(1,sizeof(struct stringRefMap_node));
	data->key = key;
	data->value = value;
	struct rb_node **new = &(stringRefMap->rb_node), *parent = 0;

	/* Figure out where to put new node */
	while (*new) {
		struct stringRefMap_node* this = container_of(*new, struct stringRefMap_node, node);

		parent = *new;
		if (strcmp(data->key,this->key)<0)
			new = &((*new)->rb_left);
		else if (strcmp(data->key,this->key)>0)
			new = &((*new)->rb_right);
		else {
		    free(data);
		    return 0;
		}
	}

	/* Add new node and rebalance tree. */
	rb_link_node(&data->node, parent, new);
	rb_insert_color(&data->node, stringRefMap);

	return data;
}

void stringRefMap_remove(struct rb_root* stringRefMap, struct stringRefMap_node* node) {
	rb_erase(&node->node, stringRefMap);
}

void stringRefMap_destroy(struct rb_root* stringRefMap) {

    struct rb_node * p = rb_first(stringRefMap);
    while(p) {
        struct stringRefMap_node* data = (struct stringRefMap_node*)p;
        rb_erase(p, stringRefMap);
        p = rb_next(p);
        free(data);
    }
}

size_t stringRefMap_count(const struct rb_root* stringRefMap) {

	struct rb_node * p = rb_first(stringRefMap);
	size_t count = 0;
	while(p) {
		count++;
		p = rb_next(p);
	}
	return count;
}

struct nfsdb_fileMap_node* fileMap_search(const struct rb_root* fileMap, unsigned long key) {

	struct rb_node *node = fileMap->rb_node;

	while (node) {
		struct nfsdb_fileMap_node* data = container_of(node, struct nfsdb_fileMap_node, node);

		if (key<data->key) {
			node = node->rb_left;
		}
		else if (key>data->key) {
			node = node->rb_right;
		}
		else
			return data;
	}

	return 0;
}

struct nfsdb_fileMap_node* fileMap_insert_key(struct rb_root* fileMap, unsigned long key) {

	struct nfsdb_fileMap_node* data = calloc(1,sizeof(struct nfsdb_fileMap_node));
	data->key = key;
	struct rb_node **new = &(fileMap->rb_node), *parent = 0;

	/* Figure out where to put new node */
	while (*new) {
		struct nfsdb_fileMap_node* this = container_of(*new, struct nfsdb_fileMap_node, node);

		parent = *new;
		if (data->key<this->key)
			new = &((*new)->rb_left);
		else if (data->key>this->key)
			new = &((*new)->rb_right);
		else {
		    return 0;
		}
	}

	/* Add new node and rebalance tree. */
	rb_link_node(&data->node, parent, new);
	rb_insert_color(&data->node, fileMap);

	return data;
}

void fileMap_destroy(struct rb_root* fileMap) {

    struct rb_node * p = rb_first(fileMap);
    while(p) {
        struct nfsdb_fileMap_node* data = (struct nfsdb_fileMap_node*)p;
        rb_erase(p, fileMap);
        p = rb_next(p);
        free(data->rd_entry_list);
        free(data->wr_entry_list);
        free(data->rw_entry_list);
        free(data);
    }
}

size_t fileMap_count(const struct rb_root* fileMap) {

	struct rb_node * p = rb_first(fileMap);
	size_t count = 0;
	while(p) {
		count++;
		p = rb_next(p);
	}
	return count;
}

size_t fileMap_rd_entry_count(const struct rb_root* fileMap) {

	struct rb_node * p = rb_first(fileMap);
	size_t count = 0;
	while(p) {
		struct nfsdb_fileMap_node* data = (struct nfsdb_fileMap_node*)p;
		count+=data->rd_entry_count;
		p = rb_next(p);
	}
	return count;
}

size_t fileMap_wr_entry_count(const struct rb_root* fileMap) {

	struct rb_node * p = rb_first(fileMap);
	size_t count = 0;
	while(p) {
		struct nfsdb_fileMap_node* data = (struct nfsdb_fileMap_node*)p;
		count+=data->wr_entry_count;
		p = rb_next(p);
	}
	return count;
}

size_t fileMap_rw_entry_count(const struct rb_root* fileMap) {

	struct rb_node * p = rb_first(fileMap);
	size_t count = 0;
	while(p) {
		struct nfsdb_fileMap_node* data = (struct nfsdb_fileMap_node*)p;
		count+=data->rw_entry_count;
		p = rb_next(p);
	}
	return count;
}

struct ulong_entryMap_node* ulong_entryMap_search(const struct rb_root* ulong_entryMap, unsigned long key) {

	struct rb_node *node = ulong_entryMap->rb_node;

	while (node) {
		struct ulong_entryMap_node* data = container_of(node, struct ulong_entryMap_node, node);

		if (key<data->key) {
			node = node->rb_left;
		}
		else if (key>data->key) {
			node = node->rb_right;
		}
		else
			return data;
	}

	return 0;
}

int ulong_entryMap_insert(struct rb_root* ulong_entryMap, unsigned long key, void* entry) {

	struct ulong_entryMap_node* data = calloc(1,sizeof(struct ulong_entryMap_node));
	data->key = key;
	data->entry = entry;
	struct rb_node **new = &(ulong_entryMap->rb_node), *parent = 0;

	/* Figure out where to put new node */
	while (*new) {
		struct ulong_entryMap_node* this = container_of(*new, struct ulong_entryMap_node, node);

		parent = *new;
		if (data->key<this->key)
			new = &((*new)->rb_left);
		else if (data->key>this->key)
			new = &((*new)->rb_right);
		else {
		    free(data);
		    return 0;
		}
	}

	/* Add new node and rebalance tree. */
	rb_link_node(&data->node, parent, new);
	rb_insert_color(&data->node, ulong_entryMap);

	return 1;
}

void ulong_entryMap_destroy(struct rb_root* ulong_entryMap) {

    struct rb_node * p = rb_first(ulong_entryMap);
    while(p) {
        struct ulong_entryMap_node* data = (struct ulong_entryMap_node*)p;
        rb_erase(p, ulong_entryMap);
        p = rb_next(p);
        free(data);
    }
}

size_t ulong_entryMap_count(const struct rb_root* ulong_entryMap) {

	struct rb_node * p = rb_first(ulong_entryMap);
	size_t count = 0;
	while(p) {
		count++;
		p = rb_next(p);
	}
	return count;
}

struct stringRef_entryMap_node* stringRef_entryMap_search(const struct rb_root* stringRef_entryMap, const char* key) {

	struct rb_node *node = stringRef_entryMap->rb_node;

	while (node) {
		struct stringRef_entryMap_node* data = container_of(node, struct stringRef_entryMap_node, node);

		if (strcmp(key,data->key)<0) {
			node = node->rb_left;
		}
		else if (strcmp(key,data->key)>0) {
			node = node->rb_right;
		}
		else
			return data;
	}

	return 0;
}

int stringRef_entryMap_insert(struct rb_root* stringRef_entryMap, const char* key, void* entry) {

	struct stringRef_entryMap_node* data = calloc(1,sizeof(struct stringRef_entryMap_node));
	data->key = key;
	data->entry = entry;
	struct rb_node **new = &(stringRef_entryMap->rb_node), *parent = 0;

	/* Figure out where to put new node */
	while (*new) {
		struct stringRef_entryMap_node* this = container_of(*new, struct stringRef_entryMap_node, node);

		parent = *new;
		if (strcmp(data->key,this->key)<0)
			new = &((*new)->rb_left);
		else if (strcmp(data->key,this->key)>0)
			new = &((*new)->rb_right);
		else {
		    free(data);
		    return 0;
		}
	}

	/* Add new node and rebalance tree. */
	rb_link_node(&data->node, parent, new);
	rb_insert_color(&data->node, stringRef_entryMap);

	return 1;
}

void stringRef_entryMap_destroy(struct rb_root* stringRef_entryMap) {

    struct rb_node * p = rb_first(stringRef_entryMap);
    while(p) {
        struct stringRef_entryMap_node* data = (struct stringRef_entryMap_node*)p;
        rb_erase(p, stringRef_entryMap);
        p = rb_next(p);
        free(data);
    }
}

size_t stringRef_entryMap_count(const struct rb_root* stringRef_entryMap) {

	struct rb_node * p = rb_first(stringRef_entryMap);
	size_t count = 0;
	while(p) {
		count++;
		p = rb_next(p);
	}
	return count;
}

struct stringRef_entryListMap_node* stringRef_entryListMap_search(const struct rb_root* stringRef_entryListMap, const char* key) {

	struct rb_node *node = stringRef_entryListMap->rb_node;

	while (node) {
		struct stringRef_entryListMap_node* data = container_of(node, struct stringRef_entryListMap_node, node);

		if (strcmp(key,data->key)<0) {
			node = node->rb_left;
		}
		else if (strcmp(key,data->key)>0) {
			node = node->rb_right;
		}
		else
			return data;
	}

	return 0;
}

int stringRef_entryListMap_insert(struct rb_root* stringRef_entryListMap, const char* key, void** entry_list, unsigned long entry_count) {

	struct stringRef_entryListMap_node* data = calloc(1,sizeof(struct stringRef_entryListMap_node));
	data->key = key;
	data->entry_list = entry_list;
	data->entry_count = entry_count;
	struct rb_node **new = &(stringRef_entryListMap->rb_node), *parent = 0;

	/* Figure out where to put new node */
	while (*new) {
		struct stringRef_entryListMap_node* this = container_of(*new, struct stringRef_entryListMap_node, node);

		parent = *new;
		if (strcmp(data->key,this->key)<0)
			new = &((*new)->rb_left);
		else if (strcmp(data->key,this->key)>0)
			new = &((*new)->rb_right);
		else {
			free(data->entry_list);
			free(data);
			return 0;
		}
	}

	/* Add new node and rebalance tree. */
	rb_link_node(&data->node, parent, new);
	rb_insert_color(&data->node, stringRef_entryListMap);

	return 1;
}

void stringRef_entryListMap_destroy(struct rb_root* stringRef_entryListMap) {

    struct rb_node * p = rb_first(stringRef_entryListMap);
    while(p) {
        struct stringRef_entryListMap_node* data = (struct stringRef_entryListMap_node*)p;
        rb_erase(p, stringRef_entryListMap);
        p = rb_next(p);
        free(data->entry_list);
        free(data);
    }
}

size_t stringRef_entryListMap_count(const struct rb_root* stringRef_entryListMap) {

	struct rb_node * p = rb_first(stringRef_entryListMap);
	size_t count = 0;
	while(p) {
		count++;
		p = rb_next(p);
	}
	return count;
}

size_t stringRef_entryListMap_entry_count(const struct rb_root* stringRef_entryListMap) {

	struct rb_node * p = rb_first(stringRef_entryListMap);
	size_t count = 0;
	while(p) {
		struct stringRef_entryListMap_node* data = (struct stringRef_entryListMap_node*)p;
		count+=data->entry_count;
		p = rb_next(p);
	}
	return count;
}
