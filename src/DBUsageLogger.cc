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

#include <stdio.h>
#include <stdlib.h>

#include "DBUsageLogger.h"

using std::string;

DBUsageLogger::DBUsageLogger(SQLDB *pDb,
	const string &remoteAddress, unsigned int remotePort) :
	UsageLogger(),
	m_pDb(pDb),
	m_remoteAddress(remoteAddress),
	m_remotePort(remotePort),
	m_currentTime(time(NULL))
{
}

DBUsageLogger::~DBUsageLogger()
{
}

time_t DBUsageLogger::findLastRequestTime(void)
{
	if ((m_keyId.empty() == true) ||
		(m_hash.empty() == true))
	{
		return 0;
	}

	SQLResults *pRequestResults = m_pDb->executeStatement("SELECT "
		"TimeStamp FROM WebAPIUsage WHERE KeyID='%s' AND Hash='%s' "
		"ORDER BY TimeStamp DESC;",
		m_pDb->escapeString(m_keyId).c_str(), m_pDb->escapeString(m_hash).c_str());
	if (pRequestResults == NULL)
	{
		return 0;
	}

	// We are only interested in the top row
	SQLRow *pRequestRow = pRequestResults->nextRow();
	if (pRequestRow == NULL)
	{
		delete pRequestResults;
		return 0;
	}

	time_t lastRequestTime = (time_t)atoi(pRequestRow->getColumn(0).c_str());

	delete pRequestRow;
	delete pRequestResults;

	return lastRequestTime;
}

void DBUsageLogger::logRequest(const string &callName, int status)
{
	char numStr[64];

	if (m_pDb == NULL)
	{
		return;
	}

	string insertSql("INSERT INTO WebAPIUsage (UsageID, TimeStamp, Status, KeyID, "
			"Hash, CallName, RemoteAddress, RemotePort) VALUES('");
	insertSql += m_pDb->escapeString(m_pDb->getUniversalUniqueId());
	insertSql += "', '";
	snprintf(numStr, 64, "%ld", (long)m_currentTime);
	insertSql += numStr;
	insertSql += "', '";
	snprintf(numStr, 64, "%d", status);
	insertSql += numStr;
	insertSql += "', '";
	insertSql += m_pDb->escapeString(m_keyId);
	insertSql += "', '";
	insertSql += m_pDb->escapeString(m_hash);
	insertSql += "', '";
	insertSql += m_pDb->escapeString(callName);
	insertSql += "', '";
	insertSql += m_pDb->escapeString(m_remoteAddress);
	insertSql += "', '";
	snprintf(numStr, 64, "%u", m_remotePort);
	insertSql += numStr;
	insertSql += "');";

	m_pDb->executeSimpleStatement(insertSql);
}

void DBUsageLogger::setKeyId(const string &keyId)
{
	m_keyId = keyId;
}

void DBUsageLogger::setHash(const string &hash)
{
	m_hash = hash;
}

