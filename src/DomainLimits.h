/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 *  Copyright 2008 Global Sign In
 *  Copyright 2009-2010 Fabrice Colin
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

#ifndef _DOMAINLIMITS_H_
#define _DOMAINLIMITS_H_

#include <string>
#include <set>

#include "Resolver.h"

/// Wrapper for domain specific configuration items.
class DomainLimits
{
	public:
		DomainLimits(const std::string &domainName);
		DomainLimits(const DomainLimits &other);
		virtual ~DomainLimits();

		DomainLimits &operator=(const DomainLimits &other);
		bool operator<(const DomainLimits &other) const;

		std::string m_domainName;
		unsigned int m_maxMsgsPerServer;
		bool m_useSubmissionPort;
		std::set<ResourceRecord> m_mxRecords;

};

#endif // _DOMAINLIMITS_H_
