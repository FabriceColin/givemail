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

#include "SMTPOptions.h"

using std::string;

SMTPOptions::SMTPOptions() :
	m_dsnNotify("NEVER"),
	m_mailRelayPort(25),
	m_mailRelayTLS(false)
{
}

SMTPOptions::SMTPOptions(const SMTPOptions &other) :
	m_msgIdSuffix(other.m_msgIdSuffix),
	m_complaints(other.m_complaints),
	m_dsnNotify(other.m_dsnNotify),
	m_mailRelayAddress(other.m_mailRelayAddress),
	m_mailRelayPort(other.m_mailRelayPort),
	m_internalDomain(other.m_internalDomain),
	m_mailRelayUserName(other.m_mailRelayUserName),
	m_mailRelayPassword(other.m_mailRelayPassword),
	m_mailRelayTLS(other.m_mailRelayTLS),
	m_dumpFileBaseName(other.m_dumpFileBaseName)
{
}

SMTPOptions::~SMTPOptions()
{
}

SMTPOptions &SMTPOptions::operator=(const SMTPOptions &other)
{
	m_msgIdSuffix = other.m_msgIdSuffix;
	m_complaints = other.m_complaints;
	m_dsnNotify = other.m_dsnNotify;
	m_mailRelayAddress = other.m_mailRelayAddress;
	m_mailRelayPort = other.m_mailRelayPort;
	m_internalDomain = other.m_internalDomain;
	m_mailRelayUserName = other.m_mailRelayUserName;
	m_mailRelayPassword = other.m_mailRelayPassword;
	m_mailRelayTLS = other.m_mailRelayTLS;
	m_dumpFileBaseName = other.m_dumpFileBaseName;

	return *this;
}

