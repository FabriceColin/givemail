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

#include <sys/types.h>
#include <time.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <sstream>
#include <iostream>
#include <algorithm>

#include "config.h"
#include "Base64.h"
#include "LibESMTPProvider.h"
#include "QuotedPrintable.h"
#include "SMTPSession.h"

using std::clog;
using std::endl;
using std::map;
using std::set;
using std::string;
using std::stringstream;
using std::min;

static int authenticationCallback(auth_client_request_t request, char **ppResult, int fields, void *pArg)
{
	SMTPProvider *pProvider = NULL;

	if (pArg != NULL)
	{
		pProvider = (SMTPProvider *)pArg;
	}

	for (int fieldNum = 0; fieldNum < fields; ++fieldNum)
	{
		clog << "LibESMTPProvider: prompted for " << request[fieldNum].name << ": " << request[fieldNum].prompt
			<< (request[fieldNum].flags & AUTH_CLEARTEXT ? " (not encrypted)" : " (encrypted)") << endl;

		// FIXME: check request[fieldNum].size
		if (pProvider == NULL)
		{
			continue;
		}

		if (request[fieldNum].flags & AUTH_PASS)
		{
			ppResult[fieldNum] = const_cast<char*>(pProvider->getAuthPassword().c_str());
		}
		else
		{
			ppResult[fieldNum] = const_cast<char*>(pProvider->getAuthUserName().c_str());
		}
	}

	return 1;
}

static void eventCallback(smtp_session_t session, int eventNum, void *pArg, ...)
{
	SMTPSession *pSession = NULL;
	string domainName, errMsg;
	va_list alist;
	int *ok = NULL;

	if (pArg != NULL)
	{
		pSession = (SMTPSession *)pArg;
		domainName = pSession->getDomainName();

		if ((pSession->getError(errMsg) == SMTP_ERR_DROPPED_CONNECTION) &&
			(eventNum != SMTP_EV_DISCONNECT))
		{
			// We were not disconnected right away, reset the error condition
			pSession->recordError(true);
		}
	}

	va_start(alist, pArg);
	switch(eventNum)
	{
		case SMTP_EV_CONNECT:
			if (pSession != NULL)
			{
				// Assume we'll get disconnected right away
				pSession->setError(SMTP_ERR_DROPPED_CONNECTION);
			}
			clog << "LibESMTPProvider: " << domainName << " SMTP_EV_CONNECT" << endl;
			break;
		case SMTP_EV_MAILSTATUS:
			clog << "LibESMTPProvider: " << domainName << " SMTP_EV_MAILSTATUS" << endl;
			break;
		case SMTP_EV_RCPTSTATUS:
			clog << "LibESMTPProvider: " << domainName << " SMTP_EV_RCPTSTATUS" << endl;
			break;
#ifdef DEBUG
		case SMTP_EV_MESSAGEDATA:
			clog << "LibESMTPProvider: " << domainName << " SMTP_EV_MESSAGEDATA" << endl;
			break;
#endif
		case SMTP_EV_MESSAGESENT:
			clog << "LibESMTPProvider: " << domainName << " SMTP_EV_MESSAGESENT" << endl;
			break;
		case SMTP_EV_DISCONNECT:
			if ((pSession != NULL) &&
				(pSession->getError(errMsg) == SMTP_ERR_DROPPED_CONNECTION))
			{
				clog << "LibESMTPProvider: " << domainName << " SMTP_EV_DISCONNECT right after SMTP_EV_CONNECT" << endl;
			}
			else
			{
				clog << "LibESMTPProvider: " << domainName << " SMTP_EV_DISCONNECT" << endl;
			}
			break;
		case SMTP_EV_WEAK_CIPHER:
		{
			int bits = va_arg(alist, long);
			ok = va_arg(alist, int*);
			clog << "LibESMTPProvider: " << domainName << " SMTP_EV_WEAK_CIPHER bits=" << bits << endl;
			break;
		}
		case SMTP_EV_STARTTLS_OK:
			clog << "LibESMTPProvider: " << domainName << " SMTP_EV_STARTTLS_OK" << endl;
			break;
		case SMTP_EV_INVALID_PEER_CERTIFICATE:
		{
			long vfy_result = va_arg(alist, long);
			ok = va_arg(alist, int*);
			clog << "LibESMTPProvider: " << domainName << " SMTP_EV_INVALID_PEER_CERTIFICATE vfy_result=" << vfy_result << endl;
			break;
		}
		case SMTP_EV_NO_PEER_CERTIFICATE:
		{
			ok = va_arg(alist, int*); 
			clog << "LibESMTPProvider: " << domainName << " SMTP_EV_NO_PEER_CERTIFICATE" << endl;
			*ok = 1;
			break;
		}
		case SMTP_EV_WRONG_PEER_CERTIFICATE:
		{
			ok = va_arg(alist, int*);
			clog << "LibESMTPProvider: " << domainName << " SMTP_EV_WRONG_PEER_CERTIFICATE" << endl;
			*ok = 1;
			break;
		}
		case SMTP_EV_NO_CLIENT_CERTIFICATE:
		{
			ok = va_arg(alist, int*); 
			clog << "LibESMTPProvider: " << domainName << " SMTP_EV_NO_CLIENT_CERTIFICATE" << endl;
			*ok = 1;
			break;
		}
		default:
#ifdef DEBUG
			clog << "LibESMTPProvider: unknown event " << eventNum << endl;
#endif
			break;
	}
	va_end(alist);
}

string g_attachmentLine;
string g_extraLine("\r\n");
string g_startMixed("Content-Type: multipart/mixed;\r\n\tboundary=\"----=_NextMixedPart\"\r\n\r\nThis is a message in multipart MIME format. Your mail client should not be displaying this. Consider upgrading your mail client to view this message correctly.\r\n\r\n------=_NextMixedPart\r\n");
string g_startAlt("Content-Type: multipart/alternative;\r\n\tboundary=\"----=_NextAltPart\"\r\n\r\n\r\n------=_NextAltPart\r\n");
string g_startAltNoMulti("Content-Type: multipart/alternative;\r\n\tboundary=\"----=_NextAltPart\"\r\n\r\nThis is a multi-part message in MIME format.\r\n\r\n------=_NextAltPart\r\n");
string g_startPlain("Content-Type: text/plain;\r\n\tcharset=\"UTF-8\"\r\nContent-Transfer-Encoding: quoted-printable\r\n\r\n");
string g_altTransition("------=_NextAltPart\r\n");
string g_startRelated("Content-Type: multipart/related;\r\n\tboundary=\"----=_NextRelatedPart\"\r\n\r\n------=_NextRelatedPart\r\n");
string g_startHtml("Content-Type: text/html;\r\n\tcharset=\"UTF-8\"\r\nContent-Transfer-Encoding: quoted-printable\r\n\r\n");
string g_startInline("------=_NextRelatedPart\r\n");
string g_endRelated("\r\n------=_NextRelatedPart--\r\n\r\n");
string g_endAlt("\r\n------=_NextAltPart--\r\n\r\n");
string g_startAttachment("------=_NextMixedPart\r\n");
string g_endMixed("\r\n------=_NextMixedPart--\r\n\r\n");

const char *messageDataCallback(void **ppBuf, int *pLen, void *pArg)
{
	if (pArg == NULL)
	{
		return NULL;
	}
	LibESMTPMessage *pMsg = (LibESMTPMessage *)pArg;

	// We don't need to malloc a buffer
	*ppBuf = NULL;
	if (pLen == NULL)
	{
		// Rewind to the beginning of the message 
		pMsg->m_stage = LibESMTPMessage::EXTRA_HEADERS;
		pMsg->m_currentAttachment = 0;
		pMsg->m_encodedPos = 0;
#ifdef DEBUG
		clog << "LibESMTPProvider: rewound message" << endl;
#endif
		return NULL;
	}

	string headersDump(pMsg->getHeadersDump());
	const char *pBuf = NULL;

#ifdef DEBUG
	clog << "LibESMTPProvider: stage " << pMsg->m_stage << endl;
#endif
	// EXTRA_HEADERS, START_MIXED, START_ALT,
	// START_PLAIN, PLAIN, END_PLAIN,
	// ALT_TRANSITION, START_RELATED,
	// START_HTML, HTML, END_HTML,
	// START_INLINE, INLINE_HEADERS, INLINE_LINE, INLINE_LINE_END, END_INLINE, NEXT_INLINE,
	// END_RELATED, END_ALT,
	// START_ATTACHMENT, ATTACHMENT_HEADERS, ATTACHMENT_LINE, ATTACHMENT_LINE_END, END_ATTACHMENT, NEXT_ATTACHMENT,
	// END_MIXED, END
	switch (pMsg->m_stage)
	{
		case LibESMTPMessage::EXTRA_HEADERS:
			if (headersDump.empty() == false)
			{
				*pLen = (int)headersDump.length();
				pBuf = headersDump.c_str();
				break;
			}
			pMsg->m_stage = LibESMTPMessage::START_MIXED;
		case LibESMTPMessage::START_MIXED:
			if (pMsg->requiresMixedParts() == true)
			{
				*pLen = (int)g_startMixed.length();
				pBuf = g_startMixed.c_str();
				break;
			}
			pMsg->m_stage = LibESMTPMessage::START_ALT;
		case LibESMTPMessage::START_ALT:
			if (pMsg->requiresAlternativeParts() == true)
			{
				if (pMsg->requiresMixedParts() == true)
				{
					*pLen = (int)g_startAlt.length();
					pBuf = g_startAlt.c_str();
				}
				else
				{
					*pLen = (int)g_startAltNoMulti.length();
					pBuf = g_startAltNoMulti.c_str();
				}
				break;
			}
			pMsg->m_stage = LibESMTPMessage::START_PLAIN;
		case LibESMTPMessage::START_PLAIN:
			if (pMsg->m_plainContent.empty() == false)
			{
				*pLen = (int)g_startPlain.length();
				pBuf = g_startPlain.c_str();
				break;
			}
			pMsg->m_stage = LibESMTPMessage::PLAIN;
		case LibESMTPMessage::PLAIN:
			// This is optional
			if (pMsg->m_plainContent.empty() == false)
			{
				*pLen = (int)pMsg->m_plainContent.length();
				pBuf = pMsg->m_plainContent.c_str();
				break;
			}
			pMsg->m_stage = LibESMTPMessage::END_PLAIN;
		case LibESMTPMessage::END_PLAIN:
			if ((pMsg->m_plainContent.empty() == false) &&
				(pMsg->m_plainContent[pMsg->m_htmlContent.length()] != '\n'))
			{
				*pLen = (int)g_extraLine.length();
				pBuf = g_extraLine.c_str();
				break;
			}
			pMsg->m_stage = LibESMTPMessage::ALT_TRANSITION;
		case LibESMTPMessage::ALT_TRANSITION:
			if (pMsg->requiresAlternativeParts() == true)
			{
				*pLen = (int)g_altTransition.length();
				pBuf = g_altTransition.c_str();
				break;
			}
			pMsg->m_stage = LibESMTPMessage::START_RELATED;
		case LibESMTPMessage::START_RELATED:
			if ((pMsg->m_htmlContent.empty() == false) &&
				(pMsg->requiresRelatedParts() == true))
			{
				*pLen = (int)g_startRelated.length();
				pBuf = g_startRelated.c_str();
				break;
			}
			pMsg->m_stage = LibESMTPMessage::START_HTML;
		case LibESMTPMessage::START_HTML:
			if (pMsg->m_htmlContent.empty() == false)
			{
				*pLen = (int)g_startHtml.length();
				pBuf = g_startHtml.c_str();
				break;
			}
			pMsg->m_stage = LibESMTPMessage::HTML;
		case LibESMTPMessage::HTML:
			// This is optional
			if (pMsg->m_htmlContent.empty() == false)
			{
				*pLen = (int)pMsg->m_htmlContent.length();
				pBuf = pMsg->m_htmlContent.c_str();
				break;
			}
			pMsg->m_stage = LibESMTPMessage::END_HTML;
		case LibESMTPMessage::END_HTML:
			if ((pMsg->m_htmlContent.empty() == false) &&
				(pMsg->m_htmlContent[pMsg->m_htmlContent.length()] != '\n'))
			{
				*pLen = (int)g_extraLine.length();
				pBuf = g_extraLine.c_str();
				break;
			}
			pMsg->m_stage = LibESMTPMessage::START_INLINE;
		case LibESMTPMessage::START_INLINE:
			if (pMsg->m_pDetails->getAttachmentCount(true) > 0)
			{
				*pLen = (int)g_startInline.length();
				pBuf = g_startInline.c_str();
				break;
			}
			pMsg->m_stage = LibESMTPMessage::INLINE_HEADERS;
		case LibESMTPMessage::INLINE_HEADERS:
			if (pMsg->m_pDetails->getAttachmentCount(true) > 0)
			{
				string headers(pMsg->getAttachmentHeaders(pMsg->m_currentAttachment, true));
				if (headers.empty() == false)
				{
					*pLen = (int)headers.length();
					pBuf = headers.c_str();
#ifdef DEBUG
					clog << "LibESMTPProvider: got headers for inline " << pMsg->m_currentAttachment << endl;
#endif
					break;
				}
			}
			pMsg->m_stage = LibESMTPMessage::INLINE_LINE;
		case LibESMTPMessage::INLINE_LINE:
			// This is optional
			if (pMsg->m_pDetails->getAttachmentCount(true) > 0)
			{
				off_t encodedLen = 0;
				const char *pEncodedData = pMsg->getAttachmentLine(encodedLen, true);
				if ((pEncodedData != NULL) &&
					(encodedLen > 0))
				{
#ifdef DEBUG
					clog << "LibESMTPProvider: got " << encodedLen << " bytes long line for inline "
						<< pMsg->m_currentAttachment << endl;
#endif
					// FIXME: if we return the line as is, libesmtp will for some reason
					// ignore the length and go all the way to the terminating NULL
					g_attachmentLine = string(pEncodedData, (string::size_type)encodedLen);
					*pLen = (int)g_attachmentLine.length();
					pBuf = g_attachmentLine.c_str();
					break;
				}
			}
			pMsg->m_stage = LibESMTPMessage::INLINE_LINE_END;
		case LibESMTPMessage::INLINE_LINE_END:
			if (pMsg->m_pDetails->getAttachmentCount(true) > 0)
			{
				*pLen = (int)g_extraLine.length();
				pBuf = g_extraLine.c_str();
#ifdef DEBUG
				clog << "LibESMTPProvider: end of line for inline " << pMsg->m_currentAttachment << endl;
#endif
				if (pMsg->hasMoreAttachmentLines() == true)
				{
					// Jump back so that the next stage outputs the next line
					pMsg->m_stage = LibESMTPMessage::INLINE_HEADERS;
				}
				break;
			}
			pMsg->m_stage = LibESMTPMessage::END_INLINE;
		case LibESMTPMessage::END_INLINE:
			if (pMsg->m_pDetails->getAttachmentCount(true) > 0)
			{
				*pLen = (int)g_extraLine.length();
				pBuf = g_extraLine.c_str();
				++pMsg->m_currentAttachment;
				pMsg->m_encodedPos = 0;
				if (pMsg->m_currentAttachment >= pMsg->m_pDetails->getAttachmentCount(true))
				{
					// No more inline
					pMsg->m_stage = LibESMTPMessage::NEXT_INLINE;
				}
				break;
			}
			pMsg->m_stage = LibESMTPMessage::NEXT_INLINE;
		case LibESMTPMessage::NEXT_INLINE:
			if (pMsg->m_currentAttachment < pMsg->m_pDetails->getAttachmentCount(true))
			{
				*pLen = (int)g_extraLine.length();
				pBuf = g_extraLine.c_str();
				// Jump back so that the next stage starts the next inline
				pMsg->m_stage = LibESMTPMessage::END_HTML;
				break;
			}
			pMsg->m_stage = LibESMTPMessage::END_RELATED;
		case LibESMTPMessage::END_RELATED:
			if (pMsg->requiresRelatedParts() == true)
			{
				*pLen = (int)g_endRelated.length();
				pBuf = g_endRelated.c_str();
				// Reset the attachment counter
				pMsg->m_currentAttachment = 0;
				break;
			}
			pMsg->m_stage = LibESMTPMessage::END_ALT;
		case LibESMTPMessage::END_ALT:
			if (pMsg->requiresAlternativeParts() == true)
			{
				*pLen = (int)g_endAlt.length();
				pBuf = g_endAlt.c_str();
				break;
			}
			pMsg->m_stage = LibESMTPMessage::START_ATTACHMENT;
		case LibESMTPMessage::START_ATTACHMENT:
			if (pMsg->m_pDetails->getAttachmentCount(false) > 0)
			{
				*pLen = (int)g_startAttachment.length();
				pBuf = g_startAttachment.c_str();
				break;
			}
			pMsg->m_stage = LibESMTPMessage::ATTACHMENT_HEADERS;
		case LibESMTPMessage::ATTACHMENT_HEADERS:
			if (pMsg->m_pDetails->getAttachmentCount(false) > 0)
			{
				string headers(pMsg->getAttachmentHeaders(pMsg->m_currentAttachment, false));
				if (headers.empty() == false)
				{
					*pLen = (int)headers.length();
					pBuf = headers.c_str();
#ifdef DEBUG
					clog << "LibESMTPProvider: got headers for attachment " << pMsg->m_currentAttachment << endl;
#endif
					break;
				}
			}
			pMsg->m_stage = LibESMTPMessage::ATTACHMENT_LINE;
		case LibESMTPMessage::ATTACHMENT_LINE:
			// This is optional
			if (pMsg->m_pDetails->getAttachmentCount(false) > 0)
			{
				off_t encodedLen = 0;
				const char *pEncodedData = pMsg->getAttachmentLine(encodedLen, false);
				if ((pEncodedData != NULL) &&
					(encodedLen > 0))
				{
#ifdef DEBUG
					clog << "LibESMTPProvider: got " << encodedLen << " bytes long line for attachment "
						<< pMsg->m_currentAttachment << endl;
#endif
					// FIXME: if we return the line as is, libesmtp will for some reason
					// ignore the length and go all the way to the terminating NULL
					g_attachmentLine = string(pEncodedData, (string::size_type)encodedLen);
					*pLen = (int)g_attachmentLine.length();
					pBuf = g_attachmentLine.c_str();
					break;
				}
			}
			pMsg->m_stage = LibESMTPMessage::ATTACHMENT_LINE_END;
		case LibESMTPMessage::ATTACHMENT_LINE_END:
			if (pMsg->m_pDetails->getAttachmentCount(false) > 0)
			{
				*pLen = (int)g_extraLine.length();
				pBuf = g_extraLine.c_str();
#ifdef DEBUG
				clog << "LibESMTPProvider: end of line for attachment " << pMsg->m_currentAttachment << endl;
#endif
				if (pMsg->hasMoreAttachmentLines() == true)
				{
					// Jump back so that the next stage outputs the next line
					pMsg->m_stage = LibESMTPMessage::ATTACHMENT_HEADERS;
				}
				break;
			}
			pMsg->m_stage = LibESMTPMessage::END_ATTACHMENT;
		case LibESMTPMessage::END_ATTACHMENT:
			if (pMsg->m_pDetails->getAttachmentCount(false) > 0)
			{
				*pLen = (int)g_extraLine.length();
				pBuf = g_extraLine.c_str();
				++pMsg->m_currentAttachment;
				pMsg->m_encodedPos = 0;
				if (pMsg->m_currentAttachment >= pMsg->m_pDetails->getAttachmentCount(false))
				{
					// No more attachment
					pMsg->m_stage = LibESMTPMessage::NEXT_ATTACHMENT;
				}
				break;
			}
			pMsg->m_stage = LibESMTPMessage::NEXT_ATTACHMENT;
		case LibESMTPMessage::NEXT_ATTACHMENT:
			if (pMsg->m_currentAttachment < pMsg->m_pDetails->getAttachmentCount(false))
			{
				*pLen = (int)g_extraLine.length();
				pBuf = g_extraLine.c_str();
				// Jump back so that the next stage starts the next attachment
				pMsg->m_stage = LibESMTPMessage::END_ALT;
				break;
			}
			pMsg->m_stage = LibESMTPMessage::END_MIXED;
		case LibESMTPMessage::END_MIXED:
			if (pMsg->requiresMixedParts() == true)
			{
				*pLen = (int)g_endMixed.length();
				pBuf = g_endMixed.c_str();
				break;
			}
			pMsg->m_stage = LibESMTPMessage::END;
		case LibESMTPMessage::END:
		default:
#ifdef DEBUG
			clog << "LibESMTPProvider: end of message" << endl;
#endif
			*pLen = 0;
			pBuf = NULL;
			break;
	}

	pMsg->m_stage = (LibESMTPMessage::Stage)((int)pMsg->m_stage + 1);

	return pBuf;
}

void recipientStatusCallback(smtp_recipient_t recipient, const char *pMailbox, void *pArg)
{
	const smtp_status_t *pStatus = smtp_recipient_status(recipient);
	if ((pMailbox != NULL) &&
		(pStatus != NULL))
	{
		if (pArg != NULL)
		{
			StatusUpdater *pUpdater = (StatusUpdater *)pArg;

			pUpdater->updateRecipientStatus(pMailbox,
				pStatus->code, pStatus->text);
		}
	}
}

void messageStatusCallback(smtp_message_t message, void *pArg)
{
	const smtp_status_t *pStatus = smtp_message_transfer_status(message);

	clog << "SMTP status for message: " << pStatus->code
		<< " (" << ((pStatus->text != NULL) ? pStatus->text : "") << ")" << endl;
	// FIXME: if pStatus->code != 250, consider all recipients failed ?

	// Check all recipients for this message
	smtp_enumerate_recipients(message, recipientStatusCallback, pArg);
}

void resetRecipientsCallback(smtp_recipient_t recipient, const char *pMailbox, void *pArg)
{
	smtp_recipient_reset_status(recipient);

	if (pMailbox != NULL)
	{
		clog << "Reset recipient " << pMailbox << endl;
	}
}

void resetMessagesCallback(smtp_message_t message, void *pArg)
{
	smtp_message_reset_status(message);
	clog << "Reset message" << endl;

	smtp_enumerate_recipients(message, resetRecipientsCallback, pArg);
}

static void qpEncode(string &content)
{
	if (content.empty() == true)
	{
		return;
	}

	// We have to do this every single time fields have been substituted
	size_t encodedLength = content.length();
	char *pEncodedContent = QuotedPrintable::encode(content.c_str(), encodedLength);
	if (pEncodedContent != NULL)
	{
		content.clear();

		char *pCurrentPos = pEncodedContent;
		char *pNLPos = strchr(pCurrentPos, '\n');
		while (pNLPos != NULL)
		{
			size_t lineLen = pNLPos - pCurrentPos;

			content.append(pCurrentPos, lineLen);
			content.append("\r\n", 2);

			// Next
			pCurrentPos = pNLPos + 1;
			if ((size_t)(pCurrentPos - pEncodedContent) >= encodedLength)
			{
				break;
			}
			pNLPos = strchr(pCurrentPos, '\n');
		} 
		if ((size_t)(pCurrentPos - pEncodedContent) < encodedLength)
		{
			content.append(pCurrentPos, encodedLength - (pCurrentPos - pEncodedContent));
		}

		delete[] pEncodedContent;
	}
#ifdef DEBUG
	else clog << "qpEncode: couldn't encode content" << endl;
#endif
}

LibESMTPMessage::LibESMTPMessage(const map<string, string> &fieldValues,
	MessageDetails *pDetails, DSNNotification dsnFlags,
	bool enableMdn,
	const string &msgIdSuffix,
	const string &complaints) :
	SMTPMessage(fieldValues, pDetails, dsnFlags, enableMdn, msgIdSuffix, complaints),
	m_stage(EXTRA_HEADERS),
	m_currentAttachment(0),
	m_encodedPos(0),
	m_message(NULL),
	m_hasMoreLines(false)
{
	if (m_pDetails != NULL)
	{
		// Substitute fields in content once
		substitute(fieldValues);

		qpEncode(m_plainContent);
		qpEncode(m_htmlContent);

		// Load attachments
		m_pDetails->loadAttachments();
	}
	buildHeaders();
}

LibESMTPMessage::~LibESMTPMessage()
{
}

void LibESMTPMessage::buildHeaders(void)
{
	if (m_pDetails != NULL)
	{
		struct tm timeTm;
		time_t aTime = time(NULL);
		if (localtime_r(&aTime, &timeTm) != NULL)
		{
			char dateStr[128];

			// Generate a RFC [2]822 compliant date
#if defined(__GNU_LIBRARY__)
			// %z is a GNU extension
			if (strftime(dateStr, 128, "%a, %d %b %Y %H:%M:%S %z", &timeTm) > 0)
#else
			if (strftime(dateStr, 128, "%a, %d %b %Y %H:%M:%S %Z", &timeTm) > 0)
#endif
			{
				appendHeader("Date", dateStr, "");
			}
		}
	}

	SMTPMessage::buildHeaders();
}

bool LibESMTPMessage::addHeader(const string &header,
	const string &value, const string &path)
{
	stringstream headerStr;

	if (SMTPMessage::addHeader(header, value, path) == false)
	{
		return false;
	}

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

	headerStr << header << ":";
	if (value.empty() == false)
	{
		headerStr << " " << value;
	}
	if (path.empty() == false)
	{
		headerStr << " <" << path << ">";
	}
	headerStr << "\r\n";

#ifdef DEBUG
	clog << "LibESMTPMessage::addHeader: " << headerStr.str();
#endif
	m_headersDump.append(headerStr.str());

	return true;
}

string LibESMTPMessage::getUserAgent(void) const
{
	if ((m_pDetails != NULL) &&
		(m_pDetails->m_userAgent.empty() == false))
	{
		return m_pDetails->m_userAgent;
	}

	return PACKAGE_NAME"/esmtp "PACKAGE_VERSION;
}

bool LibESMTPMessage::setSignatureHeader(const string &header,
	const string &value)
{
	if (SMTPMessage::setSignatureHeader(header, value) == false)
	{
		return false;
	}

	// Prepend the signature
	m_headersDump.insert(0, m_signatureHeader);

	return true;
}

void LibESMTPMessage::setEnvId(const string &dsnEnvId)
{
	if ((m_message != NULL) &&
		(dsnEnvId.empty() == false) &&
		(m_dsnFlags != SMTPMessage::NEVER))
	{
		smtp_dsn_set_envid(m_message, dsnEnvId.c_str());
#ifdef DEBUG
		clog << "LibESMTPMessage::setEnvId: envid " << dsnEnvId << endl;
#endif
	}
}

void LibESMTPMessage::setReversePath(const string &reversePath)
{
	if ((m_message != NULL) &&
		(reversePath.empty() == false))
	{
		smtp_set_reverse_path(m_message, reversePath.c_str());
	}
}

void LibESMTPMessage::addRecipient(const string &emailAddress)
{
	if (m_message == NULL)
	{
		return;
	}

	smtp_recipient_t recipient = smtp_add_recipient(m_message, emailAddress.c_str());

	// Activate Delivery Status Notification ?
	if (m_dsnFlags == SMTPMessage::NEVER)
	{
		smtp_dsn_set_notify(recipient, Notify_NEVER);
	}
	else if (m_dsnFlags == SMTPMessage::SUCCESS)
	{
		smtp_dsn_set_notify(recipient, Notify_SUCCESS);
	}
	else if (m_dsnFlags == SMTPMessage::FAILURE)
	{
		smtp_dsn_set_notify(recipient, Notify_FAILURE);
	}
}

string LibESMTPMessage::getHeadersDump(void) const
{
	return m_headersDump;
}

string LibESMTPMessage::getAttachmentHeaders(unsigned int attachmentNum,
	bool inlineParts)
{
	Attachment *pAttachment = m_pDetails->getAttachment(m_currentAttachment, inlineParts);
	string fileName(pAttachment->getFileName());
	string headers("Content-Type: ");

	headers += pAttachment->m_contentType;
	headers += ";\r\n\tname=\"";
	headers += fileName;
	headers += "\"\r\nContent-Transfer-Encoding: base64\r\n";
	if (pAttachment->m_contentId.empty() == false)
	{
		headers += "Content-ID: <";
		headers += pAttachment->m_contentId;
		headers += ">\r\nContent-Disposition: inline";
	}
	else
	{
		headers += "Content-Disposition: attachment";
	}
	headers += ";\r\n\tfilename=\"";
	headers += fileName;
	headers += "\"\r\n\r\n";

	return headers;
}

const char *LibESMTPMessage::getAttachmentLine(off_t &lineLen,
	bool inlineParts)
{
	Attachment *pAttachment = m_pDetails->getAttachment(m_currentAttachment, inlineParts);

	m_hasMoreLines = false;

	if ((pAttachment == NULL) ||
		(pAttachment->m_pContent == NULL) ||
		(pAttachment->m_contentLength == 0))
	{
		return NULL;
	}

	// Encode the content, if necessary
	if (pAttachment->m_pEncodedContent == NULL)
	{
		unsigned long encodedLen = (unsigned long)pAttachment->m_contentLength;

		pAttachment->m_pEncodedContent = Base64::encode(pAttachment->m_pContent, encodedLen);
		if (pAttachment->m_pEncodedContent == NULL)
		{
			clog << "Couldn't encode contents of " << pAttachment->m_filePath << endl;
			pAttachment->m_encodedLength = 0;
		}
		else
		{
			pAttachment->m_encodedLength = (off_t)encodedLen;
		}
	}

	if ((pAttachment->m_pEncodedContent != NULL) &&
		(pAttachment->m_encodedLength > 0) &&
		(m_encodedPos < pAttachment->m_encodedLength))
	{
		const char *pLine = pAttachment->m_pEncodedContent + m_encodedPos;

		lineLen = min((off_t)76, pAttachment->m_encodedLength - m_encodedPos);

		// Move forward
		m_encodedPos += lineLen;
		// Any more line after this ?
		if (m_encodedPos < pAttachment->m_encodedLength)
		{
			m_hasMoreLines = true;
		}

		return pLine;
	}

	return NULL;
}

bool LibESMTPMessage::hasMoreAttachmentLines(void) const
{
	return m_hasMoreLines;
}

LibESMTPProvider::LibESMTPProvider() :
	SMTPProvider(),
	m_authContext(NULL),
	m_session(NULL)
{
}

LibESMTPProvider::~LibESMTPProvider()
{
}

string LibESMTPProvider::getMessageData(SMTPMessage *pMsg)
{
	void *ppBuf = NULL;
	int length = 0;
	const char *pBuf = NULL;
	string fullMessage;

	if (pMsg == NULL)
	{
		return "";
	}

	// Rewind
	messageDataCallback(&ppBuf, NULL, pMsg);
	do
	{
		// Get data
		pBuf = messageDataCallback(&ppBuf, &length, pMsg);
		if (pBuf != NULL)
		{
			fullMessage += pBuf;
		}
	} while (pBuf != NULL);
#ifdef DEBUG
	clog << "LibESMTPProvider::getMessageData: message is " << fullMessage << endl;
#endif

	return fullMessage;
}

bool LibESMTPProvider::createSession(SMTPSession *pSession)
{
	m_session = smtp_create_session();
	if (m_session == NULL)
	{
		return false;
	}

	smtp_set_eventcb(m_session, eventCallback, pSession);

	return true;
}

bool LibESMTPProvider::hasSession(void)
{
	if (m_session == NULL)
	{
		return false;
	}

	return true;
}

void LibESMTPProvider::enableAuthentication(const string &authRealm,
	const string &authUserName,
	const string &authPassword)
{
	SMTPProvider::enableAuthentication(authRealm,
		authUserName, authPassword);

	if ((m_authRealm.empty() == true) ||
		(m_authUserName.empty() == true))
	{
		return;
	}

	auth_client_init();

	// Create a (plain only) authentication context 
	m_authContext = auth_create_context();
	if (m_authContext == NULL)
	{
		return;
	}

	auth_set_mechanism_flags(m_authContext, AUTH_PLUGIN_PLAIN, 0);
	// FIXME: the documentation says this isn't thread-safe
	auth_set_interact_cb(m_authContext, authenticationCallback, this);

	if (m_session != NULL)
	{
		smtp_auth_set_context(m_session, m_authContext);
	}
}

void LibESMTPProvider::enableStartTLS(void)
{
	// FIXME: implement this
}

void LibESMTPProvider::destroySession(void)
{
	if (m_session != NULL)
	{
		smtp_destroy_session(m_session);
		m_session = NULL;
	}

	// Is relay authentication enabled ?
	if (m_authContext != NULL)
	{
		auth_destroy_context(m_authContext);
		m_authContext = NULL;
		auth_client_exit();
	}
}

bool LibESMTPProvider::setServer(const string &hostName,
	unsigned int port)
{
	stringstream hostNameAndPortStr;

	hostNameAndPortStr << hostName;
	hostNameAndPortStr << ":";
	hostNameAndPortStr << port;

	if ((m_session == NULL) ||
		(hostName.empty() == true) ||
		(smtp_set_server(m_session, hostNameAndPortStr.str().c_str()) == 0))
	{
		return false;
	}

	return true;
}

SMTPMessage *LibESMTPProvider::newMessage(const map<string, string> &fieldValues,
	MessageDetails *pDetails, SMTPMessage::DSNNotification dsnFlags,
	bool enableMdn,
	const string &msgIdSuffix,
	const string &complaints)
{
	return new LibESMTPMessage(fieldValues, pDetails,
		dsnFlags, enableMdn, msgIdSuffix, complaints);
}

void LibESMTPProvider::queueMessage(SMTPMessage *pMsg,
	const string &defaultReturnPath)
{
	if ((m_session == NULL) ||
		(pMsg == NULL))
	{
		return;
	}

	LibESMTPMessage *pESMTPMsg = dynamic_cast<LibESMTPMessage*>(pMsg);
	if (pESMTPMsg == NULL)
	{
		return;
	}

	pESMTPMsg->m_message = smtp_add_message(m_session);

	smtp_set_messagecb(pESMTPMsg->m_message, messageDataCallback, pMsg);

	// Enable Message Disposition Notification ?
	if (pMsg->m_enableMdn == true)
	{
		// Request MDN to be sent to the same address as the reverse path
		smtp_set_header(pESMTPMsg->m_message, "Disposition-Notification-To", NULL, NULL);
	}
	if (defaultReturnPath.empty() == false)
	{
		smtp_set_reverse_path(pESMTPMsg->m_message, defaultReturnPath.c_str());
#ifdef DEBUG
		clog << "LibESMTPProvider::queueMessage: set default Return-Path to "
			<< defaultReturnPath << endl;
#endif
	}
	// Is Delivery Status Notification required ?
	if (pMsg->m_dsnFlags != SMTPMessage::NEVER)
	{
		// Instruct the reporting MTA to include headers
		smtp_dsn_set_ret(pESMTPMsg->m_message, Ret_HDRS);
	}
}

bool LibESMTPProvider::startSession(bool reset)
{
	if (m_session == NULL)
	{
		return false;
	}

	if (reset == true)
	{
		smtp_enumerate_messages(m_session, resetMessagesCallback, NULL);
	}

	if (!smtp_start_session(m_session))
	{
		return false;
	}

	return true;
}

void LibESMTPProvider::updateRecipientsStatus(StatusUpdater *pUpdater)
{
	if ((m_session != NULL) &&
		(pUpdater != NULL))
	{
		// Check all messages for this session
		smtp_enumerate_messages(m_session, messageStatusCallback, pUpdater);
	}
}

int LibESMTPProvider::getCurrentError(void)
{
	return smtp_errno();
}

string LibESMTPProvider::getErrorMessage(int errorNum)
{
	char errBuf[512];

	char *pErrorString = smtp_strerror(errorNum, errBuf, 512);
	if (pErrorString != NULL)
	{
		return pErrorString;
	}

	return "";
}

bool LibESMTPProvider::isInternalError(int errNum)
{
	if (errNum == SMTP_ERR_INVAL)
	{
		return true;
	}

	return false;
}

bool LibESMTPProvider::isConnectionError(int errNum)
{
	if ((errNum == SMTP_ERR_DROPPED_CONNECTION) ||
		(errNum == SMTP_ERR_EAI_NONAME))
	{
		return true;
	}

	return false;
}

