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

#ifndef _RECIPIENT_H_
#define _RECIPIENT_H_

#include <unistd.h>
#include <time.h>
#include <string>
#include <map>

/// A recipient is an address to which email is sent.
class Recipient
{
	public:
		Recipient();
		Recipient(const std::string &id, const std::string &name,
			const std::string &status, const std::string &emailAddress,
			const std::string &returnPathEmailAddress);
		Recipient(const Recipient &other);
		~Recipient();

		typedef enum { AS_TO = 0, AS_CC, AS_BCC } RecipientType;

		Recipient &operator=(const Recipient &other);
		bool operator<(const Recipient &other) const;

		/// Checks whether the given status is valid.
		static bool isValidStatus(const std::string &status);

		/// Checks whether the current status is valid.
		bool hasValidStatus(void) const;

		/// Extracts name and email address.
		static Recipient extractNameAndEmailAddress(const std::string &details);

		std::string m_id;
		std::string m_name;
		std::string m_status;
		std::string m_emailAddress;
		std::string m_returnPathEmailAddress;
		std::string m_statusCode;
		std::map<std::string, std::string> m_customFields;
		time_t m_timeSent;
		off_t m_numAttempts;
		RecipientType m_type;

};

#endif // _RECIPIENT_H_
