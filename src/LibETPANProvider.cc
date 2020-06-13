/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <sasl/sasl.h>
#include <sstream>
#include <iostream>
#include <algorithm>

#include "config.h"
#include "LibETPANProvider.h"
#include "QuotedPrintable.h"
#include "SMTPSession.h"

using std::clog;
using std::endl;
using std::string;
using std::stringstream;
using std::map;
using std::vector;
using std::for_each;

// This function from DINH Viêt Hoà
// See http://article.gmane.org/gmane.mail.libetpan.user/377
static int fill_remote_ip_port(mailstream *stream, char *remote_ip_port,
	size_t remote_ip_port_len)
{
	mailstream_low *low;
	int fd;
	struct sockaddr_in name;
	socklen_t namelen;
	char remote_ip_port_buf[128];
	int r;

	low = mailstream_get_low(stream);
	fd = mailstream_low_get_fd(low);

	namelen = sizeof(name);
	r = getpeername(fd, (struct sockaddr *) &name, &namelen);
	if (r < 0)
	{
		return -1;
	}

	if (inet_ntop(AF_INET, &name.sin_addr, remote_ip_port_buf,
		sizeof(remote_ip_port_buf)))
	{
		return -1;
	}

	snprintf(remote_ip_port, remote_ip_port_len, "%s;%i",
		remote_ip_port_buf, ntohs(name.sin_port));

	return 0;
}

// This function from DINH Viêt Hoà
// See http://article.gmane.org/gmane.mail.libetpan.user/377
static int fill_local_ip_port(mailstream *stream, char *local_ip_port,
	size_t local_ip_port_len)
{
	mailstream_low *low;
	int fd;
	struct sockaddr_in name;
	socklen_t namelen;
	char local_ip_port_buf[128];
	int r;

	low = mailstream_get_low(stream);
	fd = mailstream_low_get_fd(low);

	namelen = sizeof(name);
	r = getpeername(fd, (struct sockaddr *) &name, &namelen);
	if (r < 0)
	{
		return -1;
	}

	if (inet_ntop(AF_INET, &name.sin_addr, local_ip_port_buf,
		sizeof(local_ip_port_buf)))
	{
		return -1;
	}

	snprintf(local_ip_port, local_ip_port_len, "%s;%i",
		local_ip_port_buf, ntohs(name.sin_port));

	return 0;
}

static string catValuePath(const string &value, const string &path)
{
	string whole(value);

	if (path.empty() == false)
	{
		if (whole.empty() == false)
		{
			whole += " ";
		}
		whole += "<";
		whole += path;
		whole += ">";
	}

	return whole;
}

static char *asciiOnly(const string &value)
{
	string encodedValue;
	string::size_type pos = 0;
	char *pEncodedWord = NULL;
	size_t wordLen = value.length();
	bool isAscii = true;

	while (pos < value.length())
	{
		if (isascii(value[pos]) == 0)
		{
			isAscii = false;
			break;
		}
		++pos;
	}

	if (isAscii == false)
	{
#ifdef DEBUG
		clog << "asciiOnly: '" << value << "' needs to be QP-encoded" << endl;
#endif
		pEncodedWord = QuotedPrintable::encode(value.c_str(), wordLen);
	}
	if (pEncodedWord == NULL)
	{
		return strndup(value.c_str(), value.length());
	}

	string encodedWord(pEncodedWord);

	// http://en.wikipedia.org/wiki/MIME#Encoded-Word
	// QuotedPrintable encodes '='
	string::size_type extraPos = encodedWord.find_first_of("? _");
	while (extraPos != string::npos)
	{
		if (encodedWord[extraPos] == '?')
		{
			encodedWord.replace(extraPos, 1, "=3F");
			extraPos += 2;
		}
		else if (encodedWord[extraPos] == ' ')
		{
			encodedWord.replace(extraPos, 1, "_");
			if (extraPos + 1 < encodedWord.length())
			{
				extraPos += 1;
			}
			else
			{
				break;
			}
		}
		else if (encodedWord[extraPos] == '_')
		{
			encodedWord.replace(extraPos, 1, "=5F");
			extraPos += 2;
		}

		// Next
		extraPos = encodedWord.find_first_of("? _", extraPos);
	}
	// Erase line breaks
	extraPos = encodedWord.find("=\n");
	while (extraPos != string::npos)
	{
		encodedWord.erase(extraPos, 2);

		// Next
		extraPos = encodedWord.find("=\n", extraPos);
	}
#ifdef DEBUG
	clog << "asciiOnly: encoded to '" << encodedWord << "'" << endl;
#endif
	encodedValue += "=?UTF-8?Q?";
	encodedValue += encodedWord;
	encodedValue += "?=";

	delete[] pEncodedWord;

	return strndup(encodedValue.c_str(), encodedValue.length());
}

static int getEncoding(const string &encoding, int defaultValue)
{
	if (encoding == "base64")
	{
		return MAILMIME_MECHANISM_BASE64;
	}
	else if (encoding == "binary")
	{
		return MAILMIME_MECHANISM_BINARY;
	}
	else if (encoding == "quoted-printable")
	{
		return MAILMIME_MECHANISM_QUOTED_PRINTABLE;
	}
	else if (encoding == "7bit")
	{
		return MAILMIME_MECHANISM_7BIT;
	}
	else if (encoding == "8bit")
	{
		return MAILMIME_MECHANISM_8BIT;
	}

	return defaultValue;
}

static struct mailimf_mailbox *newMailbox(const string &value, const string &path)
{
	char *pValue = NULL;
	char *pPath = NULL;

	if ((value.empty() == true) &&
		(path.empty() == true))
	{
		return NULL;
	}

	// These must be malloc-ated
	if (value.empty() == false)
	{
		pValue = asciiOnly(value);
	}
	if (path.empty() == false)
	{
		pPath = strndup(path.c_str(), path.length());
	}

	return mailimf_mailbox_new(pValue, pPath);
}

static struct mailimf_mailbox_list *newMailboxList(const string &value, const string &path)
{
	struct mailimf_mailbox *pMailbox = newMailbox(value, path);
	if (pMailbox == NULL)
	{
		return NULL;
	}

	struct mailimf_mailbox_list *pMailboxList = mailimf_mailbox_list_new_empty();
	if (pMailboxList == NULL)
	{
		mailimf_mailbox_free(pMailbox);
		return NULL;
	}

	if (mailimf_mailbox_list_add(pMailboxList, pMailbox) != MAILIMF_NO_ERROR)
	{
		mailimf_mailbox_free(pMailbox);
		mailimf_mailbox_list_free(pMailboxList);
		return NULL;
	}

	return pMailboxList;
}

struct mailimf_address *newAddress(const string &value, const string &path)
{
	struct mailimf_mailbox *pMailbox = newMailbox(value, path);
	if (pMailbox == NULL)
	{
		return NULL;
	}

	return mailimf_address_new(MAILIMF_ADDRESS_MAILBOX, pMailbox, NULL);
}

struct mailimf_address_list *newAddressList(const string &value, const string &path)
{
	struct mailimf_address *pAddress = newAddress(value, path);
	if (pAddress == NULL)
	{
		return NULL;
	}

	struct mailimf_address_list *pAddressList = mailimf_address_list_new_empty();
	if (pAddressList == NULL)
	{
		return NULL;
	}

	if (mailimf_address_list_add(pAddressList, pAddress) != MAILIMF_NO_ERROR)
	{
		mailimf_address_free(pAddress);
		mailimf_address_list_free(pAddressList);
		return NULL;
	}

	return pAddressList;
}

static mailimf_address_list *parseAddressesList(const string &addressesList)
{
	string::size_type startPos = 0, sepPos = addressesList.find(',');

	// Parse the list of recipients
	if (sepPos == string::npos)
	{
		Recipient recipient = Recipient::extractNameAndEmailAddress(addressesList);

		return newAddressList(recipient.m_name, recipient.m_emailAddress);
	}

	struct mailimf_address_list *pAddressList = mailimf_address_list_new_empty();
	if (pAddressList == NULL)
	{
		return NULL;
	}

	while (sepPos != string::npos)
	{
		string recipientDetails(addressesList.substr(startPos, sepPos - startPos));
		Recipient recipient = Recipient::extractNameAndEmailAddress(recipientDetails);
		struct mailimf_address *pAddress = newAddress(recipient.m_name, recipient.m_emailAddress);
		if (pAddress != NULL)
		{
			if (mailimf_address_list_add(pAddressList, pAddress) != MAILIMF_NO_ERROR)
			{
				clog << "Error parsing " << recipientDetails << endl;
				mailimf_address_free(pAddress);
			}
		}

		// Next
		startPos = sepPos + 1;
		sepPos = addressesList.find(',', startPos);
	}

	if (startPos < addressesList.length())
	{
		string lastRecipientDetails(addressesList.substr(startPos));
		Recipient recipient = Recipient::extractNameAndEmailAddress(lastRecipientDetails);
		struct mailimf_address *pAddress = newAddress(recipient.m_name, recipient.m_emailAddress);
		if (pAddress != NULL)
		{
			if (mailimf_address_list_add(pAddressList, pAddress) != MAILIMF_NO_ERROR)
			{
				clog << "Error parsing " << lastRecipientDetails << endl;
				mailimf_address_free(pAddress);
			}
		}
	}

	return pAddressList;
}

static mailmime *buildSimplePart(const string &partType,
	const string &partContent = string(""),
	const string &reportType = string(""))
{
	struct mailmime_fields *mime_fields = mailmime_fields_new_empty();
	if (mime_fields == NULL)
	{
		return NULL;
	}

	struct mailmime_content *content = mailmime_content_new_with_str(partType.c_str());
	if (content == NULL)
	{
		mailmime_fields_free(mime_fields);
		return NULL;
	}

	if (reportType.empty() == false)
	{
		struct mailmime_parameter *param = mailmime_param_new_with_data("report-type", const_cast<char*>(reportType.c_str()));
		if (param != NULL)
		{
			if (clist_append(content->ct_parameters, param) < 0)
			{
				mailmime_parameter_free(param);
				param = NULL;
			}
		}
	}

	struct mailmime *mime_sub = mailmime_new_empty(content, mime_fields);
	if (mime_sub != NULL)
	{
		if ((partContent.empty() == false) &&
			(mailmime_set_body_text(mime_sub, const_cast<char*>(partContent.c_str()), partContent.length()) != MAILIMF_NO_ERROR))
		{
			mailmime_free(mime_sub);
			mailmime_content_free(content);
			mailmime_fields_free(mime_fields);
		}
	}
	else
	{
		mailmime_content_free(content);
		mailmime_fields_free(mime_fields);
	}

	return mime_sub;
}

static mailmime *buildBodyPart(const string &partType,
	const string &partContent, const string &partEncoding,
	const string &partCharset)
{
	int encoding = getEncoding(partEncoding,
		MAILMIME_MECHANISM_QUOTED_PRINTABLE);
	struct mailmime_fields *mime_fields = mailmime_fields_new_encoding(encoding);
	if (mime_fields == NULL)
	{
		return NULL;
	}

	struct mailmime_content *content = mailmime_content_new_with_str(partType.c_str());
	if (content == NULL)
	{
		mailmime_fields_free(mime_fields);
		return NULL;
	}

	if (partCharset.empty() == false)
	{
		struct mailmime_parameter *param = mailmime_param_new_with_data("charset", const_cast<char*>(partCharset.c_str()));
		if (param != NULL)
		{
			if (clist_append(content->ct_parameters, param) < 0)
			{
				mailmime_parameter_free(param);
				param = NULL;
			}
		}
	}

	struct mailmime *mime_sub = mailmime_new_empty(content, mime_fields);
	if (mime_sub != NULL)
	{
		if ((partContent.empty() == false) &&
			(mailmime_set_body_text(mime_sub, const_cast<char*>(partContent.c_str()), partContent.length()) != MAILIMF_NO_ERROR))
		{
			mailmime_free(mime_sub);
			mailmime_content_free(content);
			mailmime_fields_free(mime_fields);
		}
	}
	else
	{
		mailmime_content_free(content);
		mailmime_fields_free(mime_fields);
	}

	return mime_sub;
}

static mailmime *buildFilePart(Attachment *pAttachment)
{
	if ((pAttachment == NULL) ||
		(pAttachment->m_contentType.empty() == true))
	{
		return NULL;
	}

	string fileName(pAttachment->getFileName());
	char *pFileName = strndup(fileName.c_str(), fileName.length());
	if (pFileName == NULL)
	{
		return NULL;
	}

	char *pCID = strndup(pAttachment->m_contentId.c_str(), pAttachment->m_contentId.length());
	if (pCID == NULL)
	{
		free(pFileName);
		return NULL;
	}

	struct mailmime_fields *mime_fields = NULL;
	struct mailmime_field *mime_id = NULL;

	if (pAttachment->isInline() == true)
	{
		int encoding = getEncoding(pAttachment->m_encoding,
			MAILMIME_MECHANISM_BASE64);

		// These must be malloc-ed
#if LIBETPAN_VERSION_MINOR >= 6
		mime_id = mailmime_field_new(MAILMIME_FIELD_ID, NULL, NULL,
			pCID, NULL, 0, NULL, NULL, NULL);
#else
		mime_id = mailmime_field_new(MAILMIME_FIELD_ID, NULL, NULL,
			pCID, NULL, 0, NULL, NULL);
#endif
		if (mime_id == NULL)
		{
#ifdef DEBUG
			clog << "buildFilePart: failed to set Content-Id " << pAttachment->m_contentId << endl;
#endif
			free(pFileName);
			free(pCID);
			return NULL;
		}

		mime_fields = mailmime_fields_new_filename(MAILMIME_DISPOSITION_TYPE_INLINE,
			pFileName, encoding);
	}
	else
	{
#ifdef DEBUG
		clog << "buildFilePart: attachment " << pFileName << " is not inline" << endl;
#endif
		mime_fields = mailmime_fields_new_filename(MAILMIME_DISPOSITION_TYPE_ATTACHMENT,
			pFileName, MAILMIME_MECHANISM_BASE64);
	}
	if (mime_fields == NULL)
	{
		free(pFileName);
		free(pCID);
		return NULL;
	}

	if (mime_id != NULL)
	{
#ifdef DEBUG
		clog << "buildFilePart: appending Content-Id " << pAttachment->m_contentId << endl;
#endif
		clist_append(mime_fields->fld_list, mime_id);
	}

	struct mailmime_content *content = mailmime_content_new_with_str(const_cast<char*>(pAttachment->m_contentType.c_str()));
	if (content == NULL)
	{
		mailmime_fields_free(mime_fields);
		return NULL;
	}

	struct mailmime *mime_sub = mailmime_new_empty(content, mime_fields);
	if (mime_sub != NULL)
	{
		// The file has already been loaded
		if ((pAttachment->m_pContent != NULL) &&
			(pAttachment->m_contentLength > 0))
		{
#ifdef DEBUG
			clog << "buildFilePart: attaching " << pAttachment->m_contentLength << " bytes of file content" << endl;
#endif
			if (mailmime_set_body_text(mime_sub, pAttachment->m_pContent, (size_t)pAttachment->m_contentLength) != MAILIMF_NO_ERROR)
			{
				mailmime_free(mime_sub);
				mime_sub = NULL;
			}
		}
		else
		{
			char *pFilePath = strndup(pAttachment->m_filePath.c_str(), pAttachment->m_filePath.length());
			if (pFilePath != NULL)
			{
#ifdef DEBUG
				clog << "buildFilePart: attaching file " << pFilePath << endl;
#endif
				if (mailmime_set_body_file(mime_sub, pFilePath) != MAILIMF_NO_ERROR)
				{
					free(pFilePath);
					mailmime_free(mime_sub);
					mime_sub = NULL;
				}

				struct stat fileStat;
				if (stat(pAttachment->m_filePath.c_str(), &fileStat) != -1)
				{
					pAttachment->m_contentLength = (off_t)fileStat.st_size;
				}
			}
		}
	}
	else
	{
		mailmime_content_free(content);
		mailmime_fields_free(mime_fields);
	}

	return mime_sub;
}

static size_t attachFileParts(MessageDetails *pDetails, struct mailmime *pPart, bool inlineParts)
{
	if ((pDetails == NULL) ||
		(pPart == NULL))
	{
		return 0;
	}

	unsigned int attachmentCount = pDetails->getAttachmentCount(inlineParts);
	size_t messageSizeIncrement = 0;

	for (unsigned int attachmentNum = 0; attachmentNum < attachmentCount; ++attachmentNum)
	{
		Attachment *pAttachment = pDetails->getAttachment(attachmentNum, inlineParts);

		struct mailmime *pFilePart = buildFilePart(pAttachment);
		if (pFilePart != NULL)
		{
			mailmime_smart_add_part(pPart, pFilePart);

			messageSizeIncrement += pAttachment->m_contentLength;
#ifdef DEBUG
			clog << "attachFileParts: attachment #" << attachmentNum << " size "
				<< messageSizeIncrement << endl;
#endif
		}
	}

	return messageSizeIncrement;
}

int errorToStatusCode(int errNum)
{
	int statusCode = 0;

	switch (errNum)
	{
		case MAILSMTP_NO_ERROR:
			statusCode = 250;
			break;
		case MAILSMTP_ERROR_AUTH_TRANSITION_NEEDED:
			statusCode = 432;
			break;
		case MAILSMTP_ERROR_AUTH_TEMPORARY_FAILTURE:
			statusCode = 454;
			break;
		case MAILSMTP_ERROR_MAILBOX_UNAVAILABLE:
			// FIXME: could be 550 too !
			statusCode = 450;
			break;
		case MAILSMTP_ERROR_IN_PROCESSING:
			statusCode = 451;
			break;
		case MAILSMTP_ERROR_INSUFFICIENT_SYSTEM_STORAGE:
			statusCode = 452;
			break;
		case MAILSMTP_ERROR_STARTTLS_TEMPORARY_FAILURE:
			statusCode = 454;
			break;
		case MAILSMTP_ERROR_BAD_SEQUENCE_OF_COMMAND:
			statusCode = 503;
			break;
		case MAILSMTP_ERROR_AUTH_NOT_SUPPORTED:
		case MAILSMTP_ERROR_NOT_IMPLEMENTED:
			statusCode = 504;
			break;
		case MAILSMTP_ERROR_AUTH_REQUIRED:
			statusCode = 530;
			break;
		case MAILSMTP_ERROR_AUTH_TOO_WEAK:
			statusCode = 534;
			break;
		case MAILSMTP_ERROR_AUTH_AUTHENTICATION_FAILED:
			statusCode = 535;
			break;
		case MAILSMTP_ERROR_AUTH_ENCRYPTION_REQUIRED:
			statusCode = 538;
			break;
		case MAILSMTP_ERROR_ACTION_NOT_TAKEN:
			statusCode = 550;
			break;
		case MAILSMTP_ERROR_USER_NOT_LOCAL:
			statusCode = 551;
			break;
		case MAILSMTP_ERROR_EXCEED_STORAGE_ALLOCATION:
			statusCode = 552;
			break;
		case MAILSMTP_ERROR_MAILBOX_NAME_NOT_ALLOWED:
			statusCode = 553;
			break;
		case MAILSMTP_ERROR_SERVICE_NOT_AVAILABLE:
		case MAILSMTP_ERROR_TRANSACTION_FAILED:
			statusCode = 554;
			break;
		default:
			statusCode = 0;
			break;
	}

	return statusCode;
}

// A function object to delete optional fields with for_each().
struct DeleteOptionalFieldFunc
{
	public:
		void operator()(vector<struct mailimf_field *>::value_type &p)
		{
			mailimf_field_free(p);
		}

};

LibETPANMessage::LibETPANMessage(const map<string, string> &fieldValues,
	MessageDetails *pDetails, DSNNotification dsnFlags,
	bool enableMdn,
	const string &msgIdSuffix,
	const string &complaints) :
	SMTPMessage(fieldValues, pDetails, dsnFlags, enableMdn, msgIdSuffix, complaints),
	m_date(time(NULL)),
	m_pString(NULL),
	m_sent(false),
	m_relatedFirst(false)
{
	char *pEnvVar = getenv("GIVEMAIL_RELATED_FIRST");

	// This gives priority to related over alternative
	if ((pEnvVar != NULL) &&
		(strncasecmp(pEnvVar, "Y", 1) == 0))
	{
		m_relatedFirst = true;
	}

	// Substitute fields in content once
	substituteContent(fieldValues);

	buildHeaders();
}

LibETPANMessage::~LibETPANMessage()
{
	if (m_pString != NULL)
	{
		mmap_string_free(m_pString);
	}
}

struct mailimf_fields *LibETPANMessage::headersToFields(void)
{
	vector<struct mailimf_field *> optionalFields;
	struct mailimf_date_time *pDate = mailimf_get_date(m_date);
	struct mailimf_mailbox_list *pFrom = NULL;
	struct mailimf_mailbox *pSender = NULL;
	struct mailimf_address_list *pReplyTo = NULL;
	struct mailimf_address_list *pTo = NULL;
	struct mailimf_address_list *pCC = NULL;
	struct mailimf_address_list *pBCC = NULL;
	char *pMessageId = NULL;
	char *pSubject = NULL;

	// Enable Message Disposition Notification ?
	if (m_enableMdn == true)
	{
		// FIXME: request MDN to be sent to the same address as the reverse path
	}

	for (vector<SMTPHeader>::const_iterator headerIter = m_headers.begin();
		headerIter != m_headers.end(); ++headerIter)
	{
		string headerName(headerIter->m_name);
		string value(headerIter->m_value);
		string path(headerIter->m_path);

		if (headerName == "From")
		{
			pFrom = newMailboxList(value, path);
		}
		else if ((headerName == "Sender") &&
			(path.empty() == false))
		{
			// This must be malloc-ated
			char *pPath = strndup(path.c_str(), path.length());
			pSender = mailimf_mailbox_new(NULL, pPath);
		}
		else if (headerName == "Reply-To")
		{
			pReplyTo = newAddressList(value, path);
		}
		// To, CC, BCC should be known at this stage
		else if (headerName == "To")
		{
			pTo = parseAddressesList(catValuePath(value, path));
		}
		else if (headerName == "CC")
		{
			pCC = parseAddressesList(catValuePath(value, path));
		}
		else if (headerName == "BCC")
		{
			pBCC = parseAddressesList(catValuePath(value, path));
		}
		else if (headerName == "Message-Id")
		{
			// This must be malloc-ated
			pMessageId = strndup(value.c_str(), value.length());
		}
		else if (headerName == "Subject")
		{
			// This must be malloc-ated
			if (value.empty() == false)
			{
				pSubject = asciiOnly(value);
			}
			else
			{
				pSubject = strndup("No subject", 10);
			}
		}
		else
		{
			// These must be malloc-ated
			struct mailimf_field *pOptionalField = mailimf_field_new_custom(strdup(headerName.c_str()),
				strdup(catValuePath(value, path).c_str()));
			optionalFields.push_back(pOptionalField);
#ifdef DEBUG
			clog << "LibETPANMessage::headersToFields: optional field for header " << headerName << endl;
#endif
		}
	}

	string reversePath(getReversePath());

	if (reversePath.empty() == false)
	{
		// This must be malloc-ated
		struct mailimf_field *pOptionalField = mailimf_field_new_custom(strdup("Return-Path"),
			strdup(reversePath.c_str()));
		optionalFields.push_back(pOptionalField);
#ifdef DEBUG
		clog << "LibETPANMessage::headersToFields: set Return-Path to "
			<< reversePath << endl;
#endif
	}

	// Build the message now that it's being requested
	struct mailimf_fields *pFields = mailimf_fields_new_with_data_all(pDate,
		pFrom, pSender, pReplyTo, pTo, pCC, pBCC,
		pMessageId, NULL, NULL, pSubject);
	if (pFields == NULL)
	{
		// Delete optional fields we may have created
		for_each(optionalFields.begin(), optionalFields.end(),
			DeleteOptionalFieldFunc());
		// FIXME: delete other fields and strings too !!!

		return NULL;
	}

	// Add optional fields
	for (vector<struct mailimf_field *>::const_iterator optIter = optionalFields.begin();
		optIter != optionalFields.end(); ++optIter)
	{
		struct mailimf_field *pField = (*optIter);

		// Add this field
		mailimf_fields_add(pFields, pField);
	}

	return pFields;
}

bool LibETPANMessage::serialize(size_t messageSizeEstimate,
	struct mailmime *pMessage)
{
	int col = 0;
	bool serialized = true;

	// Delete any previously allocated string
	if (m_pString != NULL)
	{
		mmap_string_free(m_pString);
		m_pString = NULL;
	}
#ifdef DEBUG
	clog << "LibETPANMessage::serialize: estimated message size to " << messageSizeEstimate << " +/- 4096" << endl;
#endif

	// Hopefully that mmap'ed string is long enough
	m_pString = mmap_string_sized_new(messageSizeEstimate);
	if (mailmime_write_mem(m_pString, &col, pMessage) != MAILIMF_NO_ERROR)
	{
		mmap_string_free(m_pString);
		m_pString = NULL;

		serialized = false;
	}

	mailmime_free(pMessage);

	return serialized;
}

string LibETPANMessage::getUserAgent(void) const
{
	if ((m_pDetails != NULL) &&
		(m_pDetails->m_userAgent.empty() == false))
	{
		return m_pDetails->m_userAgent;
	}
	
	return PACKAGE_NAME "/etpan " PACKAGE_VERSION;
}

bool LibETPANMessage::setSignatureHeader(const string &header,
	const string &value)
{
	if ((m_pString == NULL) ||
		(SMTPMessage::setSignatureHeader(header, value) == false))
	{
		return false;
	}

	// Prepend the signature
	m_pString = mmap_string_prepend_len(m_pString,
		m_signatureHeader.c_str(), m_signatureHeader.length());
	if (m_pString != NULL)
	{
		return true;
	}

	return false;
}

bool LibETPANMessage::addHeader(const string &header,
	const string &value, const string &path)
{
	// This is provided out of the box
	if (header == "MIME-Version")
	{
		return true;
	}

	return SMTPMessage::addHeader(header, value, path);
}

void LibETPANMessage::setEnvId(const string &dsnEnvId)
{
	m_dsnEnvId = dsnEnvId;
}

void LibETPANMessage::addRecipient(const string &emailAddress)
{
#ifdef DEBUG
	clog << "LibETPANMessage::addRecipient: adding " << emailAddress << endl;
#endif
	// Store this
	m_recipients[emailAddress] = -1;
}

bool LibETPANMessage::buildMessage(void)
{
	struct mailimf_fields *pFields = headersToFields();
	if (pFields == NULL)
	{
		return false;
	}

	struct mailmime *pMessage = mailmime_new_message_data(NULL);
	if (pMessage != NULL)
	{
		struct mailmime *pTop = pMessage;
		struct mailmime *pReportPart = NULL;
		struct mailmime *pMessagePart = NULL;
		struct mailmime *pRFC822Part = NULL;
		struct mailmime *pMixedPart = NULL;
		struct mailmime *pAltPart = NULL;
		struct mailmime *pRelatedPart = NULL;
		struct mailmime *pPlainPart = NULL;
		struct mailmime *pHtmlPart = NULL;
		string reportPartType("message/delivery-status");
		// Headers will definitely fit in 4kB
		size_t messageSizeEstimate = 4096;

		mailmime_set_imf_fields(pTop, pFields);

		if (isDeliveryReceipt() == true)
		{
			pReportPart = buildSimplePart("multipart/report", "", "delivery-status");
		}
		else if (isReadNotification() == true)
		{
			pReportPart = buildSimplePart("multipart/report", "", "disposition-notification");
			reportPartType = "message/disposition-notification";
		}

		if (pReportPart != NULL)
		{
			mailmime_smart_add_part(pTop, pReportPart);

			if ((isReadNotification() == true) &&
				(m_plainContent.empty() == false))
			{
				pPlainPart = buildBodyPart("text/plain", m_plainContent,
					(m_pDetails ? m_pDetails->getEncoding("text/plain") : ""),
					(m_pDetails ? m_pDetails->m_charset : "UTF-8"));
				if (pPlainPart != NULL)
				{
					mailmime_smart_add_part(pReportPart, pPlainPart);
					messageSizeEstimate += m_plainContent.length();
				}
			}

			pMessagePart = buildSimplePart(reportPartType);

			if (pMessagePart != NULL)
			{
				mailmime_smart_add_part(pReportPart, pMessagePart);
			}

			if (isDeliveryReceipt() == true)
			{
				pRFC822Part = buildBodyPart("message/rfc822",
					"", "8bit", "");

				if (pRFC822Part != NULL)
				{
					mailmime_smart_add_part(pReportPart, pRFC822Part);
				}

				// Anything else will be appended to this part
				pTop = pRFC822Part;
			}
			else if (isReadNotification() == true)
			{
				return serialize(messageSizeEstimate, pMessage);
			}
		}

		if (requiresMixedParts() == true)
		{
			// FIXME: how can we add the same text as for multipart/alternative?
			pMixedPart = buildSimplePart("multipart/mixed");
		}
		if (pMixedPart != NULL)
		{
			mailmime_smart_add_part(pTop, pMixedPart);
		}

		if ((m_relatedFirst == true) &&
			(requiresRelatedParts() == true))
		{
			pRelatedPart = buildSimplePart("multipart/related");
			if (pRelatedPart != NULL)
			{
				if (pMixedPart != NULL)
				{
					mailmime_smart_add_part(pMixedPart, pRelatedPart);
				}
				else
				{
					mailmime_smart_add_part(pTop, pRelatedPart);
				}
			}
		}
		if (requiresAlternativeParts() == true)
		{
			pAltPart = buildSimplePart("multipart/alternative");
			//	"This is a multi-part message in MIME format.\r\n\r\n");
			if (pAltPart != NULL)
			{
				if ((m_relatedFirst == true) &&
					(pRelatedPart != NULL))
				{
					mailmime_smart_add_part(pRelatedPart, pAltPart);
				}
				else if (pMixedPart != NULL)
				{
					mailmime_smart_add_part(pMixedPart, pAltPart);
				}
				else
				{
					mailmime_smart_add_part(pTop, pAltPart);
				}
			}
		}

		if (m_plainContent.empty() == false)
		{
			pPlainPart = buildBodyPart("text/plain", m_plainContent,
				(m_pDetails ? m_pDetails->getEncoding("text/plain") : ""),
				(m_pDetails ? m_pDetails->m_charset : "UTF-8"));
			if (pPlainPart != NULL)
			{
				if (pAltPart != NULL)
				{
					mailmime_smart_add_part(pAltPart, pPlainPart);
				}
				else
				{
					mailmime_smart_add_part(pTop, pPlainPart);
				}
				messageSizeEstimate += m_plainContent.length();
			}
		}

		if (m_htmlContent.empty() == false)
		{
			if ((m_relatedFirst == false) &&
				(requiresRelatedParts() == true))
			{
				pRelatedPart = buildSimplePart("multipart/related");
				if (pRelatedPart != NULL)
				{
					if (pAltPart != NULL)
					{
						mailmime_smart_add_part(pAltPart, pRelatedPart);
					}
					else if (pMixedPart != NULL)
					{
						mailmime_smart_add_part(pMixedPart, pRelatedPart);
					}
					else
					{
						mailmime_smart_add_part(pTop, pRelatedPart);
					}
				}
			}

			pHtmlPart = buildBodyPart("text/html", m_htmlContent,
				(m_pDetails ? m_pDetails->getEncoding("/html") : ""),
				(m_pDetails ? m_pDetails->m_charset : "UTF-8"));
			if (pHtmlPart != NULL)
			{
				if ((m_relatedFirst == false) &&
					(pRelatedPart != NULL))
				{
					mailmime_smart_add_part(pRelatedPart, pHtmlPart);
				}
				else if (pAltPart != NULL)
				{
					mailmime_smart_add_part(pAltPart, pHtmlPart);
				}
				else if ((m_relatedFirst == true) &&
					(pRelatedPart != NULL))
				{
					mailmime_smart_add_part(pRelatedPart, pHtmlPart);
				}
				else if (pMixedPart != NULL)
				{
					mailmime_smart_add_part(pMixedPart, pHtmlPart);
				}
				else
				{
					mailmime_smart_add_part(pTop, pHtmlPart);
				}
				messageSizeEstimate += m_htmlContent.length();

				if (pRelatedPart != NULL)
				{
					// Add inline attachments
					messageSizeEstimate += attachFileParts(m_pDetails, pRelatedPart, true); 
				}
			}
		}

		// Add attachments
		if (requiresMixedParts() == true)
		{
			if (pMixedPart != NULL)
			{
				messageSizeEstimate += attachFileParts(m_pDetails, pMixedPart, false); 
			}
			else
			{
				messageSizeEstimate += attachFileParts(m_pDetails, pTop, false);
			}
		}

		return serialize(messageSizeEstimate, pMessage);
	}
	else
	{
		mailimf_fields_free(pFields);
	}

	return false;
}

const char *LibETPANMessage::getEnvId(void)
{
	if (m_dsnFlags == SMTPMessage::NEVER)
	{
#ifdef DEBUG
		clog << "LibETPANProvider::getEnvId: no envid" << endl;
#endif
		return NULL;
	}
#ifdef DEBUG
	clog << "LibETPANProvider::getEnvId: envid " << m_dsnEnvId << endl;
#endif
	return m_dsnEnvId.c_str();
}

LibETPANProvider::LibETPANProvider() :
	SMTPProvider(),
	m_session(NULL),
	m_port(25),
	m_authenticate(false),
	m_startTLS(false),
	m_error(0)
{
	char *pEnvVar = getenv("GIVEMAIL_DEBUG");

	// This activates debugging
	if ((pEnvVar != NULL) &&
		(strncasecmp(pEnvVar, "Y", 1) == 0))
	{
		mailstream_debug = 1;
	}
	else
	{
		mailstream_debug = 0;
	}
}

LibETPANProvider::~LibETPANProvider()
{
}

string LibETPANProvider::getMessageData(SMTPMessage *pMsg)
{
	if (pMsg == NULL)
	{
		return "";
	}

	LibETPANMessage *pETPANMsg = dynamic_cast<LibETPANMessage*>(pMsg);
	if (pETPANMsg == NULL)
	{
		return "";
	}

	string messageData;

	// Build the message now that it's being requested
	if ((pETPANMsg->buildMessage() == true) &&
		(pETPANMsg->m_pString != NULL))
	{
		messageData.assign(pETPANMsg->m_pString->str, pETPANMsg->m_pString->len);
	}
#ifdef DEBUG
	clog << "LibETPANProvider::getMessageData: message is " << messageData << endl;
#endif

	return messageData;
}

bool LibETPANProvider::createSession(SMTPSession *pSession)
{
	m_session = mailsmtp_new(0, NULL);
	if (m_session != NULL)
	{
		return true;
	}

	return false;
}

bool LibETPANProvider::hasSession(void)
{
	if (m_session == NULL)
	{
		return false;
	}

	return true;
}

void LibETPANProvider::enableAuthentication(const string &authRealm,
	const string &authUserName,
	const string &authPassword)
{
	SMTPProvider::enableAuthentication(authRealm,
		authUserName, authPassword);

	if ((m_authRealm.empty() == false) &&
		(m_authUserName.empty() == false))
	{
		m_authenticate = true;
	}
	else
	{
		m_authenticate = false;
	}
}

void LibETPANProvider::enableStartTLS(void)
{
	m_startTLS = true;
}

void LibETPANProvider::destroySession(void)
{
	if (m_session != NULL)
	{
		mailsmtp_free(m_session);
		m_session = NULL;

		m_hostName.clear();
		m_port = 25;
		m_authenticate = false;
	}
}

bool LibETPANProvider::setServer(const string &hostName,
	unsigned int port)
{
	m_hostName = hostName;
	m_port = port;

	return true;
}

SMTPMessage *LibETPANProvider::newMessage(const map<string, string> &fieldValues,
	MessageDetails *pDetails, SMTPMessage::DSNNotification dsnFlags,
	bool enableMdn,
	const string &msgIdSuffix,
	const string &complaints)
{
	return new LibETPANMessage(fieldValues, pDetails,
		dsnFlags, enableMdn, msgIdSuffix, complaints);
}

void LibETPANProvider::queueMessage(SMTPMessage *pMsg)
{
	if ((m_session == NULL) ||
		(pMsg == NULL))
	{
		return;
	}

	LibETPANMessage *pETPANMsg = dynamic_cast<LibETPANMessage*>(pMsg);
	if (pETPANMsg == NULL)
	{
		return;
	}

	// Store this
	m_messages.push_back(pETPANMsg);
}

int LibETPANProvider::authenticate(void)
{
	string authType("PLAIN");
	char local_ip_port_buf[128];
	char remote_ip_port_buf[128];
	char *local_ip_port = NULL;
	char *remote_ip_port = NULL;

	int returnValue = fill_local_ip_port(m_session->stream, local_ip_port_buf,
		sizeof(local_ip_port_buf));
	if (returnValue < 0)
	{
		local_ip_port = NULL;
	}
	else
	{
		local_ip_port = local_ip_port_buf;
	}

	returnValue = fill_remote_ip_port(m_session->stream, remote_ip_port_buf,
		sizeof(remote_ip_port_buf));
	if (returnValue < 0)
	{
		remote_ip_port = NULL;
	}
	else
	{
		remote_ip_port = remote_ip_port_buf;
	}

	if (m_session->auth & MAILSMTP_AUTH_PLAIN)
	{
		authType = "PLAIN";
	}
	else if (m_session->auth & MAILSMTP_AUTH_LOGIN)
	{
		authType = "LOGIN";
	}
	else if (m_session->auth & MAILSMTP_AUTH_DIGEST_MD5)
	{
		authType = "DIGEST-MD5";
	}
	else if (m_session->auth & MAILSMTP_AUTH_CRAM_MD5)
	{
		authType = "CRAM-MD5";
	}
#ifdef MAILSMTP_AUTH_NTLM
	else if (m_session->auth & MAILSMTP_AUTH_NTLM)
	{
		authType = "NTLM";
	}
#endif
#ifdef MAILSMTP_AUTH_GSSAPI
	else if (m_session->auth & MAILSMTP_AUTH_GSSAPI)
	{
		authType = "GSSAPI";
	}
#endif
	clog << "Authenticating with method " << authType << " (" << m_session->auth
		<< ") on " << m_authRealm << endl;

	// Set login and auth_name to the user name
	returnValue = mailesmtp_auth_sasl(m_session, authType.c_str(),
		m_authRealm.c_str(),
		local_ip_port, remote_ip_port,
		m_authUserName.c_str(), m_authUserName.c_str(),
		m_authPassword.c_str(),
		m_authRealm.c_str());
	if (returnValue != MAILSMTP_NO_ERROR)
	{
		m_error = returnValue;
		clog << "SMTP authentication error " << m_error << ": " << getErrorMessage(m_error) << endl;
		clog << "SASL authentication error " << m_error << ": " << sasl_errstring(m_error, NULL, NULL) << endl;
	}

	return returnValue;
}

bool LibETPANProvider::startSession(bool reset)
{
	bool isESMTP = false;

	if (m_session == NULL)
	{
#ifdef DEBUG
		clog << "LibETPANProvider::startSession: no session" << endl;
#endif
		return false;
	}

	if (m_messages.empty() == true)
	{
#ifdef DEBUG
		clog << "LibETPANProvider::startSession: no message" << endl;
#endif
		return true;
	}

	// Open the stream
	m_error = mailsmtp_socket_connect(m_session, m_hostName.c_str(), m_port);
	if (m_error == MAILSMTP_NO_ERROR)
	{
		int returnValue = mailesmtp_ehlo(m_session);

		if (returnValue == MAILSMTP_NO_ERROR)
		{
			m_error = returnValue;
			isESMTP = true;
		}
		else if (returnValue == MAILSMTP_ERROR_NOT_IMPLEMENTED)
		{
			m_error = mailsmtp_helo(m_session);
		}
#ifdef DEBUG
		clog << "LibETPANProvider::startSession: sent HELO" << endl;
#endif

		if ((m_error == MAILSMTP_NO_ERROR) &&
			(isESMTP == true) &&
			(m_startTLS == true))
		{
#ifdef DEBUG
			clog << "LibETPANProvider::startSession: trying STARTTLS" << endl;
#endif
			returnValue = mailesmtp_starttls(m_session);
			if (returnValue == MAILSMTP_NO_ERROR)
			{
				mailstream_low *low = mailstream_get_low(m_session->stream);
				int fd = mailstream_low_get_fd(low);
				mailstream_low *new_low = mailstream_low_tls_open(fd);

				mailstream_low_free(low);
				mailstream_set_low(m_session->stream, new_low);

				m_error = mailesmtp_ehlo(m_session);
			}
		}

		if ((m_error == MAILSMTP_NO_ERROR) &&
			(isESMTP == true) &&
			(m_authenticate == true))
		{
			m_error = authenticate();

			if (m_error != MAILSMTP_NO_ERROR)
			{
				for (vector<LibETPANMessage*>::iterator msgIter = m_messages.begin();
					msgIter != m_messages.end(); ++msgIter)
				{
					LibETPANMessage *pETPANMsg = (*msgIter);

					for (map<string, int>::iterator recipIter = pETPANMsg->m_recipients.begin();
							recipIter != pETPANMsg->m_recipients.end(); ++recipIter)
					{
						recipIter->second = -2;
					}
				}
			}
		}

		if (m_error == MAILSMTP_NO_ERROR)
		{
#ifdef DEBUG
			clog << "LibETPANProvider::startSession: " << m_messages.size() << " messages" << endl;
#endif
			for (vector<LibETPANMessage*>::iterator msgIter = m_messages.begin();
				msgIter != m_messages.end(); ++msgIter)
			{
				LibETPANMessage *pETPANMsg = (*msgIter);

				if ((pETPANMsg == NULL) ||
					(pETPANMsg->m_sent == true))
				{
					continue;
				}
				pETPANMsg->m_sent = true;

				if (isESMTP == true)
				{
					m_error = mailesmtp_mail(m_session,
						pETPANMsg->m_smtpFrom.c_str(), 1,
						pETPANMsg->getEnvId());
				}
				else
				{
					m_error = mailsmtp_mail(m_session, pETPANMsg->m_smtpFrom.c_str());
				}

				if (m_error != MAILSMTP_NO_ERROR)
				{
					break;
				}
#ifdef DEBUG
				clog << "LibETPANProvider::startSession: " << pETPANMsg->m_recipients.size() << " recipients" << endl;
#endif

				unsigned int successfulRecipients = 0;

				// Add recipients
				for (map<string, int>::iterator recipIter = pETPANMsg->m_recipients.begin();
					recipIter != pETPANMsg->m_recipients.end(); ++recipIter)
				{
					if (isESMTP == true)
					{
						int notify = MAILSMTP_DSN_NOTIFY_NEVER;

						if (pETPANMsg->m_dsnFlags == SMTPMessage::SUCCESS)
						{
							notify = MAILSMTP_DSN_NOTIFY_SUCCESS;
						}
						else if (pETPANMsg->m_dsnFlags == SMTPMessage::FAILURE)
						{
							notify = MAILSMTP_DSN_NOTIFY_FAILURE;
						}

						m_error = mailesmtp_rcpt(m_session, const_cast<char*>(recipIter->first.c_str()), notify, NULL);
					}
					else
					{
						m_error = mailsmtp_rcpt(m_session, const_cast<char*>(recipIter->first.c_str()));
					}

					int statusCode = errorToStatusCode(m_error);
					if (statusCode <= 250)
					{
						++successfulRecipients;
					}
					else
					{
						// Update this failed recipient now
						recipIter->second = statusCode;
					}
#ifdef DEBUG
					clog << "LibETPANProvider::startSession: recipient " << recipIter->first
						<< ", status " << statusCode << "/" << m_error << "/" << successfulRecipients << endl;
#endif
				}

				if (successfulRecipients == 0)
				{
#ifdef DEBUG
					clog << "LibETPANProvider::startSession: all recipients failed, skipping message delivery" << endl;
#endif
					continue;
				}

				// Deliver the message
				if (pETPANMsg->m_pString != NULL)
				{
					m_error = mailsmtp_data(m_session);
					if (m_error == MAILSMTP_NO_ERROR)
					{
#ifdef DEBUG
						clog << "LibETPANProvider::startSession: data is " << string(pETPANMsg->m_pString->str, pETPANMsg->m_pString->len) << endl;
#endif
						m_error = mailsmtp_data_message(m_session,
							pETPANMsg->m_pString->str, pETPANMsg->m_pString->len);
					}
#ifdef DEBUG
					else clog << "LibETPANProvider::startSession: message delivery failed" << endl;
#endif
				}
#ifdef DEBUG
				else clog << "LibETPANProvider::startSession: no message to deliver" << endl;
#endif

				successfulRecipients = 0;

				int statusCode = errorToStatusCode(m_error);
				for (map<string, int>::iterator recipIter = pETPANMsg->m_recipients.begin();
					recipIter != pETPANMsg->m_recipients.end(); ++recipIter)
				{
					// Update those recipients that didn't fail earlier
					if (recipIter->second == -1)
					{
						recipIter->second = statusCode;
						++successfulRecipients;
					}
				}
				if (m_session->response != NULL)
				{
					pETPANMsg->m_response = m_session->response;
				}
#ifdef DEBUG
				clog << "LibETPANProvider::startSession: message status "
					<< " " << statusCode << "/" << m_error << "/" << successfulRecipients
					<< ", response " << pETPANMsg->m_response << endl;
#endif
			}
		}
	}
	else
	{
#ifdef DEBUG
		clog << "LibETPANProvider::startSession: connection failed with error " << m_error << endl;
#endif
		m_error = MAILSMTP_ERROR_CONNECTION_REFUSED;
	}

	// We are done
	if ((m_session != NULL) &&
		(m_session->stream != NULL))
	{
		mailsmtp_quit(m_session);
		m_session->stream = NULL;
	}

	return true;
}

void LibETPANProvider::updateRecipientsStatus(StatusUpdater *pUpdater)
{
	if (pUpdater == NULL)
	{
		return;
	}

	for (vector<LibETPANMessage*>::iterator msgIter = m_messages.begin();
		msgIter != m_messages.end(); ++msgIter)
	{
		for (map<string, int>::iterator recipIter = (*msgIter)->m_recipients.begin();
			recipIter != (*msgIter)->m_recipients.end(); ++recipIter)
		{
			stringstream textStr;

			textStr << recipIter->second;
			textStr << " ";
			textStr << (*msgIter)->m_response;

			pUpdater->updateRecipientStatus(recipIter->first, recipIter->second,
				textStr.str().c_str(), (*msgIter)->m_msgId);
		}
	}

	// We are done with those now
	// Caller will delete the actual messages
	m_messages.clear();
}

int LibETPANProvider::getCurrentError(void)
{
	return m_error;
}

string LibETPANProvider::getErrorMessage(int errNum)
{
	if (errNum == MAILIMF_NO_ERROR)
	{
		return "";
	}

	const char *pErrorString = mailsmtp_strerror(errNum);
	if (pErrorString != NULL)
	{
		return string(pErrorString);
	}

	return "";
}

bool LibETPANProvider::isInternalError(int errNum)
{
	if ((errNum == MAILSMTP_ERROR_STREAM) ||
		(errNum == MAILSMTP_ERROR_MEMORY))
	{
		return true;
	}

	return false;
}

bool LibETPANProvider::isConnectionError(int errNum)
{
	if (errNum == MAILSMTP_ERROR_CONNECTION_REFUSED)
	{
		return true;
	}

	return false;
}

