/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 *  Copyright 2008 Global Sign In
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

#ifndef _BASE64_H_
#define _BASE64_H_

// Base64 encoding and decoding.
class Base64
{
	public:
		~Base64();

		/**
		  * Encodes data.
		  * Caller frees with delete[].
		  */
		static char *encode(const char *pData, unsigned long &dataLen);

		/**
		  * Decodes data.
		  * Caller frees with delete[].
		  */
		static char *decode(const char *pData, unsigned long &dataLen);

	protected:
		Base64();

	private:
		// Base64 objects cannot be copied.
		Base64(const Base64 &other);
                Base64 &operator=(const Base64 &other);

};

#endif // _BASE64_H_
