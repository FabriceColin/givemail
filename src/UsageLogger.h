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

#ifndef _USAGELOGGER_H_
#define _USAGELOGGER_H_

#include <string>

/// Logs WebAPI usage.
class UsageLogger
{
	public:
		UsageLogger();
		virtual ~UsageLogger();

		/// Logs a request.
		virtual void logRequest(const std::string &callName, int status) = 0;

	private:
		// UsageLogger objects cannot be copied
		UsageLogger(const UsageLogger &other);
		UsageLogger &operator=(const UsageLogger &other);

};

#endif // _USAGELOGGER_H_
