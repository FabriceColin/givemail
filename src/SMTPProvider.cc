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

#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <strings.h>
#include <iostream>

#include "config.h"
#ifdef USE_LIBETPAN
#include "LibETPANProvider.h"
#endif
#ifdef USE_LIBESMTP
#include "LibESMTPProvider.h"
#endif
#include "SMTPProvider.h"

using std::clog;
using std::endl;
using std::string;

SMTPProvider::SMTPProvider()
{
}

SMTPProvider::~SMTPProvider()
{
}

void SMTPProvider::enableAuthentication(const string &authRealm,
	const string &authUserName,
	const string &authPassword)
{
	m_authRealm = authRealm;
	m_authUserName = authUserName;
	m_authPassword = authPassword;
}

string SMTPProvider::getAuthUserName(void) const
{
	return m_authUserName;
}

string SMTPProvider::getAuthPassword(void) const
{
	return m_authPassword;
}

SMTPProvider *SMTPProviderFactory::getProvider(void)
{
#ifdef USE_LIBETPAN
	return new LibETPANProvider();
#else
#ifdef USE_LIBESMTP
	return new LibESMTPProvider();
#endif
#endif
	return NULL;
}

