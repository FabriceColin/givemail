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

#ifndef _DOMAINAUTH_H_
#define _DOMAINAUTH_H_

#include <string>

#include "ConfigurationFile.h"
#include "SMTPMessage.h"

// DomainAuth support.
class DomainAuth
{
	public:
		DomainAuth();
		virtual ~DomainAuth();

		/// Loads the private key for the given domain name.
		virtual bool loadPrivateKey(ConfigurationFile *pConfig) = 0;

		/// Returns true if messages can be signed.
		virtual bool canSign(void) const = 0;

		/**
		  * Signs message data, appends a signature header.
		  */
		virtual bool sign(const std::string &messageData,
			SMTPMessage *pMsg, bool simpleCanon) = 0;

		/// Verifies the message data against the included signature.
		virtual bool verify(const std::string &messageData,
			SMTPMessage *pMsg) = 0;

	protected:
		char *m_pPrivateKey;
		std::string m_domainName;

	private:
		// DomainAuth objects cannot be copied.
		DomainAuth(const DomainAuth &other);
                DomainAuth &operator=(const DomainAuth &other);

};

#endif // _DOMAINAUTH_H_
