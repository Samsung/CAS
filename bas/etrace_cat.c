/* cat -- concatenate files and print on the standard output.
   Copyright (C) 88, 90, 91, 1995-2006 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

/* Differences from the Unix cat:
   * Always unbuffered, -u is ignored.
   * Usually much faster than other versions of cat, the difference
   is especially apparent when using the -v option.

   By tege@sics.se, Torbjorn Granlund, advised by rms, Richard Stallman.  */

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include <malloc.h>
#include <stdarg.h>

#if HAVE_STROPTS_H
# include <stropts.h>
#endif
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/types.h>
#include <getopt.h>
#include <locale.h>
#include <libintl.h>
#include <stdio_ext.h>
#include <ctype.h>
#include <wchar.h>
#include <wctype.h>

int interrupt;

static void  INThandler(int sig)
{
     interrupt = 1;
}

size_t
full_write (int fd, const void *buf, size_t count);

#define _(msgid) gettext (msgid)
#define N_(msgid) msgid

#ifndef O_BINARY
# define O_BINARY 0
# define O_TEXT 0
#endif

int volatile exit_failure = EXIT_FAILURE;

/* The official name of this program (e.g., no `g' prefix).  */
#define PROGRAM_NAME "cat"

#define AUTHORS "Torbjorn Granlund", "Richard M. Stallman"

/* Undefine, to avoid warning about redefinition on some systems.  */
#undef max
#define max(h,i) ((h) > (i) ? (h) : (i))

/* Name under which this program was invoked.  */
char *program_name;

/* Name of input file.  May be "-".  */
static char const *infile;

/* Descriptor on which input file is open.  */
static int input_desc;

/* Buffer for line numbers.
   An 11 digit counter may overflow within an hour on a P2/466,
   an 18 digit counter needs about 1000y */
#define LINE_COUNTER_BUF_LEN 20
static char line_buf[LINE_COUNTER_BUF_LEN] =
  {
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '0',
    '\t', '\0'
  };

/* Position in `line_buf' where printing starts.  This will not change
   unless the number of lines is larger than 999999.  */
static char *line_num_print = line_buf + LINE_COUNTER_BUF_LEN - 8;

/* Position of the first digit in `line_buf'.  */
static char *line_num_start = line_buf + LINE_COUNTER_BUF_LEN - 3;

/* Position of the last digit in `line_buf'.  */
static char *line_num_end = line_buf + LINE_COUNTER_BUF_LEN - 3;

/* Preserves the `cat' function's local `newlines' between invocations.  */
static int newlines2 = 0;

#define HELP_OPTION_DESCRIPTION \
  _("      --help     display this help and exit\n")
#define VERSION_OPTION_DESCRIPTION \
  _("      --version  output version information and exit\n")

/* Name of package */
#define PACKAGE "coreutils"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "bug-coreutils@gnu.org"

/* Define to the full name of this package. */
#define PACKAGE_NAME "GNU coreutils"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "GNU coreutils 6.9"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "coreutils"

/* Define to the version of this package. */
#define PACKAGE_VERSION "6.9"

#define SAFE_READ_ERROR ((size_t) -1)

extern void error (int __status, int __errnum, const char *__format, ...)
     __attribute__ ((__format__ (__printf__, 3, 4)));

#  define static_inline static inline

/* Return 1 if an array of N objects, each of size S, cannot exist due
   to size arithmetic overflow.  S must be positive and N must be
   nonnegative.  This is a macro, not an inline function, so that it
   works correctly even when SIZE_MAX < N.

   By gnulib convention, SIZE_MAX represents overflow in size
   calculations, so the conservative dividend to use here is
   SIZE_MAX - 1, since SIZE_MAX might represent an overflowed value.
   However, malloc (SIZE_MAX) fails on all known hosts where
   sizeof (ptrdiff_t) <= sizeof (size_t), so do not bother to test for
   exactly-SIZE_MAX allocations on such hosts; this avoids a test and
   branch when S is known to be 1.  */
# define xalloc_oversized(n, s) \
    ((size_t) (sizeof (ptrdiff_t) <= sizeof (size_t) ? -1 : -2) / (s) < (n))

void
xalloc_die (void)
{
  error (exit_failure, 0, "%s", _("memory exhausted"));

  /* The `noreturn' cannot be given to error, since it may return if
     its first argument is 0.  To help compilers understand the
     xalloc_die does not return, call abort.  Also, the abort is a
     safety feature if exit_failure is 0 (which shouldn't happen).  */
  abort ();
}

/* Allocate N bytes of memory dynamically, with error checking.  */

void *
xmalloc (size_t n)
{
  void *p = malloc (n);
  if (!p && n != 0)
    xalloc_die ();
  return p;
}

void *
xrealloc (void *p, size_t n)
{
  p = realloc (p, n);
  if (!p && n != 0)
    xalloc_die ();
  return p;
}

/* Allocate an array of N objects, each with S bytes of memory,
   dynamically, with error checking.  S must be nonzero.  */

static_inline void *
xnmalloc (size_t n, size_t s)
{
  if (xalloc_oversized (n, s))
    xalloc_die ();
  return xmalloc (n * s);
}

/* Change the size of an allocated block of memory P to an array of N
   objects each of S bytes, with error checking.  S must be nonzero.  */

static_inline void *
xnrealloc (void *p, size_t n, size_t s)
{
  if (xalloc_oversized (n, s))
    xalloc_die ();
  return xrealloc (p, n * s);
}

/* Allocate an object of type T dynamically, with error checking.  */
/* extern t *XMALLOC (typename t); */
# define XMALLOC(t) ((t *) xmalloc (sizeof (t)))

/* Allocate memory for N elements of type T, with error checking.  */
/* extern t *XNMALLOC (size_t n, typename t); */
# define XNMALLOC(n, t) \
    ((t *) (sizeof (t) == 1 ? xmalloc (n) : xnmalloc (n, sizeof (t))))

/* Allocate an object of type T dynamically, with error checking,
   and zero it.  */
/* extern t *XZALLOC (typename t); */
# define XZALLOC(t) ((t *) xzalloc (sizeof (t)))

/* Allocate memory for N elements of type T, with error checking,
   and zero it.  */
/* extern t *XCALLOC (size_t n, typename t); */
# define XCALLOC(n, t) \
    ((t *) (sizeof (t) == 1 ? xzalloc (n) : xcalloc (n, sizeof (t))))

static_inline char *
xcharalloc (size_t n)
{
  return XNMALLOC (n, char);
}

void
usage (int status)
{
  if (status != EXIT_SUCCESS)
    fprintf (stderr, _("Try `%s --help' for more information.\n"),
	     program_name);
  else
    {
      printf (_("\
Usage: %s [OPTION] [FILE]...\n\
"),
	      program_name);
      fputs (_("\
Concatenate FILE(s), or standard input, to standard output.\n\
\n\
  -A, --show-all           equivalent to -vET\n\
  -b, --number-nonblank    number nonblank output lines\n\
  -e                       equivalent to -vE\n\
  -E, --show-ends          display $ at end of each line\n\
  -n, --number             number all output lines\n\
  -s, --squeeze-blank      never more than one single blank line\n\
"), stdout);
      fputs (_("\
  -t                       equivalent to -vT\n\
  -T, --show-tabs          display TAB characters as ^I\n\
  -u                       (ignored)\n\
  -v, --show-nonprinting   use ^ and M- notation, except for LFD and TAB\n\
"), stdout);
      fputs (HELP_OPTION_DESCRIPTION, stdout);
      fputs (VERSION_OPTION_DESCRIPTION, stdout);
      fputs (_("\
\n\
With no FILE, or when FILE is -, read standard input.\n\
"), stdout);
      printf (_("\
\n\
Examples:\n\
  %s f - g  Output f's contents, then standard input, then g's contents.\n\
  %s        Copy standard input to standard output.\n\
"),
	      program_name, program_name);
      printf (_("\nReport bugs to <%s>.\n"), PACKAGE_BUGREPORT);
    }
  exit (status);
}

/* Basic quoting styles.  */
enum quoting_style
  {
    /* Output names as-is (ls --quoting-style=literal).  */
    literal_quoting_style,

    /* Quote names for the shell if they contain shell metacharacters
       or would cause ambiguous output (ls --quoting-style=shell).  */
    shell_quoting_style,

    /* Quote names for the shell, even if they would normally not
       require quoting (ls --quoting-style=shell-always).  */
    shell_always_quoting_style,

    /* Quote names as for a C language string (ls --quoting-style=c).  */
    c_quoting_style,

    /* Like c_quoting_style except omit the surrounding double-quote
       characters (ls --quoting-style=escape).  */
    escape_quoting_style,

    /* Like clocale_quoting_style, but quote `like this' instead of
       "like this" in the default C locale (ls --quoting-style=locale).  */
    locale_quoting_style,

    /* Like c_quoting_style except use quotation marks appropriate for
       the locale (ls --quoting-style=clocale).  */
    clocale_quoting_style
  };

#define INT_BITS (sizeof (int) * CHAR_BIT)

struct quoting_options
{
  /* Basic quoting style.  */
  enum quoting_style style;

  /* Quote the characters indicated by this bit vector even if the
     quoting style would not normally require them to be quoted.  */
  unsigned int quote_these_too[(UCHAR_MAX / INT_BITS) + 1];
};

/* The default quoting options.  */
static struct quoting_options default_quoting_options;

/* MSGID approximates a quotation mark.  Return its translation if it
   has one; otherwise, return either it or "\"", depending on S.  */
static char const *
gettext_quote (char const *msgid, enum quoting_style s)
{
  char const *translation = _(msgid);
  if (translation == msgid && s == clocale_quoting_style)
    translation = "\"";
  return translation;
}

/* Place into buffer BUFFER (of size BUFFERSIZE) a quoted version of
   argument ARG (of size ARGSIZE), using QUOTING_STYLE and the
   non-quoting-style part of O to control quoting.
   Terminate the output with a null character, and return the written
   size of the output, not counting the terminating null.
   If BUFFERSIZE is too small to store the output string, return the
   value that would have been returned had BUFFERSIZE been large enough.
   If ARGSIZE is SIZE_MAX, use the string length of the argument for ARGSIZE.

   This function acts like quotearg_buffer (BUFFER, BUFFERSIZE, ARG,
   ARGSIZE, O), except it uses QUOTING_STYLE instead of the quoting
   style specified by O, and O may not be null.  */

static size_t
quotearg_buffer_restyled (char *buffer, size_t buffersize,
			  char const *arg, size_t argsize,
			  enum quoting_style quoting_style,
			  struct quoting_options const *o)
{
  size_t i;
  size_t len = 0;
  char const *quote_string = 0;
  size_t quote_string_len = 0;
  bool backslash_escapes = false;
  bool unibyte_locale = MB_CUR_MAX == 1;

#define STORE(c) \
    do \
      { \
	if (len < buffersize) \
	  buffer[len] = (c); \
	len++; \
      } \
    while (0)

  switch (quoting_style)
    {
    case c_quoting_style:
      STORE ('"');
      backslash_escapes = true;
      quote_string = "\"";
      quote_string_len = 1;
      break;

    case escape_quoting_style:
      backslash_escapes = true;
      break;

    case locale_quoting_style:
    case clocale_quoting_style:
      {
	/* TRANSLATORS:
	   Get translations for open and closing quotation marks.

	   The message catalog should translate "`" to a left
	   quotation mark suitable for the locale, and similarly for
	   "'".  If the catalog has no translation,
	   locale_quoting_style quotes `like this', and
	   clocale_quoting_style quotes "like this".

	   For example, an American English Unicode locale should
	   translate "`" to U+201C (LEFT DOUBLE QUOTATION MARK), and
	   should translate "'" to U+201D (RIGHT DOUBLE QUOTATION
	   MARK).  A British English Unicode locale should instead
	   translate these to U+2018 (LEFT SINGLE QUOTATION MARK) and
	   U+2019 (RIGHT SINGLE QUOTATION MARK), respectively.

	   If you don't know what to put here, please see
	   <http://en.wikipedia.org/wiki/Quotation_mark#Glyphs>
	   and use glyphs suitable for your language.  */

	char const *left = gettext_quote (N_("`"), quoting_style);
	char const *right = gettext_quote (N_("'"), quoting_style);
	for (quote_string = left; *quote_string; quote_string++)
	  STORE (*quote_string);
	backslash_escapes = true;
	quote_string = right;
	quote_string_len = strlen (quote_string);
      }
      break;

    case shell_always_quoting_style:
      STORE ('\'');
      quote_string = "'";
      quote_string_len = 1;
      break;

    default:
      break;
    }

  for (i = 0;  ! (argsize == SIZE_MAX ? arg[i] == '\0' : i == argsize);  i++)
    {
      unsigned char c;
      unsigned char esc;

      if (backslash_escapes
	  && quote_string_len
	  && i + quote_string_len <= argsize
	  && memcmp (arg + i, quote_string, quote_string_len) == 0)
	STORE ('\\');

      c = arg[i];
      switch (c)
	{
	case '\0':
	  if (backslash_escapes)
	    {
	      STORE ('\\');
	      STORE ('0');
	      STORE ('0');
	      c = '0';
	    }
	  break;

	case '?':
	  switch (quoting_style)
	    {
	    case shell_quoting_style:
	      goto use_shell_always_quoting_style;

	    case c_quoting_style:
	      if (i + 2 < argsize && arg[i + 1] == '?')
		switch (arg[i + 2])
		  {
		  case '!': case '\'':
		  case '(': case ')': case '-': case '/':
		  case '<': case '=': case '>':
		    /* Escape the second '?' in what would otherwise be
		       a trigraph.  */
		    c = arg[i + 2];
		    i += 2;
		    STORE ('?');
		    STORE ('\\');
		    STORE ('?');
		    break;

		  default:
		    break;
		  }
	      break;

	    default:
	      break;
	    }
	  break;

	case '\a': esc = 'a'; goto c_escape;
	case '\b': esc = 'b'; goto c_escape;
	case '\f': esc = 'f'; goto c_escape;
	case '\n': esc = 'n'; goto c_and_shell_escape;
	case '\r': esc = 'r'; goto c_and_shell_escape;
	case '\t': esc = 't'; goto c_and_shell_escape;
	case '\v': esc = 'v'; goto c_escape;
	case '\\': esc = c; goto c_and_shell_escape;

	c_and_shell_escape:
	  if (quoting_style == shell_quoting_style)
	    goto use_shell_always_quoting_style;
	c_escape:
	  if (backslash_escapes)
	    {
	      c = esc;
	      goto store_escape;
	    }
	  break;

	case '{': case '}': /* sometimes special if isolated */
	  if (! (argsize == SIZE_MAX ? arg[1] == '\0' : argsize == 1))
	    break;
	  /* Fall through.  */
	case '#': case '~':
	  if (i != 0)
	    break;
	  /* Fall through.  */
	case ' ':
	case '!': /* special in bash */
	case '"': case '$': case '&':
	case '(': case ')': case '*': case ';':
	case '<':
	case '=': /* sometimes special in 0th or (with "set -k") later args */
	case '>': case '[':
	case '^': /* special in old /bin/sh, e.g. SunOS 4.1.4 */
	case '`': case '|':
	  /* A shell special character.  In theory, '$' and '`' could
	     be the first bytes of multibyte characters, which means
	     we should check them with mbrtowc, but in practice this
	     doesn't happen so it's not worth worrying about.  */
	  if (quoting_style == shell_quoting_style)
	    goto use_shell_always_quoting_style;
	  break;

	case '\'':
	  switch (quoting_style)
	    {
	    case shell_quoting_style:
	      goto use_shell_always_quoting_style;

	    case shell_always_quoting_style:
	      STORE ('\'');
	      STORE ('\\');
	      STORE ('\'');
	      break;

	    default:
	      break;
	    }
	  break;

	case '%': case '+': case ',': case '-': case '.': case '/':
	case '0': case '1': case '2': case '3': case '4': case '5':
	case '6': case '7': case '8': case '9': case ':':
	case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
	case 'G': case 'H': case 'I': case 'J': case 'K': case 'L':
	case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R':
	case 'S': case 'T': case 'U': case 'V': case 'W': case 'X':
	case 'Y': case 'Z': case ']': case '_': case 'a': case 'b':
	case 'c': case 'd': case 'e': case 'f': case 'g': case 'h':
	case 'i': case 'j': case 'k': case 'l': case 'm': case 'n':
	case 'o': case 'p': case 'q': case 'r': case 's': case 't':
	case 'u': case 'v': case 'w': case 'x': case 'y': case 'z':
	  /* These characters don't cause problems, no matter what the
	     quoting style is.  They cannot start multibyte sequences.  */
	  break;

	default:
	  /* If we have a multibyte sequence, copy it until we reach
	     its end, find an error, or come back to the initial shift
	     state.  For C-like styles, if the sequence has
	     unprintable characters, escape the whole sequence, since
	     we can't easily escape single characters within it.  */
	  {
	    /* Length of multibyte sequence found so far.  */
	    size_t m;

	    bool printable;

	    if (unibyte_locale)
	      {
		m = 1;
		printable = isprint (c) != 0;
	      }
	    else
	      {
		mbstate_t mbstate;
		memset (&mbstate, 0, sizeof mbstate);

		m = 0;
		printable = true;
		if (argsize == SIZE_MAX)
		  argsize = strlen (arg);

		do
		  {
		    wchar_t w;
		    size_t bytes = mbrtowc (&w, &arg[i + m],
					    argsize - (i + m), &mbstate);
		    if (bytes == 0)
		      break;
		    else if (bytes == (size_t) -1)
		      {
			printable = false;
			break;
		      }
		    else if (bytes == (size_t) -2)
		      {
			printable = false;
			while (i + m < argsize && arg[i + m])
			  m++;
			break;
		      }
		    else
		      {
			/* Work around a bug with older shells that "see" a '\'
			   that is really the 2nd byte of a multibyte character.
			   In practice the problem is limited to ASCII
			   chars >= '@' that are shell special chars.  */
			if ('[' == 0x5b && quoting_style == shell_quoting_style)
			  {
			    size_t j;
			    for (j = 1; j < bytes; j++)
			      switch (arg[i + m + j])
				{
				case '[': case '\\': case '^':
				case '`': case '|':
				  goto use_shell_always_quoting_style;

				default:
				  break;
				}
			  }

			if (! iswprint (w))
			  printable = false;
			m += bytes;
		      }
		  }
		while (! mbsinit (&mbstate));
	      }

	    if (1 < m || (backslash_escapes && ! printable))
	      {
		/* Output a multibyte sequence, or an escaped
		   unprintable unibyte character.  */
		size_t ilim = i + m;

		for (;;)
		  {
		    if (backslash_escapes && ! printable)
		      {
			STORE ('\\');
			STORE ('0' + (c >> 6));
			STORE ('0' + ((c >> 3) & 7));
			c = '0' + (c & 7);
		      }
		    if (ilim <= i + 1)
		      break;
		    STORE (c);
		    c = arg[++i];
		  }

		goto store_c;
	      }
	  }
	}

      if (! (backslash_escapes
	     && o->quote_these_too[c / INT_BITS] & (1 << (c % INT_BITS))))
	goto store_c;

    store_escape:
      STORE ('\\');

    store_c:
      STORE (c);
    }

  if (i == 0 && quoting_style == shell_quoting_style)
    goto use_shell_always_quoting_style;

  if (quote_string)
    for (; *quote_string; quote_string++)
      STORE (*quote_string);

  if (len < buffersize)
    buffer[len] = '\0';
  return len;

 use_shell_always_quoting_style:
  return quotearg_buffer_restyled (buffer, buffersize, arg, argsize,
				   shell_always_quoting_style, o);
}

/* Place into buffer BUFFER (of size BUFFERSIZE) a quoted version of
   argument ARG (of size ARGSIZE), using O to control quoting.
   If O is null, use the default.
   Terminate the output with a null character, and return the written
   size of the output, not counting the terminating null.
   If BUFFERSIZE is too small to store the output string, return the
   value that would have been returned had BUFFERSIZE been large enough.
   If ARGSIZE is SIZE_MAX, use the string length of the argument for
   ARGSIZE.  */
size_t
quotearg_buffer (char *buffer, size_t buffersize,
		 char const *arg, size_t argsize,
		 struct quoting_options const *o)
{
  struct quoting_options const *p = o ? o : &default_quoting_options;
  int e = errno;
  size_t r = quotearg_buffer_restyled (buffer, buffersize, arg, argsize,
				       p->style, p);
  errno = e;
  return r;
}

/* A storage slot with size and pointer to a value.  */
struct slotvec
{
  size_t size;
  char *val;
};

/* Preallocate a slot 0 buffer, so that the caller can always quote
   one small component of a "memory exhausted" message in slot 0.  */
static char slot0[256];
static unsigned int nslots = 1;
static struct slotvec slotvec0 = {sizeof slot0, slot0};
static struct slotvec *slotvec = &slotvec0;

/* Use storage slot N to return a quoted version of argument ARG.
   ARG is of size ARGSIZE, but if that is SIZE_MAX, ARG is a
   null-terminated string.
   OPTIONS specifies the quoting options.
   The returned value points to static storage that can be
   reused by the next call to this function with the same value of N.
   N must be nonnegative.  N is deliberately declared with type "int"
   to allow for future extensions (using negative values).  */
static char *
quotearg_n_options (int n, char const *arg, size_t argsize,
		    struct quoting_options const *options)
{
  int e = errno;

  unsigned int n0 = n;
  struct slotvec *sv = slotvec;

  if (n < 0)
    abort ();

  if (nslots <= n0)
    {
      /* FIXME: technically, the type of n1 should be `unsigned int',
	 but that evokes an unsuppressible warning from gcc-4.0.1 and
	 older.  If gcc ever provides an option to suppress that warning,
	 revert to the original type, so that the test in xalloc_oversized
	 is once again performed only at compile time.  */
      size_t n1 = n0 + 1;
      bool preallocated = (sv == &slotvec0);

      if (xalloc_oversized (n1, sizeof *sv))
	xalloc_die ();

      slotvec = sv = xrealloc (preallocated ? NULL : sv, n1 * sizeof *sv);
      if (preallocated)
	*sv = slotvec0;
      memset (sv + nslots, 0, (n1 - nslots) * sizeof *sv);
      nslots = n1;
    }

  {
    size_t size = sv[n].size;
    char *val = sv[n].val;
    size_t qsize = quotearg_buffer (val, size, arg, argsize, options);

    if (size <= qsize)
      {
	sv[n].size = size = qsize + 1;
	if (val != slot0)
	  free (val);
	sv[n].val = val = xcharalloc (size);
	quotearg_buffer (val, size, arg, argsize, options);
      }

    errno = e;
    return val;
  }
}

/* Return quoting options for STYLE, with no extra quoting.  */
static struct quoting_options
quoting_options_from_style (enum quoting_style style)
{
  struct quoting_options o;
  o.style = style;
  memset (o.quote_these_too, 0, sizeof o.quote_these_too);
  return o;
}

char *
quotearg_n_style (int n, enum quoting_style s, char const *arg)
{
  struct quoting_options const o = quoting_options_from_style (s);
  return quotearg_n_options (n, arg, SIZE_MAX, &o);
}

/* Return an unambiguous printable representation of NAME,
   allocated in slot N, suitable for diagnostics.  */
char const *
quote_n (int n, char const *name)
{
  return quotearg_n_style (n, locale_quoting_style, name);
}

/* Return an unambiguous printable representation of NAME,
   suitable for diagnostics.  */
char const *
quote (char const *name)
{
  return quote_n (0, name);
}

/* In O (or in the default if O is null),
   set the value of the quoting options for character C to I.
   Return the old value.  Currently, the only values defined for I are
   0 (the default) and 1 (which means to quote the character even if
   it would not otherwise be quoted).  */
int
set_char_quoting (struct quoting_options *o, char c, int i)
{
  unsigned char uc = c;
  unsigned int *p =
    (o ? o : &default_quoting_options)->quote_these_too + uc / INT_BITS;
  int shift = uc % INT_BITS;
  int r = (*p >> shift) & 1;
  *p ^= ((i & 1) ^ r) << shift;
  return r;
}

char *
quotearg_char (char const *arg, char ch)
{
  struct quoting_options options;
  options = default_quoting_options;
  set_char_quoting (&options, ch, 1);
  return quotearg_n_options (0, arg, SIZE_MAX, &options);
}

char *
quotearg_colon (char const *arg)
{
  return quotearg_char (arg, ':');
}

/* Compute the next line number.  */

static void
next_line_num (void)
{
  char *endp = line_num_end;
  do
    {
      if ((*endp)++ < '9')
	return;
      *endp-- = '0';
    }
  while (endp >= line_num_start);
  if (line_num_start > line_buf)
    *--line_num_start = '1';
  else
    *line_buf = '>';
  if (line_num_start < line_num_print)
    line_num_print--;
}

#ifdef EINTR
# define IS_EINTR(x) ((x) == EINTR)
#else
# define IS_EINTR(x) 0
#endif

/* Read(write) up to COUNT bytes at BUF from(to) descriptor FD, retrying if
   interrupted.  Return the actual number of bytes read(written), zero for EOF,
   or SAFE_READ_ERROR(SAFE_WRITE_ERROR) upon error.  */
size_t
safe_rd (int fd, void const *buf, size_t count)
{

	enum { BUGGY_READ_MAXIMUM = INT_MAX & ~8191 };

	for (;;)
    {
      ssize_t result = read (fd, (void*)buf, count);

      if (0 <= result)
        return result;
      else if (IS_EINTR (errno)) {
        if (interrupt) {
          return 0;
        }
        continue;
      }
      else if (errno == EINVAL && BUGGY_READ_MAXIMUM < count)
    	  count = BUGGY_READ_MAXIMUM;
      else
        return result;
    }
}

/* Plain cat.  Copies the file behind `input_desc' to STDOUT_FILENO.
   Return true if successful.  */

static bool
simple_cat (
     /* Pointer to the buffer, used by reads and writes.  */
     char *buf,

     /* Number of characters preferably read or written by each read and write
        call.  */
     size_t bufsize)
{
  /* Actual number of characters read, and therefore written.  */
  size_t n_read;

  /* Loop until the end of the file.  */

  for (;;)
    {
      /* Read a block of input.  */

      n_read = safe_rd (input_desc, buf, bufsize);
      if (n_read == SAFE_READ_ERROR)
	{
	  error (0, errno, "%s", infile);
	  return false;
	}

      /* End of this file?  */

      if (n_read == 0)
	return true;

      /* Write this block out.  */

      {
	/* The following is ok, since we know that 0 < n_read.  */
	size_t n = n_read;
	if (full_write (STDOUT_FILENO, buf, n) != n)
	  error (EXIT_FAILURE, errno, _("write error"));
      }
    }
}

/* Read(write) up to COUNT bytes at BUF from(to) descriptor FD, retrying if
   interrupted.  Return the actual number of bytes read(written), zero for EOF,
   or SAFE_READ_ERROR(SAFE_WRITE_ERROR) upon error.  */
size_t
safe_write (int fd, void const *buf, size_t count)
{
  /* Work around a bug in Tru64 5.1.  Attempting to read more than
     INT_MAX bytes fails with errno == EINVAL.  See
     <http://lists.gnu.org/archive/html/bug-gnu-utils/2002-04/msg00010.html>.
     When decreasing COUNT, keep it block-aligned.  */
  enum { BUGGY_READ_MAXIMUM = INT_MAX & ~8191 };

  for (;;)
    {
      ssize_t result = write (fd, buf, count);

      if (0 <= result)
	return result;
      else if (IS_EINTR (errno))
	continue;
      else if (errno == EINVAL && BUGGY_READ_MAXIMUM < count)
	count = BUGGY_READ_MAXIMUM;
      else
	return result;
    }
}

size_t
safe_read (int fd, void *buf, size_t count)
{
  /* Work around a bug in Tru64 5.1.  Attempting to read more than
     INT_MAX bytes fails with errno == EINVAL.  See
     <http://lists.gnu.org/archive/html/bug-gnu-utils/2002-04/msg00010.html>.
     When decreasing COUNT, keep it block-aligned.  */
  enum { BUGGY_READ_MAXIMUM = INT_MAX & ~8191 };

  for (;;)
    {
      ssize_t result = read (fd, buf, count);

      if (0 <= result)
	return result;
      else if (IS_EINTR (errno))
	continue;
      else if (errno == EINVAL && BUGGY_READ_MAXIMUM < count)
	count = BUGGY_READ_MAXIMUM;
      else
	return result;
    }
}

/* Write(read) COUNT bytes at BUF to(from) descriptor FD, retrying if
   interrupted or if a partial write(read) occurs.  Return the number
   of bytes transferred.
   When writing, set errno if fewer than COUNT bytes are written.
   When reading, if fewer than COUNT bytes are read, you must examine
   errno to distinguish failure from EOF (errno == 0).  */
size_t
full_write (int fd, const void *buf, size_t count)
{
  size_t total = 0;
  const char *ptr = (const char *) buf;

  while (count > 0)
    {
      size_t n_rw = safe_write (fd, ptr, count);
      if (n_rw == (size_t) -1)
	break;
      if (n_rw == 0)
	{
	  errno = ENOSPC;
	  break;
	}
      total += n_rw;
      ptr += n_rw;
      count -= n_rw;
    }

  return total;
}

/* Write any pending output to STDOUT_FILENO.
   Pending is defined to be the *BPOUT - OUTBUF bytes starting at OUTBUF.
   Then set *BPOUT to OUTPUT if it's not already that value.  */

static inline void
write_pending (char *outbuf, char **bpout)
{
  size_t n_write = *bpout - outbuf;
  if (0 < n_write)
    {
      if (full_write (STDOUT_FILENO, outbuf, n_write) != n_write)
        error (EXIT_FAILURE, errno, _("write error"));
      *bpout = outbuf;
    }
}

/* Cat the file behind INPUT_DESC to the file behind OUTPUT_DESC.
   Return true if successful.
   Called if any option more than -u was specified.

   A newline character is always put at the end of the buffer, to make
   an explicit test for buffer end unnecessary.  */

static bool
cat (
     /* Pointer to the beginning of the input buffer.  */
     char *inbuf,

     /* Number of characters read in each read call.  */
     size_t insize,

     /* Pointer to the beginning of the output buffer.  */
     char *outbuf,

     /* Number of characters written by each write call.  */
     size_t outsize,

     /* Variables that have values according to the specified options.  */
     bool show_nonprinting,
     bool show_tabs,
     bool number,
     bool number_nonblank,
     bool show_ends,
     bool squeeze_blank)
{
  /* Last character read from the input buffer.  */
  unsigned char ch;

  /* Pointer to the next character in the input buffer.  */
  char *bpin;

  /* Pointer to the first non-valid byte in the input buffer, i.e. the
     current end of the buffer.  */
  char *eob;

  /* Pointer to the position where the next character shall be written.  */
  char *bpout;

  /* Number of characters read by the last read call.  */
  size_t n_read;

  /* Determines how many consecutive newlines there have been in the
     input.  0 newlines makes NEWLINES -1, 1 newline makes NEWLINES 1,
     etc.  Initially 0 to indicate that we are at the beginning of a
     new line.  The "state" of the procedure is determined by
     NEWLINES.  */
  int newlines = newlines2;

#ifdef FIONREAD
  /* If nonzero, use the FIONREAD ioctl, as an optimization.
     (On Ultrix, it is not supported on NFS file systems.)  */
  bool use_fionread = true;
#endif

  /* The inbuf pointers are initialized so that BPIN > EOB, and thereby input
     is read immediately.  */

  eob = inbuf;
  bpin = eob + 1;

  bpout = outbuf;

  for (;;)
    {
      do
	{
	  /* Write if there are at least OUTSIZE bytes in OUTBUF.  */

	  if (outbuf + outsize <= bpout)
	    {
	      char *wp = outbuf;
	      size_t remaining_bytes;
	      do
		{
		  if (full_write (STDOUT_FILENO, wp, outsize) != outsize)
		    error (EXIT_FAILURE, errno, _("write error"));
		  wp += outsize;
		  remaining_bytes = bpout - wp;
		}
	      while (outsize <= remaining_bytes);

	      /* Move the remaining bytes to the beginning of the
		 buffer.  */

	      memmove (outbuf, wp, remaining_bytes);
	      bpout = outbuf + remaining_bytes;
	    }

	  /* Is INBUF empty?  */

	  if (bpin > eob)
	    {
	      bool input_pending = false;
#ifdef FIONREAD
	      int n_to_read = 0;

	      /* Is there any input to read immediately?
		 If not, we are about to wait,
		 so write all buffered output before waiting.  */

	      if (use_fionread
		  && ioctl (input_desc, FIONREAD, &n_to_read) < 0)
		{
		  /* Ultrix returns EOPNOTSUPP on NFS;
		     HP-UX returns ENOTTY on pipes.
		     SunOS returns EINVAL and
		     More/BSD returns ENODEV on special files
		     like /dev/null.
		     Irix-5 returns ENOSYS on pipes.  */
		  if (errno == EOPNOTSUPP || errno == ENOTTY
		      || errno == EINVAL || errno == ENODEV
		      || errno == ENOSYS)
		    use_fionread = false;
		  else
		    {
		      error (0, errno, _("cannot do ioctl on %s"), quote (infile));
		      newlines2 = newlines;
		      return false;
		    }
		}
	      if (n_to_read != 0)
		input_pending = true;
#endif

	      if (input_pending)
		write_pending (outbuf, &bpout);

	      /* Read more input into INBUF.  */

	      n_read = safe_read (input_desc, inbuf, insize);
	      if (n_read == SAFE_READ_ERROR)
		{
		  error (0, errno, "%s", infile);
		  write_pending (outbuf, &bpout);
		  newlines2 = newlines;
		  return false;
		}
	      if (n_read == 0)
		{
		  write_pending (outbuf, &bpout);
		  newlines2 = newlines;
		  return true;
		}

	      /* Update the pointers and insert a sentinel at the buffer
		 end.  */

	      bpin = inbuf;
	      eob = bpin + n_read;
	      *eob = '\n';
	    }
	  else
	    {
	      /* It was a real (not a sentinel) newline.  */

	      /* Was the last line empty?
		 (i.e. have two or more consecutive newlines been read?)  */

	      if (++newlines > 0)
		{
		  if (newlines >= 2)
		    {
		      /* Limit this to 2 here.  Otherwise, with lots of
			 consecutive newlines, the counter could wrap
			 around at INT_MAX.  */
		      newlines = 2;

		      /* Are multiple adjacent empty lines to be substituted
			 by single ditto (-s), and this was the second empty
			 line?  */
		      if (squeeze_blank)
			{
			  ch = *bpin++;
			  continue;
			}
		    }

		  /* Are line numbers to be written at empty lines (-n)?  */

		  if (number & !number_nonblank)
		    {
		      next_line_num ();
		      bpout = stpcpy (bpout, line_num_print);
		    }
		}

	      /* Output a currency symbol if requested (-e).  */

	      if (show_ends)
		*bpout++ = '$';

	      /* Output the newline.  */

	      *bpout++ = '\n';
	    }
	  ch = *bpin++;
	}
      while (ch == '\n');

      /* Are we at the beginning of a line, and line numbers are requested?  */

      if (newlines >= 0 && number)
	{
	  next_line_num ();
	  bpout = stpcpy (bpout, line_num_print);
	}

      /* Here CH cannot contain a newline character.  */

      /* The loops below continue until a newline character is found,
	 which means that the buffer is empty or that a proper newline
	 has been found.  */

      /* If quoting, i.e. at least one of -v, -e, or -t specified,
	 scan for chars that need conversion.  */
      if (show_nonprinting)
	{
	  for (;;)
	    {
	      if (ch >= 32)
		{
		  if (ch < 127)
		    *bpout++ = ch;
		  else if (ch == 127)
		    {
		      *bpout++ = '^';
		      *bpout++ = '?';
		    }
		  else
		    {
		      *bpout++ = 'M';
		      *bpout++ = '-';
		      if (ch >= 128 + 32)
			{
			  if (ch < 128 + 127)
			    *bpout++ = ch - 128;
			  else
			    {
			      *bpout++ = '^';
			      *bpout++ = '?';
			    }
			}
		      else
			{
			  *bpout++ = '^';
			  *bpout++ = ch - 128 + 64;
			}
		    }
		}
	      else if (ch == '\t' && !show_tabs)
		*bpout++ = '\t';
	      else if (ch == '\n')
		{
		  newlines = -1;
		  break;
		}
	      else
		{
		  *bpout++ = '^';
		  *bpout++ = ch + 64;
		}

	      ch = *bpin++;
	    }
	}
      else
	{
	  /* Not quoting, neither of -v, -e, or -t specified.  */
	  for (;;)
	    {
	      if (ch == '\t' && show_tabs)
		{
		  *bpout++ = '^';
		  *bpout++ = ch + 64;
		}
	      else if (ch != '\n')
		*bpout++ = ch;
	      else
		{
		  newlines = -1;
		  break;
		}

	      ch = *bpin++;
	    }
	}
    }
}

/* These enum values cannot possibly conflict with the option values
   ordinarily used by commands, including CHAR_MAX + 1, etc.  Avoid
   CHAR_MIN - 1, as it may equal -1, the getopt end-of-options value.  */
enum
{
  GETOPT_HELP_CHAR = (CHAR_MIN - 2),
  GETOPT_VERSION_CHAR = (CHAR_MIN - 3)
};

#define GETOPT_HELP_OPTION_DECL \
  "help", no_argument, NULL, GETOPT_HELP_CHAR
#define GETOPT_VERSION_OPTION_DECL \
  "version", no_argument, NULL, GETOPT_VERSION_CHAR

#define case_GETOPT_HELP_CHAR			\
  case GETOPT_HELP_CHAR:			\
    usage (EXIT_SUCCESS);			\
    break;

#define LOCALEDIR "/usr/local/share/locale"

static const char *file_name;

int
close_stream (FILE *stream)
{
  bool some_pending = (__fpending (stream) != 0);
  bool prev_fail = (ferror (stream) != 0);
  bool fclose_fail = (fclose (stream) != 0);

  /* Return an error indication if there was a previous failure or if
     fclose failed, with one exception: ignore an fclose failure if
     there was no previous error, no data remains to be flushed, and
     fclose failed with EBADF.  That can happen when a program like cp
     is invoked like this `cp a b >&-' (i.e., with standard output
     closed) and doesn't generate any output (hence no previous error
     and nothing to be flushed).  */

  if (prev_fail || (fclose_fail && (some_pending || errno != EBADF)))
    {
      if (! fclose_fail)
	errno = 0;
      return EOF;
    }

  return 0;
}


void
close_stdout (void)
{
  if (close_stream (stdout) != 0)
    {
      char const *write_error = _("write error");
      if (file_name)
	error (0, errno, "%s: %s", quotearg_colon (file_name),
	       write_error);
      else
	error (0, errno, "%s", write_error);

      _exit (exit_failure);
    }

   if (close_stream (stderr) != 0)
     _exit (exit_failure);
}

const char version_etc_copyright[] =
  /* Do *not* mark this string for translation.  %s is a copyright
     symbol suitable for this locale, and %d is the copyright
     year.  */
  "Copyright %s %d Free Software Foundation, Inc.";

enum { COPYRIGHT_YEAR = 2007 };

/* Like version_etc, below, but with the NULL-terminated author list
   provided via a variable of type va_list.  */
void
version_etc_va (FILE *stream,
		const char *command_name, const char *package,
		const char *version, va_list authors)
{
  size_t n_authors;

  /* Count the number of authors.  */
  {
    va_list tmp_authors;

    va_copy (tmp_authors, authors);

    n_authors = 0;
    while (va_arg (tmp_authors, const char *) != NULL)
      ++n_authors;
  }

  if (command_name)
    fprintf (stream, "%s (%s) %s\n", command_name, package, version);
  else
    fprintf (stream, "%s %s\n", package, version);

  /* TRANSLATORS: Translate "(C)" to the copyright symbol
     (C-in-a-circle), if this symbol is available in the user's
     locale.  Otherwise, do not translate "(C)"; leave it as-is.  */
  fprintf (stream, version_etc_copyright, _("(C)"), COPYRIGHT_YEAR);

  fputs (_("\
\n\
This is free software.  You may redistribute copies of it under the terms of\n\
the GNU General Public License <http://www.gnu.org/licenses/gpl.html>.\n\
There is NO WARRANTY, to the extent permitted by law.\n\
\n\
"),
	 stream);

  switch (n_authors)
    {
    case 0:
      /* The caller must provide at least one author name.  */
      abort ();
    case 1:
      /* TRANSLATORS: %s denotes an author name.  */
      vfprintf (stream, _("Written by %s.\n"), authors);
      break;
    case 2:
      /* TRANSLATORS: Each %s denotes an author name.  */
      vfprintf (stream, _("Written by %s and %s.\n"), authors);
      break;
    case 3:
      /* TRANSLATORS: Each %s denotes an author name.  */
      vfprintf (stream, _("Written by %s, %s, and %s.\n"), authors);
      break;
    case 4:
      /* TRANSLATORS: Each %s denotes an author name.
	 You can use line breaks, estimating that each author name occupies
	 ca. 16 screen columns and that a screen line has ca. 80 columns.  */
      vfprintf (stream, _("Written by %s, %s, %s,\nand %s.\n"), authors);
      break;
    case 5:
      /* TRANSLATORS: Each %s denotes an author name.
	 You can use line breaks, estimating that each author name occupies
	 ca. 16 screen columns and that a screen line has ca. 80 columns.  */
      vfprintf (stream, _("Written by %s, %s, %s,\n%s, and %s.\n"), authors);
      break;
    case 6:
      /* TRANSLATORS: Each %s denotes an author name.
	 You can use line breaks, estimating that each author name occupies
	 ca. 16 screen columns and that a screen line has ca. 80 columns.  */
      vfprintf (stream, _("Written by %s, %s, %s,\n%s, %s, and %s.\n"),
		authors);
      break;
    case 7:
      /* TRANSLATORS: Each %s denotes an author name.
	 You can use line breaks, estimating that each author name occupies
	 ca. 16 screen columns and that a screen line has ca. 80 columns.  */
      vfprintf (stream, _("Written by %s, %s, %s,\n%s, %s, %s, and %s.\n"),
		authors);
      break;
    case 8:
      /* TRANSLATORS: Each %s denotes an author name.
	 You can use line breaks, estimating that each author name occupies
	 ca. 16 screen columns and that a screen line has ca. 80 columns.  */
      vfprintf (stream, _("\
Written by %s, %s, %s,\n%s, %s, %s, %s,\nand %s.\n"),
		authors);
      break;
    case 9:
      /* TRANSLATORS: Each %s denotes an author name.
	 You can use line breaks, estimating that each author name occupies
	 ca. 16 screen columns and that a screen line has ca. 80 columns.  */
      vfprintf (stream, _("\
Written by %s, %s, %s,\n%s, %s, %s, %s,\n%s, and %s.\n"),
		authors);
      break;
    default:
      /* 10 or more authors.  Use an abbreviation, since the human reader
	 will probably not want to read the entire list anyway.  */
      /* TRANSLATORS: Each %s denotes an author name.
	 You can use line breaks, estimating that each author name occupies
	 ca. 16 screen columns and that a screen line has ca. 80 columns.  */
      vfprintf (stream, _("\
Written by %s, %s, %s,\n%s, %s, %s, %s,\n%s, %s, and others.\n"),
		authors);
      break;
    }
  va_end (authors);
}

void
version_etc (FILE *stream,
	     const char *command_name, const char *package,
	     const char *version, /* const char *author1, ...*/ ...)
{
  va_list authors;

  va_start (authors, version);
  version_etc_va (stream, command_name, package, version, authors);
}

/* Redirection and wildcarding when done by the utility itself.
   Generally a noop, but used in particular for native VMS. */
#ifndef initialize_main
# define initialize_main(ac, av)
#endif

/* The concatenation of the strings `GNU ', and PACKAGE. */
#define GNU_PACKAGE "GNU coreutils"

/* Version number of package */
#define VERSION "6.9"

#define case_GETOPT_VERSION_CHAR(Program_name, Authors)			\
  case GETOPT_VERSION_CHAR:						\
    version_etc (stdout, Program_name, GNU_PACKAGE, VERSION, Authors,	\
                 (char *) NULL);					\
    exit (EXIT_SUCCESS);						\
    break;

# define ST_BLKSIZE(statbuf) ((0 < (statbuf).st_blksize \
			       && (statbuf).st_blksize <= SIZE_MAX / 8 + 1) \
			      ? (statbuf).st_blksize : DEV_BSIZE)

#define STREQ(a, b) (strcmp (a, b) == 0)

static inline void *
ptr_align (void const *ptr, size_t alignment)
{
  char const *p0 = ptr;
  char const *p1 = p0 + alignment - 1;
  return (void *) (p1 - (size_t) p1 % alignment);
}

int
main (int argc, char **argv)
{
  /* Optimal size of i/o operations of output.  */
  size_t outsize;

  /* Optimal size of i/o operations of input.  */
  size_t insize;

  size_t page_size = getpagesize ();

  /* Pointer to the input buffer.  */
  char *inbuf;

  /* Pointer to the output buffer.  */
  char *outbuf;

  bool ok = true;
  int c;

  /* Index in argv to processed argument.  */
  int argind;

  /* Device number of the output (file or whatever).  */
  dev_t out_dev;

  /* I-node number of the output.  */
  ino_t out_ino;

  /* True if the output file should not be the same as any input file.  */
  bool check_redirection = true;

  /* Nonzero if we have ever read standard input.  */
  bool have_read_stdin = false;

  struct stat stat_buf;

  /* Variables that are set according to the specified options.  */
  bool number = false;
  bool number_nonblank = false;
  bool squeeze_blank = false;
  bool show_ends = false;
  bool show_nonprinting = false;
  bool show_tabs = false;
  int file_open_mode = O_RDONLY;

  signal(SIGINT, INThandler);

  static struct option const long_options[] =
  {
    {"number-nonblank", no_argument, NULL, 'b'},
    {"number", no_argument, NULL, 'n'},
    {"squeeze-blank", no_argument, NULL, 's'},
    {"show-nonprinting", no_argument, NULL, 'v'},
    {"show-ends", no_argument, NULL, 'E'},
    {"show-tabs", no_argument, NULL, 'T'},
    {"show-all", no_argument, NULL, 'A'},
    {GETOPT_HELP_OPTION_DECL},
    {GETOPT_VERSION_OPTION_DECL},
    {NULL, 0, NULL, 0}
  };

  initialize_main (&argc, &argv);
  program_name = argv[0];
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  /* Arrange to close stdout if we exit via the
     case_GETOPT_HELP_CHAR or case_GETOPT_VERSION_CHAR code.
     Normally STDOUT_FILENO is used rather than stdout, so
     close_stdout does nothing.  */
  atexit (close_stdout);

  /* Parse command line options.  */

  while ((c = getopt_long (argc, argv, "benstuvAET", long_options, NULL))
	 != -1)
    {
      switch (c)
	{
	case 'b':
	  number = true;
	  number_nonblank = true;
	  break;

	case 'e':
	  show_ends = true;
	  show_nonprinting = true;
	  break;

	case 'n':
	  number = true;
	  break;

	case 's':
	  squeeze_blank = true;
	  break;

	case 't':
	  show_tabs = true;
	  show_nonprinting = true;
	  break;

	case 'u':
	  /* We provide the -u feature unconditionally.  */
	  break;

	case 'v':
	  show_nonprinting = true;
	  break;

	case 'A':
	  show_nonprinting = true;
	  show_ends = true;
	  show_tabs = true;
	  break;

	case 'E':
	  show_ends = true;
	  break;

	case 'T':
	  show_tabs = true;
	  break;

	case_GETOPT_HELP_CHAR;

	case_GETOPT_VERSION_CHAR (PROGRAM_NAME, AUTHORS);

	default:
	  usage (EXIT_FAILURE);
	}
    }

  /* Get device, i-node number, and optimal blocksize of output.  */

  if (fstat (STDOUT_FILENO, &stat_buf) < 0)
    error (EXIT_FAILURE, errno, _("standard output"));

  outsize = ST_BLKSIZE (stat_buf);
  /* Input file can be output file for non-regular files.
     fstat on pipes returns S_IFSOCK on some systems, S_IFIFO
     on others, so the checking should not be done for those types,
     and to allow things like cat < /dev/tty > /dev/tty, checking
     is not done for device files either.  */

  if (S_ISREG (stat_buf.st_mode))
    {
      out_dev = stat_buf.st_dev;
      out_ino = stat_buf.st_ino;
    }
  else
    {
      check_redirection = false;
#ifdef lint  /* Suppress `used before initialized' warning.  */
      out_dev = 0;
      out_ino = 0;
#endif
    }

  if (! (number | show_ends | squeeze_blank))
    {
      file_open_mode |= O_BINARY;
      if (O_BINARY && ! isatty (STDOUT_FILENO))
	freopen (NULL, "wb", stdout);
    }

  /* Check if any of the input files are the same as the output file.  */

  /* Main loop.  */

  infile = "-";
  argind = optind;

  do
    {
      if (argind < argc)
	infile = argv[argind];

      if (STREQ (infile, "-"))
	{
	  have_read_stdin = true;
	  input_desc = STDIN_FILENO;
	  if ((file_open_mode & O_BINARY) && ! isatty (STDIN_FILENO))
	    freopen (NULL, "rb", stdin);
	}
      else
	{
	  input_desc = open (infile, file_open_mode);
	  if (input_desc < 0)
	    {
	      error (0, errno, "%s", infile);
	      ok = false;
	      continue;
	    }
	}

      if (fstat (input_desc, &stat_buf) < 0)
	{
	  error (0, errno, "%s", infile);
	  ok = false;
	  goto contin;
	}
      insize = ST_BLKSIZE (stat_buf);

      /* Compare the device and i-node numbers of this input file with
	 the corresponding values of the (output file associated with)
	 stdout, and skip this input file if they coincide.  Input
	 files cannot be redirected to themselves.  */

      if (check_redirection
	  && stat_buf.st_dev == out_dev && stat_buf.st_ino == out_ino
	  && (input_desc != STDIN_FILENO))
	{
	  error (0, 0, _("%s: input file is output file"), infile);
	  ok = false;
	  goto contin;
	}

      /* Select which version of `cat' to use.  If any format-oriented
	 options were given use `cat'; otherwise use `simple_cat'.  */

      if (! (number | show_ends | show_nonprinting
	     | show_tabs | squeeze_blank))
	{
	  insize = max (insize, outsize);
	  inbuf = xmalloc (insize + page_size - 1);

	  ok &= simple_cat (ptr_align (inbuf, page_size), insize);
	}
      else
	{
	  inbuf = xmalloc (insize + 1 + page_size - 1);

	  /* Why are
	     (OUTSIZE - 1 + INSIZE * 4 + LINE_COUNTER_BUF_LEN + PAGE_SIZE - 1)
	     bytes allocated for the output buffer?

	     A test whether output needs to be written is done when the input
	     buffer empties or when a newline appears in the input.  After
	     output is written, at most (OUTSIZE - 1) bytes will remain in the
	     buffer.  Now INSIZE bytes of input is read.  Each input character
	     may grow by a factor of 4 (by the prepending of M-^).  If all
	     characters do, and no newlines appear in this block of input, we
	     will have at most (OUTSIZE - 1 + INSIZE * 4) bytes in the buffer.
	     If the last character in the preceding block of input was a
	     newline, a line number may be written (according to the given
	     options) as the first thing in the output buffer. (Done after the
	     new input is read, but before processing of the input begins.)
	     A line number requires seldom more than LINE_COUNTER_BUF_LEN
	     positions.

	     Align the output buffer to a page size boundary, for efficency on
	     some paging implementations, so add PAGE_SIZE - 1 bytes to the
	     request to make room for the alignment.  */

	  outbuf = xmalloc (outsize - 1 + insize * 4 + LINE_COUNTER_BUF_LEN
			    + page_size - 1);

	  ok &= cat (ptr_align (inbuf, page_size), insize,
		     ptr_align (outbuf, page_size), outsize, show_nonprinting,
		     show_tabs, number, number_nonblank, show_ends,
		     squeeze_blank);

	  free (outbuf);
	}

      free (inbuf);

    contin:
      if (!STREQ (infile, "-") && close (input_desc) < 0)
	{
	  error (0, errno, "%s", infile);
	  ok = false;
	}
    }
  while (++argind < argc);

  if (have_read_stdin && close (STDIN_FILENO) < 0)
    error (EXIT_FAILURE, errno, _("closing standard input"));

  exit (ok ? EXIT_SUCCESS : EXIT_FAILURE);
}
