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

#ifndef _MESSAGEDETAILS_H_
#define _MESSAGEDETAILS_H_

#include <string>
#include <vector>
#include <set>
#include <map>

#include "Substituter.h"

/// An attachment.
class Attachment
{
	public:
		Attachment(const std::string &filePath,
			const std::string &contentType,
			const std::string &contentId,
			const std::string &encoding = std::string("base64"));
		virtual ~Attachment();

		/// Returns the attachment's file name.
		std::string getFileName(void) const;

		/// Indicates whether this is an inline attachment.
		bool isInline(void) const;

		std::string m_filePath;
		std::string m_contentType;
		std::string m_contentId;
		std::string m_encoding;
		off_t m_contentLength;
		char *m_pContent;
		off_t m_encodedLength;
		char *m_pEncodedContent;

	protected:
		void resolveContentType(void);

	private:
		Attachment(const Attachment &other);
		Attachment &operator=(const Attachment &other);

};

/// A piece of content.
class ContentPiece: public Attachment
{
	public:
		ContentPiece(const std::string &contentType,
			const std::string &encoding = std::string("quoted-printable"),
			bool personalize = true);
		virtual ~ContentPiece();

		bool m_personalize;

	private:
		ContentPiece(const ContentPiece &other);
		ContentPiece &operator=(const ContentPiece &other);

};

/// A message's details.
class MessageDetails
{
	public:
		MessageDetails();
		virtual ~MessageDetails();

		/**
		  * Loads the contents of the file raw.
		  * Caller frees with delete[].
		  */
		static char *loadRaw(const std::string &filePath, off_t &fileSize);

		/// Generates the string to substitute {Recipient ID} with.
		static std::string encodeRecipientId(const std::string &recipientId);

		/// Generates a message Id.
		std::string createMessageId(const std::string &suffix, bool isReplyId);

		/// Sets content of the specified type, replacing any existing such content.
		bool setContentPiece(const std::string &contentType,
			const char *pContentPiece,
			const std::string &encoding = std::string("quoted-printable"),
			bool personalize = true);

		/// Returns true if this is a report of the specified type.
		bool isReport(const std::string &contentType) const;

		/// Returns true if there's content of the specified type.
		bool hasContentPiece(const std::string &contentType) const;

		/// Gets the content of the specified type, if any.
		std::string getContent(const std::string &contentType) const;

		/// Gets the encoding of the specified type, if any.
		std::string getEncoding(const std::string &contentType) const;

		/// Gets a Substituter for plain text content.
		Substituter *getPlainSubstituter(const std::string &dictionaryId);

		/// Gets a Substituter for HTML content.
		Substituter *getHtmlSubstituter(const std::string &dictionaryId);

		/// Substitutes short strings such as subject.
		std::string substitute(const std::string &dictionaryId,
			const std::string &content,
			const std::map<std::string, std::string> &fieldValues);

		/// Returns whether the message has to be personalized for each recipient.
		bool isRecipientPersonalized(void);
 
		/// Adds an attachment.
		bool addAttachment(Attachment *pAttachment);

		/// Sets the list of files to attach to the message.
		void setAttachments(const std::vector<std::string> &filePaths,
			bool append = false);

		/// Loads all attached files' content.
		void loadAttachments(void);

		/// Returns the number of attachments.
		unsigned int getAttachmentCount(bool inlineParts = false) const;

		/// Gets an attachment.
		Attachment *getAttachment(unsigned int attachmentNum,
			bool inlineParts = false) const;

		std::string m_charset;
		std::string m_reportType;
		std::string m_subject;
		std::string m_to;
		std::string m_cc;
		std::string m_fromName;
		std::string m_fromEmailAddress;
		std::string m_replyToName;
		std::string m_replyToEmailAddress;
		std::string m_senderEmailAddress;
		std::string m_userAgent;
		bool m_useXMailer;
		std::vector<ContentPiece *> m_contentPieces;
		std::string m_unsubscribeLink;
		std::string m_msgId;
		bool m_isReply;

	protected:
		unsigned int m_version;
		Substituter *m_pPlainSub;
		Substituter *m_pHtmlSub;
		std::vector<Attachment *> m_attachments;
		unsigned int m_attachmentsCount;
		unsigned int m_inlinePartsCount;

		bool checkFields(const std::string &contentType);

		Substituter *newSubstituter(const std::string &dictionaryId,
			const std::string &contentTemplate,
			bool escapeEntities);

	private:
		MessageDetails(const MessageDetails &other);
		MessageDetails &operator=(const MessageDetails &other);

};

#endif // _MESSAGEDETAILS_H_
