#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
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
