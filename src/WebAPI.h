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

#ifndef _WEBAPI_H_
#define _WEBAPI_H_

#include <time.h>
#include <libxml/parser.h>
#include <string>
#include <map>
#include <ostream>

#include "Campaign.h"
#include "CampaignSQL.h"
#include "CSVParser.h"
#include "MessageDetails.h"
#include "Recipient.h"
#include "UsageLogger.h"

/// A pretty-printer class for recipients lists.
class RecipientsPrinter
{
	public:
		RecipientsPrinter();
		virtual ~RecipientsPrinter();

		virtual void print(bool minimumDetails,
			off_t maxCount,
			off_t startOffset,
			off_t totalCount,
			std::map<std::string, Recipient> &recipients) = 0;

};

class WebAPI;

/// Dumps a recipients list as XML.
class RecipientsXMLPrinter : public RecipientsPrinter
{
	public:
		RecipientsXMLPrinter(WebAPI *pAPI);
		virtual ~RecipientsXMLPrinter();

		virtual void print(bool minimumDetails,
			off_t maxCount,
			off_t startOffset,
			off_t totalCount,
			std::map<std::string, Recipient> &recipients);

	protected:
		WebAPI *m_pAPI;

};

/// Dumps a recipients list as CSV.
class RecipientsCSVPrinter : public RecipientsPrinter
{
	public:
		RecipientsCSVPrinter(std::ostream &outputStream);
		virtual ~RecipientsCSVPrinter();

		virtual void print(bool minimumDetails,
			off_t maxCount,
			off_t startOffset,
			off_t totalCount,
			std::map<std::string, Recipient> &recipients);

	protected:
		std::ostream &m_outputStream;

		static std::string quoteColumn(const std::string &text);

};

/// Parses XML sent to the Web API and outputs the response.
class WebAPI
{
	public:
		WebAPI(CampaignSQL *pCampaignData, UsageLogger *pLogger,
			std::ostream &outputStream);
		virtual ~WebAPI();

		/// Encodes XML entities.
		static std::string encodeEntities(const std::string &xml);

		/// Parses the XML request held in a buffer.
		bool parse(const char *pBuffer, unsigned int bufferLen);

		/// Lists campaigns that match the given name or status.
		void listCampaigns(const Campaign &campaign,
			const std::string &detailsLevel, off_t maxCount,
			off_t startOffset);

		/// Lists recipients belonging to the given campaign ID.
		void listRecipients(const Campaign &campaign,
			bool minimumDetails, off_t maxCount,
			off_t startOffset,
			RecipientsPrinter &printer);

		typedef enum { CSV_NAME = 0, CSV_EMAIL_ADDRESS, CSV_STATUS,
			CSV_RETURN_PATH, CSV_TIME_SENT, CSV_NUM_ATTEMPTS,
			CSV_CUSTOM_FIELD1, CSV_CUSTOM_FIELD2, CSV_CUSTOM_FIELD3,
			CSV_CUSTOM_FIELD4, CSV_CUSTOM_FIELD5, CSV_CUSTOM_FIELD6 } CSVColumn;

		/// Imports recipients from a CSV file.
		off_t importCSV(const Campaign &campaign, CSVParser *pParser,
			const std::map<unsigned int, CSVColumn> &columns,
			bool skipHeader);

	protected:
		friend class RecipientsXMLPrinter;
		CampaignSQL *m_pCampaignData;
		UsageLogger *m_pLogger;
		std::ostream &m_outputStream;
		std::string m_callName;
		std::string m_errorMsg;
		int m_errorCode;

		void outputCampaign(const Campaign &campaign,
			const std::string &detailLevels);

		void outputAttachments(const MessageDetails *pDetails,
			bool inlineParts);

		void outputRecipient(const Recipient &recipient,
			bool minimumDetails = false);

		MessageDetails *loadCampaign(xmlNode *pCampaignNode, Campaign &campaign);

		MessageDetails *loadMessage(xmlNode *pMessageNode);

		void loadAttachment(xmlNode *pAttachmentNode, MessageDetails *pDetails);

		void loadRecipient(xmlNode *pRecipientNode, Recipient &recipient);

		bool createAction(xmlNode *pCreateNode);

		bool getAction(xmlNode *pGetNode);

		bool setAction(xmlNode *pSetNode);

		bool importAction(xmlNode *pListNode);

		bool listAction(xmlNode *pListNode);

		bool listChangesAction(xmlNode *pListChangesNode);

		bool deleteAction(xmlNode *pDeleteNode);

	private:
		// WebAPI objects cannot be copied
		WebAPI(const WebAPI &other);
		WebAPI &operator=(const WebAPI &other);

};

#endif // _WEBAPI_H_
