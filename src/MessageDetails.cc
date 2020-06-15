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

#include <ctype.h>
#include <stdlib.h>
#include <sstream>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

#include "Base64.h"
#include "MessageDetails.h"

using std::clog;
using std::endl;
using std::string;
using std::vector;
using std::set;
using std::map;
using std::ifstream;
using std::stringstream;
using std::for_each;
using std::ios;

struct DeleteContentPieceFunc
{
	public:
		void operator()(vector<ContentPiece *>::value_type &p)
		{
			delete p;
		}

};

struct DeleteAttachmentFunc
{
	public:
		void operator()(vector<Attachment *>::value_type &p)
		{
			delete p;
		}

};

Attachment::Attachment(const string &filePath,
	const string &contentType,
	const string &contentId,
	const string &encoding) :
	m_filePath(filePath),
	m_contentType(contentType),
	m_contentId(contentId),
	m_encoding(encoding),
	m_contentLength(0),
	m_pContent(NULL),
	m_encodedLength(0),
	m_pEncodedContent(NULL)
{
	resolveContentType();
}

void Attachment::resolveContentType(void)
{
	if (m_contentType.empty() == false)
	{
		return;
	}

	string fileName(getFileName());

#ifdef DEBUG
	clog << "Attachment::resolveContentType: checking " << fileName << endl;
#endif

	// What's the extension ?
	string::size_type lastPos = fileName.find_last_of('.');
	if (lastPos != string::npos)
	{
		string fileExtension(fileName.substr(lastPos + 1));

		// Guess the file type
		if (fileExtension == "pdf")
		{
			m_contentType = "application/pdf";
		}
		else if (fileExtension == "rtf")
		{
			m_contentType = "application/rtf";
		}
		else if (fileExtension == "doc")
		{
			m_contentType = "application/vnd.ms-word";
		}
		else if (fileExtension == "xls")
		{
			m_contentType = "application/vnd.ms-excel";
		}
		else if (fileExtension == "ppt")
		{
			m_contentType = "application/vnd.ms-powerpoint";
		}
		else if ((fileExtension == "html") ||
			(fileExtension == "htm"))
		{
			m_contentType = "text/html";
		}
		else if (fileExtension == "txt")
		{
			m_contentType = "text/plain";
		}
		else if (fileExtension == "gif")
		{
			m_contentType = "image/gif";
		}
		else if (fileExtension == "jpg")
		{
			m_contentType = "image/jpeg";
		}
		else if (fileExtension == "png")
		{
			m_contentType = "image/png";
		}
		else
		{
			m_contentType = "application/octet-stream";
		}
	}
}

Attachment::~Attachment()
{
	if (m_pContent != NULL)
	{
		delete[] m_pContent;
		m_pContent = NULL;
	}
	if (m_pEncodedContent != NULL)
	{
		delete[] m_pEncodedContent;
		m_pEncodedContent = NULL;
	}
}

string Attachment::getFileName(void) const
{
	string fileName("Attachment");

	// Get the actual file name
	string::size_type lastPos = m_filePath.find_last_of("/");
	if (lastPos != string::npos)
	{
		fileName = m_filePath.substr(lastPos + 1);
	}
	else if (m_filePath.empty() == false)
	{
		fileName = m_filePath;
	}

	return fileName;
}

bool Attachment::isInline(void) const
{
	if (m_contentId.empty() == false)
	{
		return true;
	}

	return false;
}

ContentPiece::ContentPiece(const string &contentType,
	const string &encoding, bool personalize) :
	Attachment("", contentType, "", encoding),
	m_personalize(personalize)
{
}

ContentPiece::~ContentPiece()
{
}

MessageDetails::MessageDetails() :
	m_charset("UTF-8"),
	m_useXMailer(false),
	m_isReply(false),
	m_version(2),
	m_pPlainSub(NULL),
	m_pHtmlSub(NULL),
	m_attachmentsCount(0),
	m_inlinePartsCount(0)
{
}

MessageDetails::~MessageDetails()
{
	if (m_pPlainSub != NULL)
	{
		delete m_pPlainSub;
	}
	if (m_pHtmlSub != NULL)
	{
		delete m_pHtmlSub;
	}

	for_each(m_contentPieces.begin(), m_contentPieces.end(),
		DeleteContentPieceFunc());
	for_each(m_attachments.begin(), m_attachments.end(),
		DeleteAttachmentFunc());
}

char *MessageDetails::loadRaw(const string &filePath, off_t &fileSize)
{
	ifstream dataFile;

	dataFile.open(filePath.c_str());
	if (dataFile.is_open() == true)
	{
		dataFile.seekg(0, ios::end);
		off_t length = dataFile.tellg();
		dataFile.seekg(0, ios::beg);

		char *pBuffer = new char[length + 1];
		dataFile.read(pBuffer, length);
		if (dataFile.fail() == false)
		{
			pBuffer[length] = '\0';
			dataFile.close();
			fileSize = length;

			return pBuffer;
		}

		dataFile.close();
	}

	return NULL;
}

string MessageDetails::encodeRecipientId(const string &recipientId)
{
	string idStr("&recipientId=");
	idStr += recipientId;
	idStr += "&";

	unsigned long dataLen = idStr.size();
	char *pData = Base64::encode(idStr.c_str(), dataLen);

	if ((pData == NULL) ||
		(dataLen == 0))
	{
		return "";
	}

	string encodedData(pData, dataLen);

	delete[] pData;

	return encodedData;
}

string MessageDetails::createMessageId(const string &suffix, bool isReplyId)
{
	string messageId;
	bool addSuffix = true;

	if (m_isReply == isReplyId)
	{
		messageId = m_msgId;
		addSuffix = false;
	}
	if (messageId.empty() == true)
	{
		stringstream randomStr;

		randomStr << lrand48() << m_fromEmailAddress << lrand48();

		string pseudoName(randomStr.str());
		unsigned long msgIdLen = pseudoName.length();
		char *pMessageId = Base64::encode(pseudoName.c_str(), msgIdLen);
		if (pMessageId != NULL)
		{
			string::size_type pos = 0;

			messageId.append(pMessageId, msgIdLen);
			delete[] pMessageId;

			// Remove non-alphanumerics
			while (pos < messageId.length())
			{
				if (isalnum(messageId[pos]) == 0)
				{
					messageId.erase(pos, 1);
					continue;
				}
				++pos;
			}
			if (messageId.length() > 40)
			{
				messageId.resize(40);
			}
		}
		addSuffix = true;
	}
	if (addSuffix == true)
	{
		messageId += suffix;
	}
#ifdef DEBUG
	clog << "MessageDetails::createMessageId: ID " << messageId << endl;
#endif

	return messageId;
}

Substituter *MessageDetails::getPlainSubstituter(const string &dictionaryId)
{
	if (m_pPlainSub == NULL)
	{
		m_pPlainSub = newSubstituter(dictionaryId,
			getContent("text/plain"), false);
	}

	return m_pPlainSub;
}

Substituter *MessageDetails::getHtmlSubstituter(const string &dictionaryId)
{
	if (m_pHtmlSub == NULL)
	{
		m_pHtmlSub = newSubstituter(dictionaryId,
			getContent("/html"), true);
	}

	return m_pHtmlSub;
}

string MessageDetails::substitute(const string &dictionaryId,
	const string &content, const map<string, string> &fieldValues)
{
	// m_version 1 is obsolete
	Substituter *pSub = new CTemplateSubstituter(dictionaryId,
		content, false);
	string subContent;

	pSub->substitute(fieldValues, subContent);

	delete pSub;

	return subContent;
}

bool MessageDetails::isRecipientPersonalized(void)
{
	if ((checkFields("text/plain") == true) ||
		(checkFields("/html") == true))
	{
		return true;
	}

	return false;
}

bool MessageDetails::addAttachment(Attachment *pAttachment)
{
	if (pAttachment == NULL)
	{
		return false;
	}

	// What kind of attachment is it ?
	if (pAttachment->isInline() == true)
	{
		++m_inlinePartsCount;
	}
	else
	{
		++m_attachmentsCount;
	}

	m_attachments.push_back(pAttachment);

	return true;
}

void MessageDetails::setAttachments(const vector<string> &filePaths,
	bool append)
{
	if (append == false)
	{
		// Delete and clear all attachments, if any
		for_each(m_attachments.begin(), m_attachments.end(),
			DeleteAttachmentFunc());
		m_attachments.clear();
	}

	for (vector<string>::const_iterator fileIter = filePaths.begin();
		fileIter != filePaths.end(); ++fileIter)
	{
		addAttachment(new Attachment(*fileIter, "", ""));
	}
}

void MessageDetails::loadAttachments(void)
{
	for (vector<Attachment *>::iterator attachmentIter = m_attachments.begin();
		attachmentIter != m_attachments.end(); ++attachmentIter)
	{
		Attachment *pAttachment = (*attachmentIter);
		if ((pAttachment == NULL) ||
			(pAttachment->m_filePath.empty() == true))
		{
			continue;
		}

		// Load the file
		if (pAttachment->m_contentLength == 0)
		{
			pAttachment->m_pContent = loadRaw(pAttachment->m_filePath, pAttachment->m_contentLength);
			if (pAttachment->m_pContent == NULL)
			{
				clog << "Couldn't load contents of " << pAttachment->m_filePath << endl;
			}
		}
	}
}

unsigned int MessageDetails::getAttachmentCount(bool inlineParts) const
{
	if (inlineParts == true)
	{
		return m_inlinePartsCount;
	}

	return m_attachmentsCount;
}

Attachment *MessageDetails::getAttachment(unsigned int attachmentNum,
	bool inlineParts) const
{
	vector<Attachment *>::const_iterator attachmentIter = m_attachments.begin();
	unsigned int pos = 0;

	while (attachmentIter != m_attachments.end())
	{
		if ((*attachmentIter)->isInline() != inlineParts)
		{
			// Skip this
			++attachmentIter;
			continue;
		}

		if (pos == attachmentNum)
		{
			Attachment *pAttachment = (*attachmentIter);

			return pAttachment;
		}

		// Next
		++attachmentIter;
		++pos;
	}

	return NULL;
}

bool MessageDetails::setContentPiece(const string &contentType,
	const char *pContent, const string &encoding,
	bool personalize)
{
	ContentPiece *pContentObj = NULL;
	off_t contentLength = (off_t)strlen(pContent);
	bool pushObject = false;

	for (vector<ContentPiece *>::iterator contentIter = m_contentPieces.begin();
		contentIter != m_contentPieces.end(); ++contentIter)
	{
		pContentObj = *contentIter;

		// Assume content pieces have unique content type
		if ((pContentObj != NULL) &&
			(pContentObj->m_contentType.find(contentType) != string::npos))
		{
			pContentObj->m_encoding = encoding;
			if (pContentObj->m_pContent != NULL)
			{
				delete[] pContentObj->m_pContent;
				pContentObj->m_pContent = NULL;
			}
			pContentObj->m_contentLength = 0;
			pContentObj->m_personalize = personalize;
			break;
		}

		pContentObj = NULL;
	}

	if (pContentObj == NULL)
	{
		pContentObj = new ContentPiece(contentType, encoding, personalize);
		pushObject = true;
	}

	pContentObj->m_pContent = new char[contentLength + 1];
	strncpy(pContentObj->m_pContent, pContent, contentLength);
	pContentObj->m_pContent[contentLength] = '\0';
	pContentObj->m_contentLength = contentLength;

	if (pushObject == true)
	{
		m_contentPieces.push_back(pContentObj);
	}

	return false;
}

bool MessageDetails::isReport(const string &contentType) const
{
	if (m_reportType.find(contentType) == 0)
	{
		return true;
	}

	return false;
}

bool MessageDetails::hasContentPiece(const string &contentType) const
{
	if (m_contentPieces.empty() == true)
	{
		return false;
	}

	for (vector<ContentPiece *>::const_iterator contentIter = m_contentPieces.begin();
		contentIter != m_contentPieces.end(); ++contentIter)
	{
		ContentPiece *pContentObj = *contentIter;

		// Assume content pieces have unique content type
		if ((pContentObj != NULL) &&
			(pContentObj->m_contentType.find(contentType) != string::npos))
		{
			return true;
		}
	}

	return false;
}

string MessageDetails::getContent(const string &contentType) const
{
	if (m_contentPieces.empty() == true)
	{
#ifdef DEBUG
		clog << "MessageDetails::getContent: no content" << endl;
#endif
		return "";
	}

	for (vector<ContentPiece *>::const_iterator contentIter = m_contentPieces.begin();
		contentIter != m_contentPieces.end(); ++contentIter)
	{
		ContentPiece *pContentObj = *contentIter;

		// Assume content pieces have unique content type
		if ((pContentObj != NULL) &&
			(pContentObj->m_contentType.find(contentType) != string::npos) &&
			(pContentObj->m_pContent != NULL))
		{
			// FIXME: don't copy, adapt Substituter accordingly
			return string(pContentObj->m_pContent,
				pContentObj->m_contentLength);
		}
	}
#ifdef DEBUG
	clog << "MessageDetails::getContent: found no content for " << contentType << endl;
#endif

	return "";
}

string MessageDetails::getEncoding(const string &contentType) const
{
	if (m_contentPieces.empty() == true)
	{
		return "";
	}

	for (vector<ContentPiece *>::const_iterator contentIter = m_contentPieces.begin();
		contentIter != m_contentPieces.end(); ++contentIter)
	{
		ContentPiece *pContentObj = *contentIter;

		if ((pContentObj != NULL) &&
			(pContentObj->m_contentType.find(contentType) != string::npos))
		{
			return pContentObj->m_encoding;
		}
	}

	return "";
}

bool MessageDetails::checkFields(const string &contentType)
{
	string content(getContent(contentType));

	// Does content have any recipient-specific field?
	if ((CTemplateSubstituter::hasField(content, "Name") == true) ||
		(CTemplateSubstituter::hasField(content, "emailaddress") == true) ||
		(CTemplateSubstituter::hasField(content, "recipientId") == true))
	{
#ifdef DEBUG
		clog << "MessageDetails::checkFields: content type " << contentType << " is personalized" << endl;
#endif
		return true;
	}
#ifdef DEBUG
	clog << "MessageDetails::checkFields: content type " << contentType << " (" << content.size() <<  " bytes) is not personalized" << endl;
#endif

	return false;
}

Substituter *MessageDetails::newSubstituter(const string &dictionaryId,
	const string &contentTemplate, bool escapeEntities)
{
	// m_version 1 is obsolete
	Substituter *pSub = new CTemplateSubstituter(dictionaryId,
		contentTemplate, escapeEntities);

	return pSub;
}

