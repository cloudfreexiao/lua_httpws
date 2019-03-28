/* mini_utf8.h
 *
 * Gunnar Zötl <gz@tset.de> 2014
 * 
 * a tiny library to deal with utf8 encoded strings. Tries to fault
 * invalid unicode codepoints and invalid utf8 sequences.
 * 
 * Stuff starting with _mini_utf8_* is reserved and private. Don't name your
 * identifiers like that, and don't use stuff named like that.
 * 
 * Needed #includes:
 * -----------------
 * 	-
 * 
 * Functions:
 * ----------
 * 
 * 	int mini_utf8_check_encoding(const char* str)
 * 		test all characters in a string for valid utf8 encoding. Returns
 * 		0 if the string is valid utf8, 1 if it is pure ASCII, or -1, if
 * 		the string is not valid utf8. We do a somewhat relaxed test in
 * 		that all chars in the range [0x01-0x1F] are considered valid.
 * 
 * 	int mini_utf8_decode(const char **str)
 * 		returns the next valid utf8 character from *str, updating *str
 * 		to point behind that char. If *str points to a 0 byte, 0 is
 * 		returned and *str is not updated. If *str does not point to a
 * 		valid utf8 encoded char, -1 is returned and *str is not updated.
 * 
 * 	int mini_utf8_encode(int cp, const char* str, int len)
 * 		encodes the codepoint cp into an utf8 byte sequence and stores
 * 		that into str, where len bytes are available. If that went without
 * 		errors, the length of the encoded sequence is returned. If cp is
 * 		not a valid code point, -1 is returned, for all other problems,
 * 		0 is returned. If cp is 0, it is stored as a single byte 0, even
 * 		if that is not really valid utf8. Also, all chars in the range
 * 		[0x01-0x1F] are considered valid.
 * 
 * 	int mini_utf8_strlen(const char *str)
 * 		returns the number of utf8 codepoints in the string str, or -1 if
 * 		the string contains invalid utf8 sequences.
 * 
 * 	int mini_utf8_byteoffset(const char *str, int cpno)
 * 		returns the number of bytes from the start of the string to the
 * 		start of codepoint number cpno. Returns >=0 for the offset, or
 * 		-1 if the string had less than cpno codepoints, or contained an
 * 		invalid utf8 sequence.
 * 
 * Example:
 * --------
 * 
	#include <stdio.h>
	#include <stdlib.h>
	#include "mini_utf8.h"

	int main(int argc, char **argv)
	{
		int size = 0x11FFFF;
		int l = size * 4 + 1, i = 0, ok = 1, cp = 0;
		int *ibuf = calloc(size, sizeof(int));
		char *cbuf = calloc(l, sizeof(char));
		char *str = cbuf;
		
		while (cp < size) {
			cp = cp + 1;
			int n = mini_utf8_encode(cp, str, l);
			if (n > 0) {
				l -= n;
				str += n;
				ibuf[i++] = cp;
			}
		}
		*str = 0;
		size = i;
		
		str = cbuf;
		for (i = 0; ok && (i < size); ++i) {
			cp = mini_utf8_decode((const char**)&str);
			ok = (cp == ibuf[i]);
		}

		ok = ok && (mini_utf8_strlen(cbuf) == size);

		printf("Roundtrip test %s.\n", ok ? "succeeded" : "failed");

		ok = mini_utf8_check_encoding(cbuf);

		printf("utf8 check %s.\n", ok >= 0 ? "succeeded" : "failed");

		return ok < 0;
	}
 *
 * License:
 * --------
 * 
 * Copyright (c) 2014 Gunnar Zötl <gz@tset.de>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _mini_utf8
#define _mini_utf8

#define _mini_utf8_in_range(c, s, e) ((s) <= (c) && (c) <= (e))

/* The patterns for the encoding check are taken from 
 * http://www.w3.org/International/questions/qa-forms-utf-8
 */
static inline int mini_utf8_check_encoding(const char *str)
{
	const unsigned char *s = (const unsigned char*) str;
	int isu = 1;
	int isa = 1;
	
	while (*s && isu) {
		if (*s <= 0x7F) {
			s += 1;
			continue;	/* [\x09\x0A\x0D\x20-\x7E]			# ASCII (somewhat relaxed) */
		}
		isa = 0;		/* if we get here, the file is not pure ASCII */
		if (_mini_utf8_in_range(*s, 0xC2, 0xDF) && _mini_utf8_in_range(s[1], 0x80, 0xBF)) {
			s += 2;		/* [\xC2-\xDF][\x80-\xBF]			# non-overlong 2-byte */
		} else if (*s == 0xE0 && _mini_utf8_in_range(s[1], 0xA0, 0xBF) && _mini_utf8_in_range(s[2], 0x80, 0xBF)) {
			s += 3;		/* \xE0[\xA0-\xBF][\x80-\xBF]		# excluding overlongs */
		} else if ((*s <= 0xEC || *s == 0xEE || *s == 0xEF) && _mini_utf8_in_range(s[1], 0x80, 0xBF) && _mini_utf8_in_range(s[2], 0x80, 0xBF)) {
			s += 3;		/* [\xE1-\xEC\xEE\xEF][\x80-\xBF]{2}	# straight 3-byte */
		} else if (*s == 0xED && _mini_utf8_in_range(s[1], 0x80, 0x9F) && _mini_utf8_in_range(s[2], 0x80, 0xBF)) {
			s += 3;		/* \xED[\x80-\x9F][\x80-\xBF]		# excluding surrogates */
		} else if (*s == 0xF0 && _mini_utf8_in_range(s[1], 0x90, 0xBF) && _mini_utf8_in_range(s[2], 0x80, 0xBF) && _mini_utf8_in_range(s[3], 0x80, 0xBF)) {
			s += 4;		/* \xF0[\x90-\xBF][\x80-\xBF]{2}	# planes 1-3 */
		} else if (*s <= 0xF3 && _mini_utf8_in_range(s[1], 0x80, 0xBF) && _mini_utf8_in_range(s[2], 0x80, 0xBF) && _mini_utf8_in_range(s[3], 0x80, 0xBF)) {
			s += 4; 	/* [\xF1-\xF3][\x80-\xBF]{3}		# planes 4-15 */
		} else if (*s == 0xF4 &&  _mini_utf8_in_range(s[1], 0x80, 0x8F) && _mini_utf8_in_range(s[2], 0x80, 0xBF) && _mini_utf8_in_range(s[3], 0x80, 0xBF)) {
			s += 4;		/* \xF4[\x80-\x8F][\x80-\xBF]{2}	# plane 16 */
		} else
			isu = 0;
	}
	
	if (isa && isu)
		return 1;
	else if (isu)
		return 0;
	return -1;
}

/* bits start   end     bytes  encoding
 * 7    U+0000	 U+007F   1     0xxxxxxx
 * 11   U+0080	 U+07FF   2     110xxxxx 10xxxxxx
 * 16   U+0800	 U+FFFF   3     1110xxxx 10xxxxxx 10xxxxxx
 * 21   U+10000  U+1FFFFF 4     11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
 * 
 * validity checking derived from above patterns
*/
static inline int mini_utf8_decode(const char **str)
{
	const unsigned char *s = (const unsigned char*) *str;
	int ret = -1;
	if (!*s) return 0;

	if (*s <= 0x7F) {
		ret = s[0];		/* ASCII */
		*str = (char*) s+1;
		return ret;
	} else if (*s < 0xC2) {
		return -1;
	} else if (*s<= 0xDF) {
		if ((s[1] & 0xC0) != 0x80) return -1;
		ret = ((s[0] & 0x1F) << 6) | (s[1] & 0x3F);
		*str = (char*) s+2;
		return ret;
	} else if (*s <= 0xEF) {
		if ((s[1] & 0xC0) != 0x80) return -1;
		if (*s == 0xE0 && s[1] < 0xA0) return -1;
		if (*s == 0xED && s[1] > 0x9F) return -1;
		if ((s[2] & 0xC0) != 0x80) return -1;
		ret = ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
		*str = (char*) s+3;
		return ret;
	} else if (*s <= 0xF4) {
		if ((s[1] & 0xC0) != 0x80) return -1;
		if (*s == 0xF0 && s[1] < 0x90) return -1;
		if (*s == 0xF4 && s[1] > 0x8F) return -1;
		if ((s[2] & 0xC0) != 0x80) return -1;
		if ((s[3] & 0xC0) != 0x80) return -1;
		ret = ((s[0] & 0x0F) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
		*str = (char*) s+4;
		return ret;
	}
	
	return ret;
}

/* only utf16 surrogate pairs (0xD800-0xDFFF) are invalid unicode
 * codepoints
 */
static inline int mini_utf8_encode(int cp, char *str, int len)
{
	unsigned char *s = (unsigned char*) str;
	if (cp <= 0x7F) {
		if (len < 1) return 0;
		*s = (cp & 0x7F);
		return 1;
	} else if (cp <= 0x7FF) {
		if (len < 2) return 0;
		*s++ = (cp >> 6) | 0xC0;
		*s = (cp & 0x3F) | 0x80;
		return 2;
	} else if (cp <= 0xFFFF) {
		if (0xD800 <= cp && cp <= 0xDFFF) return -1;
		if (len < 3) return 0;
		*s++ = (cp >> 12) | 0xE0;
		*s++ = ((cp >> 6) & 0x3F) | 0x80;
		*s = (cp & 0x3F) | 0x80;
		return 3;
	} else if (cp <= 0x10FFFF) {
		if (len < 4) return 0;
		*s++ =(cp >> 18) | 0xF0;
		*s++ =((cp >> 12) & 0x3F) | 0x80;
		*s++ =((cp >> 6) & 0x3F) | 0x80;
		*s =(cp & 0x3F) | 0x80;
		return 4;
	}
	return -1;
}

static inline int mini_utf8_strlen(const char *str)
{
	const char *s = str;
	int len = 0;
	int ok = mini_utf8_decode(&s);
	while (ok > 0) {
		++len;
		ok = mini_utf8_decode(&s);
	}
	if (ok == 0)
		return len;
	return -1;
}

static inline int mini_utf8_byteoffset(const char *str, int cpno)
{
	const char *s = str;
	int cnt = 0;
	int ok = 1;
	for (cnt = 0; (cnt < cpno) && (ok > 0); ++cnt) {
		ok = mini_utf8_decode(&s);
	}
	if (ok > 0)
		return (int)(s - str);
	return -1;
}

#endif /* _mini_utf8 */
