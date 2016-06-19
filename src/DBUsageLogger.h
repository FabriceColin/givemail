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

#ifndef _DBUSAGELOGGER_H_
#define _DBUSAGELOGGER_H_

#include <time.h>
#include <string>

#include "SQLDB.h"
#include "UsageLogger.h"

/// Logs WebAPI usage to the database.
class DBUsageLogger : public UsageLogger
{
	public:
		DBUsageLogger(SQLDB *pDb,
			const std::string &remoteAddress, unsigned int remotePort);
		virtual ~DBUsageLogger();

		/// Finds the last time a request was received.
		virtual time_t findLastRequestTime(void);

		/// Logs a request.
		virtual void logRequest(const std::string &callName, int status);

		/// Sets the key Id under which future requests should be logged.
		void setKeyId(const std::string &keyId);

		/// Sets the current request's hash.
		void setHash(const std::string &hash);

	protected:
		SQLDB *m_pDb;
		std::string m_remoteAddress;
		unsigned int m_remotePort;
		time_t m_currentTime;
		std::string m_keyId;
		std::string m_hash;

	private:
		// DBUsageLogger objects cannot be copied
		DBUsageLogger(const DBUsageLogger &other);
		DBUsageLogger &operator=(const DBUsageLogger &other);

};

#endif // _DBUSAGELOGGER_H_
