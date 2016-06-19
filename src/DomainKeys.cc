/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 *  Copyright 2008 Global Sign In
 *  Copyright 2009-2014 Fabrice Colin
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
#include <unistd.h>
#include <stdlib.h>
#include <openssl/err.h>
#include <fstream>
#include <iostream>
#include <sstream>

#include "DomainKeys.h"
#include "MessageDetails.h"

//#define _DEBUG_FEEDING
//#define _MUST_ADD_HEADERS_LIST

using std::clog;
using std::endl;
using std::string;
using std::stringstream;
using std::ofstream;

DK_LIB *DomainKeys::m_pLib = NULL;

DomainKeys::DomainKeys() :
	DomainAuth(),
	m_pDkObj(NULL)
{
}

DomainKeys::~DomainKeys()
{
}

bool DomainKeys::loadPrivateKey(const string &domainName,
	const string &privateKeyFileName)
{
	if (m_pPrivateKey != NULL)
	{
		delete[] m_pPrivateKey;
		m_pPrivateKey = NULL;
	}

	if ((domainName.empty() == true) ||
		(privateKeyFileName.empty() == true))
	{
		return false;
	}

	// Load the private key
	off_t keySize = 0;
	m_pPrivateKey = MessageDetails::loadRaw(privateKeyFileName, keySize);
	if (m_pPrivateKey == NULL)
	{
		return false;
	}

	m_domainName = domainName;

	return true;
}

bool DomainKeys::initialize(void)
{
	DK_STAT stat;

	m_pLib = dk_init(&stat);
	if (stat != DK_STAT_OK)
	{
		return false;
	}

	if (m_pLib == NULL)
	{
		return false;
	}

	return true;
}

void DomainKeys::shutdown(void)
{
	dk_shutdown(m_pLib);
}

void DomainKeys::cleanupThread(void)
{
	// Because DomainKeys uses OpenSSL, each thread should call this at exit time
	ERR_remove_state(0);
}

bool DomainKeys::canSign(void) const
{
	if ((m_pLib == NULL) ||
		(m_pPrivateKey == NULL) ||
		(m_domainName.empty() == true))
	{
		return false;
	}

	return true;
}

bool DomainKeys::sign(const string &messageData,
	const string &headersList, bool simpleCanon,
	string &headerName, string &headerValue)
{
	DK_STAT stat;
	DK_FLAGS flags = (DK_FLAGS)0;

	if ((canSign() == false) ||
		(messageData.empty() == true))
	{
		return false;
	}

	if (simpleCanon == true)
	{
		m_pDkObj = dk_sign(m_pLib, &stat, DK_CANON_SIMPLE);
	}
	else
	{
		m_pDkObj = dk_sign(m_pLib, &stat, DK_CANON_NOFWS);
	}
	if (stat != DK_STAT_OK)
	{
		clog << "DomainKeys: " << DK_STAT_to_string(stat) << endl;
		return false;
	}

	stat = dk_setopts(m_pDkObj, DKOPT_RDUPE|DKOPT_TRACE_h|DKOPT_TRACE_H|DKOPT_TRACE_b|DKOPT_TRACE_B);
	if (stat != DK_STAT_OK)
	{
		clog << "DomainKeys: " << DK_STAT_to_string(stat) << endl;
		return false;
	}

#ifdef _DEBUG_FEEDING
	ofstream dataFile("data.txt");
#endif
	// Feed the message data, line by line
	string::size_type startPos = 0, pos = messageData.find_first_of("\r\n");
	string::size_type messageDataLen = messageData.length();
	while (pos != string::npos)
	{
		if (pos != startPos)
		{
			dk_message(m_pDkObj, (const unsigned char*)(messageData.c_str() + startPos), pos - startPos);
#ifdef _DEBUG_FEEDING
			dataFile << string(messageData.c_str() + startPos, pos - startPos);
#endif
		}

		// Next
		if (messageData[pos] == '\r')
		{
			startPos = pos + 2;
		}
		else
		{
			startPos = pos + 1;
		}
		if (startPos >= messageDataLen)
		{
			break;
		}

		dk_message(m_pDkObj, (const unsigned char*)"\r\n", 2);
#ifdef _DEBUG_FEEDING
		dataFile << "\r\n";
#endif
		pos = messageData.find_first_of("\r\n", startPos);
	}
	if (startPos < messageData.length())
	{
		dk_message(m_pDkObj, (const unsigned char*)(messageData.c_str() + startPos), messageDataLen - startPos);
#ifdef _DEBUG_FEEDING
		dataFile << string(messageData.c_str() + startPos);
#endif
	}
	// End Of Message
	dk_message(m_pDkObj, (const unsigned char*)"\r\n", 2);
#ifdef _DEBUG_FEEDING
	dataFile << "\r\n";
	dataFile.close();
#endif
	dk_end(m_pDkObj, &flags);

	unsigned char signature[2048];
	stat = dk_getsig(m_pDkObj, m_pPrivateKey, signature, 2048);
	if (stat != DK_STAT_OK)
	{
		clog << "DomainKeys: " << DK_STAT_to_string(stat) << endl;
		return false;
	}

	stringstream headerStr;

	headerStr << "a=rsa-sha1; q=dns; c=";
	if (simpleCanon == true)
	{
		headerStr << "simple";
	}
	else
	{
		headerStr << "nofws";
	}
	headerStr << "; s=default; d=" << m_domainName << ";\r\n";
#ifdef _MUST_ADD_HEADERS_LIST
	if (headersList.empty() == false)
	{
		headerStr << "  h=" << headersList << ";\r\n";
	}
#endif
	headerStr << "  b=" << signature << ";\r\n";

	headerName = "DomainKey-Signature";
	headerValue = headerStr.str();

	return true;
}

bool DomainKeys::verify(const string &messageData)
{
	DK_STAT stat;
	DK_FLAGS flags = (DK_FLAGS)0;

	if ((canSign() == false) ||
		(messageData.empty() == true))
	{
		return false;
	}

	m_pDkObj = dk_verify(m_pLib, &stat);
	if (stat != DK_STAT_OK)
	{
		clog << "DomainKeys: " << DK_STAT_to_string(stat) << endl;
		return false;
	}

	stat = dk_setopts(m_pDkObj, DKOPT_TRACE_h|DKOPT_TRACE_H|DKOPT_TRACE_b|DKOPT_TRACE_B);
	if (stat != DK_STAT_OK)
	{
		clog << "DomainKeys: " << DK_STAT_to_string(stat) << endl;
		return false;
	}

	// Feed the message data
	dk_message(m_pDkObj, (const unsigned char*)messageData.c_str(), messageData.length());
	dk_end(m_pDkObj, &flags);

	if (stat != DK_STAT_OK)
	{
		clog << "DomainKeys: " << DK_STAT_to_string(stat) << endl;
		return false;
	}

	return true;
}

