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

#include "Recipient.h"

using std::string;

static const char *g_validStatus[] = { "Waiting", "Sent", "Failed", NULL };

Recipient::Recipient() :
	m_status("Waiting"),
	m_statusCode("0"),
	m_timeSent(0),
	m_numAttempts(0),
	m_type(AS_TO)
{
}

Recipient::Recipient(const string &id,
	const string &name, const string &status,
	const string &emailAddress,
	const string &returnPathEmailAddress) :
	m_id(id),
	m_name(name),
	m_status(status),
	m_emailAddress(emailAddress),
	m_returnPathEmailAddress(returnPathEmailAddress),
	m_statusCode("0"),
	m_timeSent(0),
	m_numAttempts(0),
	m_type(AS_TO)
{
}

Recipient::Recipient(const Recipient &other) :
	m_id(other.m_id),
	m_name(other.m_name),
	m_status(other.m_status),
	m_emailAddress(other.m_emailAddress),
	m_returnPathEmailAddress(other.m_returnPathEmailAddress),
	m_statusCode(other.m_statusCode),
	m_customFields(other.m_customFields),
	m_timeSent(other.m_timeSent),
	m_numAttempts(other.m_numAttempts),
	m_type(other.m_type)
{
}

Recipient::~Recipient()
{
}

Recipient &Recipient::operator=(const Recipient &other)
{
	m_id = other.m_id;
	m_name = other.m_name;
	m_status = other.m_status;
	m_emailAddress = other.m_emailAddress;
	m_returnPathEmailAddress = other.m_returnPathEmailAddress;
	m_statusCode = other.m_statusCode;
	m_customFields = other.m_customFields;
	m_timeSent = other.m_timeSent;
	m_numAttempts = other.m_numAttempts;
	m_type = other.m_type;

	return *this;
}

bool Recipient::operator<(const Recipient &other) const
{
	if (m_id < other.m_id)
	{
		return true;
	}
	else if (m_id == other.m_id)
	{
		// IDs are unique so this code should never be reached
		if (m_name < other.m_name)
		{
			return true;
		}
	}

	return false;
}

bool Recipient::isValidStatus(const std::string &status)
{
	for (unsigned int statusNum = 0; g_validStatus[statusNum] != NULL; ++statusNum)
	{
		if (status == g_validStatus[statusNum])
		{
			return true;
		}
	}

	return false;
}

bool Recipient::hasValidStatus(void) const
{
	return isValidStatus(m_status);
}

Recipient Recipient::extractNameAndEmailAddress(const string &details)
{
	string name, emailAddress;

	if (details.empty() == true)
	{
		return Recipient("", "", "Ready", "", "");
	}

	string::size_type ltPos = details.find('<');
	string::size_type gtPos = details.find('>');
	if ((ltPos != string::npos) ||
		(gtPos != string::npos) ||
		(ltPos < gtPos))
	{
		name = details.substr(0, ltPos);
		emailAddress = details.substr(ltPos + 1, gtPos - ltPos - 1);
	}
	else if ((ltPos == string::npos) &&
		(gtPos == string::npos))
	{
		emailAddress = details;
	}

	return Recipient(emailAddress, name,
		"Ready", emailAddress, "");
}

