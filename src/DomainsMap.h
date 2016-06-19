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

#ifndef _DOMAINSMAP_H_
#define _DOMAINSMAP_H_

#include <unistd.h>
#include <map>
#include <string>

/// Map of domains that have recipients.
class DomainsMap
{
	public:
		virtual ~DomainsMap();

		static DomainsMap *getInstance(void);

		/**
		  * Returns a reference to the map.
		  * This is a convenience method to use only to populate the map.
		  */
		std::multimap<off_t, std::string> &getMap(void);

		/**
		  * Returns a domain from the top of the map.
		  * Top domains have the most recipients.
		  */
		bool getTopDomain(std::string &domainName,
			unsigned int &recipientsCount);

		/**
		  * Returns a domain from the bottom of the map.
		  * Bottom domains have few recipients.
		  */
		bool getBottomDomain(std::string &domainName,
			unsigned int &recipientsCount);

	protected:
		static DomainsMap *m_pInstance;
		pthread_mutex_t m_mutex;
		std::multimap<off_t, std::string> m_domainsByRecipients;
		off_t m_lowestDomainCount;
		std::string m_lowestDomainName;

		DomainsMap();

	private:
		// DomainsMap objects cannot be copied
		DomainsMap(const DomainsMap &other);
		DomainsMap &operator=(const DomainsMap &other);

};

#endif // _DOMAINSMAP_H_
