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

#ifndef _LIBESMTPPROVIDER_H_
#define _LIBESMTPPROVIDER_H_

#include <auth-client.h>
#include <libesmtp.h>

#include "SMTPProvider.h"

/// A libesmtp message.
class LibESMTPMessage : public SMTPMessage
{
	public:
		LibESMTPMessage(const std::map<std::string, std::string> &fieldValues,
			MessageDetails *pDetails, DSNNotification dsnFlags,
			bool enableMdn = false,
			const std::string &msgIdSuffix = "",
			const std::string &complaints = "");
		virtual ~LibESMTPMessage();

		typedef enum { EXTRA_HEADERS = 0, START_MIXED, START_ALT,
			START_PLAIN, PLAIN, END_PLAIN,
			ALT_TRANSITION, START_RELATED,
			START_HTML, HTML, END_HTML,
			START_INLINE, INLINE_HEADERS, INLINE_LINE, INLINE_LINE_END, END_INLINE, NEXT_INLINE,
			END_RELATED, END_ALT,
			START_ATTACHMENT, ATTACHMENT_HEADERS, ATTACHMENT_LINE, ATTACHMENT_LINE_END, END_ATTACHMENT, NEXT_ATTACHMENT,
			END_MIXED, END } Stage;

		Stage m_stage;
		unsigned int m_currentAttachment;
		off_t m_encodedPos;
		std::string m_headersDump;
		smtp_message_t m_message;

		/// Returns the User-Agent string.
		virtual std::string getUserAgent(void) const;

		/// Sets the signature header.
		virtual bool setSignatureHeader(const std::string &header,
			const std::string &value);

		/// Adds a header.
		virtual bool addHeader(const std::string &header,
			const std::string &value, const std::string &path);

		/// Sets the DSN Env Id.
		virtual void setEnvId(const std::string &dsnEnvId);

		/// Adds a recipient.
		virtual void addRecipient(const std::string &emailAddress);

		/// Gets headers for an attachment.
		std::string getAttachmentHeaders(unsigned int attachmentNum,
			bool inlineParts = false);

		/**
		  * Returns a pointer to the current attachment,
		  * one line (up to 76 bytes long) at a time.
		  */
		const char *getAttachmentLine(off_t &lineLen,
			bool inlineParts = false);

		/// Returns whether the current attachment has more lines.
		bool hasMoreAttachmentLines(void) const;

	protected:
		bool m_hasMoreLines;

		virtual void buildHeaders(void);

	private:
		LibESMTPMessage(const LibESMTPMessage &other);
		LibESMTPMessage &operator=(const LibESMTPMessage &other);

};

/// A SMTP provider based on libesmtp.
class LibESMTPProvider : public SMTPProvider
{
	public:
		LibESMTPProvider();
		virtual ~LibESMTPProvider();

		virtual std::string getMessageData(SMTPMessage *pMsg);

		virtual bool createSession(SMTPSession *pSession);

		virtual bool hasSession(void);

		virtual void enableAuthentication(const std::string &authRealm,
			const std::string &authUserName,
			const std::string &authPassword);

		virtual void enableStartTLS(void);

		virtual void destroySession(void);

		virtual bool setServer(const std::string &hostName,
			unsigned int port);

		virtual SMTPMessage *newMessage(const std::map<std::string, std::string> &fieldValues,
			MessageDetails *pDetails, SMTPMessage::DSNNotification dsnFlags,
			bool enableMdn = false,
			const std::string &msgIdSuffix = "",
			const std::string &complaints = "");

		virtual void queueMessage(SMTPMessage *pMsg);

		virtual bool startSession(bool reset);

		virtual void updateRecipientsStatus(StatusUpdater *pUpdater);

		virtual int getCurrentError(void);

		virtual std::string getErrorMessage(int errorNum);

		virtual bool isInternalError(int errNum);

		virtual bool isConnectionError(int errNum);

	protected:
		auth_context_t m_authContext;
		smtp_session_t m_session;

	private:
		LibESMTPProvider(const LibESMTPProvider &other);
		LibESMTPProvider &operator=(const LibESMTPProvider &other);

};

#endif // _LIBESMTPPROVIDER_H_
