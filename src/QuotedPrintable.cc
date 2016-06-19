/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 *  Copyright (C) 1999, 2000, 2001, 2004, 2005,
 *   2007 Free Software Foundation, Inc.
 *  Copyright 2008 Global Sign In
 *  Copyright 2010 Fabrice Colin 
 *
 *  Based on the file filter_trans.c, part of the GNU Mailutils package.
 *
 *  This code is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdio.h>
#include <string.h>
#include <iostream>

#include "QuotedPrintable.h"

using std::clog;
using std::clog;
using std::endl;

static const char _hexdigits[] = "0123456789ABCDEF";

#define QP_LINE_MAX	76
#define ISWS(c) ((c)==' ' || (c)=='\t')

static int
qp_decode (const char *iptr, size_t isize, char *optr, size_t osize,
	   size_t *nbytes, int *line_len)
{
  char c;
  int last_char = 0;
  size_t consumed = 0;
  size_t wscount = 0;
  
  *nbytes = 0;
  while (consumed < isize && *nbytes < osize)
    {
      c = *iptr++;

      if (ISWS (c))
	{
	  wscount++;
	  consumed++;
	}
      else
	{
	  /* Octets with values of 9 and 32 MAY be
	     represented as US-ASCII TAB (HT) and SPACE characters,
	     respectively, but MUST NOT be so represented at the end
	     of an encoded line.  Any TAB (HT) or SPACE characters
	     on an encoded line MUST thus be followed on that line
	     by a printable character. */
	  
	  if (wscount)
	    {
	      if (c != '\r' && c != '\n')
		{
		  size_t sz;
		  
		  if (consumed >= isize)
		    break;

		  if (*nbytes + wscount > osize)
		    sz = osize - *nbytes;
		  else
		    sz = wscount;
		  memcpy (optr, iptr - wscount - 1, sz);
		  optr += sz;
		  (*nbytes) += sz;
		  if (wscount > sz)
		    {
		      wscount -= sz;
		      break;
		    }
		}
	      wscount = 0;
	      if (*nbytes == osize)
		break;
	    }
		
	  if (c == '=')
	    {
	      /* There must be 2 more characters before I consume this.  */
	      if (consumed + 2 >= isize)
		break;
	      else
		{
		  /* you get =XX where XX are hex characters.  */
		  char 	chr[3];
		  int 	new_c;

		  chr[2] = 0;
		  chr[0] = *iptr++;
		  /* Ignore LF.  */
		  if (chr[0] != '\n')
		    {
		      chr[1] = *iptr++;
		      new_c = strtoul (chr, NULL, 16);
		      *optr++ = new_c;
		      (*nbytes)++;
		      consumed += 3;
		    }
		  else
		    consumed += 2;
		}
	    }
	  /* CR character.  */
	  else if (c == '\r')
	    {
	      /* There must be at least 1 more character before
		 I consume this.  */
	      if (consumed + 1 >= isize)
		break;
	      else
		{
		  iptr++; /* Skip the CR character.  */
		  *optr++ = '\n';
		  (*nbytes)++;
		  consumed += 2;
		}
	    }
	  else
	    {
	      *optr++ = c;
	      (*nbytes)++;
	      consumed++;
	    }
	}	  
      last_char = c;
    }
  return consumed - wscount;
}

static int
qp_encode (const char *iptr, size_t isize, char *optr, size_t osize,
	   size_t *nbytes, int *line_len)
{
  unsigned int c;
  size_t consumed = 0;

  *nbytes = 0;

  /* Strategy: check if we have enough room in the output buffer only
     once the required size has been computed. If there is not enough,
     return and hope that the caller will free up the output buffer a
     bit. */

  while (consumed < isize)
    {
      int simple_char;
      
      /* candidate byte to convert */
      c = *(unsigned char*) iptr;
      simple_char = (c >= 32 && c <= 60)
     	             || (c >= 62 && c <= 126)
	             || c == '\t'
	             || c == '\n';

      if (*line_len == QP_LINE_MAX
	  || (c == '\n' && consumed && ISWS (optr[-1]))
	  || (!simple_char && *line_len >= (QP_LINE_MAX - 3)))
	{
	  /* to cut a qp line requires two bytes */
	  if (*nbytes + 2 > osize) 
	    break;

	  *optr++ = '=';
	  *optr++ = '\n';
	  (*nbytes) += 2;
	  *line_len = 0;
	}
	  
      if (simple_char)
	{
	  /* a non-quoted character uses up one byte */
	  if (*nbytes + 1 > osize) 
	    break;

	  *optr++ = c;
	  (*nbytes)++;
	  (*line_len)++;

	  iptr++;
	  consumed++;

	  if (c == '\n')
	    *line_len = 0;
	}
      else
	{
	  /* a quoted character uses up three bytes */
	  if ((*nbytes) + 3 > osize) 
	    break;

	  *optr++ = '=';
	  *optr++ = _hexdigits[(c >> 4) & 0xf];
	  *optr++ = _hexdigits[c & 0xf];

	  (*nbytes) += 3;
	  (*line_len) += 3;

	  /* we've actuall used up one byte of input */
	  iptr++;
	  consumed++;
	}
    }
  return consumed;
}

QuotedPrintable::QuotedPrintable()
{
}

QuotedPrintable::~QuotedPrintable()
{
}

char *QuotedPrintable::encode(const char *pData, size_t &dataLen)
{
	size_t outputLen = (dataLen * 3) + 1;

	if ((pData == NULL) ||
		(dataLen == 0))
	{
		return NULL;
	}

	size_t nbytes = 0;
	int lineLen = 0;
	char *pEncodedData = new char[outputLen];
	int consumed = qp_encode(pData, (size_t)dataLen, pEncodedData, outputLen, &nbytes, &lineLen);
	if (dataLen > (size_t)consumed)
	{
		// Not everything was consumed, probably because the output buffer is too small
		clog << "QuotedPrintable consumed only " << consumed << " out of " << dataLen << endl;

		delete[] pEncodedData;
		dataLen = 0;

		return NULL;
	}

#ifdef DEBUG
	clog << "QuotedPrintable::encode: " << nbytes << "/" << outputLen << " bytes" << endl;
#endif
	if (nbytes < outputLen)
	{
		pEncodedData[nbytes] = '\0';
	}
	else
	{
		clog << "QuotedPrintable couldn't terminate encoded data" << endl;

		delete[] pEncodedData;
		dataLen = 0;

		return NULL;
	}

	dataLen = nbytes;

	return pEncodedData;
}

