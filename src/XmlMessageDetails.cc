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
#include <string.h>
#include <libxml/xmlreader.h>
#include <iostream>
#include <sstream>
#include <string>

#include "XmlMessageDetails.h"

using std::clog;
using std::endl;
using std::string;
using std::stringstream;

XmlMessageDetails::XmlMessageDetails() :
	MessageDetails()
{
}

XmlMessageDetails::~XmlMessageDetails()
{
}

bool XmlMessageDetails::parse(const string &fileName)
{
	xmlDoc *pDoc = NULL;
	bool parsedDoc = false;

	if (fileName.empty() == true)
	{
		return false;
	}

	// Initialize the library and check potential ABI mismatches between
	// the version it was compiled for and the actual shared library used
	LIBXML_TEST_VERSION

	// Parse the file and get the document
#if LIBXML_VERSION < 20600
	pDoc = xmlParseFile(fileName.c_str());
#else
	pDoc = xmlReadFile(fileName.c_str(), NULL, XML_PARSE_NOCDATA);
#endif
	if (pDoc != NULL)
	{
		parse(pDoc);
		parsedDoc = true;

		// Free the document
		xmlFreeDoc(pDoc);
	}

	// Cleanup
	xmlCleanupParser();

	return parsedDoc;
}

bool XmlMessageDetails::parse(const char *pBuffer, int size)
{
	xmlDoc *pDoc = NULL;
	bool parsedDoc = false;

	if ((pBuffer == NULL) ||
		(size == 0))
	{
		return false;
	}

	// Initialize the library and check potential ABI mismatches between
	// the version it was compiled for and the actual shared library used
	LIBXML_TEST_VERSION

	// Parse the buffer and get the document
	pDoc = xmlParseMemory(pBuffer, size);
	if (pDoc != NULL)
	{
		parse(pDoc);
		parsedDoc = true;

		// Free the document
		xmlFreeDoc(pDoc);
	}

	// Cleanup
	xmlCleanupParser();

	return parsedDoc;
}

void XmlMessageDetails::parse(xmlDoc *pDoc)
{
	if (pDoc == NULL)
	{
		return;
	}

	if (pDoc->encoding != NULL)
	{
		m_charset = (const char*)pDoc->encoding;
	}

	// Get the root element node
	xmlNode *pRootElement = xmlDocGetRootElement(pDoc);

	char *pRootElementAttr = (char*)xmlGetProp(pRootElement, BAD_CAST"version");
	if (pRootElementAttr != NULL)
	{
		m_version = (unsigned int)atoi((const char*)pRootElementAttr);

		xmlFree(pRootElementAttr);
	}

	for (xmlNode *pCurrentNode = pRootElement->children; pCurrentNode != NULL;
		pCurrentNode = pCurrentNode->next)
	{
		// What type of tag is it ?
		if (pCurrentNode->type != XML_ELEMENT_NODE)
		{
			continue;
		}

		// Get the element's content
		char *pContent = (char*)xmlNodeGetContent(pCurrentNode);
		if (pContent == NULL)
		{
			continue;
		}

		// What tag is it ?
		if (xmlStrncmp(pCurrentNode->name, BAD_CAST"subject", 7) == 0)
		{
			m_subject = (const char*)pContent;
		}
		else if ((xmlStrncmp(pCurrentNode->name, BAD_CAST"from", 4) == 0) ||
			(xmlStrncmp(pCurrentNode->name, BAD_CAST"replyto", 7) == 0))
		{
			bool isReplyTo = false;

			if (xmlStrncmp(pCurrentNode->name, BAD_CAST"replyto", 7) == 0)
			{
				isReplyTo = true;
			}

			for (xmlNode *pCurrentFromNode = pCurrentNode->children;
				pCurrentFromNode != NULL; pCurrentFromNode = pCurrentFromNode->next)
			{
				if (pCurrentFromNode->type != XML_ELEMENT_NODE)
				{
					continue;
				}

				char *pChildContent = (char*)xmlNodeGetContent(pCurrentFromNode);
				if (pChildContent == NULL)
				{
					continue;
				}

				if (xmlStrncmp(pCurrentFromNode->name, BAD_CAST"name", 4) == 0)
				{
					if (isReplyTo == false)
					{
						m_fromName = (const char*)pChildContent;
					}
					else
					{
						m_replyToName = (const char*)pChildContent;
					}
				}
				else if (xmlStrncmp(pCurrentFromNode->name, BAD_CAST"emailaddress", 12) == 0)
				{
					if (isReplyTo == false)
					{
						m_fromEmailAddress = (const char*)pChildContent;
					}
					else
					{
						m_replyToEmailAddress = (const char*)pChildContent;
					}
				}

				// Free
				xmlFree(pChildContent);
			}
		}
		else if (xmlStrncmp(pCurrentNode->name, BAD_CAST"sender", 6) == 0)
		{
			m_senderEmailAddress = (const char*)pContent;
		}
		else if (xmlStrncmp(pCurrentNode->name, BAD_CAST"to", 2) == 0)
		{
			m_to = (const char*)pContent;
		}
		else if (xmlStrncmp(pCurrentNode->name, BAD_CAST"cc", 2) == 0)
		{
			m_cc = (const char*)pContent;
		}
		else if (xmlStrncmp(pCurrentNode->name, BAD_CAST"reporttype", 10) == 0)
		{
			m_reportType = (const char*)pContent;
		}
		else if (xmlStrncmp(pCurrentNode->name, BAD_CAST"useragent", 9) == 0)
		{
			m_userAgent = (const char*)pContent;
			m_useXMailer = false;
		}
		else if (xmlStrncmp(pCurrentNode->name, BAD_CAST"xmailer", 7) == 0)
		{
			m_userAgent = (const char*)pContent;
			m_useXMailer = true;
		}
		else if (xmlStrncmp(pCurrentNode->name, BAD_CAST"plaintext", 9) == 0)
		{
			string encoding;

			char *pNodeAttr = (char*)xmlGetProp(pCurrentNode, BAD_CAST"encoding");
			if (pNodeAttr != NULL)
			{
				encoding = pNodeAttr;

				xmlFree(pNodeAttr);
			}

			setContentPiece("text/plain", pContent, encoding);
		}
		else if (xmlStrncmp(pCurrentNode->name, BAD_CAST"html", 4) == 0)
		{
			string encoding;

			char *pNodeAttr = (char*)xmlGetProp(pCurrentNode, BAD_CAST"encoding");
			if (pNodeAttr != NULL)
			{
				encoding = pNodeAttr;

				xmlFree(pNodeAttr);
			}

			setContentPiece("text/html", pContent, encoding);
		}
		else if (xmlStrncmp(pCurrentNode->name, BAD_CAST"attachment", 10) == 0)
		{
			string filePath, contentType, contentId, encoding, checkHTML;
			char *pAttachmentContent = NULL;
			size_t contentLength = 0;

			// Get attributes
			char *pNodeAttr = (char*)xmlGetProp(pCurrentNode, BAD_CAST"type");
			if (pNodeAttr != NULL)
			{
				contentType = pNodeAttr;

				xmlFree(pNodeAttr);
			}
			pNodeAttr = (char*)xmlGetProp(pCurrentNode, BAD_CAST"id");
			if (pNodeAttr != NULL)
			{
				contentId = pNodeAttr;

				xmlFree(pNodeAttr);
			}
			pNodeAttr = (char*)xmlGetProp(pCurrentNode, BAD_CAST"encoding");
			if (pNodeAttr != NULL)
			{
				encoding = pNodeAttr;

				xmlFree(pNodeAttr);
			}
			pNodeAttr = (char*)xmlGetProp(pCurrentNode, BAD_CAST"checkhtml");
			if (pNodeAttr != NULL)
			{
				checkHTML = pNodeAttr;

				xmlFree(pNodeAttr);
			}

			for (xmlNode *pCurrentAttachNode = pCurrentNode->children;
				pCurrentAttachNode != NULL; pCurrentAttachNode = pCurrentAttachNode->next)
			{
				if (pCurrentAttachNode->type != XML_ELEMENT_NODE)
				{
					continue;
				}

				char *pChildContent = (char*)xmlNodeGetContent(pCurrentAttachNode);
				if (pChildContent == NULL)
				{
					continue;
				}

				if (xmlStrncmp(pCurrentAttachNode->name, BAD_CAST"filename", 8) == 0)
				{
					filePath = (const char*)pChildContent;
				}
				else if (xmlStrncmp(pCurrentAttachNode->name, BAD_CAST"content", 7) == 0)
				{
					contentLength = (off_t)strlen((const char*)pChildContent);
					pAttachmentContent = new char[contentLength + 1];

					strncpy(pAttachmentContent, (const char*)pChildContent, contentLength);
					pAttachmentContent[contentLength] = '\0';
				}

				// Free
				xmlFree(pChildContent);
			}

			if (contentId.empty() == true)
			{
				// If id is unset, don't bother with checkhtml
				checkHTML.clear();
			}

			if (filePath.empty() == true)
			{
				if (pAttachmentContent != NULL)
				{
					delete[] pAttachmentContent;
					pAttachmentContent = NULL;
					contentLength = 0;
				}
			}
			else if ((checkHTML != "YES") ||
				(getContent("/html").find(contentId) != string::npos))
			{
				Attachment *pAttachment = new Attachment(filePath,
					contentType, contentId, encoding);

#ifdef DEBUG
				clog << "XmlMessageDetails::parse: content for attachment " << filePath << " with Content-Id "
					<< contentId << " is " << contentLength << " bytes big" << endl;
#endif

				pAttachment->m_pContent = pAttachmentContent;
				pAttachment->m_contentLength = contentLength;
				addAttachment(pAttachment);
			}
		}
		else if (xmlStrncmp(pCurrentNode->name, BAD_CAST"customfield", 11) == 0)
		{
			stringstream nameStr;
			int nameLen = xmlStrlen(pCurrentNode->name);

			if (nameLen == 11)
			{
				// A name is required
				char *pNodeAttr = (char*)xmlGetProp(pCurrentNode, BAD_CAST"name");
				if (pNodeAttr != NULL)
				{
					nameStr << pNodeAttr;
					m_customFields[nameStr.str()] = (const char*)pContent;

					xmlFree(pNodeAttr);
				}
			}
			else if (nameLen > 11)
			{
				unsigned int customFieldNum = (unsigned int)atoi(((const char*)pCurrentNode->name) + 11);

				nameStr << "customfield" << customFieldNum;
				m_customFields[nameStr.str()] = (const char*)pContent;
			}
		}

		// Free
		xmlFree(pContent);
	}
}

