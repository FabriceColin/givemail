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

#ifndef _CAMPAIGNSQL_H_
#define _CAMPAIGNSQL_H_

#include <time.h>
#include <string>
#include <vector>
#include <set>
#include <map>

#include "Campaign.h"
#include "MessageDetails.h"
#include "SQLDB.h"
#include "Recipient.h"
#include "SMTPSession.h"

/// A wrapper for all information about campaigns and recipients held in the database.
class CampaignSQL
{
	public:
		CampaignSQL(SQLDB *pDb);
		virtual ~CampaignSQL();

		typedef enum { NONE = 0, STATUS, NAME } GetCriteria;

		/// Creates a new campaign.
		bool createNewCampaign(Campaign &campaign, MessageDetails *pDetails);

		/// Gets a campaign.
		Campaign *getCampaign(const std::string &campaignId);

		/**
		  * Gets a list of campaigns.
		  * Status may be empty if no filtering is desirable.
		  */
		bool getCampaigns(const std::string &criteriaValue, GetCriteria criteriaType,
			off_t maxCount, off_t startOffset, off_t &totalCount,
			std::set<Campaign> &campaigns);

		/// Gets a list of campaigns that have changed since a given time.
		bool getChangedCampaigns(time_t sinceTime, std::vector<std::string> &campaignIds);

		/**
		  * Gets a list of Sent campaigns that have recipients with
		  * temporary failures. The time boundaries apply to the campaigns
		  * processing time and the recipients send time.
		  */
		bool getCampaignsWithTemporaryFailures(time_t oldestTime, time_t newestTime,
			off_t maxCount, off_t startOffset, off_t &totalCount,
			std::set<Campaign> &campaigns);

		/// Updates a campaign's message details.
		bool updateMessage(const std::string &campaignId,
			const MessageDetails *pDetails);

		/// Gets a campaign's message details.
		MessageDetails *getMessage(const std::string &campaignId);

		/// Checks whether a recipient already exists with this email address.
		bool hasRecipient(const std::string &campaignId,
			const std::string &emailAddress);

		/// Creates a new recipient.
		bool createNewRecipient(const std::string &campaignId, Recipient &recipient);

		/**
		  * Lists domains.
		  * Status may not be empty.
		  */
		off_t listDomains(const std::string &campaignId,
			const std::string &status,
			std::multimap<off_t, std::string> &domainsBreakdown,
			off_t count = 0, off_t offset = 0);

		/// Gets a recipient.
		Recipient *getRecipient(const std::string &recipientId);

		/// Counts recipients.
		off_t countRecipients(const std::string &campaignId,
			const std::string &status, const std::string &statusCode,
			bool isLike);

		/// Gets a list of recipients.
		bool getRecipients(const std::string &campaignId,
			off_t maxCount, off_t startOffset,
			std::map<std::string, Recipient> &recipients);

		/**
		  * Gets a list of recipients.
		  * Status and domain name may be empty if no filtering is desirable.
		  */
		bool getRecipients(const std::string &campaignId,
			const std::string &status, const std::string &domainName,
			off_t maxCount,
			std::map<std::string, Recipient> &recipients);

		/// Gets a list of recipients that have changed since a given time.
		bool getChangedRecipients(time_t sinceTime, std::vector<std::string> &recipientIds);

		/// Resets failed recipients to Waiting.
		bool resetFailedRecipients(const std::string &campaignId,
			const std::string &statuscode, bool isLike);

		/// Sets a campaign's properties.
		bool setCampaign(const Campaign &campaign);

		/// Sets a recipient's properties.
		bool setRecipient(Recipient &recipient);

		/// Sets recipients' status.
		bool setRecipientsStatus(const std::string &campaignId,
			const std::string &status, const std::string &currentStatus);

		/// Deletes a recipient.
		bool deleteRecipient(const std::string &recipientId);

		/// Deletes recipients with the given status, or all if status is empty.
		bool deleteRecipients(const std::string &campaignId,
			const std::string &status);

		/// Deletes a campaign and all its recipients.
		bool deleteCampaign(const std::string &campaignId);

	protected:
		SQLDB *m_pDb;

		void setAttachments(const std::string &campaignId,
			const MessageDetails *pDetails,
			bool inlineParts);

		bool getAttachments(const std::string &campaignId,
			MessageDetails *pDetails);

		bool getCustomFields(Recipient *pRecipient);

	private:
		CampaignSQL(const CampaignSQL &other);
		CampaignSQL &operator=(const CampaignSQL &other);

};

#endif // _CAMPAIGNSQL_H_
