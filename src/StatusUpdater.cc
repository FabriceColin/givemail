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

#include <utility>
#include <iostream>

#include "StatusUpdater.h"

using std::clog;
using std::endl;
using std::ofstream;
using std::ios;
using std::string;
using std::map;
using std::pair;

StatusUpdater::StatusUpdater(const string &statusFileName)
{
	if (statusFileName.empty() == false)
	{
		m_statusFile.open(statusFileName.c_str(), ios::out|ios::app);
	}
}

StatusUpdater::~StatusUpdater()
{
	if (m_statusFile.is_open() == true)
	{
		m_statusFile.close();
	}
}

void StatusUpdater::updateRecipientsStatus(const string &domainName,
	int statusCode, const char *pText)
{
	clog << "SMTP status for " << domainName << ": " << statusCode
		<< " (" << ((pText != NULL) ? pText : "") << ")" << endl;

	m_status.insert(pair<string, int>(domainName, statusCode));
	if (m_statusFile.is_open() == true)
	{
		m_statusFile << statusCode << "," << domainName << endl;
	}
}

void StatusUpdater::updateRecipientStatus(const string &emailAddress,
	int statusCode, const char *pText,
	const string &msgId)
{
	clog << "SMTP status for " << emailAddress << ": " << statusCode
		<< " (" << ((pText != NULL) ? pText : "") << ")" << endl;

	m_status.insert(pair<string, int>(emailAddress, statusCode));
	if (m_statusFile.is_open() == true)
	{
		m_statusFile << statusCode << "," << emailAddress << "," << msgId << endl;
	}
}

const map<string, int> &StatusUpdater::getStatus(void) const
{
	return m_status;
}

void StatusUpdater::clear(void)
{
	m_status.clear();
}

