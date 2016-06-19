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

#include <strings.h>
#include <time.h>
#include <stdlib.h>
#include <string>
#include <iostream>

#include "DomainLimits.h"

using std::clog;
using std::endl;
using std::string;

DomainLimits::DomainLimits(const string &domainName) :
	m_domainName(domainName),
	m_maxMsgsPerServer(10),
	m_useSubmissionPort(false)
{
}

DomainLimits::DomainLimits(const DomainLimits &other) :
	m_domainName(other.m_domainName),
	m_maxMsgsPerServer(other.m_maxMsgsPerServer),
	m_useSubmissionPort(other.m_useSubmissionPort),
	m_mxRecords(other.m_mxRecords)
{
}

DomainLimits::~DomainLimits()
{
}

DomainLimits &DomainLimits::operator=(const DomainLimits &other)
{
	if (this != &other)
	{
		m_domainName = other.m_domainName;
		m_maxMsgsPerServer = other.m_maxMsgsPerServer;
		m_useSubmissionPort = other.m_useSubmissionPort;
		m_mxRecords = other.m_mxRecords;
	}

	return *this;
}

bool DomainLimits::operator<(const DomainLimits &other) const
{
	if (m_domainName < other.m_domainName)
	{
		return true;
	}

	return false;
}

