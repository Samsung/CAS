#ifndef __MAPS_H__
#define __MAPS_H__

#include <stdbool.h>
#include "rbtree.h"

struct stringRefMap_node {
    struct rb_node node;
    const char *key;
    unsigned long value;
};

struct stringRefMap_node *stringRefMap_search(const struct rb_root *stringMap, const char *key);
struct stringRefMap_node *stringRefMap_insert(struct rb_root *stringMap, const char *key, unsigned long value);
void stringRefMap_remove(struct rb_root *stringRefMap, struct stringRefMap_node *node);
void stringRefMap_destroy(struct rb_root *stringMap);
size_t stringRefMap_count(const struct rb_root *stringMap);

struct ulong_entryMap_node {
    struct rb_node node;
    unsigned long key;
    void *entry;
};

typedef struct ulong_entryMap_node ftdb_ulong_type_entryMap;
typedef struct ulong_entryMap_node ftdb_ulong_func_entryMap;
typedef struct ulong_entryMap_node ftdb_ulong_funcdecl_entryMap;
typedef struct ulong_entryMap_node ftdb_ulong_global_entryMap;
typedef struct ulong_entryMap_node ftdb_ulong_static_funcs_map_entryMap;

struct stringRef_entryMap_node {
    struct rb_node node;
    const char *key;
    void *entry;
};

typedef struct stringRef_entryMap_node ftdb_stringRef_type_entryMap;
typedef struct stringRef_entryMap_node ftdb_stringRef_func_entryMap;
typedef struct stringRef_entryMap_node ftdb_stringRef_funcdecl_entryMap;
typedef struct stringRef_entryMap_node ftdb_stringRef_global_entryMap;
typedef struct stringRef_entryMap_node ftdb_stringRef_BAS_data_entryMap;

struct stringRef_entryListMap_node {
    struct rb_node node;
    const char *key;
    void **entry_list;
    unsigned long entry_count;
};

typedef struct stringRef_entryListMap_node ftdb_stringRef_func_entryListMap;
typedef struct stringRef_entryListMap_node ftdb_stringRef_global_entryListMap;

struct ulong_entryMap_node *ulong_entryMap_search(const struct rb_root *ulong_entryMap, unsigned long key);
int ulong_entryMap_insert(struct rb_root *ulong_entryMap, unsigned long key, void *entry);
void ulong_entryMap_destroy(struct rb_root *ulong_entryMap);
size_t ulong_entryMap_count(const struct rb_root *ulong_entryMap);

struct stringRef_entryMap_node *stringRef_entryMap_search(const struct rb_root *stringRef_entryMap, const char *key);
int stringRef_entryMap_insert(struct rb_root *stringRef_entryMap, const char *key, void *entry);
void stringRef_entryMap_destroy(struct rb_root *stringRef_entryMap);
size_t stringRef_entryMap_count(const struct rb_root *stringRef_entryMap);

struct stringRef_entryListMap_node *stringRef_entryListMap_search(const struct rb_root *stringRef_entryListMap, const char *key);
int stringRef_entryListMap_insert(struct rb_root *stringRef_entryListMap, const char *key, void **entry_list, unsigned long entry_count);
void stringRef_entryListMap_destroy(struct rb_root *stringRef_entryListMap);
size_t stringRef_entryListMap_count(const struct rb_root *stringRef_entryListMap);
size_t stringRef_entryListMap_entry_count(const struct rb_root *stringRef_entryListMap);

#endif /* __MAPS_H__ */
