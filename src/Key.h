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

#ifndef _KEY_H_
#define _KEY_H_

#include <time.h>
#include <string>

/// Defines an authentication key.
class Key
{
	public:
		Key();
		Key(const std::string &dbId,
			const std::string &appId,
			const std::string &value,
			time_t creationTime,
			time_t expiryTime);
		Key(const std::string &appId,
			const std::string &value,
			time_t creationTime,
			time_t expiryTime);
		Key(const Key &other);
		~Key();

		Key &operator=(const Key &other);
		bool operator<(const Key &other) const;

		/**
		  * Returns the number of seconds before this key expires.
		  * If negative, the key has already expired.
		  */
		double getTimeDiff(time_t timeNow);

		std::string m_dbId;
		std::string m_appId;
		std::string m_value;
		time_t m_creationTime;
		time_t m_expiryTime;

};

#endif // _KEY_H_
