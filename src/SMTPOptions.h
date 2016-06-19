/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 *  Copyright 2008 Global Sign In
 *  Copyright 2009-2011 Fabrice Colin
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

#ifndef _SMTPOPTIONS_H_
#define _SMTPOPTIONS_H_

#include <string>

/// SMTP session options.
class SMTPOptions
{
	public:
		SMTPOptions();
		SMTPOptions(const SMTPOptions &other);
		~SMTPOptions();

		SMTPOptions &operator=(const SMTPOptions &other);

		std::string m_msgIdSuffix;
		std::string m_complaints;
		std::string m_dsnNotify;
		std::string m_mailRelayAddress;
		int m_mailRelayPort;
		std::string m_internalDomain;
		std::string m_mailRelayUserName;
		std::string m_mailRelayPassword;
		bool m_mailRelayTLS;
		std::string m_dumpFileBaseName;

};

#endif // _SMTPOPTIONS_H_
