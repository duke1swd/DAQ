#include <p18cxxx.h>
#include "system\typedefs.h"
#include "string.h"
#include "cli.h"

#define _USE_NTFLAG	0
#define	_USE_SJIS	0

/*
 * This code is taken from tff version 0.6
 */

/*-----------------------------------------------------------------------*/
/*                                                                       */
/* Pick a paragraph and create the name in format of directory entry     */
/*                                                                       */
/* This routine does a lot of validation and conversion of the           */
/* path name component.  I do not understand all of it by any means.     */
/*                                                                       */
/* Arg 1 is a pointer to the pointer to the path.                        */
/*       The first component of the path name is converted into          */
/*       canonical format and stored in the name buffer.                 */
/* Arg 2 is a pointer to the directory name buffer                       */
/*       The name buffer is 12 bytes of canonicalized name.              */
/*                                                                       */
/* Returns 1 on error -- invalid format.                                 */
/* Returns 0 if we just converted the last component of the path.        */
/* Returns '/' if there is another component of the path.  The path      */
/*  pointer will have been updated to point to the next path component   */
/*                                                                       */
/*-----------------------------------------------------------------------*/

unsigned char
make_dirfile(const char **path, char *dirname)
{
	byte n, t, c, a, b;


	/* Fill buffer with spaces */
	memset(dirname, ' ', 8+3);

	a = 0; b = 0x18;	/* NT flag */
	n = 0; t = 8;

	for (;;) {
		c = *(*path)++;
		if (c == '\0' || c == '/') {	
			/* Reached to end of str or directory separator */
			if (n == 0)
				return 1;
			dirname[11] = _USE_NTFLAG ? (a & b) : 0;
			return c;
		}
		if (c <= ' ' || c == 0x7F)
			return 2;		/* Reject invisible chars */
		if (c == '.') {
			/* Enter extension part */
			if (!(a & 1) && n >= 1 && n <= 8) {
				n = 8;
				t = 11;
				continue;
			}
			return 3;
		}

		/* Accept S-JIS code */
		if (_USE_SJIS &&
			((c >= 0x81 && c <= 0x9F) ||	
		    (c >= 0xE0 && c <= 0xFC))) {

			/* Change heading \xE5 to \x05 */
			if (n == 0 && c == 0xE5)	
				c = 0x05;
			a ^= 1; goto md_l2;
		}

		/* Reject " */
		if (c == '"')
			return 4;			

		/* Accept ! # $ % & ' ( ) */
		if (c <= ')')
			goto md_l1;

		/* Reject * + , */
		if (c <= ',')
			return 5;		

		/* Accept - 0-9 */
		if (c <= '9')
			goto md_l1;		

		/* Reject : ; < = > ? */
		if (c <= '?')
			return 6;

		/* These checks are not applied to S-JIS 2nd byte */
		if (!(a & 1)) {
			/* Reject | */
			if (c == '|')
				return 7;	

			/* Reject [ \ ] */
			if (c >= '[' && c <= ']')
				return 8;

			if (_USE_NTFLAG && c >= 'A' && c <= 'Z')
				(t == 8) ? (b &= 0xF7) : (b &= 0xEF);

			/* Convert to upper case */
			if (c >= 'a' && c <= 'z') {	
				c -= 0x20;
				if (_USE_NTFLAG) (t == 8) ? (a |= 0x08) : (a |= 0x10);
			}
		}
	md_l1:
		a &= 0xFE;
	md_l2:
		if (n >= t)
			return 9;
		dirname[n++] = c;
	}
	return 10;
}
