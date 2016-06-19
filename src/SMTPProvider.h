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

#ifndef _SMTPPROVIDER_H_
#define _SMTPPROVIDER_H_

#include <string>
#include <map>

#include "MessageDetails.h"
#include "StatusUpdater.h"

class SMTPSession;

/// Interface for SMTP messages.
class SMTPMessage
{
	public:
		typedef enum { NEVER = 0, SUCCESS, FAILURE } DSNNotification;

		SMTPMessage(const std::map<std::string, std::string> &fieldValues,
			MessageDetails *pDetails, DSNNotification dsnFlags,
			bool enableMdn = false,
			const std::string &msgIdSuffix = "",
			const std::string &complaints = "");
		virtual ~SMTPMessage();

		std::map<std::string, std::string> m_fieldValues;
		std::string m_smtpFrom;
		std::string m_headersList;
		std::string m_plainContent;
		std::string m_htmlContent;
		MessageDetails *m_pDetails;
		DSNNotification m_dsnFlags;
		bool m_enableMdn;
		std::string m_msgIdSuffix;
		std::string m_complaints;
		std::string m_msgId;

		/// Returns true if this is a delivery receipt.
		bool isDeliveryReceipt(void) const;

		/// Returns true if this is a read notification.
		bool isReadNotification(void) const;

		/// Returns true if mixed parts are necessary.
		bool requiresMixedParts(void) const;

		/// Returns true if alternative parts are necessary.
		bool requiresAlternativeParts(void) const;

		/// Returns true if related parts are necessary.
		bool requiresRelatedParts(void) const;

		/// Returns the From email address.
		std::string getFromEmailAddress(void) const;

		/// Returns the Sender email address.
		std::string getSenderEmailAddress(void) const;

		/// Returns the User-Agent string.
		virtual std::string getUserAgent(void) const;

		/// Adds the signature header.
		virtual bool addSignatureHeader(const std::string &header,
			const std::string &value) = 0;

		/// Adds a header.
		virtual bool addHeader(const std::string &header,
			const std::string &value, const std::string &path) = 0;

		/// Sets the DSN Env Id.
		virtual void setEnvId(const std::string &dsnEnvId) = 0;

		/// Sets the reverse path.
		virtual void setReversePath(const std::string &reversePath) = 0;

		/// Adds a recipient.
		virtual void addRecipient(const std::string &emailAddress) = 0;

		/// Dumps plain and HTML parts to file.
		void dumpToFile(const std::string &fileBaseName) const;

	protected:
		void substitute(const std::map<std::string, std::string> &fieldValues);

		virtual void buildHeaders(void);

		virtual void appendHeader(const std::string &header,
			const std::string &value, const std::string &path) = 0;

	private:
		SMTPMessage(const SMTPMessage &other);
		SMTPMessage &operator=(const SMTPMessage &other);

};

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

		virtual void queueMessage(SMTPMessage *pMsg,
			const std::string &defaultReturnPath) = 0;

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
