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

#include "DomainAuth.h"
#include "MessageDetails.h"

DomainAuth::DomainAuth() :
	m_pPrivateKey(NULL)
{
}

DomainAuth::~DomainAuth()
{
	if (m_pPrivateKey != NULL)
	{
		delete[] m_pPrivateKey;
	}
}

bool DomainAuth::loadPrivateKey(ConfigurationFile *pConfig)
{
	if ((pConfig == NULL) ||
		(pConfig->m_dkPrivateKey.empty() == true) ||
		(pConfig->m_dkDomain.empty() == true))
	{
		return false;
	}

	// Free any previously loaded key
	if (m_pPrivateKey != NULL)
	{
		delete[] m_pPrivateKey;
		m_pPrivateKey = NULL;
	}

	// Load the private key
	off_t keySize = 0;
	m_pPrivateKey = MessageDetails::loadRaw(pConfig->m_dkPrivateKey, keySize);
	if (m_pPrivateKey == NULL)
	{
		return false;
	}

	m_domainName = pConfig->m_dkDomain;

	return true;
}

