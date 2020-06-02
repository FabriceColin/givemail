/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 *  Copyright 2008 Global Sign In
 *  Copyright 2009 Fabrice Colin
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

#ifndef _DOMAINKEYS_H_
#define _DOMAINKEYS_H_

extern "C"
{
#include <domainkeys.h>
}
#include <string>

#include "ConfigurationFile.h"
#include "DomainAuth.h"
#include "SMTPMessage.h"

// DomainKeys support.
class DomainKeys : public DomainAuth
{
	public:
		DomainKeys();
		virtual ~DomainKeys();

		/// Initializes DomainKeys.
		static bool initialize(void);

		/// Shuts DomainKeys down.
		static void shutdown(void);

		/// Cleans up the current thread's state.
		static void cleanupThread(void);

		/// Loads the private key for the given domain name.
		virtual bool loadPrivateKey(ConfigurationFile *pConfig);

		/// Returns true if messages can be signed.
		virtual bool canSign(void) const;

		/**
		  * Signs message data, appends a signature header.
		  */
		virtual bool sign(const std::string &messageData,
			SMTPMessage *pMsg, bool simpleCanon);

		/// Verifies the message data against the included signature.
		virtual bool verify(const std::string &messageData,
			SMTPMessage *pMsg);

	protected:
		static DK_LIB *m_pLib;
		DK *m_pDkObj;

	private:
		// DomainKeys objects cannot be copied.
		DomainKeys(const DomainKeys &other);
                DomainKeys &operator=(const DomainKeys &other);

};

#endif // _DOMAINKEYS_H_
