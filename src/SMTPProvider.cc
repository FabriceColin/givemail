/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 *  Copyright 2008 Global Sign In
 *  Copyright 2009-2015 Fabrice Colin
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

#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <strings.h>
#include <iostream>
#include <fstream>

#include "config.h"
#ifdef USE_LIBETPAN
#include "LibETPANProvider.h"
#endif
#ifdef USE_LIBESMTP
#include "LibESMTPProvider.h"
#endif
#include "SMTPProvider.h"

#define _CAN_SPECIFY_SENDER

using std::clog;
using std::endl;
using std::map;
using std::string;
using std::ofstream;

SMTPMessage::SMTPMessage(const map<string, string> &fieldValues,
	MessageDetails *pDetails, DSNNotification dsnFlags,
	bool enableMdn,
	const string &msgIdSuffix,
	const string &complaints) :
	m_fieldValues(fieldValues),
	m_pDetails(pDetails),
	m_dsnFlags(dsnFlags),
	m_enableMdn(enableMdn),
	m_msgIdSuffix(msgIdSuffix),
	m_complaints(complaints)
{
}

SMTPMessage::~SMTPMessage()
{
}

void SMTPMessage::substitute(const map<string, string> &fieldValues)
{
	if (m_pDetails == NULL)
	{
		return;
	}

	m_pDetails->getPlainSubstituteObj()->substitute(fieldValues, m_plainContent);
	m_pDetails->getHtmlSubstituteObj()->substitute(fieldValues, m_htmlContent);
}

void SMTPMessage::buildHeaders(void)
{
	string suffix(m_msgIdSuffix);
	string userAgent(getUserAgent());

	appendHeader("MIME-Version", "1.0", "");

	if (userAgent.empty() == false)
	{
		if ((m_pDetails != NULL) &&
			(m_pDetails->m_useXMailer == true))
		{
			appendHeader("X-Mailer", userAgent, "");
		}
		else
		{
			appendHeader("User-Agent", userAgent, "");
		}
	}

	if (m_complaints.empty() == false)
	{
		appendHeader("X-Complaints-To", m_complaints, "");
	}

	// All these headers are short so it should be okay to substitute their fields for each message
	if (m_pDetails != NULL)
	{
		if (m_pDetails->m_subject.empty() == false)
		{
			appendHeader("Subject",
				m_pDetails->substitute(m_pDetails->m_subject, m_fieldValues), "");
		}

		if (m_pDetails->m_senderEmailAddress.empty() == false)
		{
			string senderEmailAddress(m_pDetails->substitute(m_pDetails->m_senderEmailAddress, m_fieldValues));
			string::size_type atPos = senderEmailAddress.find('@');

			if (atPos != string::npos)
			{
				suffix = senderEmailAddress.substr(atPos);
			}
#ifdef _CAN_SPECIFY_SENDER
			appendHeader("Sender", "", senderEmailAddress);
#endif
			appendHeader("Resent-From", "", senderEmailAddress);
		}

		appendHeader("Resent-Message-Id", m_pDetails->createMessageId(suffix, false), "");
		m_msgId = m_pDetails->createMessageId(suffix, false);
		appendHeader("Message-Id", m_msgId, "");
		if (m_pDetails->m_isReply == true)
		{
			appendHeader("References", m_pDetails->createMessageId(suffix, true), "");
			appendHeader("In-Reply-To", m_pDetails->createMessageId(suffix, true), "");
		}

		if (m_pDetails->m_fromEmailAddress.empty() == false)
		{
			m_smtpFrom = m_pDetails->substitute(m_pDetails->m_fromEmailAddress, m_fieldValues);

			appendHeader("From",
				m_pDetails->substitute(m_pDetails->m_fromName, m_fieldValues),
				m_smtpFrom);
		}

		if (m_pDetails->m_replyToEmailAddress.empty() == false)
		{
			appendHeader("Reply-To",
				m_pDetails->substitute(m_pDetails->m_replyToName, m_fieldValues),
				m_pDetails->substitute(m_pDetails->m_replyToEmailAddress, m_fieldValues));
		}
	}
}

bool SMTPMessage::isDeliveryReceipt(void) const
{
	if ((m_pDetails != NULL) &&
		(m_pDetails->isReport("message/delivery-status") == true))
	{
		return true;
	}

	return false;
}

bool SMTPMessage::isReadNotification(void) const
{
	if ((m_pDetails != NULL) &&
		(m_pDetails->isReport("message/disposition-notification") == true))
	{
		return true;
	}

	return false;
}

bool SMTPMessage::requiresMixedParts(void) const
{
	if ((m_pDetails != NULL) &&
		(m_pDetails->getAttachmentCount(false) > 0))
	{
#ifdef DEBUG
		clog << "SMTPMessage::requiresMixedParts: yes" << endl;
#endif
		return true;
	}

	return false;
}

bool SMTPMessage::requiresAlternativeParts(void) const
{
	if ((m_plainContent.empty() == false) &&
		(m_htmlContent.empty() == false))
	{
#ifdef DEBUG
		clog << "SMTPMessage::requiresAlternativeParts: yes" << endl;
#endif
		return true;
	}

	return false;
}

bool SMTPMessage::requiresRelatedParts(void) const
{
	if ((m_pDetails != NULL) &&
		(m_pDetails->getAttachmentCount(true) > 0))
	{
#ifdef DEBUG
		clog << "SMTPMessage::requiresRelatedParts: yes" << endl;
#endif
		return true;
	}

	return false;
}

string SMTPMessage::getFromEmailAddress(void) const
{
	if (m_pDetails != NULL)
	{
		return m_pDetails->m_fromEmailAddress;
	}

	return "";
}

string SMTPMessage::getSenderEmailAddress(void) const
{
	if (m_pDetails != NULL)
	{
		return m_pDetails->m_senderEmailAddress;
	}

	return "";
}

string SMTPMessage::getUserAgent(void) const
{
	if ((m_pDetails != NULL) &&
		(m_pDetails->m_userAgent.empty() == false))
	{
		return m_pDetails->m_userAgent;
	}

	return PACKAGE_NAME;
}

void SMTPMessage::dumpToFile(const string &fileBaseName) const
{
	string msgFileName(fileBaseName);

	if (m_msgId.empty() == true)
	{
		return;
	}
	if (msgFileName.empty() == false)
	{
		msgFileName += "-";
	}
	msgFileName += m_msgId;

	if (m_plainContent.empty() == false)
	{
		string txtFileName(msgFileName + ".txt");
		ofstream dumpFile(txtFileName.c_str());

		if (dumpFile.is_open() == true)
		{
			dumpFile << m_plainContent;
			dumpFile.close();
		}
	}
	if (m_htmlContent.empty() == false)
	{
		string htmlFileName(msgFileName + ".html");
		ofstream dumpFile(htmlFileName.c_str());

		if (dumpFile.is_open() == true)
		{
			dumpFile << m_htmlContent;
			dumpFile.close();
		}
	}
}

SMTPProvider::SMTPProvider()
{
}

SMTPProvider::~SMTPProvider()
{
}

void SMTPProvider::enableAuthentication(const string &authRealm,
	const string &authUserName,
	const string &authPassword)
{
	m_authRealm = authRealm;
	m_authUserName = authUserName;
	m_authPassword = authPassword;
}

string SMTPProvider::getAuthUserName(void) const
{
	return m_authUserName;
}

string SMTPProvider::getAuthPassword(void) const
{
	return m_authPassword;
}

SMTPProvider *SMTPProviderFactory::getProvider(void)
{
#ifdef USE_LIBETPAN
	return new LibETPANProvider();
#else
#ifdef USE_LIBESMTP
	return new LibESMTPProvider();
#endif
#endif
	return NULL;
}

