/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
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

#ifndef _LIBETPANPROVIDER_H_
#define _LIBETPANPROVIDER_H_

#include <time.h>
#include <libetpan/libetpan.h>
#include <string>
#include <map>
#include <vector>

#include "SMTPProvider.h"

/// Wraps a libetpan message.
class LibETPANMessage : public SMTPMessage
{
	public:
		LibETPANMessage(const std::map<std::string, std::string> &fieldValues,
			MessageDetails *pDetails, DSNNotification dsnFlags,
			bool enableMdn = false,
			const std::string &msgIdSuffix = "",
			const std::string &complaints = "");
		virtual ~LibETPANMessage();

		time_t m_date;
		std::string m_defaultReturnPath;
		std::string m_dsnEnvId;
		std::string m_reversePath;
		std::map<std::string, int> m_recipients;
		MMAPString *m_pString;
		std::string m_response;
		bool m_sent;

		/// Returns the User-Agent string.
		virtual std::string getUserAgent(void) const;

		/// Adds the signature header.
		virtual bool addSignatureHeader(const std::string &header,
			const std::string &value);

		/// Sets the DSN Env Id.
		virtual void setEnvId(const std::string &dsnEnvId);

		/// Sets the reverse path.
		virtual void setReversePath(const std::string &reversePath);

		/// Adds a recipient.
		virtual void addRecipient(const std::string &emailAddress);

		/// Builds the message string.
		bool buildMessage(void);

		/// Gets the DSN Env Id.
		const char *getEnvId(void);

	protected:
		bool m_relatedFirst;

		struct mailimf_fields *headersToFields(void);

		bool serialize(size_t messageSizeEstimate,
			struct mailmime *pMessage);

	private:
		LibETPANMessage(const LibETPANMessage &other);
		LibETPANMessage &operator=(const LibETPANMessage &other);

};

/// A SMTP provider based on libetpan.
class LibETPANProvider : public SMTPProvider
{
	public:
		LibETPANProvider();
		virtual ~LibETPANProvider();

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

		virtual void queueMessage(SMTPMessage *pMsg,
			const std::string &defaultReturnPath);

		virtual bool startSession(bool reset);

		virtual void updateRecipientsStatus(StatusUpdater *pUpdater);

		virtual int getCurrentError(void);

		virtual std::string getErrorMessage(int errNum);

		virtual bool isInternalError(int errNum);

		virtual bool isConnectionError(int errNum);

	protected:
		mailsmtp *m_session;
		std::string m_hostName;
		unsigned int m_port;
		bool m_authenticate;
		bool m_startTLS;
		std::vector<LibETPANMessage*> m_messages;
		int m_error;

		int authenticate(void);

	private:
		LibETPANProvider(const LibETPANProvider &other);
		LibETPANProvider &operator=(const LibETPANProvider &other);

};

#endif // _LIBETPANPROVIDER_H_
