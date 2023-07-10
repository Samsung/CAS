#ifndef __UTILS_H__
#define __UTILS_H__

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <sys/time.h>

/* Taken from Python 3.8 source code which is GPL compatible */
#define Py_RETURN_RICHCOMPARE_internal(val1, val2, op)                      \
    do {                                                                    \
        switch (op) {                                                       \
        case Py_EQ: if ((val1) == (val2)) Py_RETURN_TRUE; Py_RETURN_FALSE;  \
        case Py_NE: if ((val1) != (val2)) Py_RETURN_TRUE; Py_RETURN_FALSE;  \
        case Py_LT: if ((val1) < (val2)) Py_RETURN_TRUE; Py_RETURN_FALSE;   \
        case Py_GT: if ((val1) > (val2)) Py_RETURN_TRUE; Py_RETURN_FALSE;   \
        case Py_LE: if ((val1) <= (val2)) Py_RETURN_TRUE; Py_RETURN_FALSE;  \
        case Py_GE: if ((val1) >= (val2)) Py_RETURN_TRUE; Py_RETURN_FALSE;  \
        default:                                                            \
            Py_UNREACHABLE();                                               \
        }                                                                   \
    } while (0)

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

#define lazy_alloc_output() do { \
	if (!output) { \
		output = __output = malloc(2*len+1); \
		memcpy(output,s,i); \
		output+=i; \
	} \
} while(0)

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
	char *normpath(const char *in);
	const char* path_join(const char* start, const char* end);
	unsigned char* base64_decode (const char *text, size_t* out_len);
	char * base64_encode (const unsigned char* data, size_t len);

	enum format_type {
		FORMAT_TYPE_CHAR,
		FORMAT_TYPE_STR,
		FORMAT_TYPE_PTR,
		FORMAT_TYPE_LONG_LONG,
		FORMAT_TYPE_ULONG,
		FORMAT_TYPE_LONG,
		FORMAT_TYPE_SIZE_T,
		FORMAT_TYPE_PTRDIFF,
		FORMAT_TYPE_UBYTE,
		FORMAT_TYPE_BYTE,
		FORMAT_TYPE_USHORT,
		FORMAT_TYPE_SHORT,
		FORMAT_TYPE_UINT,
		FORMAT_TYPE_INT,
		FORMAT_TYPE_WIDTH,
		FORMAT_TYPE_PRECISION,
		/* Internal */
		FORMAT_TYPE_NONE,
		FORMAT_TYPE_PERCENT_CHAR,
		FORMAT_TYPE_INVALID,
	};
	int vsnprintf_parse_format(enum format_type** par_type, size_t count, const char *fmt);
#if defined __cplusplus
}
#endif

#endif /* __UTILS_H__ */
