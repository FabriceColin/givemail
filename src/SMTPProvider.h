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

#ifndef _SMTPPROVIDER_H_
#define _SMTPPROVIDER_H_

#include <string>
#include <map>

#include "MessageDetails.h"
#include "StatusUpdater.h"
#include "SMTPMessage.h"

class SMTPSession;

/// Interface for SMTP providers.
class SMTPProvider
{
	public:
		SMTPProvider();
		virtual ~SMTPProvider();

		virtual std::string getMessageData(SMTPMessage *pMsg) = 0;

		virtual bool createSession(SMTPSession *pSession) = 0;

		virtual bool hasSession(void) = 0;

		virtual void enableAuthentication(const std::string &authRealm,
			const std::string &authUserName,
			const std::string &authPassword);

		virtual void enableStartTLS(void) = 0;

		std::string getAuthUserName(void) const;

		std::string getAuthPassword(void) const;

		virtual void destroySession(void) = 0;

		virtual bool setServer(const std::string &hostName,
			unsigned int port) = 0;

		virtual SMTPMessage *newMessage(const std::map<std::string, std::string> &fieldValues,
			MessageDetails *pDetails, SMTPMessage::DSNNotification dsnFlags,
			bool enableMdn = false,
			const std::string &msgIdSuffix = "",
			const std::string &complaints = "") = 0;

		virtual void queueMessage(SMTPMessage *pMsg) = 0;

		virtual bool startSession(bool reset) = 0;

		virtual void updateRecipientsStatus(StatusUpdater *pUpdater) = 0;

		virtual int getCurrentError(void) = 0;

		virtual std::string getErrorMessage(int errNum) = 0;

		virtual bool isInternalError(int errNum) = 0;

		virtual bool isConnectionError(int errNum) = 0;

	protected:
		std::string m_authRealm;
		std::string m_authUserName;
		std::string m_authPassword;

	private:
		SMTPProvider(const SMTPProvider &other);
		SMTPProvider &operator=(const SMTPProvider &other);

};

/// Provider factory.
class SMTPProviderFactory
{
	public:
		static SMTPProvider *getProvider(void);

	protected:
		SMTPProviderFactory() { }
		~SMTPProviderFactory() { }

	private:
		SMTPProviderFactory(const SMTPProviderFactory &other);
		SMTPProviderFactory &operator=(const SMTPProviderFactory &other);

};

#endif // _SMTPPROVIDER_H_
