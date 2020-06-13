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

#ifndef _SMTPMESSAGE_H_
#define _SMTPMESSAGE_H_

#include <string>
#include <map>
#include <vector>

#include "MessageDetails.h"

/// Header of a SMTP message
class SMTPHeader
{
	public:
		SMTPHeader(const std::string &name, const std::string &value, const std::string &path);
		SMTPHeader(const SMTPHeader &other);
		virtual ~SMTPHeader();

		std::string m_name;
		std::string m_value;
		std::string m_path;

		SMTPHeader &operator=(const SMTPHeader &other);

};

/// Interface for SMTP messages
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
		std::string m_allHeaders;
		std::vector<SMTPHeader> m_headers;
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

		/// Sets the signature header.
		virtual bool setSignatureHeader(const std::string &header,
			const std::string &value);

		/// Gets the signature header.
		virtual std::string getSignatureHeader(void) const;

		/// Adds a header, which will be signed.
		virtual bool addHeader(const std::string &header,
			const std::string &value, const std::string &path);

		/// Sets the DSN Env Id.
		virtual void setEnvId(const std::string &dsnEnvId) = 0;

		/// Sets the reverse path.
		void setReversePath(const std::string &reversePath);

		/// Gets the reverse path.
		std::string getReversePath(void) const;

		/// Adds a recipient.
		virtual void addRecipient(const std::string &emailAddress) = 0;

		/// Dumps plain and HTML parts to file.
		void dumpToFile(const std::string &fileBaseName) const;

	protected:
		std::string m_reversePath;
		std::string m_signatureHeader;

		void substitute(const std::map<std::string, std::string> &fieldValues);

		virtual void buildHeaders(void);

		virtual void appendHeader(const std::string &header,
			const std::string &value, const std::string &path);

	private:
		SMTPMessage(const SMTPMessage &other);
		SMTPMessage &operator=(const SMTPMessage &other);

};

#endif // _SMTPMESSAGE_H_
