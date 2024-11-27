#ifndef __MAPS_H__
#define __MAPS_H__

#include <stdbool.h>
#include "rbtree.h"

struct nfsdb_entryMap_node {
    struct rb_node node;
    unsigned long key;
    struct nfsdb_entry **entry_list;
    unsigned long entry_count;
    unsigned long custom_data;
};

struct ulongMap_node {
    struct rb_node node;
    unsigned long key;
    unsigned long *value_list;
    unsigned long value_count;
    unsigned long value_alloc;
};

struct ulongPairMap_node {
    struct rb_node node;
    unsigned long key0;
    unsigned long key1;
    unsigned long *value_list;
    unsigned long value_count;
    unsigned long value_alloc;
};

struct stringRefMap_node {
    struct rb_node node;
    const char *key;
    unsigned long value;
};

enum file_access_type {
    FILE_ACCESS_TYPE_OPEN,
    FILE_ACCESS_TYPE_EXEC,
    FILE_ACCESS_TYPE_OPENEXEC,
};

struct nfsdb_fileMap_node {
    struct rb_node node;
    unsigned long key;
    struct nfsdb_entry **rd_entry_list;
    unsigned long *rd_entry_index;
    unsigned long rd_entry_count;
    struct nfsdb_entry **wr_entry_list;
    unsigned long *wr_entry_index;
    unsigned long wr_entry_count;
    struct nfsdb_entry **rw_entry_list;
    unsigned long *rw_entry_index;
    unsigned long rw_entry_count;
    struct nfsdb_entry **ga_entry_list;
    unsigned long *ga_entry_index;
    unsigned long ga_entry_count;
    unsigned long global_access;
    enum file_access_type access_type;
};

struct nfsdb_entryMap_node *nfsdb_entryMap_search(const struct rb_root *nfsdb_entryMap, unsigned long key);
int nfsdb_entryMap_insert(struct rb_root *nfsdb_entryMap, unsigned long key, struct nfsdb_entry **entry_list, unsigned long count);
void nfsdb_entryMap_destroy(struct rb_root *nfsdb_entryMap);
size_t nfsdb_entryMap_count(const struct rb_root *nfsdb_entryMap);
size_t nfsdb_entryMap_entry_count(const struct rb_root *nfsdb_entryMap);
struct ulongMap_node *ulongMap_search(const struct rb_root *ulongMap, unsigned long key);
int ulongMap_insert(struct rb_root *ulongMap, unsigned long key, unsigned long *value, unsigned long count, unsigned long alloc_size);
void ulongMap_destroy(struct rb_root *ulongMap);
size_t ulongMap_count(const struct rb_root *ulongMap);
size_t ulongMap_entry_count(const struct rb_root *ulongMap);
struct ulongPairMap_node *ulongPairMap_search(const struct rb_root *ulongPairMap, unsigned long key0, unsigned long key1);
int ulongPairMap_insert(struct rb_root *ulongPairMap, unsigned long key0, unsigned long key1, unsigned long *value, unsigned long count, unsigned long alloc);
void ulongPairMap_destroy(struct rb_root *ulongPairMap);
size_t ulongPairMap_count(const struct rb_root *ulongPairMap);
size_t ulongPairMap_entry_count(const struct rb_root *ulongPairMap);
struct stringRefMap_node *stringRefMap_search(const struct rb_root *stringMap, const char *key);
struct stringRefMap_node *stringRefMap_insert(struct rb_root *stringMap, const char *key, unsigned long value);
void stringRefMap_remove(struct rb_root *stringRefMap, struct stringRefMap_node *node);
void stringRefMap_destroy(struct rb_root *stringMap);
size_t stringRefMap_count(const struct rb_root *stringMap);
struct nfsdb_fileMap_node *fileMap_search(const struct rb_root *fileMap, unsigned long key);
struct nfsdb_fileMap_node *fileMap_insert_key(struct rb_root *fileMap, unsigned long key);
void fileMap_destroy(struct rb_root *fileMap);
size_t fileMap_count(const struct rb_root *fileMap);
size_t fileMap_rd_entry_count(const struct rb_root *fileMap);
size_t fileMap_wr_entry_count(const struct rb_root *fileMap);
size_t fileMap_rw_entry_count(const struct rb_root *fileMap);

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
