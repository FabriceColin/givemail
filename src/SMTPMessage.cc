/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 *  Copyright 2008 Global Sign In
 *  Copyright 2009-2020 Fabrice Colin
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
#include <sstream>
#include <utility>

#include "config.h"
#include "SMTPMessage.h"

#define _CAN_SPECIFY_SENDER

using std::clog;
using std::endl;
using std::map;
using std::pair;
using std::string;
using std::ofstream;
using std::stringstream;

SMTPHeader::SMTPHeader(const std::string &name, const std::string &value, const std::string &path) :
	m_name(name),
	m_value(value),
	m_path(path)
{
}

SMTPHeader::SMTPHeader(const SMTPHeader &other) :
	m_name(other.m_name),
	m_value(other.m_value),
	m_path(other.m_path)
{
}

SMTPHeader::~SMTPHeader()
{
}

SMTPHeader &SMTPHeader::operator=(const SMTPHeader &other)
{
	if (this != &other)
	{
		m_name = other.m_name;
		m_value = other.m_value;
		m_path = other.m_path;
	}

	return *this;
}

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
	m_msgId(""),
	m_complaints(complaints),
	m_subId(0)
{
	// Generate a message ID early on so that it may used as dictionary ID
	// for all substitutions related to this message
	if (m_pDetails != NULL)
	{
		m_msgId = m_pDetails->createMessageId(m_msgIdSuffix, false);
	}
}

SMTPMessage::~SMTPMessage()
{
}

void SMTPMessage::substituteContent(const map<string, string> &fieldValues)
{
	if (m_pDetails == NULL)
	{
		return;
	}

	m_pDetails->getPlainSubstituter(m_msgId + "p")->substitute(fieldValues, m_plainContent);
	m_pDetails->getHtmlSubstituter(m_msgId + "h")->substitute(fieldValues, m_htmlContent);
}

void SMTPMessage::buildHeaders(void)
{
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
		string suffix(m_msgIdSuffix);

		if (m_pDetails->m_subject.empty() == false)
		{
			appendHeader("Subject",
				substitute(m_pDetails->m_subject, m_fieldValues), "");
		}

		if (m_pDetails->m_senderEmailAddress.empty() == false)
		{
			string senderEmailAddress(substitute(m_pDetails->m_senderEmailAddress, m_fieldValues));
			string::size_type atPos = senderEmailAddress.find('@');

			if (atPos != string::npos)
			{
				suffix = senderEmailAddress.substr(atPos);
			}
#ifdef _CAN_SPECIFY_SENDER
			appendHeader("Sender", senderEmailAddress, "");
#endif
			appendHeader("Resent-From", "", senderEmailAddress);
		}

		appendHeader("Resent-Message-Id", m_pDetails->createMessageId(suffix, false), "");
		appendHeader("Message-Id", m_msgId, "");
		if (m_pDetails->m_isReply == true)
		{
			appendHeader("References", m_pDetails->createMessageId(suffix, true), "");
			appendHeader("In-Reply-To", m_pDetails->createMessageId(suffix, true), "");
		}

		if (m_pDetails->m_fromEmailAddress.empty() == false)
		{
			m_smtpFrom = substitute(m_pDetails->m_fromEmailAddress, m_fieldValues);

			appendHeader("From",
				substitute(m_pDetails->m_fromName, m_fieldValues),
				m_smtpFrom);
		}

		if (m_pDetails->m_replyToEmailAddress.empty() == false)
		{
			appendHeader("Reply-To",
				substitute(m_pDetails->m_replyToName, m_fieldValues),
				substitute(m_pDetails->m_replyToEmailAddress, m_fieldValues));
		}
	}
}

void SMTPMessage::appendHeader(const string &header,
	const string &value, const string &path)
{
	if (addHeader(header, value, path) == true)
	{
		// Build a colon separated list of headers
		// This will be useful when signing the message with DomainKeys
		if (m_allHeaders.empty() == false)
		{
			m_allHeaders.append(":");
		}
		m_allHeaders.append(header);
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

bool SMTPMessage::setSignatureHeader(const string &header,
	const string &value)
{
	if ((header.empty() == true) ||
		(value.empty() == true))
	{
		return false;
	}

	m_signatureHeader = header;
	m_signatureHeader += ": ";
	m_signatureHeader += value;

	return true;
}

string SMTPMessage::getSignatureHeader(void) const
{
	return m_signatureHeader;
}

string SMTPMessage::substitute(const string &content,
	const map<string, string> &fieldValues)
{
	stringstream idStr;

	idStr << m_msgId;
	idStr << m_subId;
	if (m_subId == 0)
	{
#ifdef DEBUG
			clog << "SMTPMessage::substitute: no ID" << endl;
#endif
	}
	++m_subId;

	return m_pDetails->substitute(idStr.str(), content, fieldValues);
}

bool SMTPMessage::addHeader(const string &header,
	const string &value, const string &path)
{
	// We need at least header and a value or a path
	if (header.empty() == true)
	{
		return false;
	}
	if ((value.empty() == true) &&
		(path.empty() == true))
	{
		return false;
	}

	// Store this
	m_headers.push_back(SMTPHeader(header, value, path));
#ifdef DEBUG
	clog << "SMTPMessage::addHeader: " << header << " = " << value << endl;
#endif

	return true;
}

void SMTPMessage::setReversePath(const string &reversePath)
{
#ifdef DEBUG
	clog << "SMTPMessage::setReversePath: " << reversePath << endl;
#endif
	m_reversePath = reversePath;
}

string SMTPMessage::getReversePath(void) const
{
	if (m_reversePath.empty() == false)
	{
		return m_reversePath;
	}
	// Else, make up a Return-Path header

	string reversePath(getSenderEmailAddress());

	if (reversePath.empty() == false)
	{
		return reversePath;
	}

	reversePath = getFromEmailAddress();

	return reversePath;
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

