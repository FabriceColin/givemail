/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 *  Copyright 2008 Global Sign In
 * 
 *  Based on the file base64.c, part of the Sofia-SIP package.
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

#include <openssl/hmac.h>

#include "Base64.h"
#include "HMAC.h"

HMAC::HMAC()
{
}

HMAC::~HMAC()
{
}

char *HMAC::sign(Key *pKey, const char *pData, off_t &dataLen)
{
	if ((pKey == NULL) ||
		(pData == NULL) ||
		(dataLen == 0))
	{
		return NULL;
	}

	unsigned char *pSignature = new unsigned char[EVP_MAX_MD_SIZE];
	unsigned int signatureLength = EVP_MAX_MD_SIZE;

	::HMAC(EVP_sha1(),
		(const void *)pKey->m_value.c_str(), (int)pKey->m_value.length(),
		(const unsigned char *)pData, (int)dataLen,
		(unsigned char *)pSignature, &signatureLength);
	if (signatureLength > 0)
	{
		unsigned long encodedLen = (unsigned long)signatureLength;

		char *pEncodedSignature = Base64::encode((const char *)pSignature, encodedLen);

		delete[] pSignature;

		dataLen = (off_t)encodedLen;

		return pEncodedSignature;
	}

	delete[] pSignature;

	return NULL;
}

