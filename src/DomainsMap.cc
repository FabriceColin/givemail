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

#include <iostream>

#include "DomainsMap.h"

using std::clog;
using std::endl;
using std::string;
using std::multimap;

DomainsMap *DomainsMap::m_pInstance = NULL;

DomainsMap::DomainsMap()
{
	pthread_mutex_init(&m_mutex, 0);
}

DomainsMap::~DomainsMap()
{
	pthread_mutex_destroy(&m_mutex);
}

DomainsMap *DomainsMap::getInstance(void)
{
	if (m_pInstance == NULL)
	{
		m_pInstance = new DomainsMap();
	}

	return m_pInstance;
}

multimap<off_t, string> &DomainsMap::getMap(void)
{
	return m_domainsByRecipients;
}

bool DomainsMap::getTopDomain(string &domainName,
	unsigned int &recipientsCount)
{
	bool foundTop = false;

	// Lock the map
	if (pthread_mutex_lock(&m_mutex) == 0)
	{
		multimap<off_t, std::string>::iterator domainIter = m_domainsByRecipients.begin();
		if (domainIter != m_domainsByRecipients.end())
		{
			foundTop = true;

			if ((m_lowestDomainCount == domainIter->first) &&
				(m_lowestDomainName == domainIter->second))
			{
#ifdef DEBUG
				clog << "DomainsMap::getTopDomain: reached low water-mark " << domainIter->second << endl;
#endif
				foundTop = false;
			}
			else
			{
				recipientsCount = domainIter->first;
				domainName = domainIter->second;
				m_domainsByRecipients.erase(domainIter);
#ifdef DEBUG
				clog << "DomainsMap::getTopDomain: returning domain " << domainName << endl;
#endif
			}
		}

		// Unlock the map
		pthread_mutex_unlock(&m_mutex);
	}

	return foundTop;
}

bool DomainsMap::getBottomDomain(string &domainName,
	unsigned int &recipientsCount)
{
	bool foundBottom = false;

	// Lock the map
	if (pthread_mutex_lock(&m_mutex) == 0)
	{
		multimap<off_t, string>::reverse_iterator domainIter = m_domainsByRecipients.rbegin();

		// Go up to the low watermark
		while (domainIter != m_domainsByRecipients.rend())
		{
			if ((m_lowestDomainCount == domainIter->first) &&
				(m_lowestDomainName == domainIter->second))
			{
				// Here we are
				break;
			}

			// Next
			++domainIter;
		} 

		if (domainIter != m_domainsByRecipients.rend())
		{
			// Get the next one
			++domainIter;

			if (domainIter != m_domainsByRecipients.rend())
			{
				// Record this as the new low watermark beyond which domains have been processed
				m_lowestDomainCount = domainIter->first;
				m_lowestDomainName = domainIter->second;

				recipientsCount = domainIter->first;
				domainName = domainIter->second;
				foundBottom = true;
#ifdef DEBUG
				clog << "DomainsMap::getBottomDomain: returning domain " << domainName << endl;
#endif
			}
		}

		// Unlock the map
		pthread_mutex_unlock(&m_mutex);
	}

	return foundBottom;
}
