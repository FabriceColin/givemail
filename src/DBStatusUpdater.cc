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

#include <iostream>
#include <algorithm>
#include <sstream>

#include "DBStatusUpdater.h"

using std::clog;
using std::endl;
using std::min;
using std::string;
using std::stringstream;

DBStatusUpdater::DBStatusUpdater(SQLDB *pDb, const string &campaignId) :
	StatusUpdater(),
	m_pDb(pDb),
	m_campaignId(campaignId),
	m_recipientsCount(0)
{
}

DBStatusUpdater::~DBStatusUpdater()
{
}

unsigned int DBStatusUpdater::getRecipientsCount(void)
{
	return m_recipientsCount;
}

void DBStatusUpdater::updateRecipientsStatus(const string &domainName,
	int statusCode, const char *pText)
{
	string updateSql("UPDATE evaa_emailrecipient SET status='");
	string statusValue;
	stringstream statusStr;

	if (m_pDb == NULL)
	{
		return;
	}

	statusStr << statusCode;
	if (pText != NULL)
	{
		statusStr << " " << pText << endl;
	}

	// Could this recipient be sent email ?
	if ((statusCode == 250) ||
		((statusCode == 0) && (pText == NULL)))
	{
		statusValue = "Sent";
	}
	else
	{
		statusValue = "Failed";
	}
	updateSql += statusValue;
	updateSql += "', statuscode='";
	statusValue = m_pDb->escapeString(statusStr.str());
	updateSql += statusValue.substr(0, min((string::size_type)255, statusValue.length()));
	updateSql += "', timesent=UNIX_TIMESTAMP(), numattempts=numattempts+1";

	// Apply this to all recipients of this domain
	updateSql += " WHERE domainname='";
	updateSql += domainName;
	updateSql += "' AND evaa_emailcampaignid=";
	updateSql += m_campaignId;
	updateSql += ";";

	if (m_pDb->executeSimpleStatement(updateSql) == false)
	{
		clog << "Couldn't update recipients at " << domainName << endl;
	}

	// Call parent's implementation
	StatusUpdater::updateRecipientsStatus(domainName, statusCode, pText);
}

void DBStatusUpdater::updateRecipientStatus(const string &emailAddress,
	int statusCode, const char *pText,
	const string &msgId)
{
	string updateSql("UPDATE evaa_emailrecipient SET status='");
	string statusValue;
	stringstream statusStr;

	if (m_pDb == NULL)
	{
		return;
	}

	statusStr << statusCode;
	if (pText != NULL)
	{
		statusStr << " " << pText << endl;
	}

	// Could this recipient be sent email ?
	if ((statusCode == 250) ||
		((statusCode == 0) && (pText == NULL)))
	{
		statusValue = "Sent";
	}
	else
	{
		statusValue = "Failed";
	}
	updateSql += statusValue;
	updateSql += "', statuscode='";
	statusValue = m_pDb->escapeString(statusStr.str());
	updateSql += statusValue.substr(0, min((string::size_type)255, statusValue.length()));
	updateSql += "', timesent=UNIX_TIMESTAMP(), numattempts=numattempts+1";

	// Email addresses are unique within a campaign, so we don't have to
	// remember recipientId
	updateSql += " WHERE emailaddress='";
	updateSql += emailAddress;
	updateSql += "' AND evaa_emailcampaignid=";
	updateSql += m_campaignId;
	updateSql += ";";

	if (m_pDb->executeSimpleStatement(updateSql) == true)
	{
		++m_recipientsCount;
	}
	else
	{
		clog << "Couldn't update recipient at " << emailAddress << endl;
	}

	// Call parent's implementation
	StatusUpdater::updateRecipientStatus(emailAddress, statusCode, pText, msgId);
}

