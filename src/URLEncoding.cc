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

#ifdef USE_CURL
#include <curl/curl.h>
#else
#ifdef USE_NEON
#include <neon/ne_uri.h>
#endif
#endif

#include "URLEncoding.h"

using std::string;

bool URLEncoding::m_initialized = false;

URLEncoding::URLEncoding()
{
}

URLEncoding::~URLEncoding()
{
}

string URLEncoding::decode(const char *pData, unsigned int dataLen)
{
	string unescapedData;

	if ((pData == NULL) ||
		(dataLen == 0))
	{
		return "";
	}

	if (m_initialized == false)
	{
		m_initialized = true;

#ifdef USE_CURL
		// Initialize
		curl_global_init(CURL_GLOBAL_ALL);
#endif
	}

#ifdef USE_CURL
	CURL *pCurlHandler = curl_easy_init();
	if (pCurlHandler != NULL)
	{
		int unescapedLength = 0;

		char *pUnescaped = curl_easy_unescape(pCurlHandler, pData, dataLen, &unescapedLength);
		if (pUnescaped != NULL)
		{
			unescapedData = string(pUnescaped, unescapedLength);
			dataLen = (unsigned int)unescapedLength;

			// Free now
			curl_free((void*)pUnescaped);
		}

		curl_easy_cleanup(pCurlHandler);
	}
#else
#ifdef USE_NEON
	char *pUnescaped = ne_path_unescape(pData);
	if (pUnescaped != NULL)
	{
		dataLen = (unsigned int)strlen(pUnescaped);
		unescapedData = string(pUnescaped, dataLen);

		// Free now
		free((void*)pUnescaped);
	}
#endif
#endif

	if (m_initialized == true)
	{
#ifdef USE_CURL
		// Shutdown
		curl_global_cleanup();
#endif

		m_initialized = false;
	}

	return unescapedData;
}

