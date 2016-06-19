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

#ifndef _URLENCODING_H_
#define _URLENCODING_H_

#include <string>

// URL encoding.
class URLEncoding
{
	public:
		~URLEncoding();

		/// Decodes data.
		static std::string decode(const char *pData, unsigned int dataLen);

	protected:
		static bool m_initialized;

		URLEncoding();

	private:
		// URLEncoding objects cannot be copied.
		URLEncoding(const URLEncoding &other);
                URLEncoding &operator=(const URLEncoding &other);

};

#endif // _URLENCODING_H_
