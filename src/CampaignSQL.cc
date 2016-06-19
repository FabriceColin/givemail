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

#include <time.h>
#include <stdlib.h>
#include <vector>
#include <sstream>
#include <iostream>
#include <utility>

#include "CampaignSQL.h"
#include "ConfigurationFile.h"
#include "StatusUpdater.h"

using std::clog;
using std::endl;
using std::string;
using std::stringstream;
using std::set;
using std::map;
using std::multimap;
using std::vector;
using std::pair;

static string getDomainName(const string &emailAddress)
{
	string domainName;

	string::size_type atPos = emailAddress.find('@');
	if ((atPos != string::npos) &&
		(atPos + 1 < emailAddress.length()))
	{
		domainName = emailAddress.substr(atPos + 1);
	}

	return domainName;
}

CampaignSQL::CampaignSQL(SQLDB *pDb) :
	m_pDb(pDb)
{
}

CampaignSQL::~CampaignSQL()
{
}

void CampaignSQL::setAttachments(const string &campaignId,
	const MessageDetails *pDetails, bool inlineParts)
{
        unsigned int attachmentCount = pDetails->getAttachmentCount(inlineParts);
        for (unsigned int attachmentNum = 0; attachmentNum < attachmentCount; ++attachmentNum)
        {
                Attachment *pAttachment = pDetails->getAttachment(attachmentNum, inlineParts);

		if (pAttachment == NULL)
		{
			continue;
		}

		SQLResults *pAttachmentResults = m_pDb->executeStatement("INSERT INTO Attachments "
			"(CampaignID, AttachmentID, AttachmentValue, AttachmentType) "
			"VALUES('%s', '%s', '%s', '%s');",
			m_pDb->escapeString(campaignId).c_str(),
			m_pDb->escapeString(pAttachment->m_contentId).c_str(),
			m_pDb->escapeString(pAttachment->m_filePath).c_str(),
			m_pDb->escapeString(pAttachment->m_contentType).c_str());
		if (pAttachmentResults != NULL)
		{
			delete pAttachmentResults;
		}
        }
}

bool CampaignSQL::getAttachments(const string &campaignId,
	MessageDetails *pDetails)
{
	if ((m_pDb == NULL) ||
		(campaignId.empty() == true))
	{
		return false;
	}

	SQLResults *pResults = m_pDb->executeStatement("SELECT "
		"AttachmentID, AttachmentValue, AttachmentType "
		"FROM Attachments WHERE CampaignID='%s';",
		m_pDb->escapeString(campaignId).c_str());
	if (pResults == NULL)
	{
		return false;
	}

	SQLRow *pRow = pResults->nextRow();
	while (pRow != NULL)
	{
		pDetails->addAttachment(new Attachment(pRow->getColumn(1),
			pRow->getColumn(2),
			pRow->getColumn(0)));

		// Next row
		delete pRow;
		pRow = pResults->nextRow();
	}
	delete pResults;

	return true;
}

bool CampaignSQL::getCustomFields(Recipient *pRecipient)
{
	if ((m_pDb == NULL) ||
		(pRecipient == NULL) ||
		(pRecipient->m_id.empty() == true))
	{
		return false;
	}

	SQLResults *pRecipientResults = m_pDb->executeStatement("SELECT "
		"CustomFieldName, CustomFieldValue FROM CustomFields "
		"WHERE RecipientID='%s';",
		m_pDb->escapeString(pRecipient->m_id).c_str());
	if (pRecipientResults == NULL)
	{
		return false;
	}

	SQLRow *pRecipientRow = pRecipientResults->nextRow();
	while (pRecipientRow != NULL)
	{
		// FIXME: CustomFieldName should really be a name, not an index
		unsigned int fieldNum = (unsigned int)atoi(pRecipientRow->getColumn(0).c_str());
		if ((fieldNum >= 1) ||
			(fieldNum <= 6))
		{
			stringstream nameStr;

			nameStr << "customfield" << fieldNum;

			pRecipient->m_customFields[nameStr.str()] = pRecipientRow->getColumn(1);
		}

		// Next row
		delete pRecipientRow;
		pRecipientRow = pRecipientResults->nextRow();
	}
	delete pRecipientResults;

	return true;
}

bool CampaignSQL::createNewCampaign(Campaign &campaign,
	MessageDetails *pDetails)
{
	if ((m_pDb == NULL) ||
		(campaign.m_name.empty() == true) ||
		(campaign.hasValidStatus() == false) ||
		(pDetails == NULL))
	{
		return false;
	}

	campaign.m_id = m_pDb->getUniversalUniqueId();

	stringstream timeStr;
	string insertSql("INSERT INTO Campaigns (CampaignID, "
		"CampaignName, Status, HtmlContent, PlainContent, "
		"PersonalisedHtml, PersonalisedPlain, Subject, "
		"FromName, FromEmailAddress, ReplyName, ReplyEmailAddress, "
		"ProcessingDate, SenderEmailAddress, UnsubscribeLink) VALUES('");
	insertSql += m_pDb->escapeString(campaign.m_id);
	insertSql += "', '";
	insertSql += m_pDb->escapeString(campaign.m_name);
	insertSql += "', '";
	insertSql += m_pDb->escapeString(campaign.m_status);
	insertSql += "', '";
	insertSql += m_pDb->escapeString(pDetails->getContent("/html"));
	insertSql += "', '";
	insertSql += m_pDb->escapeString(pDetails->getContent("text/plain"));
	insertSql += "', 1, 1, '";
	insertSql += m_pDb->escapeString(pDetails->m_subject);
	insertSql += "', '";
	insertSql += m_pDb->escapeString(pDetails->m_fromName);
	insertSql += "', '";
	insertSql += m_pDb->escapeString(pDetails->m_fromEmailAddress);
	insertSql += "', '";
	insertSql += m_pDb->escapeString(pDetails->m_replyToName);
	insertSql += "', '";
	insertSql += m_pDb->escapeString(pDetails->m_replyToEmailAddress);
	insertSql += "', ";
	if (campaign.m_timestamp == 0)
	{
		// Use the current date and time
		campaign.m_timestamp = time(NULL);
	}
	timeStr << campaign.m_timestamp;
	insertSql += timeStr.str();
	insertSql += ", '";
	insertSql += m_pDb->escapeString(pDetails->m_senderEmailAddress);
	insertSql += "', '";
	insertSql += m_pDb->escapeString(pDetails->m_unsubscribeLink);
	insertSql += "');";

	if (m_pDb->executeSimpleStatement(insertSql) == false)
	{
		return false;
	}

	setAttachments(campaign.m_id, pDetails, false);
	setAttachments(campaign.m_id, pDetails, true);

	return true;
}

Campaign *CampaignSQL::getCampaign(const string &campaignId)
{
	if ((m_pDb == NULL) ||
		(campaignId.empty() == true))
	{
		return NULL;
	}

	SQLResults *pCampaignResults = m_pDb->executeStatement("SELECT "
		"CampaignID, CampaignName, Status, ProcessingDate "
		"FROM Campaigns WHERE CampaignID='%s';",
		m_pDb->escapeString(campaignId).c_str());
	if (pCampaignResults == NULL)
	{
		return NULL;
	}

	// There should only be one row matching campaignId
	SQLRow *pCampaignRow = pCampaignResults->nextRow();
	if (pCampaignRow == NULL)
	{
		delete pCampaignResults;
		return NULL;
	}

	Campaign *pCampaign = new Campaign(pCampaignRow->getColumn(0),
		pCampaignRow->getColumn(1),
		pCampaignRow->getColumn(2),
		(time_t)atoi(pCampaignRow->getColumn(3).c_str()));

	delete pCampaignRow;
	delete pCampaignResults;

	return pCampaign;
}

bool CampaignSQL::getCampaigns(const string &criteriaValue, GetCriteria criteriaType,
	off_t maxCount, off_t startOffset, off_t &totalCount,
	set<Campaign> &campaigns)
{
	string::size_type percentPos = string::npos;

	if (m_pDb == NULL)
	{
		return false;
	}

	string fromClause("FROM Campaigns");
	switch (criteriaType)
	{
		case STATUS:
			fromClause += " WHERE Status='";
			fromClause += m_pDb->escapeString(criteriaValue);
			fromClause += "'";
			break;
		case NAME:
			// Any wildcard ?
			percentPos = criteriaValue.find('%');
			if (percentPos != string::npos)
                        {
				fromClause += " WHERE CampaignName LIKE '";
                        }
			else
			{
				fromClause += " WHERE CampaignName='";
			}
			fromClause += m_pDb->escapeString(criteriaValue);
			fromClause += "'";
			break;
		case NONE:
		default:
			break;
	}

	string selectSql("SELECT COUNT(*) ");
	selectSql += fromClause;

	// Get the total number of rows matching this, on the first run
	if (startOffset == 0)
	{
		SQLResults *pCampaignResults = m_pDb->executeStatement(selectSql.c_str());
		if (pCampaignResults != NULL)
		{
			SQLRow *pCampaignRow = pCampaignResults->nextRow();
			if (pCampaignRow != NULL)
			{
				totalCount = (off_t)atoll(pCampaignRow->getColumn(0).c_str());

				delete pCampaignRow;
			}

			delete pCampaignResults;
		}
	}

	// Get the actual rows
	// Don't terminate this statement with a semi-colon
	selectSql = "SELECT CampaignID, CampaignName, Status, ProcessingDate ";
	selectSql += fromClause;
	selectSql += " ORDER BY CampaignID";

	SQLResults *pCampaignResults = m_pDb->executeStatement(selectSql,
		startOffset, startOffset + maxCount);
	if ((pCampaignResults == NULL) ||
		(pCampaignResults->getRowsCount() == 0))
	{
		if (pCampaignResults != NULL)
		{
			delete pCampaignResults;
		}

		return false;
	}
	clog << "Got " << pCampaignResults->getRowsCount() << "/" << maxCount << " campaigns" << endl;

	SQLRow *pCampaignRow = pCampaignResults->nextRow();
	while (pCampaignRow != NULL)
	{
		campaigns.insert(Campaign(pCampaignRow->getColumn(0),
			pCampaignRow->getColumn(1),
			pCampaignRow->getColumn(2),
			(time_t)atoi(pCampaignRow->getColumn(3).c_str())));

		// Next row
		delete pCampaignRow;
		pCampaignRow = pCampaignResults->nextRow();
	}
	delete pCampaignResults;

	return true;
}

bool CampaignSQL::getChangedCampaigns(time_t sinceTime, vector<string> &campaignIds)
{
	if (m_pDb == NULL)
	{
		return false;
	}

	SQLResults *pCampaignResults = m_pDb->executeStatement("SELECT "
		"CampaignID FROM Campaigns WHERE ProcessingDate>%u;",
		sinceTime);
	if (pCampaignResults == NULL)
	{
		return false;
	}

	SQLRow *pCampaignRow = pCampaignResults->nextRow();
	while (pCampaignRow != NULL)
	{
		campaignIds.push_back(pCampaignRow->getColumn(0));

		// Next row
		delete pCampaignRow;
		pCampaignRow = pCampaignResults->nextRow();
	}
	delete pCampaignResults;

	return true;
}

bool CampaignSQL::getCampaignsWithTemporaryFailures(time_t oldestTime, time_t newestTime,
	off_t maxCount, off_t startOffset, off_t &totalCount,
	set<Campaign> &campaigns)
{
	stringstream oldestTimeStr, newestTimeStr;

	newestTimeStr << newestTime;
	oldestTimeStr << oldestTime;

	string fromClause("FROM Campaigns c, Recipients r WHERE c.Status='Sent' "
		"AND c.ProcessingDate<");
	fromClause += newestTimeStr.str();
	fromClause += " AND c.ProcessingDate>";
	fromClause += oldestTimeStr.str();
	fromClause += " AND c.CampaignID=r.CampaignID AND r.Status='Failed' "
		"AND r.StatusCode LIKE '4%%' AND r.SendDate<";
	fromClause += newestTimeStr.str();
	fromClause += " AND r.SendDate>";
	fromClause += oldestTimeStr.str();
	fromClause += " GROUP BY c.CampaignName";

	string selectSql("SELECT COUNT(*) ");
	selectSql += fromClause;

	// Get the total number of rows matching this, on the first run
	if (startOffset == 0)
	{
		SQLResults *pCampaignResults = m_pDb->executeStatement(selectSql.c_str());
		if (pCampaignResults != NULL)
		{
			SQLRow *pCampaignRow = pCampaignResults->nextRow();
			if (pCampaignRow != NULL)
			{
				totalCount = (off_t)atoll(pCampaignRow->getColumn(0).c_str());

				delete pCampaignRow;
			}

			delete pCampaignResults;
		}
	}

	// Get the actual rows
	// Don't terminate this statement with a semi-colon
	selectSql = "SELECT c.CampaignID, c.CampaignName, c.Status, "
		"c.ProcessingDate, count(r.RecipientName) AS failedrecipientscount ";
	selectSql += fromClause;
	selectSql += " ORDER BY failedrecipientscount DESC";

	SQLResults *pCampaignResults = m_pDb->executeStatement(selectSql,
		startOffset, startOffset + maxCount);
	if ((pCampaignResults == NULL) ||
		(pCampaignResults->getRowsCount() == 0))
	{
		if (pCampaignResults != NULL)
		{
			delete pCampaignResults;
		}

		return false;
	}

	SQLRow *pCampaignRow = pCampaignResults->nextRow();
	while (pCampaignRow != NULL)
	{
		off_t recipients = (off_t)atoll(pCampaignRow->getColumn(4).c_str());

		if (recipients > 0)
		{
			clog << "Campaign " << pCampaignRow->getColumn(0) << " has " << recipients << " temporary failures" << endl;

			campaigns.insert(Campaign(pCampaignRow->getColumn(0),
				pCampaignRow->getColumn(1),
				pCampaignRow->getColumn(2),
				(time_t)atoi(pCampaignRow->getColumn(3).c_str())));
		}

		// Next row
		delete pCampaignRow;
		pCampaignRow = pCampaignResults->nextRow();
	}
	delete pCampaignResults;

	return true;
}

bool CampaignSQL::updateMessage(const string &campaignId,
	const MessageDetails *pDetails)
{
	if ((m_pDb == NULL) ||
		(campaignId.empty() == true) ||
		(pDetails == NULL))
	{
		return false;
	}

	string updateSql("UPDATE Campaigns SET HtmlContent='");
	updateSql += m_pDb->escapeString(pDetails->getContent("/html"));
	updateSql += "', PlainContent='";
	updateSql += m_pDb->escapeString(pDetails->getContent("text/plain"));
	updateSql += "', PersonalisedHtml=1, PersonalisedPlain=1, Subject='";
	updateSql += m_pDb->escapeString(pDetails->m_subject);
	updateSql += "', FromName='";
	updateSql += m_pDb->escapeString(pDetails->m_fromName);
	updateSql += "', FromEmailAddress='";
	updateSql += m_pDb->escapeString(pDetails->m_fromEmailAddress);
	updateSql += "', ReplyName='";
	updateSql += m_pDb->escapeString(pDetails->m_replyToName);
	updateSql += "', ReplyEmailAddress='";
	updateSql += m_pDb->escapeString(pDetails->m_replyToEmailAddress);
	updateSql += "', SenderEmailAddress='";
	updateSql += m_pDb->escapeString(pDetails->m_senderEmailAddress);
	updateSql += "', UnsubscribeLink='";
	updateSql += m_pDb->escapeString(pDetails->m_unsubscribeLink);
	updateSql += "' WHERE CampaignID='";
	updateSql += campaignId;
	updateSql += "';";

	if (m_pDb->executeSimpleStatement(updateSql) == false)
	{
		return false;
	}

	string deleteSql("DELETE FROM Attachments WHERE CampaignID='");
	deleteSql += m_pDb->escapeString(campaignId);
	deleteSql += "';";

	m_pDb->executeSimpleStatement(deleteSql);
	setAttachments(campaignId, pDetails, false);
	setAttachments(campaignId, pDetails, true);

	return true;
}

MessageDetails *CampaignSQL::getMessage(const string &campaignId)
{
	if ((m_pDb == NULL) ||
		(campaignId.empty() == true))
	{
		return NULL;
	}

	SQLResults *pCampaignResults = m_pDb->executeStatement("SELECT "
		"HtmlContent, PlainContent, PersonalisedHtml, PersonalisedPlain, "
		"Subject, FromName, FromEmailAddress, ReplyName, ReplyEmailAddress, "
		"SenderEmailAddress, UnsubscribeLink "
		"FROM Campaigns WHERE CampaignID='%s';",
		campaignId.c_str());
	if (pCampaignResults == NULL)
	{
		return NULL;
	}

	// There should only be one row matching campaignId
	SQLRow *pCampaignRow = pCampaignResults->nextRow();
	if (pCampaignRow == NULL)
	{
		delete pCampaignResults;
		return NULL;
	}

	MessageDetails *pDetails = new MessageDetails();
	pDetails->setContentPiece("text/html", pCampaignRow->getColumn(0).c_str(),
		"quoted-printable",
		(pCampaignRow->getColumn(2) == "0" ? false : true));
	pDetails->setContentPiece("text/plain", pCampaignRow->getColumn(1).c_str(),
		"quoted-printable",
		(pCampaignRow->getColumn(3) == "0" ? false : true));
	pDetails->m_subject = pCampaignRow->getColumn(4);
	pDetails->m_fromName = pCampaignRow->getColumn(5);
	pDetails->m_fromEmailAddress = pCampaignRow->getColumn(6);
	pDetails->m_replyToName = pCampaignRow->getColumn(7);
	pDetails->m_replyToEmailAddress = pCampaignRow->getColumn(8);
	pDetails->m_senderEmailAddress = pCampaignRow->getColumn(9);
	pDetails->m_unsubscribeLink = pCampaignRow->getColumn(10);
	getAttachments(campaignId, pDetails);

	delete pCampaignRow;
	delete pCampaignResults;

	return pDetails;
}

bool CampaignSQL::hasRecipient(const string &campaignId,
	const string &emailAddress)
{
	bool foundRecipient = false;

	if ((m_pDb == NULL) ||
		(campaignId.empty() == true))
	{
		return false;
	}

	string selectSql("SELECT RecipientID FROM Recipients WHERE EmailAddress='");
	selectSql += m_pDb->escapeString(emailAddress);
	selectSql += "' AND CampaignID='";
	selectSql += m_pDb->escapeString(campaignId);
	selectSql += "';";

	SQLResults *pResults = m_pDb->executeStatement(selectSql.c_str());
	if (pResults == NULL)
	{
		return false;
	}

	SQLRow *pRow = pResults->nextRow();
	if (pRow != NULL)
	{
		foundRecipient = true;

		delete pRow;
	}

	delete pResults;

	return foundRecipient;
}

bool CampaignSQL::createNewRecipient(const string &campaignId, Recipient &recipient)
{
	if ((m_pDb == NULL) ||
		(campaignId.empty() == true) ||
		(recipient.m_emailAddress.empty() == true) ||
		(recipient.hasValidStatus() == false))
	{
		return false;
	}

	recipient.m_id = m_pDb->getUniversalUniqueId();

	string domainName(getDomainName(recipient.m_emailAddress));
	string insertSql("INSERT INTO Recipients (RecipientID, "
		"CampaignID, RecipientName, Status, StatusCode, EmailAddress, "
		"ReturnPath, DomainName, SendDate, AttemptsCount) VALUES('");
	insertSql += m_pDb->escapeString(recipient.m_id);
	insertSql += "', '";
	insertSql += m_pDb->escapeString(campaignId);
	insertSql += "', '";
	insertSql += m_pDb->escapeString(recipient.m_name);
	insertSql += "', '";
	insertSql += m_pDb->escapeString(recipient.m_status);
	insertSql += "', '0', '";
	insertSql += m_pDb->escapeString(recipient.m_emailAddress);
	insertSql += "', '";
	insertSql += m_pDb->escapeString(recipient.m_returnPathEmailAddress);
	insertSql += "', '";
	insertSql += m_pDb->escapeString(domainName);
	insertSql += "', '0', '0');";

	recipient.m_timeSent = 0;
	recipient.m_numAttempts = 0;
	if (m_pDb->executeSimpleStatement(insertSql) == false)
	{
		return false;
	}

	for (unsigned int fieldNum = 1; fieldNum <= 6; ++fieldNum)
	{
		stringstream nameStr, numStr;

		nameStr << "customfield" << fieldNum;
		numStr << fieldNum;

		insertSql = "INSERT INTO CustomFields (RecipientID, "
			"CustomFieldName, CustomFieldValue) VALUES('";
		insertSql += recipient.m_id;
		insertSql += "', '";
		insertSql += numStr.str();
		insertSql += "', '";
		insertSql += m_pDb->escapeString(recipient.m_customFields[nameStr.str()]);
		insertSql += "');";

		m_pDb->executeSimpleStatement(insertSql);
	}

	return true;
}

off_t CampaignSQL::listDomains(const string &campaignId,
	const string &status, multimap<off_t, string> &domainsBreakdown,
	off_t count, off_t offset)
{
	if ((m_pDb == NULL) ||
		(campaignId.empty() == true) ||
		(status.empty() == true))
	{
		return 0;
	}

	string selectSql("SELECT DomainName, Status, COUNT(*) AS recipientscount "
		"FROM Recipients WHERE CampaignID='");
	selectSql += m_pDb->escapeString(campaignId);
	selectSql += "' AND Status='";
	selectSql += m_pDb->escapeString(status);
	selectSql += "' GROUP BY DomainName, Status ORDER BY recipientscount DESC;";

	SQLResults *pDomainResults = m_pDb->executeStatement(selectSql.c_str());
	if (pDomainResults == NULL)
	{
		return 0;
	}

	SQLRow *pDomainRow = pDomainResults->nextRow();
	off_t domainNum = 0;
	off_t externalRecipientsCount = 0;
	bool hasRelay = false;

	// Is a relay defined ?
	ConfigurationFile *pConfig = ConfigurationFile::getInstance("");
	if ((pConfig != NULL) &&
		(pConfig->m_options.m_mailRelayAddress.empty() == false))
	{
		clog << "Relaying campaign's " << pDomainResults->getRowsCount() << " domains to "
			<< pConfig->m_options.m_mailRelayAddress << ":" << pConfig->m_options.m_mailRelayPort << endl;
		hasRelay = true;
	}
	else
	{
		clog << "Campaign has " << pDomainResults->getRowsCount() << " domains" << endl;
	}

	// Sort the domains by the number of recipients they include
	while (pDomainRow != NULL)
	{
		string domainName(pDomainRow->getColumn(0));
		string domainStatus(pDomainRow->getColumn(1));
		off_t domainRecipients = (off_t)atoll(pDomainRow->getColumn(2).c_str());
		bool goodDomain = false;

		// Make sure the domain name is set
		if (domainName.empty() == false)
		{
			if ((count > 0) &&
				(offset >= 0))
			{
				if ((domainNum >= offset) &&
					(((domainNum - offset) % count) == 0))
				{
					goodDomain = true;
				}
			}
			else
			{
				goodDomain = true;
			}
		}

		if (goodDomain == true)
		{
			clog << "Domain " << domainNum << " " << domainName << ", status " << domainStatus << endl;

			if (hasRelay == false)
			{
				domainsBreakdown.insert(pair<off_t, string>(domainRecipients, domainName));
			}
			// Don't add a domain if relay and internal domain are the same
			else if ((domainName == pConfig->m_options.m_internalDomain) &&
				(pConfig->m_options.m_mailRelayAddress != pConfig->m_options.m_internalDomain))
			{
				domainsBreakdown.insert(pair<off_t, string>(domainRecipients, domainName));
			}
			else
			{
				// Add these recipients to the relay later
				externalRecipientsCount += domainRecipients;
			}
		}

		// Next row
		++domainNum;
		delete pDomainRow;
		pDomainRow = pDomainResults->nextRow();
	}
	delete pDomainResults;

	if (hasRelay == true)
	{
		// Add a domain for the relay itself
		domainsBreakdown.insert(pair<off_t, string>(externalRecipientsCount, pConfig->m_options.m_mailRelayAddress));
	}

	return (off_t)domainsBreakdown.size();
}

Recipient *CampaignSQL::getRecipient(const string &recipientId)
{
	if ((m_pDb == NULL) ||
		(recipientId.empty() == true))
	{
		return NULL;
	}

	SQLResults *pRecipientResults = m_pDb->executeStatement("SELECT "
		"RecipientID, RecipientName, Status, EmailAddress, ReturnPath, "
		"StatusCode, SendDate, AttemptsCount FROM Recipients "
		"WHERE RecipientID='%s';",
		m_pDb->escapeString(recipientId).c_str());
	if (pRecipientResults == NULL)
	{
		return NULL;
	}

	// There should only be one row matching recipientId
	SQLRow *pRecipientRow = pRecipientResults->nextRow();
	if (pRecipientRow == NULL)
	{
		delete pRecipientResults;
		return NULL;
	}

	Recipient *pRecipient = new Recipient(pRecipientRow->getColumn(0),
			pRecipientRow->getColumn(1),
			pRecipientRow->getColumn(2),
			pRecipientRow->getColumn(3),
			pRecipientRow->getColumn(4));

	pRecipient->m_statusCode = pRecipientRow->getColumn(5);
	pRecipient->m_timeSent = (time_t)atoi(pRecipientRow->getColumn(6).c_str());
	pRecipient->m_numAttempts = (off_t)atoll(pRecipientRow->getColumn(7).c_str());
	getCustomFields(pRecipient);

	delete pRecipientRow;
	delete pRecipientResults;

	return pRecipient;
}

off_t CampaignSQL::countRecipients(const string &campaignId,
	const string &status, const string &statusCode, bool isLike)
{
	off_t totalCount = 0;

	if ((m_pDb == NULL) ||
		(campaignId.empty() == true))
	{
		return false;
	}

	string selectSql("SELECT COUNT(*) FROM Recipients WHERE CampaignID='");
	selectSql += m_pDb->escapeString(campaignId);
	if (status.empty() == true)
	{
		selectSql += "';";
	}
	else
	{
		selectSql += "' AND Status='";
		selectSql += m_pDb->escapeString(status);
		if (statusCode.empty() == true)
		{
			selectSql += "';";
		}
		else
		{
			selectSql += "' AND StatusCode";
			if (isLike == true)
			{
				selectSql += " LIKE '";
				selectSql += m_pDb->escapeString(statusCode);
				selectSql += "%%';";
			}
			else
			{
				selectSql += "='";
				selectSql += m_pDb->escapeString(statusCode);
				selectSql += "';";
			}
		}
	}

	SQLResults *pRecipientResults = m_pDb->executeStatement(selectSql.c_str());
	if (pRecipientResults != NULL)
	{
		SQLRow *pRecipientRow = pRecipientResults->nextRow();
		if (pRecipientRow != NULL)
		{
			totalCount = (off_t)atoll(pRecipientRow->getColumn(0).c_str());

			delete pRecipientRow;
		}

		delete pRecipientResults;
	}

	return totalCount;
}

bool CampaignSQL::getRecipients(const string &campaignId,
	off_t maxCount, off_t startOffset,
	map<string, Recipient> &recipients)
{
	if ((m_pDb == NULL) ||
		(campaignId.empty() == true))
	{
		return false;
	}

	// Get the actual rows
	// Don't terminate this statement with a semi-colon
	string selectSql("SELECT RecipientID, RecipientName, Status, "
		"EmailAddress, ReturnPath, StatusCode, SendDate, "
		"AttemptsCount FROM Recipients WHERE CampaignID='");
	selectSql += m_pDb->escapeString(campaignId);
	selectSql += "' ORDER BY RecipientID";

	SQLResults *pRecipientResults = m_pDb->executeStatement(selectSql,
		startOffset, startOffset + maxCount);
	if ((pRecipientResults == NULL) ||
		(pRecipientResults->getRowsCount() == 0))
	{
		if (pRecipientResults != NULL)
		{
			delete pRecipientResults;
		}

		return false;
	}
	clog << "Got " << pRecipientResults->getRowsCount() << "/" << maxCount << " recipients" << endl;

	SQLRow *pRecipientRow = pRecipientResults->nextRow();
	while (pRecipientRow != NULL)
	{
		Recipient recipObj(pRecipientRow->getColumn(0),
			pRecipientRow->getColumn(1),
			pRecipientRow->getColumn(2),
			pRecipientRow->getColumn(3),
			pRecipientRow->getColumn(4));

		recipObj.m_statusCode = pRecipientRow->getColumn(5);
		recipObj.m_timeSent = (time_t)atoi(pRecipientRow->getColumn(6).c_str());
		recipObj.m_numAttempts = (off_t)atoll(pRecipientRow->getColumn(7).c_str());
		getCustomFields(&recipObj);

		clog << "Recipient " << recipObj.m_name << " " << recipObj.m_emailAddress
			<< " (" << recipObj.m_id << ")" << endl;

		// Add this to the list of recipients
		recipients[recipObj.m_emailAddress] = recipObj;

		// Next row
		delete pRecipientRow;
		pRecipientRow = pRecipientResults->nextRow();
	}
	delete pRecipientResults;

	return true;
}

bool CampaignSQL::getRecipients(const string &campaignId, const string &status,
	const string &domainName, off_t maxCount,
	map<string, Recipient> &recipients)
{
	bool hasRelay = false;

	if ((m_pDb == NULL) ||
		(campaignId.empty() == true))
	{
		return false;
	}

	// When requesting recipients for the relay, provide all recipients not on the internal domain
	// or all recipients if the internal domain and the relay are one and the same
	ConfigurationFile *pConfig = ConfigurationFile::getInstance("");
	if ((pConfig != NULL) &&
		(pConfig->m_options.m_internalDomain.empty() == false) &&
		(pConfig->m_options.m_mailRelayAddress == domainName))
	{
		hasRelay = true;
	}

	string fromClause("FROM Recipients WHERE");
	if (status.empty() == false)
	{
		fromClause += " Status='";
		fromClause += m_pDb->escapeString(status);
		fromClause += "' AND";
	}
	if (domainName.empty() == false)
	{
		if (hasRelay == false)
		{
			fromClause += " DomainName='";
			fromClause += m_pDb->escapeString(domainName);
			fromClause += "' AND";
		}
		else if (pConfig->m_options.m_mailRelayAddress != pConfig->m_options.m_internalDomain)
		{
			fromClause += " DomainName!='";
			fromClause += m_pDb->escapeString(pConfig->m_options.m_internalDomain);
			fromClause += "' AND";
		}
		// Else, all domains
	}
	fromClause += " CampaignID='";
	fromClause += campaignId;
	fromClause += "'";

	string selectSql("SELECT COUNT(*) ");
	selectSql += fromClause;

	// Get the actual rows
	// Don't terminate this statement with a semi-colon
	selectSql = "SELECT RecipientID, RecipientName, Status, "
		"EmailAddress, ReturnPath, SendDate, AttemptsCount ";
	selectSql += fromClause;
	selectSql += " ORDER BY RecipientID";

	SQLResults *pRecipientResults = m_pDb->executeStatement(selectSql,
		0, maxCount);
	if ((pRecipientResults == NULL) ||
		(pRecipientResults->getRowsCount() == 0))
	{
		if (pRecipientResults != NULL)
		{
			delete pRecipientResults;
		}

		return false;
	}
	clog << "Got " << pRecipientResults->getRowsCount() << "/" << maxCount << " recipients" << endl;

	SQLRow *pRecipientRow = pRecipientResults->nextRow();
	while (pRecipientRow != NULL)
	{
		Recipient recipObj(pRecipientRow->getColumn(0),
			pRecipientRow->getColumn(1),
			pRecipientRow->getColumn(2),
			pRecipientRow->getColumn(3),
			pRecipientRow->getColumn(4));

		recipObj.m_timeSent = (time_t)atoi(pRecipientRow->getColumn(5).c_str());
		recipObj.m_numAttempts = (off_t)atoll(pRecipientRow->getColumn(6).c_str());
		getCustomFields(&recipObj);

		clog << "Recipient " << recipObj.m_name << " " << recipObj.m_emailAddress
			<< " (" << recipObj.m_id << ")" << endl;

		// Add this to the list of recipients
		recipients[recipObj.m_emailAddress] = recipObj;

		// Next row
		delete pRecipientRow;
		pRecipientRow = pRecipientResults->nextRow();
	}
	delete pRecipientResults;

	return true;
}

bool CampaignSQL::getChangedRecipients(time_t sinceTime, vector<string> &recipientIds)
{
	if (m_pDb == NULL)
	{
		return false;
	}

	SQLResults *pRecipientResults = m_pDb->executeStatement("SELECT "
		"RecipientID FROM Recipients WHERE SendDate>%u;",
		sinceTime);
	if (pRecipientResults == NULL)
	{
		return false;
	}

	SQLRow *pRecipientRow = pRecipientResults->nextRow();
	while (pRecipientRow != NULL)
	{
		recipientIds.push_back(pRecipientRow->getColumn(0));

		// Next row
		delete pRecipientRow;
		pRecipientRow = pRecipientResults->nextRow();
	}
	delete pRecipientResults;

	return true;
}

bool CampaignSQL::resetFailedRecipients(const string &campaignId,
	const string &statusCode, bool isLike)
{
	if ((m_pDb == NULL) ||
		(campaignId.empty() == true))
	{
		return false;
	}

	string updateSql("UPDATE Recipients SET Status='Waiting', "
		"AttemptsCount=AttemptsCount+1 WHERE CampaignID='");
	updateSql += m_pDb->escapeString(campaignId);
	updateSql += "' AND Status='Failed' AND StatusCode";
	if (isLike == true)
	{
		updateSql += " LIKE '";
		updateSql += m_pDb->escapeString(statusCode);
		updateSql += "%%';";
	}
	else
	{
		updateSql += "='";
		updateSql += m_pDb->escapeString(statusCode);
		updateSql += "';";
	}

	return m_pDb->executeSimpleStatement(updateSql);
}

bool CampaignSQL::setCampaign(const Campaign &campaign)
{
	bool separateColumns = false;

	if ((m_pDb == NULL) ||
		(campaign.m_id.empty() == true))
	{
		return false;
	}

	if ((campaign.m_name.empty() == true) &&
		(campaign.m_status.empty() == true))
	{
		// Nothing to do
		return true;
	}

	string updateSql("UPDATE Campaigns SET");
	if (campaign.m_name.empty() == false)
	{
		updateSql += " CampaignName='";
		updateSql += m_pDb->escapeString(campaign.m_name);
		updateSql += "'";

		separateColumns = true;
	}
	if (campaign.hasValidStatus() == true)
	{
		if (separateColumns == true)
		{
			updateSql += ",";
		}

		updateSql += " Status='";
		updateSql += m_pDb->escapeString(campaign.m_status);
		updateSql += "'";
	}
	updateSql += ", ProcessingDate=";
	if (campaign.m_timestamp == 0)
	{
		// Use the current date and time
		updateSql += "UNIX_TIMESTAMP()";
	}
	else
	{
		stringstream timeStr;

		timeStr << campaign.m_timestamp;

		updateSql += timeStr.str();
	}
	updateSql += " WHERE CampaignID='";
	updateSql += m_pDb->escapeString(campaign.m_id);
	updateSql += "';";

	return m_pDb->executeSimpleStatement(updateSql);
}

bool CampaignSQL::setRecipient(Recipient &recipient)
{
	bool separateColumns = false;

	if ((m_pDb == NULL) ||
		(recipient.m_id.empty() == true))
	{
		return false;
	}

	if ((recipient.m_name.empty() == true) &&
		(recipient.m_status.empty() == true) &&
		(recipient.m_emailAddress.empty() == true) &&
		(recipient.m_returnPathEmailAddress.empty() == true) &&
		(recipient.m_customFields.empty() == true))
	{
		// Nothing to do
		return true;
	}

	string domainName(getDomainName(recipient.m_emailAddress));
	string updateSql("UPDATE Recipients SET");
	if (recipient.m_name.empty() == false)
	{
		updateSql += " RecipientName='";
		updateSql += m_pDb->escapeString(recipient.m_name);
		updateSql += "'";

		separateColumns = true;
	}
	if (recipient.hasValidStatus() == true)
	{
		recipient.m_timeSent = time(NULL);

		if (separateColumns == true)
		{
			updateSql += ",";
		}

		updateSql += " Status='";
		updateSql += m_pDb->escapeString(recipient.m_status);
		updateSql += "', ProcessingDate='";
		updateSql += recipient.m_timeSent;
		updateSql += "'";

		separateColumns = true;
	}
	if (recipient.m_emailAddress.empty() == false)
	{
		if (separateColumns == true)
		{
			updateSql += ",";
		}

		updateSql += " EmailAddress='";
		updateSql += m_pDb->escapeString(recipient.m_emailAddress);
		updateSql += "'";

		separateColumns = true;
	}
	if (recipient.m_returnPathEmailAddress.empty() == false)
	{
		if (separateColumns == true)
		{
			updateSql += ",";
		}

		updateSql += " ReturnPath='";
		updateSql += m_pDb->escapeString(recipient.m_returnPathEmailAddress);
		updateSql += "', DomainName='";
		updateSql += m_pDb->escapeString(domainName);
		updateSql += "'";

		separateColumns = true;
	}
	updateSql += " WHERE RecipientID='";
	updateSql += m_pDb->escapeString(recipient.m_id);
	updateSql += "';";

	if (m_pDb->executeSimpleStatement(updateSql) == false)
	{
		return false;
	}

	if (recipient.m_customFields.empty() == false)
	{
		if (separateColumns == true)
		{
			updateSql += ",";
		}

		for (unsigned int fieldNum = 1; fieldNum <= 6; ++fieldNum)
		{
			stringstream nameStr, numStr;

			nameStr << "customfield" << fieldNum;
			numStr << fieldNum;

			updateSql = "UPDATE CustomFields SET CustomFieldValue='";
			updateSql += m_pDb->escapeString(recipient.m_customFields[nameStr.str()]);
			updateSql += "' WHERE RecipientID='";
			updateSql += m_pDb->escapeString(recipient.m_id);
			updateSql += "' AND CustomFieldName='";
			updateSql += numStr.str();
			updateSql += "';";

			m_pDb->executeSimpleStatement(updateSql);
		}
	}

	return true;
}

bool CampaignSQL::setRecipientsStatus(const string &campaignId,
	const string &status, const string &currentStatus)
{
	if ((m_pDb == NULL) ||
		(campaignId.empty() == true))
	{
		return false;
	}

	string updateSql("UPDATE Recipients SET Status='");
	updateSql += m_pDb->escapeString(status);
	updateSql += "', ProcessingDate=UNIX_TIMESTAMP() WHERE CampaignID='";
	updateSql += m_pDb->escapeString(campaignId);
	if (currentStatus.empty() == false)
	{
		updateSql += "' AND Status='";
		updateSql += m_pDb->escapeString(currentStatus);
	}
	updateSql += "';";

	return m_pDb->executeSimpleStatement(updateSql);
}

bool CampaignSQL::deleteRecipient(const string &recipientId)
{
	bool deletionStatus = true;

	if ((m_pDb == NULL) ||
		(recipientId.empty() == true))
	{
		return false;
	}

	string deleteSql("DELETE FROM Recipients WHERE RecipientID='");
	deleteSql += m_pDb->escapeString(recipientId);
	deleteSql += "';";

	if (m_pDb->executeSimpleStatement(deleteSql) == false)
	{
		deletionStatus = false;
	}

	return deletionStatus;
}

bool CampaignSQL::deleteRecipients(const string &campaignId,
	const string &status)
{
	bool deletionStatus = true;

	if ((m_pDb == NULL) ||
		(campaignId.empty() == true))
	{
		return false;
	}

	string deleteSql("DELETE FROM CustomFields WHERE RecipientID IN "
		"(SELECT RecipientID FROM Recipients WHERE CampaignID='");
	deleteSql += m_pDb->escapeString(campaignId);
	if (status.empty() == false)
	{
		deleteSql += "' AND Status='";
		deleteSql += m_pDb->escapeString(status);
	}
	deleteSql += "');";

	if (m_pDb->executeSimpleStatement(deleteSql) == false)
	{
		deletionStatus = false;
	}

	deleteSql = "DELETE FROM Recipients WHERE CampaignID='";
	deleteSql += m_pDb->escapeString(campaignId);
	if (status.empty() == false)
	{
		deleteSql += "' AND Status='";
		deleteSql += m_pDb->escapeString(status);
	}
	deleteSql += "';";

	if (m_pDb->executeSimpleStatement(deleteSql) == false)
	{
		deletionStatus = false;
	}

	return deletionStatus;
}

bool CampaignSQL::deleteCampaign(const string &campaignId)
{
	bool deletionStatus = true;

	if ((m_pDb == NULL) ||
		(campaignId.empty() == true))
	{
		return false;
	}

	string deleteSql("DELETE FROM CustomFields WHERE RecipientID IN "
		"(SELECT RecipientID FROM Recipients WHERE CampaignID='");
	deleteSql += m_pDb->escapeString(campaignId);
	deleteSql += "');";

	if (m_pDb->executeSimpleStatement(deleteSql) == false)
	{
		deletionStatus = false;
	}

	deleteSql = "DELETE FROM Recipients WHERE CampaignID='";
	deleteSql += m_pDb->escapeString(campaignId);
	deleteSql += "';";

	if (m_pDb->executeSimpleStatement(deleteSql) == false)
	{
		deletionStatus = false;
	}

	deleteSql = "DELETE FROM Attachments WHERE CampaignID='";
	deleteSql += m_pDb->escapeString(campaignId);
	deleteSql += "';";

	if (m_pDb->executeSimpleStatement(deleteSql) == false)
	{
		deletionStatus = false;
	}

	deleteSql = "DELETE FROM Campaigns WHERE CampaignID='";
	deleteSql += m_pDb->escapeString(campaignId);
	deleteSql += "';";

	if (m_pDb->executeSimpleStatement(deleteSql) == false)
	{
		deletionStatus = false;
	}

	return deletionStatus;
}

