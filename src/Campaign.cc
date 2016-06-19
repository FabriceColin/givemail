/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 *  Copyright 2008 Global Sign In
 *  Copyright 2009 Fabrice Colin
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
#include <stdlib.h>
#include <sstream>

#include "Base64.h"
#include "Campaign.h"

using std::string;
using std::stringstream;

static const char *g_validStatus[] = { "Draft", "Ready", "Sending", "Sent", "Resending", NULL };

Campaign::Campaign() :
	m_status("Draft"),
	m_timestamp(0)
{
}

Campaign::Campaign(const string &id, const string &name,
	const string &status, time_t timestamp) :
	m_id(id),
	m_name(name),
	m_status(status),
	m_timestamp(timestamp)
{
}

Campaign::Campaign(const Campaign &other) :
	m_id(other.m_id),
	m_name(other.m_name),
	m_status(other.m_status),
	m_timestamp(other.m_timestamp)
{
}

Campaign::~Campaign()
{
}

Campaign &Campaign::operator=(const Campaign &other)
{
	m_id = other.m_id;
	m_name = other.m_name;
	m_status = other.m_status;
	m_timestamp = other.m_timestamp;

	return *this;
}

bool Campaign::operator<(const Campaign &other) const
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

bool Campaign::hasValidStatus(void) const
{
	for (unsigned int statusNum = 0; g_validStatus[statusNum] != NULL; ++statusNum)
	{
		if (m_status == g_validStatus[statusNum])
		{
			return true;
		}
	}

	return false;
}

