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

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <iostream>
#include <algorithm>

#include "Resolver.h"

using std::clog;
using std::endl;
using std::string;
using std::set;
using std::max;

static void addNameResourceRecords(const string &domainName, time_t queryTime,
	ns_msg *pMsg, int type, ns_sect section, set<ResourceRecord> &servers)
{
	int recordsCount = ns_msg_count(*pMsg, section);
	for (int recordNum = 0; recordNum < recordsCount; recordNum++)
	{
		ns_rr rr;
		char *pRRBuffer = NULL;
		char domainStr[NS_MAXDNAME];
		char ttlStr[NS_MAXDNAME];
		char classStr[NS_MAXDNAME];
		char typeStr[NS_MAXDNAME];
		char hostStr[NS_MAXDNAME];
		int priority = 0;

		ns_parserr(pMsg, ns_s_an, recordNum, &rr);

		// Make the buffer big enough to accomodate two domain names and some extras
		pRRBuffer = new char[NS_MAXDNAME * 3];
		ns_sprintrr(pMsg, &rr, NULL, NULL, pRRBuffer, NS_MAXDNAME * 3);
#ifdef DEBUG
		clog << "Resolver::addNameResourceRecords: RR '" << pRRBuffer << "'" << endl
			<< "\tdomain name " << ns_rr_name(rr) << endl
			<< "\tTTL (seconds) " << ns_rr_ttl(rr) << endl
			<< "\tclass " << ns_rr_class(rr) << endl
			<< "\ttype " << ns_rr_type(rr) << endl
			<< "\tdata length " << ns_rr_rdlen(rr) << endl;
#endif

		if (ns_rr_class(rr) != ns_c_in)
		{
			clog << "Class is not Internet" << endl;

			delete[] pRRBuffer;
			continue;
		}

#if 0
		char hostName[NS_MAXDNAME];
		if (ns_name_uncompress(ns_msg_base(*pMsg), ns_msg_end(*pMsg),
			ns_rr_rdata(rr), hostName, NS_MAXDNAME) < 0)
		{
			clog << "Couldn't uncompress name" << endl;
		}
		else
		{
#ifdef DEBUG
			clog << "Resolver::queryRecords: host name " << hostName << endl;
#endif
		}
#endif

		// Is this the type of record we are looking for ?
		// We only support MX and A records for the time being
		if ((type == ns_t_mx) &&
			(ns_rr_type(rr) == ns_t_mx))
		{
			// Example RR : google.com.          1h14m31s IN MX  10 smtp1.google.com.
			if (sscanf(pRRBuffer, "%s %s %s %s %d %s", &domainStr, &ttlStr, &classStr, &typeStr, &priority, &hostStr) != 6)
			{
				clog << "Couldn't parse MX RR" << endl;

				delete[] pRRBuffer;
				continue;
			}
		}
		else if ((type == ns_t_a) &&
			(ns_rr_type(rr) == ns_t_a))
		{
			if (sscanf(pRRBuffer, "%s %s %s %s %s", &domainStr, &ttlStr, &classStr, &typeStr, &hostStr) != 5)
			{
				clog << "Couldn't parse A RR" << endl;

				delete[] pRRBuffer;
				continue;
			}

			priority = recordNum;
		}

		// We don't need this buffer any more
		delete[] pRRBuffer;

		// Check the class again
		if (strncasecmp(classStr, "IN", 2) != 0)
		{
#ifdef DEBUG
			clog << "Resolver::addNameResourceRecords: class is "
				<< classStr << ", not Internet" << endl;
#endif
			continue;
		}

		// Skip dummy names, often found for domains on sale
		if ((hostStr[0] == '\0') ||
			(strncasecmp(hostStr, "dev.null", 8) == 0))
		{
			continue;
		}

		// Remove the trailing . in the host name
		string hostName(hostStr);
		if ((hostName.empty() == false) &&
			(hostName[hostName.length() - 1] == '.'))
		{
			hostName.resize(hostName.length() - 1);
		}

		clog << "Found resource " << hostName << ", type " << type
			<< " for " << domainName << endl;

		servers.insert(ResourceRecord(domainName, priority, hostName,
			max((int)ns_rr_ttl(rr), 60), queryTime));
	}
}

ResourceRecord::ResourceRecord() :
	m_priority(0),
	m_ttl(60),
	m_expiryTime(time(NULL) + 3600)
{
}

ResourceRecord::ResourceRecord(const string &domainName, int priority,
	const string &hostName, int ttl, time_t queryTime) :
	m_domainName(domainName),
	m_priority(priority),
	m_hostName(hostName),
	m_ttl(ttl),
	m_expiryTime(queryTime + ttl)
{
}

ResourceRecord::ResourceRecord(const ResourceRecord &other) :
	m_domainName(other.m_domainName),
	m_priority(other.m_priority),
	m_hostName(other.m_hostName),
	m_ttl(other.m_ttl),
	m_expiryTime(other.m_expiryTime),
	m_addresses(other.m_addresses)
{
}

ResourceRecord::~ResourceRecord()
{
}

ResourceRecord &ResourceRecord::operator=(const ResourceRecord &other)
{
	if (this != &other)
	{
		m_domainName = other.m_domainName;
		m_priority = other.m_priority;
		m_hostName = other.m_hostName;
		m_ttl = other.m_ttl;
		m_expiryTime = other.m_expiryTime;
		m_addresses = other.m_addresses;
	}

	return *this;
}

bool ResourceRecord::operator==(const ResourceRecord &other) const
{
	if ((m_domainName == other.m_domainName) &&
		(m_priority == other.m_priority) &&
		(m_hostName == other.m_hostName) &&
		(m_ttl == other.m_ttl))
	{
		return true;
	}

	return false;
}

bool ResourceRecord::operator<(const ResourceRecord &other) const
{
	if (m_domainName < other.m_domainName)
	{
		return true;
	}
	else if (m_domainName == other.m_domainName)
	{
		if (m_priority > other.m_priority)
		{
			return true;
		}
		else if (m_priority == other.m_priority)
		{
			if (m_hostName < other.m_hostName)
			{
				return true;
			}
			else if (m_hostName == other.m_hostName)
			{
				if (m_ttl < other.m_ttl)
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool ResourceRecord::hasExpired(time_t timeNow) const
{
	if (m_expiryTime <= timeNow)
	{
		return true;
	}

	return false;
}

Resolver::Resolver()
{
}

Resolver::~Resolver()
{
}

bool Resolver::queryMXRecords(const string &domainName,
	set<ResourceRecord> &servers)
{
	return Resolver::queryRecords(domainName, ns_t_mx, servers);
}

bool Resolver::queryARecords(const string &domainName,
	set<ResourceRecord> &servers)
{
	return Resolver::queryRecords(domainName, ns_t_a, servers);
}

bool Resolver::queryRecords(const string &domainName, int type,
	set<ResourceRecord> &servers)
{
	u_char nsBuffer[4096];

	if (domainName.empty() == true)
	{
		return false;
	}

#ifdef DEBUG
	_res.options |= RES_DEBUG;
#endif
	time_t timeNow = time(NULL);
	// FIXME: broken name servers may return "No such name" for queries of class ns_c_any
	int responseLength = res_query(domainName.c_str(), ns_c_in, type, nsBuffer, 4096);
	if (responseLength < 0)
	{
		if (errno == 0)
		{
			clog << "No record of type " << type << " for " << domainName << endl;
		}
		else
		{
#ifdef HAVE_STRERROR_R
			char errBuffer[1024];

			strerror_r(errno, errBuffer, 1024);
#else
			char *errBuffer = strerror(errno);
#endif
			clog << "Name query error " << errno << " on " << domainName << ": " << errBuffer << endl;
		}

		return false;
	}

	ns_msg msg;

	ns_initparse(nsBuffer, responseLength, &msg);

	// Parse the answer
	addNameResourceRecords(domainName, timeNow, &msg, type, ns_s_an, servers);
#if 0
	// Parse the authority section 
	addNameResourceRecords(domainName, timeNow, &msg, type, ns_s_ns, servers);
#endif

	return !servers.empty();
}

