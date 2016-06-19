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

#ifndef _TIMECONVERTER_H_
#define _TIMECONVERTER_H_

#include <time.h>
#include <string>

/// Timestamp conversions.
class TimeConverter
{
	public:
		/// Inverse of gmtime().
		static time_t timegm(struct tm *tm);

		/// Converts into a RFC 822 timestamp.
		static std::string toTimestamp(time_t aTime, bool inGMTime);

		/// Converts from a RFC 822 timestamp.
		static time_t fromTimestamp(const std::string &timestamp);

		/// Converts into a XML Schema dateTime.
		static std::string toDateTime(time_t aTime);

		/// Converts from a XML Schema dateTime.
		static time_t fromDateTime(const std::string &timestamp, bool inGMTime);

	protected:
		TimeConverter();

	private:
		TimeConverter(const TimeConverter &other);
		TimeConverter& operator=(const TimeConverter& other);

};

#endif // _TIMECONVERTER_H_
