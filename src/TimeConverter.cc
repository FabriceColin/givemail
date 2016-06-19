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

#include "config.h"
#ifdef USE_CURL
#include <curl/curl.h>
#else
#ifdef USE_NEON
#include <neon/ne_dates.h>
#endif
#endif
#include <string.h>
#ifdef HAVE_TIMEGM
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#include <time.h>
#undef _XOPEN_SOURCE
#else
#include <time.h>
#endif
#else
#include <time.h>
/* This comment and function are taken from Wget's mktime_from_utc()
   Converts struct tm to time_t, assuming the data in tm is UTC rather
   than local timezone.

   mktime is similar but assumes struct tm, also known as the
   "broken-down" form of time, is in local time zone.  mktime_from_utc
   uses mktime to make the conversion understanding that an offset
   will be introduced by the local time assumption.

   mktime_from_utc then measures the introduced offset by applying
   gmtime to the initial result and applying mktime to the resulting
   "broken-down" form.  The difference between the two mktime results
   is the measured offset which is then subtracted from the initial
   mktime result to yield a calendar time which is the value returned.

   tm_isdst in struct tm is set to 0 to force mktime to introduce a
   consistent offset (the non DST offset) since tm and tm+o might be
   on opposite sides of a DST change.

   Some implementations of mktime return -1 for the nonexistent
   localtime hour at the beginning of DST.  In this event, use
   mktime(tm - 1hr) + 3600.

   Schematically
     mktime(tm)   --> t+o
     gmtime(t+o)  --> tm+o
     mktime(tm+o) --> t+2o
     t+o - (t+2o - t+o) = t

   Note that glibc contains a function of the same purpose named
   `timegm' (reverse of gmtime).  But obviously, it is not universally
   available, and unfortunately it is not straightforwardly
   extractable for use here.  Perhaps configure should detect timegm
   and use it where available.

   Contributed by Roger Beeman <beeman@cisco.com>, with the help of
   Mark Baushke <mdb@cisco.com> and the rest of the Gurus at CISCO.
   Further improved by Roger with assistance from Edward J. Sabol
   based on input by Jamie Zawinski.  */
static time_t mktime_from_utc (struct tm *t)
{
  time_t tl, tb;
  struct tm *tg;

  tl = mktime (t);
  if (tl == -1)
    {
      t->tm_hour--;
      tl = mktime (t);
      if (tl == -1)
	return -1; /* can't deal with output from strptime */
      tl += 3600;
    }
  tg = gmtime (&tl);
  tg->tm_isdst = 0;
  tb = mktime (tg);
  if (tb == -1)
    {
      tg->tm_hour--;
      tb = mktime (tg);
      if (tb == -1)
	return -1; /* can't deal with output from gmtime */
      tb += 3600;
    }
  return (tl - (tb - tl));
}
#endif

#include "TimeConverter.h"

using std::string;

TimeConverter::TimeConverter()
{
}

time_t TimeConverter::timegm(struct tm *tm)
{
#ifdef HAVE_TIMEGM
	return ::timegm(tm);
#else
	return mktime_from_utc(tm);
#endif
}

string TimeConverter::toTimestamp(time_t aTime, bool inGMTime)
{
	struct tm timeTm;

	if (((inGMTime == true) &&
		(gmtime_r(&aTime, &timeTm) != NULL)) ||
		(localtime_r(&aTime, &timeTm) != NULL))
	{
		char timeStr[64];
		size_t formattedSize = 0;

		if (inGMTime == true)
		{
			formattedSize = strftime(timeStr, 64, "%a, %d %b %Y %H:%M:%S GMT", &timeTm);
		}
		else
		{
#if defined(__GNU_LIBRARY__)
			// %z is a GNU extension
			formattedSize = strftime(timeStr, 64, "%a, %d %b %Y %H:%M:%S %z", &timeTm);
#else
			formattedSize = strftime(timeStr, 64, "%a, %d %b %Y %H:%M:%S %Z", &timeTm);
#endif
		}
		if (formattedSize > 0)
		{
			return timeStr;
		}
	}

	return "";
}

time_t TimeConverter::fromTimestamp(const string &timestamp)
{
        if (timestamp.empty() == true)
        {
                return 0;
        }

#ifdef USE_CURL
        return curl_getdate(timestamp.c_str(), NULL);
#else
#ifdef USE_NEON
        return ne_rfc1123_parse(timestamp.c_str());
#endif
#endif
}

string TimeConverter::toDateTime(time_t aTime)
{
	struct tm *pTimeTm = new struct tm;

	if (gmtime_r(&aTime, pTimeTm) != NULL)
	{
		char timeStr[64];
		size_t formattedSize = 0;

		formattedSize = strftime(timeStr, 64, "%Y-%m-%dT%H:%M:%S", pTimeTm);
		if (formattedSize > 0)
		{
			delete pTimeTm;

			return timeStr;
		}
	}
	delete pTimeTm;

	return "";
}

time_t TimeConverter::fromDateTime(const string &timestamp, bool inGMTime)
{
	struct tm timeTm;

	if (timestamp.empty() == true)
	{
		return 0;
	}

	// Initialize the structure
	timeTm.tm_sec = timeTm.tm_min = timeTm.tm_hour = timeTm.tm_mday = 0;
	timeTm.tm_mon = timeTm.tm_year = timeTm.tm_wday = timeTm.tm_yday = timeTm.tm_isdst = 0;

	char *pRemainder = strptime(timestamp.c_str(), "%Y-%m-%dT%H:%M:%S%z", &timeTm);
	if (pRemainder == NULL)
	{
		pRemainder = strptime(timestamp.c_str(), "%Y-%m-%dT%H:%M:%S", &timeTm);
		if (pRemainder == NULL)
		{
			pRemainder = strptime(timestamp.c_str(), "%Y-%m-%d", &timeTm);
			if (pRemainder == NULL)
			{
				return 0;
			}
		}

		if (inGMTime == true)
		{
			// Assume timezone is GMT
			return timegm(&timeTm);
		}
	}

	return mktime(&timeTm);
}

