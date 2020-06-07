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

#include <stdlib.h>
#include <strings.h>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <fstream>
#include <sstream>

#include "ConfigurationFile.h"
#include "CSVParser.h"
#include "TimeConverter.h"
#include "WebAPI.h"

#define CREATE_ERROR		100
#define GET_ERROR		110
#define SET_ERROR		120
#define IMPORT_ERROR		130
#define LIST_ERROR		140
#define LIST_CHANGES_ERROR	150
#define DELETE_ERROR		160
#define MISC_ERROR		170

using std::vector;
using std::set;
using std::map;
using std::string;
using std::ostream;
using std::fstream;
using std::stringstream;
using std::ios;

static string trimSpaces(const string &strWithSpaces)
{
	string str(strWithSpaces);
	string::size_type pos = 0;

	while ((str.empty() == false) && (pos < str.length()))
	{
		if (isspace(str[pos]) == 0)
		{
			++pos;
			break;
		}

		str.erase(pos, 1);
	}

	for (pos = str.length() - 1;
			(str.empty() == false) && (pos >= 0); --pos)
	{
		if (isspace(str[pos]) == 0)
		{
			break;
		}

		str.erase(pos, 1);
	}

	return str;
}

RecipientsPrinter::RecipientsPrinter()
{
}

RecipientsPrinter::~RecipientsPrinter()
{
}

RecipientsXMLPrinter::RecipientsXMLPrinter(WebAPI *pAPI) :
	RecipientsPrinter(),
	m_pAPI(pAPI)
{
}

RecipientsXMLPrinter::~RecipientsXMLPrinter()
{
}

void RecipientsXMLPrinter::print(bool minimumDetails,
	off_t maxCount, off_t startOffset,
	off_t totalCount, map<string, Recipient> &recipients)
{
	if (m_pAPI == NULL)
	{
		return;
	}

	char numStr[64];
	snprintf(numStr, 64, "%ld", totalCount);
	m_pAPI->m_outputStream << "<TotalCount>" << numStr << "</TotalCount>\r\n";

	for (map<string, Recipient>::const_iterator recipientIter = recipients.begin();
		recipientIter != recipients.end(); ++recipientIter)
	{
		m_pAPI->outputRecipient(recipientIter->second, minimumDetails);
	}
}

RecipientsCSVPrinter::RecipientsCSVPrinter(ostream &outputStream) :
	RecipientsPrinter(),
	m_outputStream(outputStream)
{
}

RecipientsCSVPrinter::~RecipientsCSVPrinter()
{
}

string RecipientsCSVPrinter::quoteColumn(const string &text)
{
	string quotedText(text);

	string::size_type quotesPos = quotedText.find_first_of("\"\r\n");
	while (quotesPos != string::npos)
	{
		if (quotedText[quotesPos] == '"')
		{
			quotedText.replace(quotesPos, 1, "\"\"");
			quotesPos += 2;
		}
		else
		{
			quotedText.replace(quotesPos, 1, " ");
			++quotesPos;
		}

		if (quotesPos >= quotedText.length())
		{
			break;
		}

		// Next
		quotesPos = quotedText.find_first_of("\"\r\n", quotesPos);
	}

	return string("\"") + quotedText + string("\"");
}

void RecipientsCSVPrinter::print(bool minimumDetails,
	off_t maxCount,
	off_t startOffset,
	off_t totalCount,
	map<string, Recipient> &recipients)
{
	m_outputStream << "ID,NAME,STATUS,STATUSCODE,STATUSMSG,EMAILADDRESS,TIMESENT,NUMATTEMPTS,"
		"CUSTOM1,CUSTOM2,CUSTOM3,CUSTOM4,CUSTOM5,CUSTOM6,RETURNPATH\r\n";
	for (map<string, Recipient>::const_iterator recipientIter = recipients.begin();
		recipientIter != recipients.end(); ++recipientIter)
	{
		Recipient recipient(recipientIter->second);
		string statusCode(recipient.m_statusCode);

		m_outputStream << quoteColumn(recipient.m_id)
			<< "," << quoteColumn(recipient.m_name)
			<< "," << quoteColumn(recipient.m_status)
			<< "," << quoteColumn(statusCode.substr(0, statusCode.find(' ')))
			<< "," << quoteColumn(statusCode.substr(statusCode.find(' ') + 1))
			<< "," << quoteColumn(recipient.m_emailAddress)
			<< "," << quoteColumn(TimeConverter::toTimestamp(recipient.m_timeSent, false))
			<< "," << recipient.m_numAttempts;
		// FIXME: could other fields be in there ?
		for (map<string, string>::const_iterator customIter = recipient.m_customFields.begin();
			customIter != recipient.m_customFields.end(); ++customIter)
		{
			m_outputStream << "," << customIter->second; 
		}
		m_outputStream << "," << recipient.m_returnPathEmailAddress << "\r\n";
		m_outputStream.flush();
	}
}

WebAPI::WebAPI(CampaignSQL *pCampaignData, UsageLogger *pLogger,
	ostream &outputStream) :
	m_pCampaignData(pCampaignData),
	m_pLogger(pLogger),
	m_outputStream(outputStream)
{
}

WebAPI::~WebAPI()
{
}

string WebAPI::encodeEntities(const string &xml)
{
	if (xml.empty() == true)
	{
		return "";
	}

	xmlDoc *pDoc = xmlNewDoc(BAD_CAST"1.0");
	if (pDoc == NULL)
	{
		return "";
	}

	string encodedXml;
	xmlChar *pEncodedXML = xmlEncodeSpecialChars(pDoc, BAD_CAST(xml.c_str()));
	if (pEncodedXML != NULL)
	{
		encodedXml = (const char*)pEncodedXML;

		xmlFree(pEncodedXML);
	}

	// Free the document
	xmlFreeDoc(pDoc);

	return encodedXml;
}

void WebAPI::outputCampaign(const Campaign &campaign, const string &detailsLevel)
{
	m_outputStream << "<Campaign>\r\n";
	m_outputStream << "<Id>" << campaign.m_id << "</Id>\r\n";
	m_outputStream << "<Name>" << WebAPI::encodeEntities(campaign.m_name) << "</Name>\r\n";
	m_outputStream << "<Status>" << campaign.m_status << "</Status>\r\n";
	m_outputStream << "<Timestamp>" << TimeConverter::toDateTime(campaign.m_timestamp) << "</Timestamp>\r\n";

	if ((detailsLevel != "Medium") &&
		(detailsLevel != "Full"))
	{
		m_outputStream << "</Campaign>\r\n";
		return;
	}

	// Get the message details
	MessageDetails *pDetails = m_pCampaignData->getMessage(campaign.m_id);
	if (pDetails != NULL)
	{
		m_outputStream << "<Message>\r\n";
		m_outputStream << "<Subject>" << WebAPI::encodeEntities(pDetails->m_subject) << "</Subject>\r\n";
		m_outputStream << "<FromName>" << WebAPI::encodeEntities(pDetails->m_fromName)
			<< "</FromName><FromEmailAddress>" << WebAPI::encodeEntities(pDetails->m_fromEmailAddress) << "</FromEmailAddress>\r\n";
		m_outputStream << "<ReplyToName>" << WebAPI::encodeEntities(pDetails->m_replyToName)
			<< "</ReplyToName><ReplyToEmailAddress>" << WebAPI::encodeEntities(pDetails->m_replyToEmailAddress) << "</ReplyToEmailAddress>\r\n";
		m_outputStream << "<SenderEmailAddress>" << WebAPI::encodeEntities(pDetails->m_senderEmailAddress) << "</SenderEmailAddress>\r\n";
		m_outputStream << "<UnsubscribeLink>" << WebAPI::encodeEntities(pDetails->m_unsubscribeLink) << "</UnsubscribeLink>\r\n";
		if (detailsLevel == "Full")
		{
			m_outputStream << "<PlainContent>" << WebAPI::encodeEntities(pDetails->getContent("text/plain")) << "</PlainContent>\r\n";
			m_outputStream << "<HtmlContent>" << WebAPI::encodeEntities(pDetails->getContent("/html")) << "</HtmlContent>\r\n";
		}
		outputAttachments(pDetails, false);
		outputAttachments(pDetails, true);
		m_outputStream << "</Message>\r\n";

		delete pDetails;
	}
	m_outputStream << "</Campaign>\r\n";
}

void WebAPI::outputAttachments(const MessageDetails *pDetails, bool inlineParts)
{
        unsigned int attachmentCount = pDetails->getAttachmentCount(inlineParts);
        for (unsigned int attachmentNum = 0; attachmentNum < attachmentCount; ++attachmentNum)
        {
                Attachment *pAttachment = pDetails->getAttachment(attachmentNum, inlineParts);
                if (pAttachment == NULL)
                {
                        continue;
                }

		m_outputStream << "<EmailAttachment><FilePath>" << WebAPI::encodeEntities(pAttachment->m_filePath) << "</FilePath>\r\n";
		m_outputStream << "<ContentId>" << WebAPI::encodeEntities(pAttachment->m_contentId) << "</ContentId>\r\n";
		m_outputStream << "<ContentType>" << WebAPI::encodeEntities(pAttachment->m_contentType) << "</ContentType></EmailAttachment>\r\n";
	}
}

void WebAPI::outputRecipient(const Recipient &recipient, bool minimumDetails)
{
	string statusCode(WebAPI::encodeEntities(recipient.m_statusCode));

	m_outputStream << "<Recipient>\r\n";
	m_outputStream << "<Id>" << recipient.m_id << "</Id>\r\n";
	m_outputStream << "<Name>" << WebAPI::encodeEntities(recipient.m_name) << "</Name>\r\n";
	m_outputStream << "<Status>" << recipient.m_status << "</Status>\r\n";
	m_outputStream << "<StatusCode>" << statusCode.substr(0, statusCode.find(' ')) << "</StatusCode>\r\n";
	m_outputStream << "<StatusString>" << statusCode.substr(statusCode.find(' ') + 1) << "</StatusString>\r\n";
	m_outputStream << "<EmailAddress>" << WebAPI::encodeEntities(recipient.m_emailAddress) << "</EmailAddress>\r\n";
	m_outputStream << "<TimeSent>" << TimeConverter::toDateTime(recipient.m_timeSent) << "</TimeSent>\r\n";
	m_outputStream << "<NumAttempts>" << recipient.m_numAttempts << "</NumAttempts>\r\n";
	if (minimumDetails == false)
	{
		// FIXME: could other fields be in there ?
		for (map<string, string>::const_iterator customIter = recipient.m_customFields.begin();
			customIter != recipient.m_customFields.end(); ++customIter)
		{
			m_outputStream << "<CustomField" << customIter->first
				<< ">" << WebAPI::encodeEntities(customIter->second)
				<< "</CustomField" << customIter->first << ">\r\n";
		}
		m_outputStream << "<ReturnPath>" << WebAPI::encodeEntities(recipient.m_returnPathEmailAddress) << "</ReturnPath>\r\n";
	}
	m_outputStream << "</Recipient>\r\n";
}

MessageDetails *WebAPI::loadCampaign(xmlNode *pCampaignNode, Campaign &campaign)
{
	MessageDetails *pDetails = NULL;

	for (xmlNode *pCampaignChildNode = pCampaignNode->children;
		pCampaignChildNode != NULL; pCampaignChildNode = pCampaignChildNode->next)
	{
		if ((pCampaignChildNode->type != XML_ELEMENT_NODE) ||
			(pCampaignChildNode->name == NULL))
		{
			continue;
		}

		string nodeContent(ConfigurationFile::getNodeContent(pCampaignChildNode));

		if (xmlStrncmp(pCampaignChildNode->name, BAD_CAST"Id", 2) == 0)
		{
			campaign.m_id = nodeContent;
		}
		else if (xmlStrncmp(pCampaignChildNode->name, BAD_CAST"Name", 4) == 0)
		{
			campaign.m_name = nodeContent;
		}
		else if (xmlStrncmp(pCampaignChildNode->name, BAD_CAST"Status", 6) == 0)
		{
			campaign.m_status = nodeContent;
		}
		else if (xmlStrncmp(pCampaignChildNode->name, BAD_CAST"Message", 7) == 0)
		{
			pDetails = loadMessage(pCampaignChildNode);
		}
	}

	return pDetails;
}

MessageDetails *WebAPI::loadMessage(xmlNode *pMessageNode)
{
	MessageDetails *pDetails = new MessageDetails();

	for (xmlNode *pMessageChildNode = pMessageNode->children;
		pMessageChildNode != NULL; pMessageChildNode = pMessageChildNode->next)
	{
		if ((pMessageChildNode->type != XML_ELEMENT_NODE) ||
			(pMessageChildNode->name == NULL))
		{
			continue;
		}

		string nodeContent(ConfigurationFile::getNodeContent(pMessageChildNode));

		if (xmlStrncmp(pMessageChildNode->name, BAD_CAST"Subject", 7) == 0)
		{
			pDetails->m_subject = nodeContent;
		}
		else if (xmlStrncmp(pMessageChildNode->name, BAD_CAST"FromName", 8) == 0)
		{
			pDetails->m_fromName = nodeContent;
		}
		else if (xmlStrncmp(pMessageChildNode->name, BAD_CAST"FromEmailAddress", 16) == 0)
		{
			pDetails->m_fromEmailAddress = trimSpaces(nodeContent);
		}
		else if (xmlStrncmp(pMessageChildNode->name, BAD_CAST"ReplyToName", 11) == 0)
		{
			pDetails->m_replyToName = nodeContent;
		}
		else if (xmlStrncmp(pMessageChildNode->name, BAD_CAST"ReplyToEmailAddress", 19) == 0)
		{
			pDetails->m_replyToEmailAddress = trimSpaces(nodeContent);
		}
		else if (xmlStrncmp(pMessageChildNode->name, BAD_CAST"SenderEmailAddress", 18) == 0)
		{
			pDetails->m_senderEmailAddress = trimSpaces(nodeContent);
		}
		else if (xmlStrncmp(pMessageChildNode->name, BAD_CAST"UnsubscribeLink", 15) == 0)
		{
			pDetails->m_unsubscribeLink = nodeContent;
		}
		else if (xmlStrncmp(pMessageChildNode->name, BAD_CAST"PlainContent", 12) == 0)
		{
			pDetails->setContentPiece("text/plain", nodeContent.c_str(), "", true);
		}
		else if (xmlStrncmp(pMessageChildNode->name, BAD_CAST"HtmlContent", 11) == 0)
		{
			pDetails->setContentPiece("text/html", nodeContent.c_str(), "", true);
		}
		else if (xmlStrncmp(pMessageChildNode->name, BAD_CAST"EmailAttachment", 15) == 0)
		{
			loadAttachment(pMessageChildNode, pDetails);
		}
	}

	return pDetails;
}

void WebAPI::loadAttachment(xmlNode *pAttachmentNode, MessageDetails *pDetails)
{
	for (xmlNode *pAttachmentChildNode = pAttachmentNode->children;
		pAttachmentChildNode != NULL; pAttachmentChildNode = pAttachmentChildNode->next)
	{
		if ((pAttachmentChildNode->type != XML_ELEMENT_NODE) ||
			(pAttachmentChildNode->name == NULL))
		{
			continue;
		}

		string nodeContent(ConfigurationFile::getNodeContent(pAttachmentChildNode));
		string filePath, contentId, contentType;

		if (xmlStrncmp(pAttachmentChildNode->name, BAD_CAST"FilePath", 8) == 0)
		{
			filePath = nodeContent;
		}
		else if (xmlStrncmp(pAttachmentChildNode->name, BAD_CAST"ContentId", 9) == 0)
		{
			contentId = nodeContent;
		}
		else if (xmlStrncmp(pAttachmentChildNode->name, BAD_CAST"ContentType", 11) == 0)
		{
			contentType = nodeContent;
		}

		if (filePath.empty() == false)
		{
			pDetails->addAttachment(new Attachment(filePath,
				contentId,
				contentType));
		}
	}
}

void WebAPI::loadRecipient(xmlNode *pRecipientNode, Recipient &recipient)
{
	for (xmlNode *pRecipientChildNode = pRecipientNode->children;
		pRecipientChildNode != NULL; pRecipientChildNode = pRecipientChildNode->next)
	{
		if ((pRecipientChildNode->type != XML_ELEMENT_NODE) ||
			(pRecipientChildNode->name == NULL))
		{
			continue;
		}

		string nodeContent(ConfigurationFile::getNodeContent(pRecipientChildNode));

		if (xmlStrncmp(pRecipientChildNode->name, BAD_CAST"Id", 2) == 0)
		{
			recipient.m_id = nodeContent;
		}
		else if (xmlStrncmp(pRecipientChildNode->name, BAD_CAST"Name", 4) == 0)
		{
			recipient.m_name = nodeContent;
		}
		else if ((xmlStrncmp(pRecipientChildNode->name, BAD_CAST"Status", 6) == 0) &&
			(xmlStrlen(pRecipientChildNode->name) == 6))
		{
			recipient.m_status = nodeContent;
		}
		else if (xmlStrncmp(pRecipientChildNode->name, BAD_CAST"EmailAddress", 12) == 0)
		{
			recipient.m_emailAddress = trimSpaces(nodeContent);
		}
		else if (xmlStrncmp(pRecipientChildNode->name, BAD_CAST"CustomField", 11) == 0)
		{
			if (xmlStrlen(pRecipientChildNode->name) > 11)
			{
				stringstream nameStr;
				unsigned int customFieldNum = (unsigned int)atoi(((const char*)pRecipientChildNode->name) + 11);

				nameStr << "customfield" << customFieldNum;

				recipient.m_customFields[nameStr.str()] = nodeContent;
			}
		}
		else if (xmlStrncmp(pRecipientChildNode->name, BAD_CAST"ReturnPath", 10) == 0)
		{
			recipient.m_returnPathEmailAddress = trimSpaces(nodeContent);
		}
	}
}

bool WebAPI::createAction(xmlNode *pCreateNode)
{
	Campaign campaign;
	bool creationStatus = false;

	m_errorMsg = "Empty Create block";
	m_errorCode = CREATE_ERROR + 1;

	for (xmlNode *pCreateChildNode = pCreateNode->children;
		pCreateChildNode != NULL; pCreateChildNode = pCreateChildNode->next)
	{
		if ((pCreateChildNode->type != XML_ELEMENT_NODE) ||
			(pCreateChildNode->name == NULL))
		{
			continue;
		}

		if (xmlStrncmp(pCreateChildNode->name, BAD_CAST"Campaign", 8) == 0)
		{
			Campaign thisCampaign;

			MessageDetails *pDetails = loadCampaign(pCreateChildNode, thisCampaign);

			// Check a minimum of information is provided
			if (thisCampaign.m_name.empty() == true)
			{
				m_errorMsg = "Campaign Name not specified";
				m_errorCode = CREATE_ERROR + 2;

				// The campaign Id may be useful when creating Recipient
				campaign = thisCampaign;
			}
			else if ((m_pCampaignData->createNewCampaign(thisCampaign, pDetails) == true) &&
				(thisCampaign.m_id.empty() == false))
			{
				creationStatus = true;

				outputCampaign(thisCampaign, "Full");
			}
			else
			{
				m_errorMsg = "Create failed";
				m_errorCode = CREATE_ERROR + 3;
			}

			if (pDetails != NULL)
			{
				delete pDetails;
			}
		}
		else if (xmlStrncmp(pCreateChildNode->name, BAD_CAST"Recipient", 9) == 0)
		{
			Recipient recipient;

			loadRecipient(pCreateChildNode, recipient);

			// Check a minimum of information is provided
			if (recipient.m_emailAddress.empty() == true)
			{
				m_errorMsg = "Recipient EmailAddress not specified";
				m_errorCode = CREATE_ERROR + 4;
			}
			else if (campaign.m_id.empty() == true)
			{
				m_errorMsg = "Campaign Id not specified";
				m_errorCode = CREATE_ERROR + 5;
			}
			else if (m_pCampaignData->hasRecipient(campaign.m_id, recipient.m_emailAddress) == true)
			{
				m_errorMsg = "Recipient EmailAddress already exists";
				m_errorCode = CREATE_ERROR + 6;
			}
			else if ((m_pCampaignData->createNewRecipient(campaign.m_id, recipient) == true) &&
				(recipient.m_id.empty() == false))
			{
				creationStatus = true;

				outputRecipient(recipient);
			}
			else
			{
				m_errorMsg = "Create failed";
				m_errorCode = CREATE_ERROR + 7;
			}
		}
		else
		{
			m_errorMsg = "Unknown element";
			m_errorCode = CREATE_ERROR + 8;
		}
	}

	return creationStatus;
}

bool WebAPI::getAction(xmlNode *pGetNode)
{
	for (xmlNode *pGetChildNode = pGetNode->children;
		pGetChildNode != NULL; pGetChildNode = pGetChildNode->next)
	{
		if ((pGetChildNode->type != XML_ELEMENT_NODE) ||
			(pGetChildNode->name == NULL))
		{
			continue;
		}

		if (xmlStrncmp(pGetChildNode->name, BAD_CAST"Campaign", 8) == 0)
		{
			Campaign thisCampaign;

			MessageDetails *pDetails = loadCampaign(pGetChildNode, thisCampaign);
			if (pDetails != NULL)
			{
				// This is not useful here
				delete pDetails;
			}

			if (thisCampaign.m_id.empty() == true)
			{
				m_errorMsg = "Campaign Id not specified";
				m_errorCode = GET_ERROR + 1;
			}
			else
			{
				Campaign *pCampaign = m_pCampaignData->getCampaign(thisCampaign.m_id);
				if (pCampaign != NULL)
				{
					// Output all of the campaign's property
					outputCampaign(*pCampaign, "Full");

					delete pCampaign;
				}
				else
				{
					m_outputStream << "<Campaign/>\r\n";
				}
			}
		}
		else if (xmlStrncmp(pGetChildNode->name, BAD_CAST"Recipient", 9) == 0)
		{
			Recipient thisRecipient;

			loadRecipient(pGetChildNode, thisRecipient);

			if (thisRecipient.m_id.empty() == true)
			{
				m_errorMsg = "Recipient Id not specified";
				m_errorCode = GET_ERROR + 2;
			}
			else
			{
				Recipient *pRecipient = m_pCampaignData->getRecipient(thisRecipient.m_id);
				if (pRecipient != NULL)
				{
					outputRecipient(*pRecipient);

					delete pRecipient;
				}
				else
				{
					m_outputStream << "<Recipient/>\r\n";
				}
			}
		}
		else
		{
			m_errorMsg = "Unknown element";
			m_errorCode = GET_ERROR + 3;
			return false;
		}
	}

	return true;
}

bool WebAPI::setAction(xmlNode *pSetNode)
{
	Campaign campaign;
	Recipient recipient;
	MessageDetails *pDetails = NULL;
	string currentRecipientsStatus;
	bool loadedCampaign = false, loadedRecipient = false, setStatus = false;

	recipient.m_status.clear();
	for (xmlNode *pSetChildNode = pSetNode->children;
		pSetChildNode != NULL; pSetChildNode = pSetChildNode->next)
	{
		if ((pSetChildNode->type != XML_ELEMENT_NODE) ||
			(pSetChildNode->name == NULL))
		{
			continue;
		}

		string nodeContent(ConfigurationFile::getNodeContent(pSetChildNode));

		if (xmlStrncmp(pSetChildNode->name, BAD_CAST"Campaign", 8) == 0)
		{
			if (loadedCampaign == false)
			{
				pDetails = loadCampaign(pSetChildNode, campaign);
				loadedCampaign = true;
			}
		}
		else if (xmlStrncmp(pSetChildNode->name, BAD_CAST"Recipient", 9) == 0)
		{
			if (xmlHasProp(pSetChildNode, BAD_CAST"CurrentStatus"))
			{
				currentRecipientsStatus = (const char*)xmlGetProp(pSetChildNode, BAD_CAST"CurrentStatus");
			}

			if (loadedRecipient == false)
			{
				loadRecipient(pSetChildNode, recipient);
				loadedRecipient = true;
			}
		}
		else
		{
			m_errorMsg = "Unknown element";
			m_errorCode = SET_ERROR + 1;

			if (pDetails != NULL)
			{
				delete pDetails;
			}

			return false;
		}
	}

	// Set what ?
	if ((campaign.m_id.empty() == false) &&
		(recipient.m_status.empty() == false))
	{
		if (currentRecipientsStatus == "All")
		{
			currentRecipientsStatus.clear();
		}

		// Set this campaign's recipients status
		if ((recipient.hasValidStatus() == true) &&
			(m_pCampaignData->setRecipientsStatus(campaign.m_id,
				recipient.m_status, currentRecipientsStatus) == true))
		{
			m_outputStream << "<Campaign>\r\n<Id>" << campaign.m_id << "</Id>\r\n</Campaign>\r\n";

			setStatus = true;
		}
		else
		{
			m_errorMsg = "Set failed";
			m_errorCode = SET_ERROR + 2;
		}
	}
	else if (recipient.m_id.empty() == false)
	{
		// Set this recipient's properties
		if (m_pCampaignData->setRecipient(recipient) == true)
		{
			outputRecipient(recipient, false);

			setStatus = true;
		}
		else
		{
			m_errorMsg = "Set failed";
			m_errorCode = SET_ERROR + 3;
		}
	}
	else if (campaign.m_id.empty() == false)
	{
		// Was a message block specified ?
		if (pDetails != NULL)
		{
			// Set the message
			if (m_pCampaignData->updateMessage(campaign.m_id, pDetails) == true)
			{
				setStatus = true;
			}
		}

		// Update properties
		if (m_pCampaignData->setCampaign(campaign) == true)
		{
			setStatus = true;
		}

		if (setStatus == true)
		{
			outputCampaign(campaign, "Full");
		}
		else
		{
			m_errorMsg = "Set failed";
			m_errorCode = SET_ERROR + 4;
		}
	}
	else
	{
		m_errorMsg = "Empty Set block";
		m_errorCode = SET_ERROR + 5;
	}

	if (pDetails != NULL)
	{
		delete pDetails;
	}

	return setStatus;
}

bool WebAPI::importAction(xmlNode *pImportNode)
{
	Campaign campaign;
	fstream fileStream;
	string objects("Campaigns"), sourceCampaignId, recipientsStatus, csvFileName;
	stringstream contentStream;
	unsigned int nameIndex = 0, emailAddressIndex = 1, statusIndex = 2;
	off_t totalCount = 0;
	bool skipHeader = false;

	if (xmlHasProp(pImportNode, BAD_CAST"Objects"))
	{
		objects = (const char*)xmlGetProp(pImportNode, BAD_CAST"Objects");
	}
	if (xmlHasProp(pImportNode, BAD_CAST"SourceCampaignId"))
	{
		sourceCampaignId = (const char*)xmlGetProp(pImportNode, BAD_CAST"SourceCampaignId");
	}
	if (xmlHasProp(pImportNode, BAD_CAST"RecipientsStatus"))
	{
		recipientsStatus = (const char*)xmlGetProp(pImportNode, BAD_CAST"RecipientsStatus");
	}
	if (xmlHasProp(pImportNode, BAD_CAST"File"))
	{
		csvFileName = (const char*)xmlGetProp(pImportNode, BAD_CAST"File");
	}
	if (xmlHasProp(pImportNode, BAD_CAST"SkipHeader"))
	{
		if (atoi((const char*)xmlGetProp(pImportNode, BAD_CAST"SkipHeader")) > 0)
		{
			skipHeader = true;
		}
	}
	if (xmlHasProp(pImportNode, BAD_CAST"Name"))
	{
		nameIndex = (unsigned int)atoi((const char*)xmlGetProp(pImportNode, BAD_CAST"Name"));
	}
	if (xmlHasProp(pImportNode, BAD_CAST"EmailAddress"))
	{
		emailAddressIndex = (unsigned int)atoi((const char*)xmlGetProp(pImportNode, BAD_CAST"EmailAddress"));
	}
	if (xmlHasProp(pImportNode, BAD_CAST"Status"))
	{
		statusIndex = (unsigned int)atoi((const char*)xmlGetProp(pImportNode, BAD_CAST"Status"));
	}

	if (objects != "Recipients")
	{
		m_errorMsg = "Unknown object types";
		m_errorCode = IMPORT_ERROR + 1;
		return false;
	}

	for (xmlNode *pImportChildNode = pImportNode->children;
		pImportChildNode != NULL; pImportChildNode = pImportChildNode->next)
	{
		if ((pImportChildNode->type != XML_ELEMENT_NODE) ||
			(pImportChildNode->name == NULL))
		{
			continue;
		}

		if (xmlStrncmp(pImportChildNode->name, BAD_CAST"Campaign", 8) == 0)
		{
			MessageDetails *pDetails = loadCampaign(pImportChildNode, campaign);

			if (pDetails != NULL)
			{
				delete pDetails;
			}
		}
		else if ((csvFileName.empty() == true) &&
			(xmlStrncmp(pImportChildNode->name, BAD_CAST"Content", 7) == 0))
		{
			contentStream << ConfigurationFile::getNodeContent(pImportChildNode);
		}
	}

	if (campaign.m_id.empty() == true)
	{
		m_errorMsg = "Campaign Id not specified";
		m_errorCode = IMPORT_ERROR + 2;
		return false;
	}

	CSVParser *pParser = NULL;

	if (sourceCampaignId.empty() == false)
	{
		map<string, Recipient> recipients;

		// Copy from another campaign
		if (recipientsStatus == "All")
		{
			recipientsStatus.clear();
		}
		else if ((recipientsStatus.empty() == false) &&
			(Recipient::isValidStatus(recipientsStatus) == false))
		{
			m_errorMsg = "Invalid Recipients Status";
			m_errorCode = IMPORT_ERROR + 3;
			return false;
		}
		if (sourceCampaignId == campaign.m_id)
		{
			m_errorMsg = "Invalid Source Campaign Id";
			m_errorCode = IMPORT_ERROR + 4;
			return false;
		}

		// Get all recipients from the source campaign
		totalCount = m_pCampaignData->countRecipients(sourceCampaignId,
			recipientsStatus, "", false);
		m_pCampaignData->getRecipients(sourceCampaignId, recipientsStatus,
			"", totalCount, recipients);
		// ...and insert them into the destination campaign
		for (map<string, Recipient>::const_iterator recipientIter = recipients.begin();
			recipientIter != recipients.end(); ++recipientIter)
		{
			Recipient recipient(recipientIter->second);

			// Don't import recipients with a known email address
			if ((m_pCampaignData->hasRecipient(campaign.m_id, recipient.m_emailAddress) == true) ||
				(m_pCampaignData->createNewRecipient(campaign.m_id, recipient) == false) ||
				(recipient.m_id.empty() == true))
			{
				// This one failed
				--totalCount;
			}
		}
	}
	else if (csvFileName.empty() == false)
	{
		// Read from a CSV file
		fileStream.open(csvFileName.c_str(), ios::in);
		if (fileStream.is_open() == false)
		{
			m_errorMsg = "Couldn't open file ";
			m_errorMsg += csvFileName;
			m_errorCode = IMPORT_ERROR + 5;
			return false;
		}

		pParser = new CSVParser(fileStream);
	}
	else
	{
		// Read from the CSV data passed in Content
		pParser = new CSVParser(contentStream);
	}

	if (pParser != NULL)
	{
		map<unsigned int, CSVColumn> columns;
		char numStr[64];
		off_t totalCount = 0;

		// Each field maps to one column
		columns[nameIndex] = CSV_NAME;
		columns[emailAddressIndex] = CSV_EMAIL_ADDRESS;
		columns[statusIndex] = CSV_STATUS;

		totalCount = importCSV(campaign, pParser, columns, skipHeader);
		snprintf(numStr, 64, "%ld", totalCount);
		m_outputStream << "<TotalCount>" << numStr << "</TotalCount>\r\n";

		delete pParser;
	}

	return true;
}

bool WebAPI::listAction(xmlNode *pListNode)
{
	string objects("Campaigns"), detailsLevel("Min");
	off_t maxCount = 10, startOffset = 0;

	if (xmlHasProp(pListNode, BAD_CAST"Objects"))
	{
		objects = (const char*)xmlGetProp(pListNode, BAD_CAST"Objects");
	}
	if (xmlHasProp(pListNode, BAD_CAST"DetailsLevel"))
	{
		detailsLevel = (const char*)xmlGetProp(pListNode, BAD_CAST"DetailsLevel");
	}
	if (xmlHasProp(pListNode, BAD_CAST"MaxCount"))
	{
		maxCount = (unsigned int)atoi((const char*)xmlGetProp(pListNode, BAD_CAST"MaxCount"));
	}
	if (xmlHasProp(pListNode, BAD_CAST"StartOffset"))
	{
		startOffset = (unsigned int)atoi((const char*)xmlGetProp(pListNode, BAD_CAST"StartOffset"));
	}

	if (objects == "Campaigns")
	{
		Campaign campaign;
		bool loadedCampaign = false;

		for (xmlNode *pListChildNode = pListNode->children;
			pListChildNode != NULL; pListChildNode = pListChildNode->next)
		{
			if ((pListChildNode->type != XML_ELEMENT_NODE) ||
				(pListChildNode->name == NULL))
			{
				continue;
			}

			if (xmlStrncmp(pListChildNode->name, BAD_CAST"Campaign", 8) == 0)
			{
				MessageDetails *pDetails = loadCampaign(pListChildNode, campaign);
				if (pDetails != NULL)
				{
					// This is not useful here
					delete pDetails;
				}
				loadedCampaign = true;
			}
			else
			{
				m_errorMsg = "Unknown element";
				m_errorCode = LIST_ERROR + 1;
				return false;
			}
		}

		if (loadedCampaign == false)
		{
			campaign.m_status.clear();
		}
		listCampaigns(campaign, detailsLevel, maxCount, startOffset);
	}
	else if (objects == "Recipients")
	{
		Campaign campaign;

		for (xmlNode *pListChildNode = pListNode->children;
			pListChildNode != NULL; pListChildNode = pListChildNode->next)
		{
			if ((pListChildNode->type != XML_ELEMENT_NODE) ||
				(pListChildNode->name == NULL))
			{
				continue;
			}

			if (xmlStrncmp(pListChildNode->name, BAD_CAST"Campaign", 8) == 0)
			{
				MessageDetails *pDetails = loadCampaign(pListChildNode, campaign);
				if (pDetails != NULL)
				{
					// This is not useful here
					delete pDetails;
				}

				if (campaign.m_id.empty() == true)
				{
					m_errorMsg = "Campaign Id not specified";
					m_errorCode = LIST_ERROR + 2;
					return false;
				}
			}
			else
			{
				m_errorMsg = "Unknown element";
				m_errorCode = LIST_ERROR + 3;
				return false;
			}
		}

		RecipientsXMLPrinter printer(this);
		listRecipients(campaign, false, maxCount, startOffset, printer);
	}
	else
	{
		m_errorMsg = "Unknown object types";
		m_errorCode = LIST_ERROR + 4;
		return false;
	}

	return true;
}

bool WebAPI::listChangesAction(xmlNode *pListChangesNode)
{
	vector<string> ids;
	string since;

	if (xmlHasProp(pListChangesNode, BAD_CAST"Since"))
	{
		since = (const char*)xmlGetProp(pListChangesNode, BAD_CAST"Since");
	}

	if (since.empty() == true)
	{
		m_errorMsg = "Since not specified";
		m_errorCode = LIST_CHANGES_ERROR + 1;
		return false;
	}

	time_t sinceTime = TimeConverter::fromDateTime(since, true);

	m_pCampaignData->getChangedCampaigns(sinceTime, ids);
	for (vector<string>::const_iterator idIter = ids.begin();
		idIter != ids.end(); ++idIter)
	{
		m_outputStream << "<Campaign>\r\n<Id>" << *idIter << "</Id>\r\n</Campaign>\r\n";
	}

	ids.clear();
	m_pCampaignData->getChangedRecipients(sinceTime, ids);
	for (vector<string>::const_iterator idIter = ids.begin();
		idIter != ids.end(); ++idIter)
	{
		m_outputStream << "<Recipient>\r\n<Id>" << *idIter << "</Id>\r\n</Recipient>\r\n";
	}

	return true;
}

bool WebAPI::deleteAction(xmlNode *pDeleteNode)
{
	Campaign campaign;
	Recipient recipient;
	bool loadedCampaign = false, loadedRecipient = false, deletionStatus = false;

	recipient.m_status.clear();
	for (xmlNode *pDeleteChildNode = pDeleteNode->children;
		pDeleteChildNode != NULL; pDeleteChildNode = pDeleteChildNode->next)
	{
		if ((pDeleteChildNode->type != XML_ELEMENT_NODE) ||
			(pDeleteChildNode->name == NULL))
		{
			continue;
		}

		if (xmlStrncmp(pDeleteChildNode->name, BAD_CAST"Campaign", 8) == 0)
		{
			if (loadedCampaign == false)
			{
				MessageDetails *pDetails = loadCampaign(pDeleteChildNode, campaign);
				if (pDetails != NULL)
				{
					// This is not useful here
					delete pDetails;
				}
				loadedCampaign = true;
			}
		}
		else if (xmlStrncmp(pDeleteChildNode->name, BAD_CAST"Recipient", 9) == 0)
		{
			if (loadedRecipient == false)
			{
				loadRecipient(pDeleteChildNode, recipient);
				loadedRecipient = true;
			}
		}
		else
		{
			m_errorMsg = "Unknown element";
			m_errorCode = DELETE_ERROR + 1;
			return false;
		}
	}

	// Delete what ?
	if ((campaign.m_id.empty() == false) &&
		(recipient.m_status.empty() == false))
	{
		if (recipient.m_status == "All")
		{
			recipient.m_status.clear();
		}
		else if (recipient.hasValidStatus() == false)
		{
			m_errorMsg = "Invalid Recipient Status";
			m_errorCode = DELETE_ERROR + 2;
			return false;
		}

		// Delete the recipients with this status in this campaign
		if (m_pCampaignData->deleteRecipients(campaign.m_id,
			recipient.m_status) == true)
		{
			m_outputStream << "<Campaign>\r\n<Id>" << campaign.m_id << "</Id>\r\n</Campaign>\r\n";

			deletionStatus = true;
		}
		else
		{
			m_errorMsg = "Delete failed";
			m_errorCode = DELETE_ERROR + 3;
		}
	}
	else if (recipient.m_id.empty() == false)
	{
		// Delete this recipient
		if (m_pCampaignData->deleteRecipient(recipient.m_id) == true)
		{
			m_outputStream << "<Recipient>\r\n<Id>" << recipient.m_id << "</Id>\r\n</Recipient>\r\n";

			deletionStatus = true;
		}
		else
		{
			m_errorMsg = "Delete failed";
			m_errorCode = DELETE_ERROR + 4;
		}
	}
	else if (campaign.m_id.empty() == false)
	{
		// Delete this campaign
		if (m_pCampaignData->deleteCampaign(campaign.m_id) == true)
		{
			m_outputStream << "<Campaign>\r\n<Id>" << campaign.m_id << "</Id>\r\n</Campaign>\r\n";

			deletionStatus = true;
		}
		else
		{
			m_errorMsg = "Delete failed";
			m_errorCode = DELETE_ERROR + 5;
		}
	}
	else
	{
		m_errorMsg = "Empty Delete block";
		m_errorCode = DELETE_ERROR + 6;
	}

	return deletionStatus;
}

bool WebAPI::parse(const char *pBuffer, unsigned int bufferLen)
{
	xmlDoc *pDoc = NULL;
	xmlNode *pRootElement = NULL;
	bool parsedOk = false;

	m_callName.clear();
	m_errorMsg = "Invalid request";
	m_errorCode = MISC_ERROR + 1;

	// Initialize the library and check potential ABI mismatches between
	// the version it was compiled for and the actual shared library used
	LIBXML_TEST_VERSION

	if ((pBuffer != NULL) &&
		(bufferLen > 0))
	{
		// Parse the buffer and get the document
		pDoc = xmlParseMemory(pBuffer, (int)bufferLen);
	}

	if (pDoc == NULL)
	{
		m_outputStream << "<Error ErrorCode=\"" << m_errorCode << "\">" << m_errorMsg << "</Error>\r\n";
		if (m_pLogger != NULL)
		{
			m_pLogger->logRequest("", m_errorCode);
		}
	}
	else
	{
		// Get the root element node
		pRootElement = xmlDocGetRootElement(pDoc);

		for (xmlNode *pCurrentNode = pRootElement->children; pCurrentNode != NULL;
			pCurrentNode = pCurrentNode->next)
		{
			// What type of tag is it ?
			if ((pCurrentNode->type != XML_ELEMENT_NODE) ||
				(pCurrentNode->name == NULL))
			{
				continue;
			}

			// What tag is it ?
			if (xmlStrncmp(pCurrentNode->name, BAD_CAST"Create", 6) == 0)
			{
				m_callName = "Create";
				if (createAction(pCurrentNode) == true)
				{
					parsedOk = true;
				}
			}
			else if (xmlStrncmp(pCurrentNode->name, BAD_CAST"Get", 3) == 0)
			{
				m_callName = "Get";
				if (getAction(pCurrentNode) == true)
				{
					parsedOk = true;
				}
			}
			else if (xmlStrncmp(pCurrentNode->name, BAD_CAST"Set", 3) == 0)
			{
				m_callName = "Set";
				if (setAction(pCurrentNode) == true)
				{
					parsedOk = true;
				}
			}
			else if (xmlStrncmp(pCurrentNode->name, BAD_CAST"Import", 6) == 0)
			{
				m_callName = "Import";
				if (importAction(pCurrentNode) == true)
				{
					parsedOk = true;
				}
			}
			else if ((xmlStrncmp(pCurrentNode->name, BAD_CAST"List", 4) == 0) &&
				(xmlStrlen(pCurrentNode->name) == 4))
			{
				m_callName = "List";
				if (listAction(pCurrentNode) == true)
				{
					parsedOk = true;
				}
			}
			else if ((xmlStrncmp(pCurrentNode->name, BAD_CAST"ListChanges", 11) == 0) &&
				(xmlStrlen(pCurrentNode->name) == 11))
			{
				m_callName = "ListChanges";
				if (listChangesAction(pCurrentNode) == true)
				{
					parsedOk = true;
				}
			}
			else if (xmlStrncmp(pCurrentNode->name, BAD_CAST"Delete", 6) == 0)
			{
				m_callName = "Delete";
				if (deleteAction(pCurrentNode) == true)
				{
					parsedOk = true;
				}
			}
			else
			{
				m_errorMsg = "Unsupported command";
				m_errorCode = MISC_ERROR + 2;
			}
		}

		if (parsedOk == false)
		{
			// Output an Error block
			m_outputStream << "<Error>" << m_errorMsg << "</Error>\r\n";
		}
		else
		{
			m_errorCode = 0;
		}

		// Log this call
		if (m_pLogger != NULL)
		{
			m_pLogger->logRequest(m_callName, m_errorCode);
		}

		// Reset those
		m_callName.clear();
		m_errorMsg = "Invalid request";
		m_errorCode = MISC_ERROR + 3;

		// Free the document
		xmlFreeDoc(pDoc);
	}

	// Cleanup
	xmlCleanupParser();

	return parsedOk;
}

void WebAPI::listCampaigns(const Campaign &campaign, const string &detailsLevel,
	off_t maxCount, off_t startOffset)
{
	CampaignSQL::GetCriteria criteriaType = CampaignSQL::NONE;
	set<Campaign> campaigns;
	string criteriaValue;
	char numStr[64];
	off_t totalCount = 0;

	if (campaign.m_status.empty() == false)
	{
		criteriaType = CampaignSQL::STATUS;
		criteriaValue = campaign.m_status;
	}
	else if (campaign.m_name.empty() == false)
	{
		criteriaType = CampaignSQL::NAME;
		criteriaValue = campaign.m_name;
	}

	// Get the campaigns
	m_pCampaignData->getCampaigns(criteriaValue, criteriaType,
		maxCount, startOffset, totalCount, campaigns);

	snprintf(numStr, 64, "%ld", totalCount);

	m_outputStream << "<TotalCount>" << numStr << "</TotalCount>\r\n";

	for (set<Campaign>::const_iterator campaignIter = campaigns.begin();
		campaignIter != campaigns.end(); ++campaignIter)
	{
		outputCampaign(*campaignIter, detailsLevel);
	}
}

void WebAPI::listRecipients(const Campaign &campaign, bool minimumDetails,
	off_t maxCount, off_t startOffset,
	RecipientsPrinter &printer)
{
	map<string, Recipient> recipients;
	off_t totalCount = 0;

	totalCount = m_pCampaignData->countRecipients(campaign.m_id, "", "", false);

	// Get the recipients
	m_pCampaignData->getRecipients(campaign.m_id,
		maxCount, startOffset, recipients);

	printer.print(minimumDetails, maxCount, startOffset, totalCount, recipients);
}

off_t WebAPI::importCSV(const Campaign &campaign, CSVParser *pParser,
	const map<unsigned int, CSVColumn> &columns, bool skipHeader)
{
	off_t totalCount = 0;

	if ((pParser != NULL) &&
		(skipHeader == true))
	{
		// Consume the first line
		pParser->nextLine();
	}
	// Load recipients, line by line
	while ((pParser != NULL) &&
		(pParser->nextLine() == true))
	{
		Recipient recipient;
		string column;
		unsigned int columnNum = 0;

		// ...and column by column
		while (pParser->nextColumn(column) == true)
		{
			// What field is this column mapped to ?
			map<unsigned int, CSVColumn>::const_iterator colIter = columns.find(columnNum);
			if (colIter == columns.end())
			{
				continue;
			}

			// Fields may span several columns
			// Some are concatenated in the same order they are found, others are set to the last value found
			if (colIter->second == CSV_NAME)
			{
				if (recipient.m_name.empty() == false)
				{
					recipient.m_name += " ";
				}
				recipient.m_name += column;
			}
			else if (colIter->second == CSV_EMAIL_ADDRESS)
			{
				if (recipient.m_emailAddress.empty() == false)
				{
					recipient.m_emailAddress += " ";
				}
				recipient.m_emailAddress += trimSpaces(column);
			}
			else if (colIter->second == CSV_STATUS)
			{
				if (Recipient::isValidStatus(column) == true)
				{
					recipient.m_status = column;
				}
			}
			else if (colIter->second == CSV_RETURN_PATH)
			{
				if (recipient.m_returnPathEmailAddress.empty() == false)
				{
					recipient.m_returnPathEmailAddress += " ";
				}
				recipient.m_returnPathEmailAddress += trimSpaces(column);
			}
			else if (colIter->second == CSV_TIME_SENT)
			{
				recipient.m_timeSent = (time_t)atoi(column.c_str());
			}
			else if (colIter->second == CSV_NUM_ATTEMPTS)
			{
				recipient.m_numAttempts = (off_t)atoll(column.c_str());
			}
			else if (colIter->second == CSV_CUSTOM_FIELD1)
			{
				recipient.m_customFields["customfield1"] = column;
			}
			else if (colIter->second == CSV_CUSTOM_FIELD2)
			{
				recipient.m_customFields["customfield2"] = column;
			}
			else if (colIter->second == CSV_CUSTOM_FIELD3)
			{
				recipient.m_customFields["customfield3"] = column;
			}
			else if (colIter->second == CSV_CUSTOM_FIELD4)
			{
				recipient.m_customFields["customfield4"] = column;
			}
			else if (colIter->second == CSV_CUSTOM_FIELD5)
			{
				recipient.m_customFields["customfield5"] = column;
			}
			else if (colIter->second == CSV_CUSTOM_FIELD6)
			{
				recipient.m_customFields["customfield6"] = column;
			}

			++columnNum;
		}

		// Create this recipient if the email address is new
		if ((m_pCampaignData->hasRecipient(campaign.m_id, recipient.m_emailAddress) == false) &&
			(m_pCampaignData->createNewRecipient(campaign.m_id, recipient) == true) &&
			(recipient.m_id.empty() == false))
		{
			++totalCount;
		}
	}

	return totalCount;
}

