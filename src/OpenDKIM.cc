/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 *  Copyright 2020 Fabrice Colin
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

#include "OpenDKIM.h"
#include "MessageDetails.h"

//#define _DEBUG_FEEDING
#define DK_HEADER_NAME "DKIM-Signature"
#define JOBID "givemail-OpenDKIM"

using std::clog;
using std::endl;
using std::string;
using std::vector;
using std::stringstream;
using std::ofstream;

static DKIM_STAT keyLookup(DKIM *dkim, DKIM_SIGINFO *sig, unsigned char *buf, size_t buflen)
{
	ConfigurationFile *pConfig = ConfigurationFile::getInstance("");

	if ((buf != NULL) &&
		(pConfig != NULL) &&
		(pConfig->m_dkPublicKey.empty() == false))
	{
		off_t keySize = 0;

		memset(buf, '\0', buflen);

		char *pPublicKey = MessageDetails::loadRaw(pConfig->m_dkPublicKey, keySize);
		if (pPublicKey != NULL)
		{
#ifdef DEBUG
			clog << "OpenDKIM: serving public key at " << pConfig->m_dkPublicKey << endl;
#endif

			strncpy(reinterpret_cast<char*>(buf), pPublicKey, buflen);
		}
	}

	return DKIM_STAT_OK;
}

pthread_mutex_t OpenDKIM::m_mutex = PTHREAD_MUTEX_INITIALIZER;
DKIM_LIB *OpenDKIM::m_pLib = NULL;

OpenDKIM::OpenDKIM() :
	DomainAuth(),
	m_pObj(NULL)
{
}

OpenDKIM::~OpenDKIM()
{
}

bool OpenDKIM::feedHeader(const string &header)
{
	if (m_pObj == NULL)
	{
		return false;
	}

	DKIM_STAT status = dkim_header(m_pObj, reinterpret_cast<u_char*>(const_cast<char*>(header.c_str())), header.length());

	if (status != DKIM_STAT_OK)
	{
		clog << "OpenDKIM: status " << status << " on header " << header << endl;
		return false;
	}

	return true;
}

bool OpenDKIM::feedMessage(const string &messageData,
	SMTPMessage *pMsg)
{
	DKIM_STAT status;

	if ((messageData.empty() == true) ||
		(pMsg == NULL))
	{
		return false;
	}

	string::size_type startOfMessage = messageData.find("\r\n\r\n");

	if (startOfMessage == string::npos)
	{
		startOfMessage = 0;
	}
	else
	{
		startOfMessage += 4;
	}

#ifdef _DEBUG_FEEDING
	ofstream dataFile("data.txt");
#endif

	// Feed headers
#ifdef DEBUG
	clog << "OpenDKIM::feedMessage: " << pMsg->m_headers.size() << " headers" << endl;
#endif
	for (vector<SMTPHeader>::const_iterator headerIter = pMsg->m_headers.begin();
		headerIter != pMsg->m_headers.end(); ++headerIter)
	{
		string headerName(headerIter->m_name);
		string value(headerIter->m_value);
		string path(headerIter->m_path);
		stringstream headerStr;

		headerStr << headerName << ":";
		if (value.empty() == false)
		{
			headerStr << " " << value;
		}
		if (path.empty() == false)
		{
			headerStr << " <" << path << ">";
		}
		headerStr << "\r\n";
		string header(headerStr.str());

#ifdef _DEBUG_FEEDING
		dataFile << header;
#endif

		if (feedHeader(header) == false)
		{
			break;
		}
	}
	// End of headers
	status = dkim_eoh(m_pObj);
	if (status != DKIM_STAT_OK)
	{
		clog << "OpenDKIM: headers status " << status << endl;
	}
#ifdef _DEBUG_FEEDING
		dataFile << "\r\n";
#endif

	// Feed the message data, line by line
	string::size_type startPos = startOfMessage, pos = messageData.find_first_of("\r\n", startOfMessage);
	string::size_type messageDataLen = messageData.length() - startOfMessage;
	while (pos != string::npos)
	{
		if (pos != startPos)
		{
			status = dkim_body(m_pObj, reinterpret_cast<u_char*>(const_cast<char*>(messageData.c_str() + startPos)), pos - startPos);
			if (status != DKIM_STAT_OK)
			{
					clog << "OpenDKIM: body status " << status << endl;
					break;
			}
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
		if (startPos >= messageData.length())
		{
			break;
		}

		status = dkim_body(m_pObj, reinterpret_cast<u_char*>(const_cast<char*>("\r\n")), 2);
		if (status != DKIM_STAT_OK)
		{
				clog << "OpenDKIM: body status " << status << endl;
				break;
		}
#ifdef _DEBUG_FEEDING
		dataFile << "\r\n";
#endif
		pos = messageData.find_first_of("\r\n", startPos);
	}
	if (startPos < messageData.length())
	{
		status = dkim_body(m_pObj, reinterpret_cast<u_char*>(const_cast<char*>(messageData.c_str() + startPos)), messageDataLen - startPos);
		if (status != DKIM_STAT_OK)
		{
				clog << "OpenDKIM: body status " << status << endl;
		}
#ifdef _DEBUG_FEEDING
		dataFile << string(messageData.c_str() + startPos);
#endif
	}
	// End Of Message
	status = dkim_eom(m_pObj, NULL);
	if (status != DKIM_STAT_OK)
	{
		clog << "OpenDKIM: end of message status " << status << endl;
	}

#ifdef _DEBUG_FEEDING
	dataFile << "\r\n";
	dataFile.close();
#endif

	return true;
}

bool OpenDKIM::loadPrivateKey(ConfigurationFile *pConfig)
{
	if ((pConfig == NULL) ||
		(pConfig->m_dkSelector.empty() == true) ||
		(DomainAuth::loadPrivateKey(pConfig) == false))
	{
		return false;
	}

	m_selector = pConfig->m_dkSelector;
	m_privateKeyFileName = pConfig->m_dkPrivateKey;

	return true;
}

bool OpenDKIM::initialize(void)
{
	m_pLib = dkim_init(NULL, NULL);
	if (m_pLib == NULL)
	{
		return false;
	}

	return true;
}

void OpenDKIM::shutdown(void)
{
	dkim_close(m_pLib);
}

void OpenDKIM::cleanupThread(void)
{
	// Because OpenDKIM uses OpenSSL, each thread should call this at exit time
	ERR_remove_thread_state(NULL);
}

bool OpenDKIM::canSign(void) const
{
	if ((m_pLib == NULL) ||
		(m_pPrivateKey == NULL) ||
		(m_domainName.empty() == true) ||
		(m_selector.empty() == true) ||
		(m_privateKeyFileName.empty() == true))
	{
		return false;
	}

	return true;
}

bool OpenDKIM::lockedSign(const string &messageData,
	SMTPMessage *pMsg, bool simpleCanon)
{
	DKIM_STAT status;
	dkim_sigkey_t key = reinterpret_cast<unsigned char*>(m_pPrivateKey);
	dkim_query_t queryType = DKIM_QUERY_FILE;
	uint64_t fixedTime = (uint64_t)time(NULL);

	if ((canSign() == false) ||
		(messageData.empty() == true) ||
		(pMsg == NULL))
	{
		return false;
	}

	dkim_options(m_pLib, DKIM_OP_SETOPT, DKIM_OPTS_QUERYMETHOD,
		&queryType, sizeof(queryType));
	dkim_options(m_pLib, DKIM_OP_SETOPT, DKIM_OPTS_QUERYINFO,
		const_cast<char*>(m_privateKeyFileName.c_str()), m_privateKeyFileName.length());
	dkim_options(m_pLib, DKIM_OP_SETOPT, DKIM_OPTS_FIXEDTIME,
		&fixedTime, sizeof(fixedTime));
	dkim_options(m_pLib, DKIM_OP_SETOPT, DKIM_OPTS_SIGNHDRS,
		dkim_should_signhdrs, sizeof(u_char **));

	if (simpleCanon == true)
	{
		m_pObj = dkim_sign(m_pLib, reinterpret_cast<const unsigned char*>(JOBID), NULL, key, reinterpret_cast<const unsigned char*>(m_selector.c_str()), reinterpret_cast<const unsigned char*>(m_domainName.c_str()),
				DKIM_CANON_RELAXED, DKIM_CANON_SIMPLE,
				DKIM_SIGN_RSASHA1, -1L, &status);
	}
	else
	{
		m_pObj = dkim_sign(m_pLib, reinterpret_cast<const unsigned char*>(JOBID), NULL, key, reinterpret_cast<const unsigned char*>(m_selector.c_str()), reinterpret_cast<const unsigned char*>(m_domainName.c_str()),
				DKIM_CANON_RELAXED, DKIM_CANON_RELAXED,
				DKIM_SIGN_RSASHA1, -1L, &status);
	}

	if (m_pObj == NULL)
	{
		clog << "OpenDKIM: sign failed with status " << status << endl;
		return false;
	}
	if (status != DKIM_STAT_OK)
	{
		dkim_free(m_pObj);

		clog << "OpenDKIM: sign status " << status << endl;
		return false;
	}

	if (feedMessage(messageData, pMsg) == false)
	{
		return false;
	}

	unsigned char signature[4096];
	status = dkim_getsighdr(m_pObj, signature, sizeof(signature),
		strlen(DKIM_SIGNHEADER) + 2);

	dkim_free(m_pObj);

    stringstream headerStr;

    headerStr << signature;

	if (status != DKIM_STAT_OK)
	{
		clog << "OpenDKIM: status " << status << " on signature header " << headerStr.str() << endl;
		return false;
	}

	pMsg->setSignatureHeader(DK_HEADER_NAME, headerStr.str());

	return true;
}

bool OpenDKIM::lockedVerify(const string &messageData,
	SMTPMessage *pMsg)
{
	DKIM_STAT status;
	dkim_query_t queryType = DKIM_QUERY_FILE;

	if ((canSign() == false) ||
		(messageData.empty() == true) ||
		(pMsg == NULL))
	{
		return false;
	}

	dkim_options(m_pLib, DKIM_OP_SETOPT, DKIM_OPTS_QUERYMETHOD,
		&queryType, sizeof(queryType));
	dkim_options(m_pLib, DKIM_OP_SETOPT, DKIM_OPTS_QUERYINFO,
		const_cast<char*>(m_privateKeyFileName.c_str()), m_privateKeyFileName.length());

	m_pObj = dkim_verify(m_pLib, reinterpret_cast<const unsigned char*>(JOBID), NULL, &status);

	if (m_pObj == NULL)
	{
		clog << "OpenDKIM: verify failed with status " << status << endl;
		return false;
	}
	if (status != DKIM_STAT_OK)
	{
		dkim_free(m_pObj);

		clog << "OpenDKIM: verify status " << status << endl;
		return false;
	}

	dkim_set_key_lookup(m_pLib, keyLookup);

	feedHeader(pMsg->getSignatureHeader());
	if (feedMessage(messageData, pMsg) == false)
	{
		return false;
	}

	DKIM_SIGINFO *pSig = dkim_getsignature(m_pObj);
	DKIM_SIGERROR error = DKIM_SIGERROR_UNKNOWN;

	if (pSig != NULL)
	{
		error = dkim_sig_geterror(pSig);
	}
	else
	{
		clog << "OpenDKIM: failed to get signature object" << endl;
	}

	dkim_free(m_pObj);

	if (error != DKIM_SIGERROR_OK)
	{
		clog << "OpenDKIM: signature verification error " << error << endl;

		return false;
	}
#ifdef DEBUG
	clog << "OpenDKIM::verify: success" << endl;
#endif

	return true;
}

bool OpenDKIM::sign(const string &messageData,
	SMTPMessage *pMsg, bool simpleCanon)
{
	pthread_mutex_lock(&m_mutex);

	bool status = lockedSign(messageData, pMsg, simpleCanon);

	pthread_mutex_unlock(&m_mutex);

	return status;
}

bool OpenDKIM::verify(const string &messageData,
	SMTPMessage *pMsg)
{
	pthread_mutex_lock(&m_mutex);

	bool status = lockedVerify(messageData, pMsg);

	pthread_mutex_unlock(&m_mutex);

	return status;
}

