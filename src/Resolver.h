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

#ifndef _RESOLVER_H_
#define _RESOLVER_H_

#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <time.h>
#include <string>
#include <set>
#include <queue>

/// Wraps information about RRs.
class ResourceRecord
{
	public:
		ResourceRecord();
		ResourceRecord(const std::string &domainName, int priority,
			const std::string &hostName, int ttl, time_t queryTime);
		ResourceRecord(const ResourceRecord &other);
		~ResourceRecord();

		ResourceRecord &operator=(const ResourceRecord &other);
		bool operator==(const ResourceRecord &other) const;
		bool operator<(const ResourceRecord &other) const;

		/// Returns whether this record has expired.
		bool hasExpired(time_t timeNow) const;

		std::string m_domainName;
		int m_priority;
		std::string m_hostName;
		int m_ttl;
		time_t m_expiryTime;
		std::queue<ResourceRecord> m_addresses;

};

/// Queries the name server for records of the given FQDN.
class Resolver
{
	public:
		~Resolver();

		/// Queries the name server for the given FQDN, type MX.
		static bool queryMXRecords(const std::string &domainName,
			std::set<ResourceRecord> &servers);

		/// Queries the name server for the given FQDN, type A.
		static bool queryARecords(const std::string &domainName,
			std::set<ResourceRecord> &servers);

	protected:
		Resolver();

		/**
		  * Queries the name server for the FQDN of the specified type.
		  * Type may be ns_t_mx, ns_t_a or any other type defined in arpa/nameser.h.
		  */
		static bool queryRecords(const std::string &domainName, int type,
			std::set<ResourceRecord> &servers);

	private:
		Resolver(const Resolver &other);
		Resolver &operator=(const Resolver &other);

};

#endif // _RESOLVER_H_
