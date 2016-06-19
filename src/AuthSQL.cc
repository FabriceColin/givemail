/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 *  Copyright 2008 Global Sign In
 *  Copyright 2010-2014 Fabrice Colin
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

#include <stdlib.h>

#include "AuthSQL.h"
#include "ConfigurationFile.h"
#include "StatusUpdater.h"

using std::string;

AuthSQL::AuthSQL(SQLDB *pDb) :
	m_pDb(pDb)
{
}

AuthSQL::~AuthSQL()
{
}

bool AuthSQL::createNewKey(Key &key)
{
	char numStr[64];

	if ((m_pDb == NULL) ||
		(key.m_appId.empty() == true))
	{
		return false;
	}

	key.m_dbId = m_pDb->getUniversalUniqueId();

	string insertSql("INSERT INTO WebAPIKeys (KeyID, ApplicationKeyID, KeyValue, "
		"CreationDate, ExpiryDate) VALUES('");
	insertSql += key.m_dbId;
	insertSql += "', '";
	insertSql += key.m_appId;
	insertSql += "', '";
	insertSql += m_pDb->escapeString(key.m_value);
	insertSql += "', '";
	snprintf(numStr, 64, "%ld", (long)key.m_creationTime);
	insertSql += numStr;
	insertSql += "', '";
	snprintf(numStr, 64, "%ld", (long)key.m_expiryTime);
	insertSql += numStr;
	insertSql += "');";

	if (m_pDb->executeSimpleStatement(insertSql) == true)
	{
		return true;
	}

	return false;
}

Key *AuthSQL::getKey(const string &keyAppId)
{
	if ((m_pDb == NULL) ||
		(keyAppId.empty() == true))
	{
		return NULL;
	}

	SQLResults *pKeyResults = m_pDb->executeStatement("SELECT "
		"KeyID, KeyValue, CreationDate, ExpiryDate FROM WebAPIKeys "
		"WHERE ApplicationKeyID='%s';",
		keyAppId.c_str());
	if (pKeyResults == NULL)
	{
		return NULL;
	}

	// There should only be one row matching keyAppId
	SQLRow *pKeyRow = pKeyResults->nextRow();
	if (pKeyRow == NULL)
	{
		delete pKeyResults;
		return NULL;
	}

	Key *pKey = new Key(pKeyRow->getColumn(0),
		keyAppId,
		pKeyRow->getColumn(1),
		(time_t)atoi(pKeyRow->getColumn(2).c_str()),
		(time_t)atoi(pKeyRow->getColumn(3).c_str()));

	delete pKeyRow;
	delete pKeyResults;

	return pKey;
}

bool AuthSQL::deleteKey(const Key *pKey, bool deleteUsage)
{
	if ((m_pDb == NULL) ||
		(pKey == NULL))
	{
		return false;
	}

	string deleteSql("DELETE FROM WebAPIKeys WHERE ApplicationKeyID='");
	deleteSql += pKey->m_appId;
	deleteSql += "';";

	if (m_pDb->executeSimpleStatement(deleteSql) == false)
	{
		return false;
	}

	if (deleteUsage == false)
	{
		// Stop here
		return true;
	}

	deleteSql = "DELETE FROM WebAPIUsage WHERE KeyID='";
	deleteSql += pKey->m_dbId;
	deleteSql += "';";

	if (m_pDb->executeSimpleStatement(deleteSql) == false)
	{
		return false;
	}

	return true;
}

