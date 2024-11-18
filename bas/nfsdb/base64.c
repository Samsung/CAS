/* gbase64.c - Base64 encoding/decoding
 *
 *  Copyright (C) 2006 Alexander Larsson <alexl@redhat.com>
 *  Copyright (C) 2000-2003 Ximian Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * This is based on code in camel, written by:
 *    Michael Zucchi <notzed@ximian.com>
 *    Jeffrey Stedfast <fejj@ximian.com>
 */

#include <stdlib.h>
#include <string.h>

static const char base64_alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

size_t base64_encode_step (const unsigned char* in, size_t len, int break_lines, char* out, int* state, int* save) {

  char *outptr;
  const unsigned char *inptr;

  if (len == 0)
    return 0;

  inptr = in;
  outptr = out;

  if (len + ((char *) save) [0] > 2)
    {
      const unsigned char *inend = in+len-2;
      int c1, c2, c3;
      int already;

      already = *state;

      switch (((char *) save) [0])
        {
        case 1:
          c1 = ((unsigned char *) save) [1];
          goto skip1;
        case 2:
          c1 = ((unsigned char *) save) [1];
          c2 = ((unsigned char *) save) [2];
          goto skip2;
        }

      /*
       * yes, we jump into the loop, no i'm not going to change it,
       * it's beautiful!
       */
      while (inptr < inend)
        {
          c1 = *inptr++;
        skip1:
          c2 = *inptr++;
        skip2:
          c3 = *inptr++;
          *outptr++ = base64_alphabet [ c1 >> 2 ];
          *outptr++ = base64_alphabet [ c2 >> 4 |
                                        ((c1&0x3) << 4) ];
          *outptr++ = base64_alphabet [ ((c2 &0x0f) << 2) |
                                        (c3 >> 6) ];
          *outptr++ = base64_alphabet [ c3 & 0x3f ];
          /* this is a bit ugly ... */
          if (break_lines && (++already) >= 19)
            {
              *outptr++ = '\n';
              already = 0;
            }
        }

      ((char *)save)[0] = 0;
      len = 2 - (inptr - inend);
      *state = already;
    }

    {
      char *saveout;

      /* points to the slot for the next char to save */
      saveout = & (((char *)save)[1]) + ((char *)save)[0];

      /* len can only be 0 1 or 2 */
      switch(len)
        {
        case 2:
          *saveout++ = *inptr++;
        case 1:
          *saveout++ = *inptr++;
        }
      ((char *)save)[0] += len;
    }

  return outptr - out;
}

static size_t base64_encode_close (int  break_lines, char* out, int* state, int* save) {

  int c1, c2;
  char *outptr = out;

  c1 = ((unsigned char *) save) [1];
  c2 = ((unsigned char *) save) [2];

  switch (((char *) save) [0])
    {
    case 2:
      outptr [2] = base64_alphabet[ ( (c2 &0x0f) << 2 ) ];
      goto skip;
    case 1:
      outptr[2] = '=';
      c2 = 0;  /* saved state here is not relevant */
    skip:
      outptr [0] = base64_alphabet [ c1 >> 2 ];
      outptr [1] = base64_alphabet [ c2 >> 4 | ( (c1&0x3) << 4 )];
      outptr [3] = '=';
      outptr += 4;
      break;
    }
  if (break_lines)
    *outptr++ = '\n';

  *save = 0;
  *state = 0;

  return outptr - out;
}

char * base64_encode (const unsigned char* data, size_t len) {

  char *out;
  int state = 0, outlen;
  int save = 0;

  out = calloc (1,(len / 3 + 1) * 4 + 1);

  outlen = base64_encode_step (data, len, 0, out, &state, &save);
  outlen += base64_encode_close (0, out + outlen, &state, &save);
  out[outlen] = '\0';

  return (char *) out;
}

static const unsigned char mime_base64_rank[256] = {
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255, 62,255,255,255, 63,
   52, 53, 54, 55, 56, 57, 58, 59, 60, 61,255,255,255,  0,255,255,
  255,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
   15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,255,255,255,255,255,
  255, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
   41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
};

static size_t base64_decode_step (const char* in, size_t len, unsigned char* out, int *state, unsigned int* save) {

  const unsigned char *inptr;
  unsigned char *outptr;
  const unsigned char *inend;
  unsigned char c, rank;
  unsigned char last[2];
  unsigned int v;
  int i;

  if (len == 0)
    return 0;

  inend = (const unsigned char *)in+len;
  outptr = out;

  /* convert 4 base64 bytes to 3 normal bytes */
  v=*save;
  i=*state;

  last[0] = last[1] = 0;

  /* we use the sign in the state to determine if we got a padding character
     in the previous sequence */
  if (i < 0)
    {
      i = -i;
      last[0] = '=';
    }

  inptr = (const unsigned char *)in;
  while (inptr < inend)
    {
      c = *inptr++;
      rank = mime_base64_rank [c];
      if (rank != 0xff)
        {
          last[1] = last[0];
          last[0] = c;
          v = (v<<6) | rank;
          i++;
          if (i==4)
            {
              *outptr++ = v>>16;
              if (last[1] != '=')
                *outptr++ = v>>8;
              if (last[0] != '=')
                *outptr++ = v;
              i=0;
            }
        }
    }

  *save = v;
  *state = last[0] == '=' ? -i : i;

  return outptr - out;
}

unsigned char* base64_decode (const char *text, size_t* out_len) {
  unsigned char *ret;
  size_t input_length;
  int state = 0;
  unsigned int save = 0;

  input_length = strlen (text);

  ret = calloc (1,(input_length / 4) * 3 + 1);

  *out_len = base64_decode_step (text, input_length, ret, &state, &save);

  return ret;
}
