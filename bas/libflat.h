#ifndef __LIBFLAT_H__
#define __LIBFLAT_H__

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <memory.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>

#ifdef __linux__
#include <sys/time.h>
#else
#ifdef _WIN32
#include "wintime.h"
#endif
#endif

#include "rbtree.h"
#include "utils.h"

struct interval_tree_node {
	struct rb_node rb;
	uintptr_t start;	/* Start of interval */
	uintptr_t last;	/* Last location _in_ interval */
	uintptr_t __subtree_last;
	struct blstream* storage;
};

struct flatten_pointer {
	struct interval_tree_node* node;
	size_t offset;
};

struct flatten_header {
	size_t memory_size;
	size_t ptr_count;
	size_t fptr_count;
	size_t root_addr_count;
	uintptr_t fix_base;
	uint64_t magic;
};

#include "interval_tree.h"

#define CONFIG_DEBUG_LEVEL	3
#define __LIBFLAT_VERSION__ "0.95"
static const volatile char version[] = __LIBFLAT_VERSION__;

#define DBGL(n,...)		do { if (n<=CONFIG_DEBUG_LEVEL)	printf(__VA_ARGS__); } while(0)

#define FLATTEN_MAGIC 0x464c415454454e00ULL

/* Private debugging facilities. In order to use them include their prototypes in the source file */
/* 0001 : debug flattening stage
 * 0002 : debug memory fixing
 */
void flatten_debug_info();
void flatten_set_debug_flag(int flag);
void flatten_debug_memory();

/* Binary stream doubly linked list implementation */

struct blstream* binary_stream_append_reserve(size_t size);
struct blstream* binary_stream_insert_front_reserve(size_t size, struct blstream* where);
struct blstream* binary_stream_insert_back_reserve(size_t size, struct blstream* where);
struct blstream* binary_stream_update(const void* data, size_t size, struct blstream* where);
void binary_stream_calculate_index();
void binary_stream_update_pointers();
void binary_stream_destroy();
void binary_stream_print();
size_t binary_stream_size();
size_t binary_stream_write(FILE* f);

/* Set of memory fixup elements implemented using red-black tree */

struct fixup_set_node {
	struct rb_node node;
  	/* Storage area and offset where the original address to be fixed is stored */
	struct interval_tree_node* inode;
	size_t offset;
	/* Storage area and offset where the original address points to */
	struct flatten_pointer* ptr;
};

struct fixup_set_node* fixup_set_search(uintptr_t v);
void fixup_set_print();
size_t fixup_set_count();
void fixup_set_destroy();
size_t fixup_set_write(FILE* f);

/* Root address list */
struct root_addrnode {
	struct root_addrnode* next;
	uintptr_t root_addr;
};
size_t root_addr_count();

void fix_unflatten_memory(struct flatten_header* hdr, void* memory);

struct interval_nodelist {
	struct interval_nodelist* next;
	struct interval_tree_node* node;
};

/* Main interface functions */

void flatten_init();
int flatten_write(FILE* f);
void flatten_fini();
void unflatten_init();
int unflatten_read(FILE* f);
int unflatten_map(int fd, off_t offset);
int unflatten_map_rdonly(int fd, off_t offset);
void unflatten_fini();

enum flatten_option {
	option_silent = 0x01,
};

void flatten_set_option(int option);
void flatten_clear_option(int option);

/* Implementation */

#ifdef __linux__
#define _ALIGNAS(n)	__attribute__((aligned(n)))
#define RB_NODE_ALIGN	(sizeof(long))
#else
#ifdef _WIN32
#define _ALIGNAS(n)	__declspec(align(n))
#ifdef _M_IX86
#define RB_NODE_ALIGN	4
#elif defined _M_X64
#define RB_NODE_ALIGN	8
#endif
#endif	/* _WIN32 */
#endif /* __linux__ */

#ifdef __linux__
#include <alloca.h>
#define ALLOCA(x)	alloca(x)
#else
#ifdef _WIN32
#include <malloc.h>
#define ALLOCA(x)	_malloca(x)
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

#define MAX_FLCTRL_RECURSION_DEPTH  16
#define FLCTRL  (FLCTRL_ARR[FLCTRL_INDEX])

struct FLCONTROL {
	struct blstream* bhead;
	struct blstream* btail;
	struct rb_root fixup_set_root;
	struct rb_root imap_root;
	struct flatten_header	HDR;
	struct root_addrnode* rhead;
	struct root_addrnode* rtail;
	struct root_addrnode* last_accessed_root;
	int debug_flag;
	unsigned long option;
	void* mem;
	size_t map_size;
	void* map_mem;
};

extern struct FLCONTROL FLCTRL_ARR[MAX_FLCTRL_RECURSION_DEPTH];
extern int FLCTRL_INDEX;
extern struct flatten_pointer* flatten_plain_type(const void* _ptr, size_t _sz);
extern int fixup_set_insert(struct interval_tree_node* node, size_t offset, struct flatten_pointer* ptr);
extern int fixup_set_insert_force_update(struct interval_tree_node* node, size_t offset, struct flatten_pointer* ptr);
extern struct fixup_set_node *fixup_set_search(uintptr_t v);
extern int fixup_set_reserve(struct interval_tree_node* node, size_t offset);
extern int fixup_set_reserve_address(uintptr_t addr);
extern int fixup_set_update(struct interval_tree_node* node, size_t offset, struct flatten_pointer* ptr);
extern struct flatten_pointer* get_pointer_node(const void* _ptr);
extern void root_addr_append(size_t root_addr);
void* root_pointer_next();
void* root_pointer_seq(size_t index);

extern struct interval_tree_node * interval_tree_iter_first(struct rb_root *root, uintptr_t start, uintptr_t last);
struct interval_tree_node *	interval_tree_iter_next(struct interval_tree_node *node, uintptr_t start, uintptr_t last);
extern struct rb_node* interval_tree_insert(struct interval_tree_node *node, struct rb_root *root);

struct blstream {
	struct blstream* next;
	struct blstream* prev;
	void* data;
	size_t size;
	size_t index;
	size_t alignment;
	size_t align_offset;
};

struct blstream* binary_stream_insert_back(const void* data, size_t size, struct blstream* where);
struct blstream* binary_stream_insert_front(const void* data, size_t size, struct blstream* where);
struct blstream* binary_stream_append(const void* data, size_t size);
struct rb_node *rb_next(const struct rb_node *node);
struct rb_node *rb_prev(const struct rb_node *node);

#define DEFAULT_ITER_QUEUE_SIZE (1024*1024*8)

struct queue_block {
    struct queue_block* next;
    unsigned char data[];
};

struct bqueue {
    size_t block_size;
    size_t size;
    struct queue_block* front_block;
    size_t front_index;
    struct queue_block* back_block;
    size_t back_index;
};

void bqueue_init(struct bqueue* q, size_t block_size);
void bqueue_destroy(struct bqueue* q);
int bqueue_empty(struct bqueue* q);
void bqueue_push_back(struct bqueue* q, const void* m, size_t s);
void bqueue_pop_front(struct bqueue* q, void* m, size_t s);

struct flatten_base {};

typedef struct flatten_pointer* (*flatten_struct_t)(const struct flatten_base*, size_t n, struct bqueue*);
typedef struct flatten_pointer* (*flatten_struct_mixed_convert_t)(struct flatten_pointer*, const struct flatten_base*);

struct flatten_job {
    struct interval_tree_node* node;
    size_t offset;
    size_t size;
    struct flatten_base* ptr;
    flatten_struct_t fun;
    /* Mixed pointer support */
    const struct flatten_base* fp;
    flatten_struct_mixed_convert_t convert;
};

static inline struct flatten_pointer* make_flatten_pointer(struct interval_tree_node* node, size_t offset) {
	struct flatten_pointer* v = malloc(sizeof(struct flatten_pointer));
	assert(v!=0);
	v->node = node;
	v->offset = offset;
	return v;
}

static inline size_t strmemlen(const char* s) {
	return strlen(s)+1;
}

static inline size_t ptrarrmemlen(const void* const* m) {
	size_t count=1;
	while(*m) {
		count++;
		m++;
	}
	return count;
}

#define LIBFLAT_DBGM(...)						do { if (FLCTRL.debug_flag&1) printf(__VA_ARGS__); } while(0)
#define LIBFLAT_DBGM1(name,a1)					do { if (FLCTRL.debug_flag&1) printf(#name "(" #a1 ")\n"); } while(0)
#define LIBFLAT_DBGF(name,F,FMT,P)				do { if (FLCTRL.debug_flag&1) printf(#name "(" #F "[" FMT "])\n",P); } while(0)
#define LIBFLAT_DBGM2(name,a1,a2)				do { if (FLCTRL.debug_flag&1) printf(#name "(" #a1 "," #a2 ")\n"); } while(0)
#define LIBFLAT_DBGTF(name,T,F,FMT,P)			do { if (FLCTRL.debug_flag&1) printf(#name "(" #T "," #F "[" FMT "])\n",P); } while(0)
#define LIBFLAT_DBGTFMF(name,T,F,FMT,P,PF,FF)	do { if (FLCTRL.debug_flag&1) printf(#name "(" #T "," #F "[" FMT "]," #PF "," #FF ")\n",P); } while(0)
#define LIBFLAT_DBGTP(name,T,P)					do { if (FLCTRL.debug_flag&1) printf(#name "(" #T "," #P "[%p])\n",P); } while(0)
#define LIBFLAT_DBGM3(name,a1,a2,a3)			do { if (FLCTRL.debug_flag&1) printf(#name "(" #a1 "," #a2 "," #a3 ")\n"); } while(0)
#define LIBFLAT_DBGM4(name,a1,a2,a3,a4)			do { if (FLCTRL.debug_flag&1) printf(#name "(" #a1 "," #a2 "," #a3 "," #a4 ")\n"); } while(0)

#define ATTR(f)	((_ptr)->f)

#define STRUCT_ALIGN(n)		do { _alignment=n; } while(0)

#define FUNCTION_DEFINE_FLATTEN_STRUCT_ARRAY(FLTYPE)	\
struct flatten_pointer* flatten_struct_##FLTYPE##_array(const struct FLTYPE* _ptr, size_t n) {	\
    size_t _i;	\
	void* _fp_first=0;	\
	for (_i=0; _i<n; ++_i) {	\
		void* _fp = (void*)flatten_struct_##FLTYPE(_ptr+_i);	\
		if (!_fp_first) _fp_first=_fp;	\
		else free(_fp);	\
	}	\
    return _fp_first;	\
}

#define FUNCTION_DECLARE_FLATTEN_STRUCT_ARRAY(FLTYPE)	\
	extern struct flatten_pointer* flatten_struct_##FLTYPE##_array(const struct FLTYPE* _ptr, size_t n);

#define FUNCTION_DEFINE_FLATTEN_STRUCT2_ARRAY(FLTYPE)	\
struct flatten_pointer* flatten_struct2_##FLTYPE##_array(const struct FLTYPE* _ptr, size_t n) {	\
    size_t _i;	\
	void* _fp_first=0;	\
	for (_i=0; _i<n; ++_i) {	\
		void* _fp = (void*)flatten_struct2_##FLTYPE(_ptr+_i);	\
		if (!_fp_first) _fp_first=_fp;	\
		else free(_fp);	\
	}	\
    return _fp_first;	\
}

#define FUNCTION_DECLARE_FLATTEN_STRUCT2_ARRAY(FLTYPE)	\
	extern struct flatten_pointer* flatten_struct2_##FLTYPE##_array(const struct FLTYPE* _ptr, size_t n);

#define FUNCTION_DEFINE_FLATTEN_STRUCT_TYPE_ARRAY(FLTYPE)	\
struct flatten_pointer* flatten_struct_type_##FLTYPE##_array(const FLTYPE* _ptr, size_t n) {	\
    size_t _i;	\
	void* _fp_first=0;	\
	for (_i=0; _i<n; ++_i) {	\
		void* _fp = (void*)flatten_struct_type_##FLTYPE(_ptr+_i);	\
		if (!_fp_first) _fp_first=_fp;	\
		else free(_fp);	\
	}	\
    return _fp_first;	\
}

#define FUNCTION_DECLARE_FLATTEN_STRUCT_TYPE_ARRAY(FLTYPE)	\
	extern struct flatten_pointer* flatten_struct_type_##FLTYPE##_array(const FLTYPE* _ptr, size_t n);

#define FUNCTION_DEFINE_FLATTEN_STRUCT_ARRAY_ITER(FLTYPE) \
struct flatten_pointer* flatten_struct_##FLTYPE##_array_iter(const struct FLTYPE* _ptr, size_t n, struct bqueue* __q) {    \
    size_t _i;  \
    void* _fp_first=0;  \
	for (_i=0; _i<n; ++_i) {    \
		void* _fp = (void*)flatten_struct_##FLTYPE##_iter(_ptr+_i,__q);    \
		if (!_fp_first) _fp_first=_fp;  \
		else free(_fp); \
	}   \
	return _fp_first;   \
}

#define FUNCTION_DECLARE_FLATTEN_STRUCT_ARRAY_ITER(FLTYPE)	\
	struct flatten_pointer* flatten_struct_##FLTYPE##_array_iter(const struct FLTYPE* _ptr, size_t n, struct bqueue* __q);

#define FUNCTION_DEFINE_FLATTEN_STRUCT2_ARRAY_ITER(FLTYPE) \
struct flatten_pointer* flatten_struct2_##FLTYPE##_array_iter(const struct FLTYPE* _ptr, size_t n, struct bqueue* __q) {    \
    size_t _i;  \
    void* _fp_first=0;  \
	for (_i=0; _i<n; ++_i) {    \
		void* _fp = (void*)flatten_struct2_##FLTYPE##_iter(_ptr+_i,__q);    \
		if (!_fp_first) _fp_first=_fp;  \
		else free(_fp); \
	}   \
	return _fp_first;   \
}

#define FUNCTION_DEFINE_FLATTEN_STRUCT_TYPE2_ARRAY_ITER(FLTYPE) \
struct flatten_pointer* flatten_struct_type2_##FLTYPE##_array_iter(const FLTYPE* _ptr, size_t n, struct bqueue* __q) {    \
    size_t _i;  \
    void* _fp_first=0;  \
	for (_i=0; _i<n; ++_i) {    \
		void* _fp = (void*)flatten_struct_type2_##FLTYPE##_iter(_ptr+_i,__q);    \
		if (!_fp_first) _fp_first=_fp;  \
		else free(_fp); \
	}   \
	return _fp_first;   \
}

#define FUNCTION_DECLARE_FLATTEN_STRUCT2_ARRAY_ITER(FLTYPE)	\
	extern struct flatten_pointer* flatten_struct2_##FLTYPE##_array_iter(const struct FLTYPE* _ptr, size_t n, struct bqueue* __q);

#define FUNCTION_DECLARE_FLATTEN_STRUCT_TYPE2_ARRAY_ITER(FLTYPE)	\
	extern struct flatten_pointer* flatten_struct_type2_##FLTYPE##_array_iter(const FLTYPE* _ptr, size_t n, struct bqueue* __q);

#define FUNCTION_DEFINE_FLATTEN_STRUCT_TYPE_ARRAY_ITER(FLTYPE) \
struct flatten_pointer* flatten_struct_type_iter_##FLTYPE##_array(const struct FLTYPE* _ptr, size_t n, struct bqueue* __q) {    \
    size_t _i;  \
    void* _fp_first=0;  \
	for (_i=0; _i<n; ++_i) {    \
		void* _fp = (void*)flatten_struct_type_##FLTYPE##_iter(_ptr+_i,__q);    \
		if (!_fp_first) _fp_first=_fp;  \
		else free(_fp); \
	}   \
	return _fp_first;   \
}

#define FUNCTION_DECLARE_FLATTEN_STRUCT_TYPE_ARRAY_ITER(FLTYPE) \
	extern struct flatten_pointer* flatten_struct_type_iter_##FLTYPE##_array(const struct FLTYPE* _ptr, size_t n, struct bqueue* __q);

#define FUNCTION_DEFINE_FLATTEN_STRUCT(FLTYPE,...)	\
/* */		\
			\
struct flatten_pointer* flatten_struct_##FLTYPE(const struct FLTYPE* _ptr) {    \
            \
    typedef struct FLTYPE _container_type;  \
    size_t _alignment = 0;  \
            \
    struct interval_tree_node *__node = interval_tree_iter_first(&FLCTRL.imap_root, (uint64_t)_ptr, (uint64_t)_ptr+sizeof(struct FLTYPE)-1);    \
    LIBFLAT_DBGM("flatten_struct_" #FLTYPE "(%lx): [%zu]\n",(uintptr_t)_ptr,sizeof(struct FLTYPE));	\
    if (__node) {   \
        assert(__node->start==(uint64_t)_ptr);  \
        assert(__node->last==(uint64_t)_ptr+sizeof(struct FLTYPE)-1);   \
        return make_flatten_pointer(__node,0);  \
    }   \
    else {  \
        __node = calloc(1,sizeof(struct interval_tree_node));   \
        assert(__node!=0);  \
        __node->start = (uint64_t)_ptr; \
        __node->last = (uint64_t)_ptr + sizeof(struct FLTYPE)-1;    \
        struct blstream* storage;   \
        struct rb_node* rb = interval_tree_insert(__node, &FLCTRL.imap_root);   \
        struct rb_node* prev = rb_prev(rb); \
        if (prev) { \
            storage = binary_stream_insert_back(_ptr,sizeof(struct FLTYPE),((struct interval_tree_node*)prev)->storage);    \
        }   \
        else {  \
            struct rb_node* next = rb_next(rb); \
            if (next) { \
                storage = binary_stream_insert_front(_ptr,sizeof(struct FLTYPE),((struct interval_tree_node*)next)->storage);   \
            }   \
            else {  \
                storage = binary_stream_append(_ptr,sizeof(struct FLTYPE)); \
            }   \
        }   \
        __node->storage = storage;  \
    }   \
        \
    __VA_ARGS__ \
    __node->storage->alignment = _alignment;    \
    return make_flatten_pointer(__node,0);  \
}	\
\
FUNCTION_DEFINE_FLATTEN_STRUCT_ARRAY(FLTYPE)

#define FUNCTION_DECLARE_FLATTEN_STRUCT(FLTYPE)	\
	extern struct flatten_pointer* flatten_struct_##FLTYPE(const struct FLTYPE*);	\
	FUNCTION_DECLARE_FLATTEN_STRUCT_ARRAY(FLTYPE)

#define FUNCTION_DEFINE_FLATTEN_STRUCT2(FLTYPE,...)	\
/* */		\
			\
struct flatten_pointer* flatten_struct2_##FLTYPE(const struct FLTYPE* _ptr) {    \
            \
    typedef struct FLTYPE _container_type;  \
    size_t _alignment = 0;  \
            \
    struct interval_tree_node *__node = interval_tree_iter_first(&FLCTRL.imap_root, (uint64_t)_ptr, (uint64_t)_ptr+sizeof(struct FLTYPE)-1);    \
    LIBFLAT_DBGM("flatten_struct_" #FLTYPE "(%lx): [%zu]\n",(uintptr_t)_ptr,sizeof(struct FLTYPE));	\
    if (__node) {   \
    	uintptr_t p = (uintptr_t)_ptr;	\
    	struct interval_tree_node *prev;	\
    	while(__node) {	\
			if (__node->start>p) {	\
				assert(__node->storage!=0);	\
				struct interval_tree_node* nn = calloc(1,sizeof(struct interval_tree_node));	\
				assert(nn!=0);	\
				nn->start = p;	\
				nn->last = __node->start-1;	\
				nn->storage = binary_stream_insert_front((void*)p,__node->start-p,__node->storage);	\
				interval_tree_insert(nn, &FLCTRL.imap_root);	\
			}	\
			p = __node->last+1;	\
			prev = __node;	\
			__node = interval_tree_iter_next(__node, (uintptr_t)_ptr, (uintptr_t)_ptr+sizeof(struct FLTYPE)-1);	\
		}	\
		if ((uintptr_t)_ptr+sizeof(struct FLTYPE)>p) {	\
			assert(prev->storage!=0);	\
			struct interval_tree_node* nn = calloc(1,sizeof(struct interval_tree_node));	\
			assert(nn!=0);	\
			nn->start = p;	\
			nn->last = (uintptr_t)_ptr+sizeof(struct FLTYPE)-1;	\
			nn->storage = binary_stream_insert_back((void*)p,(uintptr_t)_ptr+sizeof(struct FLTYPE)-p,prev->storage);	\
			interval_tree_insert(nn, &FLCTRL.imap_root);	\
		}	\
    }   \
    else {  \
        __node = calloc(1,sizeof(struct interval_tree_node));   \
        assert(__node!=0);  \
        __node->start = (uint64_t)_ptr; \
        __node->last = (uint64_t)_ptr + sizeof(struct FLTYPE)-1;    \
        struct blstream* storage;   \
        struct rb_node* rb = interval_tree_insert(__node, &FLCTRL.imap_root);   \
        struct rb_node* prev = rb_prev(rb); \
        if (prev) { \
            storage = binary_stream_insert_back(_ptr,sizeof(struct FLTYPE),((struct interval_tree_node*)prev)->storage);    \
        }   \
        else {  \
            struct rb_node* next = rb_next(rb); \
            if (next) { \
                storage = binary_stream_insert_front(_ptr,sizeof(struct FLTYPE),((struct interval_tree_node*)next)->storage);   \
            }   \
            else {  \
                storage = binary_stream_append(_ptr,sizeof(struct FLTYPE)); \
            }   \
        }   \
        __node->storage = storage;  \
    }   \
        \
    __VA_ARGS__ \
	__node = interval_tree_iter_first(&FLCTRL.imap_root, (uint64_t)_ptr, (uint64_t)_ptr+sizeof(struct FLTYPE*)-1);    \
	assert(__node!=0);	\
	size_t _node_offset = (uint64_t)_ptr-__node->start;	\
    __node->storage->alignment = _alignment;    \
    __node->storage->align_offset = _node_offset;	\
    return make_flatten_pointer(__node,_node_offset);  \
}

#define FUNCTION_DECLARE_FLATTEN_STRUCT2(FLTYPE)	\
	extern struct flatten_pointer* flatten_struct2_##FLTYPE(const struct FLTYPE*);	\
	FUNCTION_DECLARE_FLATTEN_STRUCT2_ARRAY(FLTYPE)

#define FUNCTION_DEFINE_FLATTEN_STRUCT_TYPE(FLTYPE,...)	\
/* */		\
			\
struct flatten_pointer* flatten_struct_type_##FLTYPE(const FLTYPE* _ptr) {	\
			\
	typedef FLTYPE _container_type;	\
	size_t _alignment = 0;	\
			\
	struct interval_tree_node *__node = interval_tree_iter_first(&FLCTRL.imap_root, (uint64_t)_ptr, (uint64_t)_ptr+sizeof(FLTYPE)-1);	\
	if (__node) {	\
		assert(__node->start==(uint64_t)_ptr);	\
		assert(__node->last==(uint64_t)_ptr+sizeof(FLTYPE)-1);	\
		return make_flatten_pointer(__node,0);	\
	}	\
	else {	\
		__node = calloc(1,sizeof(struct interval_tree_node));	\
		assert(__node!=0);	\
		__node->start = (uint64_t)_ptr;	\
		__node->last = (uint64_t)_ptr + sizeof(FLTYPE)-1;	\
		struct blstream* storage;	\
		struct rb_node* rb = interval_tree_insert(__node, &FLCTRL.imap_root);	\
		struct rb_node* prev = rb_prev(rb);	\
		if (prev) {	\
			storage = binary_stream_insert_back(_ptr,sizeof(FLTYPE),((struct interval_tree_node*)prev)->storage);	\
		}	\
		else {	\
			struct rb_node* next = rb_next(rb);	\
			if (next) {	\
				storage = binary_stream_insert_front(_ptr,sizeof(FLTYPE),((struct interval_tree_node*)next)->storage);	\
			}	\
			else {	\
				storage = binary_stream_append(_ptr,sizeof(FLTYPE));	\
			}	\
		}	\
		__node->storage = storage;	\
	}	\
		\
	__VA_ARGS__	\
	__node->storage->alignment = _alignment;	\
    return make_flatten_pointer(__node,0);	\
}	\
\
FUNCTION_DEFINE_FLATTEN_STRUCT_TYPE_ARRAY(FLTYPE)

#define FUNCTION_DECLARE_FLATTEN_STRUCT_TYPE(FLTYPE)	\
	extern struct flatten_pointer* flatten_struct_type_##FLTYPE(const FLTYPE*);	\
	FUNCTION_DECLARE_FLATTEN_STRUCT2_TYPE_ARRAY(FLTYPE)






#define FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(FLTYPE,...)  \
/* */       \
            \
struct flatten_pointer* flatten_struct2_##FLTYPE##_iter(const struct FLTYPE* _ptr, struct bqueue* __q) {    \
            \
    typedef struct FLTYPE _container_type;  \
    size_t _alignment = 0;  \
            \
    struct interval_tree_node *__node = interval_tree_iter_first(&FLCTRL.imap_root, (uint64_t)_ptr, (uint64_t)_ptr+sizeof(struct FLTYPE)-1);    \
    (void)__q;	\
    if (__node) {   \
    	uintptr_t p = (uintptr_t)_ptr;  \
		struct interval_tree_node *prev;    \
		while(__node) { \
			if (__node->start>p) {  \
				assert(__node->storage!=0); \
				struct interval_tree_node* nn = calloc(1,sizeof(struct interval_tree_node));    \
				assert(nn!=0);  \
				nn->start = p;  \
				nn->last = __node->start-1; \
				nn->storage = binary_stream_insert_front((void*)p,__node->start-p,__node->storage); \
				interval_tree_insert(nn, &FLCTRL.imap_root);    \
			}   \
			p = __node->last+1; \
			prev = __node;  \
			__node = interval_tree_iter_next(__node, (uintptr_t)_ptr, (uintptr_t)_ptr+sizeof(struct FLTYPE)-1); \
		}   \
		if ((uintptr_t)_ptr+sizeof(struct FLTYPE)>p) {  \
			assert(prev->storage!=0);   \
			struct interval_tree_node* nn = calloc(1,sizeof(struct interval_tree_node));    \
			assert(nn!=0);  \
			nn->start = p;  \
			nn->last = (uintptr_t)_ptr+sizeof(struct FLTYPE)-1; \
			nn->storage = binary_stream_insert_back((void*)p,(uintptr_t)_ptr+sizeof(struct FLTYPE)-p,prev->storage);    \
			interval_tree_insert(nn, &FLCTRL.imap_root);    \
		}   \
    }   \
    else {  \
        __node = calloc(1,sizeof(struct interval_tree_node));   \
        assert(__node!=0);  \
        __node->start = (uint64_t)_ptr; \
        __node->last = (uint64_t)_ptr + sizeof(struct FLTYPE)-1;    \
        struct blstream* storage;   \
        struct rb_node* rb = interval_tree_insert(__node, &FLCTRL.imap_root);   \
        struct rb_node* prev = rb_prev(rb); \
        if (prev) { \
            storage = binary_stream_insert_back(_ptr,sizeof(struct FLTYPE),((struct interval_tree_node*)prev)->storage);    \
        }   \
        else {  \
            struct rb_node* next = rb_next(rb); \
            if (next) { \
                storage = binary_stream_insert_front(_ptr,sizeof(struct FLTYPE),((struct interval_tree_node*)next)->storage);   \
            }   \
            else {  \
                storage = binary_stream_append(_ptr,sizeof(struct FLTYPE)); \
            }   \
        }   \
        __node->storage = storage;  \
    }   \
        \
    __VA_ARGS__ \
	__node = interval_tree_iter_first(&FLCTRL.imap_root, (uint64_t)_ptr, (uint64_t)_ptr+sizeof(struct FLTYPE*)-1);    \
	assert(__node!=0);  \
	size_t _node_offset = (uint64_t)_ptr-__node->start; \
    __node->storage->alignment = _alignment;    \
    __node->storage->align_offset = _node_offset;   \
    return make_flatten_pointer(__node,_node_offset);  \
}	\
\
FUNCTION_DEFINE_FLATTEN_STRUCT2_ARRAY_ITER(FLTYPE)

#define FUNCTION_DECLARE_FLATTEN_STRUCT2_ITER(FLTYPE) \
    extern struct flatten_pointer* flatten_struct2_##FLTYPE##_iter(const struct FLTYPE*, struct bqueue*);	\
    FUNCTION_DECLARE_FLATTEN_STRUCT2_ARRAY_ITER(FLTYPE)


#define FUNCTION_DEFINE_FLATTEN_STRUCT_TYPE2_ITER(FLTYPE,...)  \
/* */       \
            \
struct flatten_pointer* flatten_struct_type2_##FLTYPE##_iter(const FLTYPE* _ptr, struct bqueue* __q) {    \
            \
    typedef FLTYPE _container_type;  \
    size_t _alignment = 0;  \
            \
    struct interval_tree_node *__node = interval_tree_iter_first(&FLCTRL.imap_root, (uint64_t)_ptr, (uint64_t)_ptr+sizeof(FLTYPE)-1);    \
    (void)__q;	\
    if (__node) {   \
    	uintptr_t p = (uintptr_t)_ptr;  \
		struct interval_tree_node *prev;    \
		while(__node) { \
			if (__node->start>p) {  \
				assert(__node->storage!=0); \
				struct interval_tree_node* nn = calloc(1,sizeof(struct interval_tree_node));    \
				assert(nn!=0);  \
				nn->start = p;  \
				nn->last = __node->start-1; \
				nn->storage = binary_stream_insert_front((void*)p,__node->start-p,__node->storage); \
				interval_tree_insert(nn, &FLCTRL.imap_root);    \
			}   \
			p = __node->last+1; \
			prev = __node;  \
			__node = interval_tree_iter_next(__node, (uintptr_t)_ptr, (uintptr_t)_ptr+sizeof(FLTYPE)-1); \
		}   \
		if ((uintptr_t)_ptr+sizeof(FLTYPE)>p) {  \
			assert(prev->storage!=0);   \
			struct interval_tree_node* nn = calloc(1,sizeof(struct interval_tree_node));    \
			assert(nn!=0);  \
			nn->start = p;  \
			nn->last = (uintptr_t)_ptr+sizeof(FLTYPE)-1; \
			nn->storage = binary_stream_insert_back((void*)p,(uintptr_t)_ptr+sizeof(FLTYPE)-p,prev->storage);    \
			interval_tree_insert(nn, &FLCTRL.imap_root);    \
		}   \
    }   \
    else {  \
        __node = calloc(1,sizeof(struct interval_tree_node));   \
        assert(__node!=0);  \
        __node->start = (uint64_t)_ptr; \
        __node->last = (uint64_t)_ptr + sizeof(FLTYPE)-1;    \
        struct blstream* storage;   \
        struct rb_node* rb = interval_tree_insert(__node, &FLCTRL.imap_root);   \
        struct rb_node* prev = rb_prev(rb); \
        if (prev) { \
            storage = binary_stream_insert_back(_ptr,sizeof(FLTYPE),((struct interval_tree_node*)prev)->storage);    \
        }   \
        else {  \
            struct rb_node* next = rb_next(rb); \
            if (next) { \
                storage = binary_stream_insert_front(_ptr,sizeof(FLTYPE),((struct interval_tree_node*)next)->storage);   \
            }   \
            else {  \
                storage = binary_stream_append(_ptr,sizeof(FLTYPE)); \
            }   \
        }   \
        __node->storage = storage;  \
    }   \
        \
    __VA_ARGS__ \
	__node = interval_tree_iter_first(&FLCTRL.imap_root, (uint64_t)_ptr, (uint64_t)_ptr+sizeof(FLTYPE*)-1);    \
	assert(__node!=0);  \
	size_t _node_offset = (uint64_t)_ptr-__node->start; \
    __node->storage->alignment = _alignment;    \
    __node->storage->align_offset = _node_offset;   \
    return make_flatten_pointer(__node,_node_offset);  \
}	\
\
FUNCTION_DEFINE_FLATTEN_STRUCT_TYPE2_ARRAY_ITER(FLTYPE)

#define FUNCTION_DECLARE_FLATTEN_STRUCT_TYPE2_ITER(FLTYPE) \
    extern struct flatten_pointer* flatten_struct_type2_##FLTYPE##_iter(const FLTYPE*, struct bqueue*);	\
    FUNCTION_DECLARE_FLATTEN_STRUCT_TYPE2_ARRAY_ITER(FLTYPE)



#define FUNCTION_DEFINE_FLATTEN_STRUCT_ITER(FLTYPE,...)  \
/* */       \
            \
struct flatten_pointer* flatten_struct_##FLTYPE##_iter(const struct FLTYPE* _ptr, struct bqueue* __q) {    \
            \
    typedef struct FLTYPE _container_type;  \
    size_t _alignment = 0;  \
            \
    struct interval_tree_node *__node = interval_tree_iter_first(&FLCTRL.imap_root, (uint64_t)_ptr, (uint64_t)_ptr+sizeof(struct FLTYPE)-1);    \
    (void)__q;	\
    if (__node) {   \
        assert(__node->start==(uint64_t)_ptr);  \
        assert(__node->last==(uint64_t)_ptr+sizeof(struct FLTYPE)-1);   \
        return make_flatten_pointer(__node,0);  \
    }   \
    else {  \
        __node = calloc(1,sizeof(struct interval_tree_node));   \
        assert(__node!=0);  \
        __node->start = (uint64_t)_ptr; \
        __node->last = (uint64_t)_ptr + sizeof(struct FLTYPE)-1;    \
        struct blstream* storage;   \
        struct rb_node* rb = interval_tree_insert(__node, &FLCTRL.imap_root);   \
        struct rb_node* prev = rb_prev(rb); \
        if (prev) { \
            storage = binary_stream_insert_back(_ptr,sizeof(struct FLTYPE),((struct interval_tree_node*)prev)->storage);    \
        }   \
        else {  \
            struct rb_node* next = rb_next(rb); \
            if (next) { \
                storage = binary_stream_insert_front(_ptr,sizeof(struct FLTYPE),((struct interval_tree_node*)next)->storage);   \
            }   \
            else {  \
                storage = binary_stream_append(_ptr,sizeof(struct FLTYPE)); \
            }   \
        }   \
        __node->storage = storage;  \
    }   \
        \
    __VA_ARGS__ \
    __node->storage->alignment = _alignment;    \
    return make_flatten_pointer(__node,0);  \
}	\
\
FUNCTION_DEFINE_FLATTEN_STRUCT_ARRAY_ITER(FLTYPE)

#define FUNCTION_DECLARE_FLATTEN_STRUCT_ITER(FLTYPE) \
    extern struct flatten_pointer* flatten_struct_##FLTYPE##_iter(const struct FLTYPE*, struct bqueue*);	\
    FUNCTION_DECLARE_FLATTEN_STRUCT_ARRAY_ITER(FLTYPE)

#define FUNCTION_DEFINE_FLATTEN_STRUCT_TYPE_ITER(FLTYPE,...)  \
/* */       \
            \
struct flatten_pointer* flatten_struct_type_##FLTYPE##_iter(const FLTYPE* _ptr, struct bqueue* __q) {    \
            \
    typedef FLTYPE _container_type;  \
    size_t _alignment = 0;  \
            \
    struct interval_tree_node *__node = interval_tree_iter_first(&FLCTRL.imap_root, (uint64_t)_ptr, (uint64_t)_ptr+sizeof(FLTYPE)-1);    \
    (void)__q;	\
    if (__node) {   \
        assert(__node->start==(uint64_t)_ptr);  \
        assert(__node->last==(uint64_t)_ptr+sizeof(FLTYPE)-1);   \
        return make_flatten_pointer(__node,0);  \
    }   \
    else {  \
        __node = calloc(1,sizeof(struct interval_tree_node));   \
        assert(__node!=0);  \
        __node->start = (uint64_t)_ptr; \
        __node->last = (uint64_t)_ptr + sizeof(FLTYPE)-1;    \
        struct blstream* storage;   \
        struct rb_node* rb = interval_tree_insert(__node, &FLCTRL.imap_root);   \
        struct rb_node* prev = rb_prev(rb); \
        if (prev) { \
            storage = binary_stream_insert_back(_ptr,sizeof(FLTYPE),((struct interval_tree_node*)prev)->storage);    \
        }   \
        else {  \
            struct rb_node* next = rb_next(rb); \
            if (next) { \
                storage = binary_stream_insert_front(_ptr,sizeof(FLTYPE),((struct interval_tree_node*)next)->storage);   \
            }   \
            else {  \
                storage = binary_stream_append(_ptr,sizeof(FLTYPE)); \
            }   \
        }   \
        __node->storage = storage;  \
    }   \
        \
    __VA_ARGS__ \
    __node->storage->alignment = _alignment;    \
    return make_flatten_pointer(__node,0);  \
}	\
\
FUNCTION_DEFINE_FLATTEN_STRUCT_TYPE_ARRAY_ITER(FLTYPE)

#define FUNCTION_DECLARE_FLATTEN_STRUCT_TYPE_ITER(FLTYPE) \
    extern struct flatten_pointer* flatten_struct_type_##FLTYPE##_iter(const struct FLTYPE*, struct bqueue*);	\
    FUNCTION_DECLARE_FLATTEN_STRUCT_TYPE_ARRAY_ITER(FLTYPE)

#define FLATTEN_STRUCT(T,p)	do {	\
		LIBFLAT_DBGTP(FLATTEN_STRUCT,T,p);	\
		if (p) {   \
			fixup_set_insert(__fptr->node,__fptr->offset,flatten_struct_##T((p)));	\
		}	\
	} while(0)

#define FLATTEN_STRUCT2(T,p)	do {	\
		LIBFLAT_DBGTP(FLATTEN_STRUCT2,T,p);	\
		if (p) {   \
			struct fixup_set_node* __inode = fixup_set_search((uintptr_t)p);	\
			if (!__inode) {	\
				fixup_set_reserve_address((uintptr_t)p);	\
				fixup_set_insert(__fptr->node,__fptr->offset,flatten_struct2_##T((p)));	\
			}	\
			else {	\
				struct interval_tree_node *__node = interval_tree_iter_first(&FLCTRL.imap_root, (uintptr_t)p,\
						(uintptr_t)p+sizeof(struct T)-1);    \
				fixup_set_insert(__fptr->node,__fptr->offset,make_flatten_pointer(__node,(uintptr_t)p-__node->start));	\
			}	\
		}	\
	} while(0)

#define FLATTEN_STRUCT_TYPE(T,p)	do {	\
		LIBFLAT_DBGTP(FLATTEN_STRUCT_TYPE,T,p);	\
		if (p) {   \
			fixup_set_insert(__fptr->node,__fptr->offset,flatten_struct_type_##T((p)));	\
		}	\
	} while(0)

#define FLATTEN_STRUCT_ARRAY(T,p,n)	do {	\
		LIBFLAT_DBGM3(FLATTEN_STRUCT_ARRAY,T,p,n);	\
		if (p) {   \
			fixup_set_insert(__fptr->node,__fptr->offset,flatten_struct_##T##_array((p),(n)));	\
		}	\
	} while(0)

#define FLATTEN_STRUCT_TYPE_ARRAY(T,p,n)	do {	\
		LIBFLAT_DBGM3(FLATTEN_STRUCT_TYPE_ARRAY,T,p,n);	\
		if (p) {   \
			fixup_set_insert(__fptr->node,__fptr->offset,flatten_struct_type_##T##_array((p),(n)));	\
		}	\
	} while(0)

#define FLATTEN_STRUCT_ITER(T,p) do {    \
		LIBFLAT_DBGTP(FLATTEN_STRUCT_ITER,T,p);  \
        if (p) {   \
			struct flatten_job __job;   \
			__job.node = __fptr->node;    \
			__job.offset = __fptr->offset; \
			__job.size = 1;	\
			__job.ptr = (struct flatten_base*)p;    \
			__job.fun = (flatten_struct_t)&flatten_struct_##T##_array_iter;    \
			__job.fp = 0;   \
			__job.convert = 0;  \
			bqueue_push_back(__q,&__job,sizeof(struct flatten_job));    \
        }   \
    } while(0)

#define FLATTEN_STRUCT2_ITER(T,p) do {    \
        LIBFLAT_DBGTP(FLATTEN_STRUCT2_ITER,T,p);  \
        if (p) {   \
        	struct fixup_set_node* __inode = fixup_set_search((uintptr_t)p);	\
        	if (!__inode) {	\
        		fixup_set_reserve_address((uintptr_t)p);	\
        		fixup_set_insert(__fptr->node,__fptr->offset,flatten_struct2_##T##_iter((p),__q));	\
        	}	\
			else {	\
				struct interval_tree_node *__node = interval_tree_iter_first(&FLCTRL.imap_root, (uintptr_t)p,\
						(uintptr_t)p+sizeof(struct T)-1);    \
				assert(__node!=0);  \
				fixup_set_insert(__fptr->node,__fptr->offset,make_flatten_pointer(__node,(uintptr_t)p-__node->start));	\
			}	\
        }   \
    } while(0)

#define FLATTEN_STRUCT_TYPE2_ITER(T,p) do {    \
        LIBFLAT_DBGTP(FLATTEN_STRUCT_TYPE2_ITER,T,p);  \
        if (p) {   \
        	struct fixup_set_node* __inode = fixup_set_search((uintptr_t)p);	\
        	if (!__inode) {	\
        		fixup_set_reserve_address((uintptr_t)p);	\
        		fixup_set_insert(__fptr->node,__fptr->offset,flatten_struct_type2_##T##_iter((p),__q));	\
        	}	\
			else {	\
				struct interval_tree_node *__node = interval_tree_iter_first(&FLCTRL.imap_root, (uintptr_t)p,\
						(uintptr_t)p+sizeof(T)-1);    \
				assert(__node!=0);  \
				fixup_set_insert_force_update(__fptr->node,__fptr->offset,make_flatten_pointer(__node,(uintptr_t)p-__node->start));	\
			}	\
        }   \
    } while(0)

#define FLATTEN_STRUCT_TYPE_ITER(T,p) do {    \
		LIBFLAT_DBGTP(FLATTEN_STRUCT_TYPE_ITER,T,p);  \
        if (p) {   \
			struct flatten_job __job;   \
			__job.node = __fptr->node;    \
			__job.offset = __fptr->offset; \
			__job.size = 1;	\
			__job.ptr = (struct flatten_base*)p;    \
			__job.fun = (flatten_struct_t)&flatten_struct_type_iter_##T##_array;    \
			__job.fp = 0;   \
			__job.convert = 0;  \
			bqueue_push_back(__q,&__job,sizeof(struct flatten_job));    \
        }   \
    } while(0)

#define FLATTEN_STRUCT_ARRAY_ITER(T,p,n)	do {	\
		LIBFLAT_DBGM3(FLATTEN_STRUCT_ARRAY_ITER,T,p,n);	\
		if (p) {   \
			struct flatten_job __job;   \
			__job.node = __fptr->node;    \
			__job.offset = __fptr->offset; \
			__job.size = n;	\
			__job.ptr = (struct flatten_base*)p;    \
			__job.fun = (flatten_struct_t)&flatten_struct_##T##_array_iter;    \
			__job.fp = 0;   \
			__job.convert = 0;  \
			bqueue_push_back(__q,&__job,sizeof(struct flatten_job));    \
		}	\
	} while(0)

#define FLATTEN_STRUCT_TYPE_ARRAY_ITER(T,p,n)	do {	\
		LIBFLAT_DBGM3(FLATTEN_STRUCT_TYPE_ARRAY_ITER,T,p,n);	\
		if (p) {   \
			struct flatten_job __job;   \
			__job.node = __fptr->node;    \
			__job.offset = __fptr->offset; \
			__job.size = n;	\
			__job.ptr = (struct flatten_base*)p;    \
			__job.fun = (flatten_struct_t)&flatten_struct_type_##T##_array_iter;    \
			__job.fp = 0;   \
			__job.convert = 0;  \
			bqueue_push_back(__q,&__job,sizeof(struct flatten_job));    \
		}	\
	} while(0)

#define AGGREGATE_FLATTEN_STRUCT(T,f)	do {	\
		LIBFLAT_DBGTF(AGGREGATE_FLATTEN_STRUCT,T,f,"%p",(void*)ATTR(f));	\
    	if (ATTR(f)) {	\
    		fixup_set_insert(__node,offsetof(_container_type,f),flatten_struct_##T((const struct T*)ATTR(f)));	\
		}	\
	} while(0)

/*
#define acquire_attribute_nodes(start,size)	\
		struct interval_tree_node *__node = interval_tree_iter_first(&FLCTRL.imap_root, start,start+size-1);    \
		assert(__node!=0);	\
		struct fixup_set_node* __inode = fixup_set_search(start);

acquire_attribute_nodes((uint64_t)_ptr+offsetof(_container_type,f),sizeof(struct T*))
*/

#define AGGREGATE_FLATTEN_STRUCT2(T,f)	do {	\
		LIBFLAT_DBGTF(AGGREGATE_FLATTEN_STRUCT2,T,f,"%p",(void*)ATTR(f));	\
    	if (ATTR(f)) {	\
    		size_t _off = offsetof(_container_type,f);	\
    		struct interval_tree_node *__node = interval_tree_iter_first(&FLCTRL.imap_root, (uint64_t)_ptr+_off,\
    				(uint64_t)_ptr+_off+sizeof(struct T*)-1);    \
    		assert(__node!=0);	\
			struct fixup_set_node* __inode = fixup_set_search((uint64_t)ATTR(f));	\
			if (!__inode) {	\
				fixup_set_reserve_address((uintptr_t)ATTR(f));	\
				fixup_set_insert(__node,(uint64_t)_ptr-__node->start+_off,flatten_struct2_##T((const struct T*)ATTR(f)));	\
			}	\
			else {	\
				struct interval_tree_node *__struct_node = interval_tree_iter_first(&FLCTRL.imap_root, (uintptr_t)ATTR(f),\
						(uintptr_t)ATTR(f)+sizeof(struct T)-1);    \
				assert(__struct_node!=0);	\
				fixup_set_insert(__node,(uint64_t)_ptr-__node->start+_off,	\
					make_flatten_pointer(__struct_node,(uintptr_t)ATTR(f)-__struct_node->start));	\
			}	\
		}	\
	} while(0)

#define AGGREGATE_FLATTEN_STRUCT2_STORAGE(T,p)	do {	\
		LIBFLAT_DBGTF(AGGREGATE_FLATTEN_STRUCT2_STORAGE,T,p,"%p",(void*)p);	\
    	{	\
			struct fixup_set_node* __inode = fixup_set_search((uint64_t)p);	\
			if (!__inode) {	\
				fixup_set_reserve_address((uintptr_t)p);	\
				flatten_struct2_##T((const struct T*)p);	\
			}	\
		}	\
	} while(0)

#define AGGREGATE_FLATTEN_STRUCT2_STORAGE_ITER(T,p)	do {	\
		LIBFLAT_DBGTF(AGGREGATE_FLATTEN_STRUCT2_STORAGE_ITER,T,p,"%p",(void*)p);	\
    	{	\
			struct fixup_set_node* __inode = fixup_set_search((uint64_t)p);	\
			if (!__inode) {	\
				fixup_set_reserve_address((uintptr_t)p);	\
				struct flatten_job __job;   \
				__job.node = 0;    \
				__job.offset = 0; \
				__job.size = 1;	\
				__job.ptr = (struct flatten_base*)p;    \
				__job.fun = (flatten_struct_t)&flatten_struct2_##T##_array_iter;    \
				__job.fp = 0;   \
				__job.convert = 0;  \
				bqueue_push_back(__q,&__job,sizeof(struct flatten_job));    \
			}	\
		}	\
	} while(0)

#define AGGREGATE_FLATTEN_STRUCT_TYPE(T,f)	do {	\
        LIBFLAT_DBGTF(AGGREGATE_FLATTEN_STRUCT_TYPE,T,f,"%p",(void*)ATTR(f));	\
        if (ATTR(f)) {	\
            fixup_set_insert(__node,offsetof(_container_type,f),flatten_struct_type_##T((const T*)ATTR(f)));	\
        }	\
    } while(0)

#define AGGREGATE_FLATTEN_STRUCT_MIXED_POINTER(T,f,pf,ff)	do {	\
		LIBFLAT_DBGTFMF(AGGREGATE_FLATTEN_STRUCT_MIXED_POINTER,T,f,"%p",(void*)ATTR(f),pf,ff);	\
		const struct T* _fp = pf((const struct T*)ATTR(f));	\
    	if (_fp) {	\
    		fixup_set_insert(__node,offsetof(_container_type,f),ff(flatten_struct_##T(_fp),(const struct T*)ATTR(f)));	\
		}	\
	} while(0)

#define AGGREGATE_FLATTEN_STRUCT2_MIXED_POINTER(T,f,pf,ff)	do {	\
		LIBFLAT_DBGTFMF(AGGREGATE_FLATTEN_STRUCT_MIXED_POINTER,T,f,"%p",(void*)ATTR(f),pf,ff);	\
		const struct T* _fp = pf((const struct T*)ATTR(f));	\
    	if (_fp) {	\
    		size_t _off = offsetof(_container_type,f);	\
    		struct interval_tree_node *__node = interval_tree_iter_first(&FLCTRL.imap_root, (uint64_t)_ptr+_off,\
    				(uint64_t)_ptr+_off+sizeof(struct T*)-1);    \
    		assert(__node!=0);	\
    		struct fixup_set_node* __inode = fixup_set_search((uint64_t)_fp);	\
    		if (!__inode) {	\
    			fixup_set_reserve_address((uintptr_t)_fp);	\
    			/*fixup_set_reserve(__node,(uint64_t)_ptr-__node->start+_off);	*/\
    			/*fixup_set_update(__node,(uint64_t)_ptr-__node->start+_off,ff(flatten_struct2_##T(_fp),(const struct T*)ATTR(f)));	*/\
				fixup_set_insert(__node,(uint64_t)_ptr-__node->start+_off,ff(flatten_struct2_##T(_fp),(const struct T*)ATTR(f)));	\
    		}	\
			else {	\
				struct interval_tree_node *__struct_node = interval_tree_iter_first(&FLCTRL.imap_root, (uintptr_t)_fp,\
						(uintptr_t)_fp+sizeof(struct T)-1);    \
				assert(__struct_node!=0);	\
				fixup_set_insert(__node,(uint64_t)_ptr-__node->start+_off,	\
					make_flatten_pointer(__struct_node,(uintptr_t)_fp-__struct_node->start));	\
			}	\
		}	\
	} while(0)

#define AGGREGATE_FLATTEN_STRUCT_TYPE_MIXED_POINTER(T,f,pf,ff)	do {	\
        LIBFLAT_DBGTFMF(AGGREGATE_FLATTEN_STRUCT_TYPE_MIXED_POINTER,T,f,"%p",(void*)ATTR(f),pf,ff);	\
        const T* _fp = pf((const T*)ATTR(f));	\
        if (_fp) {	\
            fixup_set_insert(__node,offsetof(_container_type,f),ff(flatten_struct_type_##T(_fp),(const T*)ATTR(f)));	\
        }	\
    } while(0)

#define AGGREGATE_FLATTEN_STRUCT_ARRAY(T,f,n)	do {	\
		LIBFLAT_DBGM3(AGGREGATE_FLATTEN_STRUCT_ARRAY,T,f,n);	\
    	if (ATTR(f)) {	\
    		size_t _i;	\
    		void* _fp_first=0;	\
    		for (_i=0; _i<(n); ++_i) {	\
    			void* _fp = (void*)flatten_struct_##T(ATTR(f)+_i);	\
    			if (!_fp_first) _fp_first=_fp;	\
    			else free(_fp);	\
    		}	\
		    fixup_set_insert(__node,offsetof(_container_type,f),_fp_first);	\
		}	\
	} while(0)

#define AGGREGATE_FLATTEN_STRUCT2_ARRAY(T,f,n)	do {	\
		LIBFLAT_DBGM3(AGGREGATE_FLATTEN_STRUCT2_ARRAY,T,f,n);	\
    	if (ATTR(f)) {	\
    		size_t _off = offsetof(_container_type,f);	\
    		size_t _i;	\
			struct interval_tree_node *__node = interval_tree_iter_first(&FLCTRL.imap_root, (uint64_t)_ptr+_off,\
					(uint64_t)_ptr+_off+sizeof(struct T*)-1);    \
			assert(__node!=0);	\
			fixup_set_insert(__node,(uint64_t)_ptr-__node->start+_off,flatten_plain_type(ATTR(f),(n)*sizeof(struct T)));	\
			for (_i=0; _i<(n); ++_i) {	\
				struct interval_tree_node *__struct_node = interval_tree_iter_first(&FLCTRL.imap_root,	\
					(uint64_t)((void*)ATTR(f)+_i*sizeof(struct T)),(uint64_t)((void*)ATTR(f)+(_i+1)*sizeof(struct T)-1));    \
				assert(__struct_node!=0);	\
				struct fixup_set_node* __struct_inode;	\
				__struct_inode = fixup_set_search((uint64_t)((void*)ATTR(f)+_i*sizeof(struct T)));	\
				if (!__struct_inode) {	\
					fixup_set_reserve_address((uint64_t)((void*)ATTR(f)+_i*sizeof(struct T)));	\
					void* _fp = (void*)flatten_struct2_##T((void*)ATTR(f)+_i*sizeof(struct T));	\
					free(_fp);	\
				}	\
			}	\
		}	\
	} while(0)

#define AGGREGATE_FLATTEN_STRUCT_TYPE_ARRAY(T,f,n)	do {	\
        LIBFLAT_DBGM3(AGGREGATE_FLATTEN_STRUCT_TYPE_ARRAY,T,f,n);	\
        if (ATTR(f)) {	\
            size_t _i;	\
            void* _fp_first=0;	\
            for (_i=0; _i<(n); ++_i) {	\
                void* _fp = (void*)flatten_struct_type_##T((const T*)(ATTR(f))+_i);	\
                if (!_fp_first) _fp_first=_fp;	\
                else free(_fp);	\
            }	\
            fixup_set_insert(__node,offsetof(_container_type,f),_fp_first);	\
        }	\
    } while(0)


#define AGGREGATE_FLATTEN_STRUCT2_TYPE_ARRAY(T,f,n)	do {	\
        LIBFLAT_DBGM3(AGGREGATE_FLATTEN_STRUCT2_TYPE_ARRAY,T,f,n);	\
        if (ATTR(f)) {	\
        	size_t _off = offsetof(_container_type,f);	\
			size_t _i;	\
            void* _fp_first=0;	\
			struct interval_tree_node *__node = interval_tree_iter_first(&FLCTRL.imap_root, (uint64_t)_ptr+_off,\
					(uint64_t)_ptr+_off+sizeof(T*)-1);    \
			assert(__node!=0);	\
			struct fixup_set_node* __inode = fixup_set_search((uint64_t)_ptr+_off);	\
			if (!__inode) {	\
				fixup_set_reserve(__node,(uint64_t)_ptr-__node->start+_off);	\
			}	\
            for (_i=0; _i<(n); ++_i) {	\
                void* _fp = (void*)flatten_struct2_type_##T((const T*)(ATTR(f))+_i);	\
                if (!_fp_first) _fp_first=_fp;	\
                else free(_fp);	\
            }	\
			if (!__inode) {	\
				fixup_set_update(__node,(uint64_t)_ptr-__node->start+_off,_fp_first);	\
			}	\
        }	\
    } while(0)

#define AGGREGATE_FLATTEN_STRUCT_ITER(T,f)   do {    \
		LIBFLAT_DBGTF(AGGREGATE_FLATTEN_STRUCT_ITER,T,f,"%p",(void*)ATTR(f));    \
        if (ATTR(f)) {  \
            struct flatten_job __job;   \
            __job.node = __node;    \
            __job.offset = offsetof(_container_type,f); \
            __job.size = 1;	\
            __job.ptr = (struct flatten_base*)ATTR(f);    \
            __job.fun = (flatten_struct_t)&flatten_struct_##T##_array_iter;    \
            __job.fp = 0;   \
            __job.convert = 0;  \
            bqueue_push_back(__q,&__job,sizeof(struct flatten_job));    \
        }   \
    } while(0)

#define AGGREGATE_FLATTEN_STRUCT2_ITER(T,f)   do {    \
        LIBFLAT_DBGTF(AGGREGATE_FLATTEN_STRUCT2_ITER,T,f,"%p",(void*)ATTR(f));    \
        if (ATTR(f)) {  \
        	size_t _off = offsetof(_container_type,f);  \
        	struct interval_tree_node *__node = interval_tree_iter_first(&FLCTRL.imap_root, (uint64_t)_ptr+_off,\
        			(uint64_t)_ptr+_off+sizeof(struct T*)-1);    \
        	assert(__node!=0);  \
        	struct fixup_set_node* __inode = fixup_set_search((uint64_t)ATTR(f));	\
        	if (!__inode) {	\
        		fixup_set_reserve_address((uintptr_t)ATTR(f));	\
                struct flatten_job __job;   \
                __job.node = __node;    \
                __job.offset = (uint64_t)_ptr-__node->start+_off; \
                __job.size = 1;	\
                __job.ptr = (struct flatten_base*)ATTR(f);    \
                __job.fun = (flatten_struct_t)&flatten_struct2_##T##_array_iter;    \
                __job.fp = 0;   \
                __job.convert = 0;  \
                bqueue_push_back(__q,&__job,sizeof(struct flatten_job));    \
        	}	\
			else {	\
				struct interval_tree_node *__struct_node = interval_tree_iter_first(&FLCTRL.imap_root, (uintptr_t)ATTR(f),\
						(uintptr_t)ATTR(f)+sizeof(struct T)-1);    \
				assert(__struct_node!=0);	\
				fixup_set_insert(__node,(uint64_t)_ptr-__node->start+_off,	\
					make_flatten_pointer(__struct_node,(uintptr_t)ATTR(f)-__struct_node->start));	\
			}	\
        }   \
    } while(0)

#define AGGREGATE_FLATTEN_STRUCT_TYPE2_ITER(T,f)   do {    \
        LIBFLAT_DBGTF(AGGREGATE_FLATTEN_STRUCT_TYPE2_ITER,T,f,"%p",(void*)ATTR(f));    \
        if (ATTR(f)) {  \
        	size_t _off = offsetof(_container_type,f);  \
        	struct interval_tree_node *__node = interval_tree_iter_first(&FLCTRL.imap_root, (uint64_t)_ptr+_off,\
        			(uint64_t)_ptr+_off+sizeof(T*)-1);    \
        	assert(__node!=0);  \
        	struct fixup_set_node* __inode = fixup_set_search((uint64_t)ATTR(f));	\
        	struct interval_tree_node *__struct_node = interval_tree_iter_first(&FLCTRL.imap_root, (uintptr_t)ATTR(f),\
        			(uintptr_t)ATTR(f)+sizeof(T)-1);    \
        	if ((!__inode)||(__struct_node==0)) {	\
        		if (!__inode) {	\
        			fixup_set_reserve_address((uintptr_t)ATTR(f));	\
        		}	\
                struct flatten_job __job;   \
                __job.node = __node;    \
                __job.offset = (uint64_t)_ptr-__node->start+_off; \
                __job.size = 1;	\
                __job.ptr = (struct flatten_base*)ATTR(f);    \
                __job.fun = (flatten_struct_t)&flatten_struct_type2_##T##_array_iter;    \
                __job.fp = 0;   \
                __job.convert = 0;  \
                bqueue_push_back(__q,&__job,sizeof(struct flatten_job));    \
        	}	\
			else {	\
				fixup_set_insert_force_update(__node,(uint64_t)_ptr-__node->start+_off,	\
					make_flatten_pointer(__struct_node,(uintptr_t)ATTR(f)-__struct_node->start));	\
			}	\
        }   \
    } while(0)

#define AGGREGATE_FLATTEN_STRUCT_TYPE_ITER(T,f)   do {    \
		LIBFLAT_DBGTF(AGGREGATE_FLATTEN_STRUCT_TYPE_ITER,T,f,"%p",(void*)ATTR(f));    \
        if (ATTR(f)) {  \
            struct flatten_job __job;   \
            __job.node = __node;    \
            __job.offset = offsetof(_container_type,f); \
            __job.size = 1;	\
            __job.ptr = (struct flatten_base*)ATTR(f);    \
            __job.fun = (flatten_struct_t)&flatten_struct_type_iter_##T##_array;    \
            __job.fp = 0;   \
            __job.convert = 0;  \
            bqueue_push_back(__q,&__job,sizeof(struct flatten_job));    \
        }   \
    } while(0)

#define AGGREGATE_FLATTEN_STRUCT_MIXED_POINTER_ITER(T,f,pf,ff)   do {    \
		LIBFLAT_DBGTFMF(AGGREGATE_FLATTEN_STRUCT_MIXED_POINTER_ITER,T,f,"%p",(void*)ATTR(f),pf,ff);  \
        const struct T* _fp = pf((const struct T*)ATTR(f)); \
        if (_fp) {  \
            struct flatten_job __job;   \
            __job.node = __node;    \
            __job.offset = offsetof(_container_type,f); \
            __job.size = 1;	\
            __job.ptr = (struct flatten_base*)ATTR(f);    \
            __job.fun = (flatten_struct_t)&flatten_struct_iter_##T_array;    \
            __job.fp = (const struct flatten_base*)_fp; \
            __job.convert = (flatten_struct_mixed_convert_t)&ff; \
            bqueue_push_back(__q,&__job,sizeof(struct flatten_job));    \
        }   \
    } while(0)

#define AGGREGATE_FLATTEN_STRUCT2_MIXED_POINTER_ITER(T,f,pf,ff)   do {    \
		LIBFLAT_DBGTFMF(AGGREGATE_FLATTEN_STRUCT2_MIXED_POINTER_ITER,T,f,"%p",(void*)ATTR(f),pf,ff);  \
		const struct T* _fp = pf((const struct T*)ATTR(f)); \
        if (_fp) {  \
        	size_t _off = offsetof(_container_type,f);  \
        	struct interval_tree_node *__node = interval_tree_iter_first(&FLCTRL.imap_root, (uint64_t)_ptr+_off,\
        			(uint64_t)_ptr+_off+sizeof(struct T*)-1);    \
        	assert(__node!=0);  \
        	struct fixup_set_node* __inode = fixup_set_search((uint64_t)_fp);	\
        	if (!__inode) {	\
        		fixup_set_reserve_address((uintptr_t)_fp);	\
                struct flatten_job __job;   \
                __job.node = __node;    \
                __job.offset = (uint64_t)_ptr-__node->start+_off; \
                __job.size = 1;	\
                __job.ptr = (struct flatten_base*)_fp;    \
                __job.fun = (flatten_struct_t)&flatten_struct2_##T##_array_iter;    \
                __job.fp = (const struct flatten_base*)_fp; \
                __job.convert = (flatten_struct_mixed_convert_t)&ff; \
                bqueue_push_back(__q,&__job,sizeof(struct flatten_job));    \
        	}	\
			else {	\
				struct interval_tree_node *__struct_node = interval_tree_iter_first(&FLCTRL.imap_root, (uintptr_t)_fp,\
						(uintptr_t)_fp+sizeof(struct T)-1);    \
				assert(__struct_node!=0);	\
				fixup_set_insert(__node,(uint64_t)_ptr-__node->start+_off,	\
					make_flatten_pointer(__struct_node,(uintptr_t)_fp-__struct_node->start));	\
			}	\
        }   \
    } while(0)

#define AGGREGATE_FLATTEN_STRUCT_TYPE2_MIXED_POINTER_ITER(T,f,pf,ff)   do {    \
		LIBFLAT_DBGTFMF(AGGREGATE_FLATTEN_STRUCT_TYPE2_MIXED_POINTER_ITER,T,f,"%p",(void*)ATTR(f),pf,ff);  \
		const T* _fp = pf((const T*)ATTR(f)); \
        if (_fp) {  \
        	size_t _off = offsetof(_container_type,f);  \
        	struct interval_tree_node *__node = interval_tree_iter_first(&FLCTRL.imap_root, (uint64_t)_ptr+_off,\
        			(uint64_t)_ptr+_off+sizeof(T*)-1);    \
        	assert(__node!=0);  \
        	struct fixup_set_node* __inode = fixup_set_search((uint64_t)_fp);	\
        	if (!__inode) {	\
        		fixup_set_reserve_address((uintptr_t)_fp);	\
                struct flatten_job __job;   \
                __job.node = __node;    \
                __job.offset = (uint64_t)_ptr-__node->start+_off; \
                __job.size = 1;	\
                __job.ptr = (struct flatten_base*)_fp;    \
                __job.fun = (flatten_struct_t)&flatten_struct_type2_##T##_array_iter;    \
                __job.fp = (const struct flatten_base*)_fp; \
                __job.convert = (flatten_struct_mixed_convert_t)&ff; \
                bqueue_push_back(__q,&__job,sizeof(struct flatten_job));    \
        	}	\
			else {	\
				struct interval_tree_node *__struct_node = interval_tree_iter_first(&FLCTRL.imap_root, (uintptr_t)_fp,\
						(uintptr_t)_fp+sizeof(T)-1);    \
				assert(__struct_node!=0);	\
				fixup_set_insert(__node,(uint64_t)_ptr-__node->start+_off,	\
					make_flatten_pointer(__struct_node,(uintptr_t)_fp-__struct_node->start));	\
			}	\
        }   \
    } while(0)

#define AGGREGATE_FLATTEN_STRUCT_TYPE_MIXED_POINTER_ITER(T,f,pf,ff)   do {    \
		LIBFLAT_DBGTFMF(AGGREGATE_FLATTEN_STRUCT_TYPE_MIXED_POINTER_ITER,T,f,"%p",(void*)ATTR(f),pf,ff);  \
        const T* _fp = pf((const T*)ATTR(f)); \
        if (_fp) {  \
            struct flatten_job __job;   \
            __job.node = __node;    \
            __job.offset = offsetof(_container_type,f); \
            __job.size = 1;	\
            __job.ptr = (struct flatten_base*)ATTR(f);    \
            __job.fun = (flatten_struct_t)&flatten_struct_type_iter_##T##_array;    \
            __job.fp = (const struct flatten_base*)_fp; \
            __job.convert = (flatten_struct_mixed_convert_t)&ff; \
            bqueue_push_back(__q,&__job,sizeof(struct flatten_job));    \
        }   \
    } while(0)

#define AGGREGATE_FLATTEN_STRUCT_ARRAY_ITER(T,f,n)	do {	\
		LIBFLAT_DBGM3(AGGREGATE_FLATTEN_STRUCT_ARRAY_ITER,T,f,n);	\
    	if (ATTR(f)) {	\
    		struct flatten_job __job;   \
			__job.node = __node;    \
			__job.offset = offsetof(_container_type,f); \
			__job.size = n;	\
			__job.ptr = (struct flatten_base*)ATTR(f);    \
			__job.fun = (flatten_struct_t)&flatten_struct_##T##_array_iter;    \
			__job.fp = 0;   \
			__job.convert = 0;  \
			bqueue_push_back(__q,&__job,sizeof(struct flatten_job));    \
		}	\
	} while(0)

#define AGGREGATE_FLATTEN_STRUCT2_ARRAY_ITER(T,f,n)	do {	\
		LIBFLAT_DBGM3(AGGREGATE_FLATTEN_STRUCT_ARRAY_ITER,T,f,n);	\
    	if (ATTR(f)&&(n>0)) {	\
    		size_t _off = offsetof(_container_type,f);  \
    		size_t _i;	\
			struct interval_tree_node *__node = interval_tree_iter_first(&FLCTRL.imap_root, (uint64_t)_ptr+_off,\
					(uint64_t)_ptr+_off+sizeof(struct T*)-1);    \
			assert(__node!=0);  \
			fixup_set_insert(__node,(uint64_t)_ptr-__node->start+_off,flatten_plain_type(ATTR(f),(n)*sizeof(struct T)));	\
			for (_i=0; _i<(n); ++_i) {	\
				struct interval_tree_node *__struct_node = interval_tree_iter_first(&FLCTRL.imap_root,	\
					(uint64_t)((void*)ATTR(f)+_i*sizeof(struct T)),(uint64_t)((void*)ATTR(f)+(_i+1)*sizeof(struct T)-1));    \
				assert(__struct_node!=0);	\
				struct fixup_set_node* __struct_inode;	\
				__struct_inode = fixup_set_search((uint64_t)((void*)ATTR(f)+_i*sizeof(struct T)));	\
				if (!__struct_inode) {	\
					fixup_set_reserve_address((uint64_t)((void*)ATTR(f)+_i*sizeof(struct T)));	\
		    		struct flatten_job __job;   \
					__job.node = 0;    \
					__job.offset = 0; \
					__job.size = 1;	\
					__job.ptr = (struct flatten_base*)((void*)ATTR(f)+_i*sizeof(struct T));    \
					__job.fun = (flatten_struct_t)&flatten_struct2_##T##_array_iter;    \
					__job.fp = 0;   \
					__job.convert = 0;  \
					bqueue_push_back(__q,&__job,sizeof(struct flatten_job));    \
				}	\
			}\
		}	\
	} while(0)

#define AGGREGATE_FLATTEN_STRUCT_TYPE_ARRAY_ITER(T,f,n)	do {	\
		LIBFLAT_DBGM3(AGGREGATE_FLATTEN_STRUCT_TYPE_ARRAY_ITER,T,f,n);	\
    	if (ATTR(f)) {	\
    		struct flatten_job __job;   \
			__job.node = __node;    \
			__job.offset = offsetof(_container_type,f); \
			__job.size = n;	\
			__job.ptr = (struct flatten_base*)ATTR(f);    \
			__job.fun = (flatten_struct_t)&flatten_struct_type_##T##_array_iter;    \
			__job.fp = 0;   \
			__job.convert = 0;  \
			bqueue_push_back(__q,&__job,sizeof(struct flatten_job));    \
		}	\
	} while(0)

#define FLATTEN_TYPE(T,p)	do {	\
		LIBFLAT_DBGM2(FLATTEN_TYPE,T,p);	\
		if (p) {   \
			fixup_set_insert(__fptr->node,__fptr->offset,flatten_plain_type((p),sizeof(T)));	\
		}	\
	} while(0)

#define FLATTEN_TYPE_ARRAY(T,p,n)	do {	\
		LIBFLAT_DBGM3(FLATTEN_TYPE_ARRAY,T,p,n);	\
		if (p) {   \
			fixup_set_insert(__fptr->node,__fptr->offset,flatten_plain_type((p),(n)*sizeof(T)));	\
		}	\
	} while(0)

#define FLATTEN_STRING(p)	do {	\
		LIBFLAT_DBGM1(FLATTEN_STRING,p);	\
		if (p&&(strmemlen(p))) {   \
			fixup_set_insert(__fptr->node,__fptr->offset,flatten_plain_type((p),strmemlen(p)));	\
		}	\
	} while(0)

#define AGGREGATE_FLATTEN_TYPE(T,f)   do {  \
		LIBFLAT_DBGM2(AGGREGATE_FLATTEN_TYPE,T,f);	\
        if (ATTR(f)) {   \
            fixup_set_insert(__node,offsetof(_container_type,f),flatten_plain_type(ATTR(f),sizeof(T)));	\
        }   \
    } while(0)

#define AGGREGATE_FLATTEN_TYPE2(T,f)   do {  \
		LIBFLAT_DBGM2(AGGREGATE_FLATTEN_TYPE2,T,f);	\
        if (ATTR(f)) {   \
        	size_t _off = offsetof(_container_type,f);	\
			struct interval_tree_node *__node = interval_tree_iter_first(&FLCTRL.imap_root, (uint64_t)_ptr+_off,\
					(uint64_t)_ptr+_off+sizeof(T*)-1);    \
			assert(__node!=0);	\
			struct fixup_set_node* __inode = fixup_set_search((uint64_t)_ptr+_off);	\
			if (!__inode) {	\
				fixup_set_reserve(__node,(uint64_t)_ptr-__node->start+_off);	\
				fixup_set_update(__node,(uint64_t)_ptr-__node->start+_off,flatten_plain_type(ATTR(f),sizeof(T)));	\
			}	\
        }   \
    } while(0)

#define AGGREGATE_FLATTEN_TYPE_ARRAY(T,f,n)   do {  \
		LIBFLAT_DBGM3(AGGREGATE_FLATTEN_TYPE_ARRAY,T,f,n);	\
        if (ATTR(f)) {   \
            fixup_set_insert(__node,offsetof(_container_type,f),flatten_plain_type(ATTR(f),(n)*sizeof(T)));	\
        }   \
    } while(0)

#define AGGREGATE_FLATTEN_TYPE2_ARRAY(T,f,n)   do {  \
		LIBFLAT_DBGM3(AGGREGATE_FLATTEN_TYPE2_ARRAY,T,f,n);	\
        if (ATTR(f)&&(n>0)) {   \
        	size_t _off = offsetof(_container_type,f);	\
			struct interval_tree_node *__node = interval_tree_iter_first(&FLCTRL.imap_root, (uint64_t)_ptr+_off,\
					(uint64_t)_ptr+_off+sizeof(T*)-1);    \
			assert(__node!=0);	\
			fixup_set_insert(__node,(uint64_t)_ptr-__node->start+_off,flatten_plain_type(ATTR(f),(n)*sizeof(T)));	\
        }   \
    } while(0)

#define AGGREGATE_FLATTEN_STRING(f)   do {  \
		LIBFLAT_DBGF(AGGREGATE_FLATTEN_STRING,f,"%p",(void*)ATTR(f));	\
        if (ATTR(f)) {   \
        	fixup_set_insert(__node,offsetof(_container_type,f),flatten_plain_type(ATTR(f),strmemlen(ATTR(f))));	\
        }   \
    } while(0)

#define AGGREGATE_FLATTEN_STRING2(f)   do {  \
		LIBFLAT_DBGF(AGGREGATE_FLATTEN_STRING2,f,"%p",(void*)ATTR(f));	\
        if (ATTR(f)&&(strmemlen(ATTR(f))>0)) {   \
        	size_t _off = offsetof(_container_type,f);  \
        	struct interval_tree_node *__node = interval_tree_iter_first(&FLCTRL.imap_root, (uint64_t)_ptr+_off,\
					(uint64_t)_ptr+_off+sizeof(struct T*)-1);    \
			assert(__node!=0);  \
        	fixup_set_insert(__node,(uint64_t)_ptr-__node->start+_off,flatten_plain_type(ATTR(f),strmemlen(ATTR(f))));	\
        }   \
    } while(0)

#define FOREACH_POINTER(PTRTYPE,v,p,s,...)	do {	\
		LIBFLAT_DBGM4(FOREACH_POINTER,PTRTYPE,v,p,s);	\
		if (p) {	\
			PTRTYPE const * _m = (PTRTYPE const *)(p);	\
			size_t _i, _sz = (s);	\
			for (_i=0; _i<_sz; ++_i) {	\
				struct flatten_pointer* __fptr = get_pointer_node(_m+_i);	\
				PTRTYPE v = *(_m+_i);	\
				__VA_ARGS__;	\
				free(__fptr);	\
			}	\
		}	\
	} while(0)


#define FOR_POINTER(PTRTYPE,v,p,...)	do {	\
		LIBFLAT_DBGM3(FOR_POINTER,PTRTYPE,v,p);	\
		if (p) {	\
			PTRTYPE const * _m = (PTRTYPE const *)(p);	\
			struct flatten_pointer* __fptr = get_pointer_node(_m);	\
			PTRTYPE v = *(_m);	\
			__VA_ARGS__;	\
			free(__fptr);	\
		}	\
	} while(0)

#define UNDER_ITER_HARNESS(...)	\
		do {	\
			struct bqueue bq;	\
			bqueue_init(&bq,DEFAULT_ITER_QUEUE_SIZE);	\
			struct bqueue* __q = &bq;	\
			__VA_ARGS__	\
			while(!bqueue_empty(&bq)) {	\
				struct flatten_job __job;	\
				bqueue_pop_front(&bq,&__job,sizeof(struct flatten_job));	\
				if (!__job.convert)	\
					fixup_set_insert(__job.node,__job.offset,(__job.fun)(__job.ptr,__job.size,&bq));	\
				else	\
					fixup_set_insert(__job.node,__job.offset,__job.convert((__job.fun)(__job.fp,__job.size,&bq),__job.ptr));	\
			}	\
			bqueue_destroy(&bq);	\
		} while(0)

#define UNDER_ITER_HARNESS2(...)	\
		do {	\
			struct bqueue bq;	\
			bqueue_init(&bq,DEFAULT_ITER_QUEUE_SIZE);	\
			struct bqueue* __q = &bq;	\
			__VA_ARGS__	\
			while(!bqueue_empty(&bq)) {	\
				struct flatten_job __job;	\
				bqueue_pop_front(&bq,&__job,sizeof(struct flatten_job));	\
				if (__job.node!=0) {	\
					if (!__job.convert)	\
						fixup_set_insert(__job.node,__job.offset,(__job.fun)(__job.ptr,__job.size,&bq));	\
					else	\
						fixup_set_insert(__job.node,__job.offset,__job.convert((__job.fun)(__job.fp,__job.size,&bq),__job.ptr));	\
				}	\
				else {	\
					void* _fp;	\
					if (!__job.convert)	\
						_fp = (__job.fun)(__job.ptr,__job.size,&bq);	\
					else	\
						_fp = __job.convert((__job.fun)(__job.fp,__job.size,&bq),__job.ptr);	\
					free(_fp);	\
				}	\
			}	\
			bqueue_destroy(&bq);	\
		} while(0)

#define PTRNODE(PTRV)	(interval_tree_iter_first(&FLCTRL.imap_root, (uintptr_t)(PTRV), (uintptr_t)(PTRV)))
#define ROOT_POINTER_NEXT(PTRTYPE)	((PTRTYPE)(root_pointer_next()))
#define ROOT_POINTER_SEQ(PTRTYPE,n)	((PTRTYPE)(root_pointer_seq(n)))

#define FOR_ROOT_POINTER(p,...)	do {	\
		LIBFLAT_DBGM1(FOR_ROOT_POINTER,p);	\
		if (p) {	\
			struct flatten_pointer* __fptr = make_flatten_pointer(0,0);	\
			__VA_ARGS__;	\
			free(__fptr);	\
		}	\
		root_addr_append( (uintptr_t)(p) );	\
	} while(0)

#define FLATTEN_MEMORY_START	((unsigned char*)FLCTRL.mem+FLCTRL.HDR.ptr_count*sizeof(size_t))
#define FLATTEN_MEMORY_END		(FLATTEN_MEMORY_START+FLCTRL.HDR.memory_size)

static inline void libflat_free (void* ptr) {

	if ( (FLCTRL.mem) && ((unsigned char*)ptr>=FLATTEN_MEMORY_START) && ((unsigned char*)ptr<FLATTEN_MEMORY_END) ) {
		/* Trying to free a part of unflatten memory. Do nothing */
	}
	else {
		/* Original free. Make sure "free" is not redefined at this point */
		free(ptr);
	}
}

static inline void* libflat_realloc (void* ptr, size_t size) {

	if ( (FLCTRL.mem) && ((unsigned char*)ptr>=FLATTEN_MEMORY_START) && ((unsigned char*)ptr<FLATTEN_MEMORY_END) ) {
		/* Trying to realloc a part of unflatten memory. Allocate new storage and let the part of unflatten memory fade away */
		void* m = malloc(size);
		if (m) {
			memcpy(m,ptr,((unsigned char*)ptr+size>FLATTEN_MEMORY_END)?((size_t)(FLATTEN_MEMORY_END-(unsigned char*)ptr)):(size));
			return m;
		}
		else return ptr;
	}
	else {
		/* Original realloc. Make sure "realloc" is not redefined at this point */
		return realloc(ptr,size);
	}
}

#endif /* __LIBFLAT_H__ */
