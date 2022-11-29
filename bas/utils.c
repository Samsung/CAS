#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <ctype.h>
#include "utils.h"

#define RD_BUFSIZE	128*1024

#ifdef EINTR
# define IS_EINTR(x) ((x) == EINTR)
#else
# define IS_EINTR(x) 0
#endif

/* Taken from https://github.com/nbsdx/SimpleJSON.git: public license */
const char* json_escape(const char* s) {
    if (!s) return 0;
	size_t len = strlen(s);
    char* output = 0;
    char* __output = 0;
    unsigned i;
    for(i = 0; i < len; ++i ) {
    	if (((unsigned char)s[i]>=0x80)||(s[i]<=0x1F)) {
    		lazy_alloc_output();
    	}
        switch( s[i] ) {
            case '\"': { lazy_alloc_output(); *output++ = '\\'; *output++ = '\"'; break; }
            case '\\': { lazy_alloc_output(); *output++ = '\\'; *output++ = '\\'; break; }
            case '\b': { lazy_alloc_output(); *output++ = '\\'; *output++ = 'b'; break; }
            case '\f': { lazy_alloc_output(); *output++ = '\\'; *output++ = 'f'; break; }
            case '\n': { lazy_alloc_output(); *output++ = '\\'; *output++ = 'n'; break; }
            case '\r': { lazy_alloc_output(); *output++ = '\\'; *output++ = 'r'; break; }
            case '\t': { lazy_alloc_output(); *output++ = '\\'; *output++ = 't'; break; }
            default  :
            {
                if (output) {
					if (((unsigned char)s[i]<0x80)&&(s[i]>0x1F)) { // Ignore non-ASCII characters
						*output++ = s[i];
					}
                }
                break;
            }
        }
    }
    if (output) {
    	*output++=0;
    }
    if (output)
    	return __output;
    else
    	return 0;
}

/* Taken from GNU CoreUtils 6.9 cat: GPL v.2 */
ssize_t safe_rd (int fd, void const *buf, size_t count)
{
  for (;;)
    {
      ssize_t result = read (fd, (void*)buf, count);

      if (0 <= result)
        return result;
      else if (IS_EINTR (errno)) {
        continue;
      }
      else
        return result;
    }
}

ssize_t count_file_lines(const char* path) {

	int fdr = open(path,O_RDONLY);
	if (fdr<0) {
		printf("Failed to open %s for reading: %d\n",path,errno);
		return fdr;
	}
	/* Count number of lines */
	void* pgbuff = malloc(RD_BUFSIZE);
	assert(pgbuff!=0);
	size_t total_nread = 0;
	ssize_t nlcount = 0;
	while(1) {
		ssize_t n_read = safe_rd (fdr, pgbuff, RD_BUFSIZE);
		if (n_read<0) {
			fprintf(stderr,"ERROR[read]: %lu, %d\n",total_nread,errno);
			close(fdr);
			return -1;
		}
		if (n_read == 0) {
			break;
		}
		/* n_read was read in */
		total_nread+=n_read;
		size_t noff = 0;
		void* ppnewl = pgbuff;
		void* pnewl = memchr(pgbuff,'\n',RD_BUFSIZE);
		while(pnewl) {
			noff+=pnewl-ppnewl+1;
			nlcount++;
			ppnewl = pnewl+1;
			pnewl = memchr(pnewl+1,'\n',RD_BUFSIZE-noff);
		}
	}
	nlcount++;
	free(pgbuff);
	close(fdr);
	return nlcount;
}

#define COMP_MAX 50

#define ispathsep(ch)   ((ch) == '/' || (ch) == '\\')
#define iseos(ch)       ((ch) == '\0')
#define ispathend(ch)   (ispathsep(ch) || iseos(ch))

/* Taken from https://gist.github.com/starwing/2761647 */
char *normpath(const char *in) {
    char* out = malloc(strlen(in)+1);
    char *pos[COMP_MAX], **top = pos, *head = out;
    int isabs = ispathsep(*in);

    if (isabs) *out++ = '/';
    *top++ = out;

    while (!iseos(*in)) {
        while (ispathsep(*in)) ++in;

        if (iseos(*in))
            break;

        if (memcmp(in, ".", 1) == 0 && ispathend(in[1])) {
            ++in;
            continue;
        }

        if (memcmp(in, "..", 2) == 0 && ispathend(in[2])) {
            in += 2;
            if (top != pos + 1)
                out = *--top;
            else if (isabs)
                out = top[-1];
            else {
                strcpy(out, "../");
                out += 3;
            }
            continue;
        }

        if (top - pos >= COMP_MAX)
            return NULL; /* path too complicated */

        *top++ = out;
        while (!ispathend(*in))
            *out++ = *in++;
        if (ispathsep(*in))
            *out++ = '/';
    }

    *out = '\0';
    if (*head == '\0')
        strcpy(head, "./");
    return head;
}

const char* path_join(const char* start, const char* end) {

	if (end[0]=='/') {
		return 0;
	}

	size_t ssz = strlen(start);
	size_t esz = strlen(end);

	char* s = malloc(ssz+esz+2);
	memcpy(s,start,ssz);
	s[ssz] = '/';
	memcpy(s+ssz+1,end,esz);
	s[ssz+1+esz] = 0;

	char* ns = normpath(s);
	free(s);

	return ns;
}

/* The following code that originally implemented 'vsnprintf' function was taken
 *  from the Linux kernel source tree: GPL v.2
 */

#define SIGN	1		/* unsigned/signed, must be 1 */
#define LEFT	2		/* left justified */
#define PLUS	4		/* show plus */
#define SPACE	8		/* space if plus */
#define ZEROPAD	16		/* pad with zero, must be 16 == '0' - ' ' */
#define SMALL	32		/* use lowercase in hex (must be 32 == 0x20) */
#define SPECIAL	64		/* prefix hex with "0x", octal with "0" */

#define STATIC_ASSERT_FAIL_ON(condition) \
	_Static_assert(!(condition), "STATIC ASSERT failed: " #condition)

struct printf_spec {
	unsigned int	type:8;		/* format_type enum */
	signed int	field_width:24;	/* width of output field */
	unsigned int	flags:8;	/* flags to number() */
	unsigned int	base:8;		/* number base, 8, 10 or 16 only */
	signed int	precision:16;	/* # of digits/chars */
};

int skip_atoi(const char **s)
{
	int i = 0;

	do {
		i = i*10 + *((*s)++) - '0';
	} while (isdigit(**s));

	return i;
}

int format_decode(const char *fmt, struct printf_spec *spec)
{
	const char *start = fmt;
	char qualifier;

	/* we finished early by reading the field width */
	if (spec->type == FORMAT_TYPE_WIDTH) {
		if (spec->field_width < 0) {
			spec->field_width = -spec->field_width;
			spec->flags |= LEFT;
		}
		spec->type = FORMAT_TYPE_NONE;
		goto precision;
	}

	/* we finished early by reading the precision */
	if (spec->type == FORMAT_TYPE_PRECISION) {
		if (spec->precision < 0)
			spec->precision = 0;

		spec->type = FORMAT_TYPE_NONE;
		goto qualifier;
	}

	/* By default */
	spec->type = FORMAT_TYPE_NONE;

	for (; *fmt ; ++fmt) {
		if (*fmt == '%')
			break;
	}

	/* Return the current non-format string */
	if (fmt != start || !*fmt)
		return fmt - start;

	/* Process flags */
	spec->flags = 0;

	while (1) { /* this also skips first '%' */
		int found = 1;

		++fmt;

		switch (*fmt) {
		case '-': spec->flags |= LEFT;    break;
		case '+': spec->flags |= PLUS;    break;
		case ' ': spec->flags |= SPACE;   break;
		case '#': spec->flags |= SPECIAL; break;
		case '0': spec->flags |= ZEROPAD; break;
		default:  found = 0;
		}

		if (!found)
			break;
	}

	/* get field width */
	spec->field_width = -1;

	if (isdigit(*fmt))
		spec->field_width = skip_atoi(&fmt);
	else if (*fmt == '*') {
		/* it's the next argument */
		spec->type = FORMAT_TYPE_WIDTH;
		return ++fmt - start;
	}

precision:
	/* get the precision */
	spec->precision = -1;
	if (*fmt == '.') {
		++fmt;
		if (isdigit(*fmt)) {
			spec->precision = skip_atoi(&fmt);
			if (spec->precision < 0)
				spec->precision = 0;
		} else if (*fmt == '*') {
			/* it's the next argument */
			spec->type = FORMAT_TYPE_PRECISION;
			return ++fmt - start;
		}
	}

qualifier:
	/* get the conversion qualifier */
	qualifier = 0;
	if (*fmt == 'h' || _tolower(*fmt) == 'l' ||
	    *fmt == 'z' || *fmt == 't') {
		qualifier = *fmt++;
		if (qualifier == *fmt) {
			if (qualifier == 'l') {
				qualifier = 'L';
				++fmt;
			} else if (qualifier == 'h') {
				qualifier = 'H';
				++fmt;
			}
		}
	}

	/* default base */
	spec->base = 10;
	switch (*fmt) {
	case 'c':
		spec->type = FORMAT_TYPE_CHAR;
		return ++fmt - start;

	case 's':
		spec->type = FORMAT_TYPE_STR;
		return ++fmt - start;

	case 'p':
		spec->type = FORMAT_TYPE_PTR;
		return ++fmt - start;

	case '%':
		spec->type = FORMAT_TYPE_PERCENT_CHAR;
		return ++fmt - start;

	/* integer number formats - set up the flags and "break" */
	case 'o':
		spec->base = 8;
		break;

	case 'x':
		spec->flags |= SMALL;
		/* fall through */

	case 'X':
		spec->base = 16;
		break;

	case 'd':
	case 'i':
		spec->flags |= SIGN;
	case 'u':
		break;

	case 'n':
		/*
		 * Since %n poses a greater security risk than
		 * utility, treat it as any other invalid or
		 * unsupported format specifier.
		 */
		/* fall through */

	default:
		printf("Please remove unsupported %%%c in format string\n", *fmt);
		spec->type = FORMAT_TYPE_INVALID;
		return fmt - start;
	}

	if (qualifier == 'L')
		spec->type = FORMAT_TYPE_LONG_LONG;
	else if (qualifier == 'l') {
		STATIC_ASSERT_FAIL_ON(FORMAT_TYPE_ULONG + SIGN != FORMAT_TYPE_LONG);
		spec->type = FORMAT_TYPE_ULONG + (spec->flags & SIGN);
	} else if (qualifier == 'z') {
		spec->type = FORMAT_TYPE_SIZE_T;
	} else if (qualifier == 't') {
		spec->type = FORMAT_TYPE_PTRDIFF;
	} else if (qualifier == 'H') {
		STATIC_ASSERT_FAIL_ON(FORMAT_TYPE_UBYTE + SIGN != FORMAT_TYPE_BYTE);
		spec->type = FORMAT_TYPE_UBYTE + (spec->flags & SIGN);
	} else if (qualifier == 'h') {
		STATIC_ASSERT_FAIL_ON(FORMAT_TYPE_USHORT + SIGN != FORMAT_TYPE_SHORT);
		spec->type = FORMAT_TYPE_USHORT + (spec->flags & SIGN);
	} else {
		STATIC_ASSERT_FAIL_ON(FORMAT_TYPE_UINT + SIGN != FORMAT_TYPE_INT);
		spec->type = FORMAT_TYPE_UINT + (spec->flags & SIGN);
	}

	return ++fmt - start;
}

/* Parses the C format type specifier and places types of format parameters into the 'par_type' array
 * Parameter 'count' specifies the initial size of the 'par_type' array. This array is expanded when
 *  the number of format type parameters exceeds the initial size
 * Returns number of format string parameters detected in the 'fmt'
 * For example:
 *  "This is format string that contains %s, %d and %p. Enjoy!"
 * should return 3 and 'par_type' array should contain:
 * [ FORMAT_TYPE_STR, FORMAT_TYPE_INT, FORMAT_TYPE_PTR ]
 * On error returns -1 (but 'par_type' can be reallocated), when no format string parameters exist returns 0
 */
int vsnprintf_parse_format(enum format_type** par_type, size_t count, const char *fmt) {
	struct printf_spec spec = {0};
	size_t alloc_count = count;
	int n = 0;

	while (*fmt) {
		int read = format_decode(fmt, &spec);

		fmt += read;

		switch (spec.type) {
			case FORMAT_TYPE_NONE:
			case FORMAT_TYPE_WIDTH:
			case FORMAT_TYPE_PRECISION:
			case FORMAT_TYPE_PERCENT_CHAR:
				break;
			case FORMAT_TYPE_INVALID:
				return -1;
			case FORMAT_TYPE_CHAR:
			case FORMAT_TYPE_STR:
			case FORMAT_TYPE_PTR:
			case FORMAT_TYPE_LONG_LONG:
			case FORMAT_TYPE_ULONG:
			case FORMAT_TYPE_LONG:
			case FORMAT_TYPE_SIZE_T:
			case FORMAT_TYPE_PTRDIFF:
			case FORMAT_TYPE_UBYTE:
			case FORMAT_TYPE_BYTE:
			case FORMAT_TYPE_USHORT:
			case FORMAT_TYPE_SHORT:
			case FORMAT_TYPE_INT:
			default:
				if (alloc_count<=n) {
					enum format_type* tmp = realloc(*par_type,((alloc_count*3)/2)*sizeof(enum format_type));
					if (tmp) {
						*par_type = tmp;
						alloc_count = (alloc_count*3)/2;
					}
					else return -1;
				}
				(*par_type)[n++] = spec.type;
		}
	}

	return n;
}
