/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 *  Copyright 2008 Global Sign In
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

#include "Key.h"

using std::string;
using std::stringstream;

Key::Key()
{
}

Key::Key(const string &dbId,
	const string &appId,
	const string &value,
	time_t creationTime,
	time_t expiryTime) :
	m_dbId(dbId),
	m_appId(appId),
	m_value(value),
	m_creationTime(creationTime),
	m_expiryTime(expiryTime)
{
}

Key::Key(const string &appId,
	const string &value,
	time_t creationTime,
	time_t expiryTime) :
	m_appId(appId),
	m_value(value),
	m_creationTime(creationTime),
	m_expiryTime(expiryTime)
{
}

Key::Key(const Key &other) :
	m_dbId(other.m_dbId),
	m_appId(other.m_appId),
	m_value(other.m_value),
	m_creationTime(other.m_creationTime),
	m_expiryTime(other.m_expiryTime)
{
}

Key::~Key()
{
}

Key &Key::operator=(const Key &other)
{
	m_dbId = other.m_dbId;
	m_appId = other.m_appId;
	m_value = other.m_value;
	m_creationTime = other.m_creationTime;
	m_expiryTime = other.m_expiryTime;

	return *this;
}

bool Key::operator<(const Key &other) const
{
	if (m_appId < other.m_appId)
	{
		return true;
	}
	else if (m_appId == other.m_appId)
	{
		// IDs are unique so this code should never be reached
		if (m_dbId < other.m_dbId)
		{
			return true;
		}
	}

	return false;
}

double Key::getTimeDiff(time_t timeNow)
{
	return difftime(m_expiryTime, timeNow);
}

