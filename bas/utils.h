#ifndef __UTILS_H__
#define __UTILS_H__

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#include <sys/time.h>

#define STRUCT_CREATE(tag)  ((type*)calloc(1,sizeof(struct tag)))
#define TYPE_CREATE(type)  ((type*)calloc(1,sizeof(type)))

#define TIME_MARK_START(start_marker)		\
		struct timeval  tv_mark_##start_marker;	\
		gettimeofday(&tv_mark_##start_marker, 0)
#define TIME_CHECK_ON(start_marker,end_marker)	do {	\
		struct timeval  tv_mark_##start_marker##_##end_marker;	\
		gettimeofday(&tv_mark_##start_marker##_##end_marker, 0);	\
		printf("@Elapsed ("#start_marker" -> "#end_marker"): (%f)[s]\n",	\
		(double) (tv_mark_##start_marker##_##end_marker.tv_usec - tv_mark_##start_marker.tv_usec) / 1000000 +	\
		         (double) (tv_mark_##start_marker##_##end_marker.tv_sec - tv_mark_##start_marker.tv_sec) );	\
	} while(0)
#define TIME_CHECK_FMT(start_marker,end_marker,fmt)	do {	\
		struct timeval  tv_mark_##start_marker##_##end_marker;	\
		gettimeofday(&tv_mark_##start_marker##_##end_marker, 0);	\
		printf(fmt,	\
		(double) (tv_mark_##start_marker##_##end_marker.tv_usec - tv_mark_##start_marker.tv_usec) / 1000000 +	\
		         (double) (tv_mark_##start_marker##_##end_marker.tv_sec - tv_mark_##start_marker.tv_sec) );	\
	} while(0)

#define ERRMSG_BUFFER_SIZE 8192

#define ASSERT_WITH_NFSDB_ERROR(__expr,__msg,...) do {	\
		if (!(__expr)) {	\
			PyErr_SetString(libetrace_nfsdbError, __msg);	\
			return 0;	\
		}	\
	} while(0)

#define ASSERT_BREAK_WITH_NFSDB_ERROR(__expr,__msg,...) {	\
		if (!(__expr)) {	\
			PyErr_SetString(libetrace_nfsdbError, __msg);	\
			break;	\
		}	\
	}

#define ASSERT_WITH_NFSDB_FORMAT_ERROR(__expr,__msg,...) do {	\
		if (!(__expr)) {	\
			snprintf(errmsg,ERRMSG_BUFFER_SIZE,__msg,__VA_ARGS__);	\
			PyErr_SetString(libetrace_nfsdbError, errmsg);	\
			return 0;	\
		}	\
	} while(0)

#define ASSERT_BREAK_WITH_NFSDB_FORMAT_ERROR(__expr,__msg,...) {	\
		if (!(__expr)) {	\
			snprintf(errmsg,ERRMSG_BUFFER_SIZE,__msg,__VA_ARGS__);	\
			PyErr_SetString(libetrace_nfsdbError, errmsg);	\
			break;	\
		}	\
	}

#define ASSERT_RETURN_WITH_NFSDB_FORMAT_ERROR(__expr,__msg,__retcode,...) do {	\
		if (!(__expr)) {	\
			snprintf(errmsg,ERRMSG_BUFFER_SIZE,__msg,__VA_ARGS__);	\
			PyErr_SetString(libetrace_nfsdbError, errmsg);	\
			return __retcode;	\
		}	\
	} while(0)

#define ASSERT_WITH_FTDB_ERROR(__expr,__msg,...) do {	\
		if (!(__expr)) {	\
			PyErr_SetString(libftdb_ftdbError, __msg);	\
			return 0;	\
		}	\
	} while(0)

#define ASSERT_WITH_FTDB_FORMAT_ERROR(__expr,__msg,...) do {	\
		if (!(__expr)) {	\
			snprintf(errmsg,ERRMSG_BUFFER_SIZE,__msg,__VA_ARGS__);	\
			PyErr_SetString(libftdb_ftdbError, errmsg);	\
			return 0;	\
		}	\
	} while(0)

#define ASSERT_RETURN_WITH_FTDB_FORMAT_ERROR(__expr,__msg,__retcode,...) do {	\
		if (!(__expr)) {	\
			snprintf(errmsg,ERRMSG_BUFFER_SIZE,__msg,__VA_ARGS__);	\
			PyErr_SetString(libftdb_ftdbError, errmsg);	\
			return __retcode;	\
		}	\
	} while(0)

#define DBG(n,...)		do { if (n)	printf(__VA_ARGS__); } while(0)
#ifndef DBGC
#define DBGC(...)
#endif
#define kv_roundup32(x) (--(x), (x)|=(x)>>1, (x)|=(x)>>2, (x)|=(x)>>4, (x)|=(x)>>8, (x)|=(x)>>16, ++(x))

#define kvec_t(type) struct { size_t n, m; type *a; }
#define kv_init(v) ((v).n = (v).m = 0, (v).a = 0)
#define kv_destroy(v) free((v).a)
#define kv_A(v, i) ((v).a[(i)])
#define kv_pop(v) ((v).a[--(v).n])
#define kv_size(v) ((v).n)
#define kv_max(v) ((v).m)

#define kv_resize(type, v, s)  ((v).m = (s), (v).a = (type*)realloc((v).a, sizeof(type) * (v).m))

#define kv_push(type, v, x) do {									\
		if ((v).n == (v).m) {										\
			(v).m = (v).m? (v).m<<1 : 2;							\
			(v).a = (type*)realloc((v).a, sizeof(type) * (v).m);	\
		}															\
		(v).a[(v).n++] = (x);										\
	} while (0)

#define kv_pushp(type, v) (((v).n == (v).m)?							\
						   ((v).m = ((v).m? (v).m<<1 : 2),				\
							(v).a = (type*)realloc((v).a, sizeof(type) * (v).m), 0)	\
						   : 0), ((v).a + ((v).n++))

#define kv_a(type, v, i) ((v).m <= (size_t)(i)?						\
						  ((v).m = (v).n = (i) + 1, kv_roundup32((v).m), \
						   (v).a = (type*)realloc((v).a, sizeof(type) * (v).m), 0) \
						  : (v).n <= (size_t)(i)? (v).n = (i)			\
						  : 0), (v).a[(i)]

#define VEC_DECL(type)				kvec_t(type)
#define VEC_INIT(v)					kv_init(v)
#define VEC_APPEND(type,v,value)	kv_push(type,v,value)
#define VEC_POPBACK(v)				kv_pop(v)
#define VEC_ACCESS(v,index)			kv_A(v,index)
#define VEC_INSERT(type,v,index)	kv_a(type,v,index)
#define VEC_BACK(v)					kv_A(v,kv_size(v)-1)
#define VEC_SIZE(v)					kv_size(v)
#define VEC_DESTROY(v)				kv_destroy(v)

typedef VEC_DECL(const char*) vs_t;
typedef vs_t* pvs_t;

#define lazy_alloc_output() do { \
	if (!output) { \
		output = __output = malloc(2*len+1); \
		memcpy(output,s,i); \
		output+=i; \
	} \
} while(0)

#define MAKE_JSON_ESCAPED(s,rfs) \
	const char* s; \
	const char* __##s; \
	do { \
		__##s = json_escape((const char*)rfs); \
		if (__##s) { \
			s = __##s; \
		} \
		else { \
			s = (const char*)rfs; \
		} \
	} \
    while(0)

#define FREE_JSON_ESCAPED(s) \
	do { \
		free((void*)__##s); \
	} \
	while(0)

static inline char* strappend(char* s, const char* news) {
	size_t ssize = strlen(s);
	size_t newsize = strlen(news);
	s = (char*)realloc(s,ssize+newsize+1);
	assert(s);
	memcpy(s+ssize,news,newsize+1);
	return s;
}

static inline char* strnappend(char* s, const char* news, size_t newsize) {
	size_t ssize = strlen(s);
	s = (char*)realloc(s,ssize+newsize+1);
	assert(s);
	memcpy(s+ssize,news,newsize);
	s[ssize+newsize] = 0;
	return s;
}

#if defined __cplusplus
extern "C" {
#endif
	const char* json_escape(const char* s);
	ssize_t count_file_lines(const char* path);
	char *normpath(const char *in);
	const char* path_join(const char* start, const char* end);
	unsigned char* base64_decode (const char *text, size_t* out_len);
	char * base64_encode (const unsigned char* data, size_t len);
#if defined __cplusplus
}
#endif

#endif /* __UTILS_H__ */
