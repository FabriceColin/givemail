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

#include "config.h"
#include "Timer.h"

Timer::Timer()
{
#ifdef HAVE_GETTIMEOFDAY
	gettimeofday(&m_start, NULL);
	gettimeofday(&m_stop, NULL);
#endif
}

Timer::Timer(const Timer &other) :
	m_start(other.m_start),
	m_stop(other.m_stop)
{
}

Timer::~Timer()
{
}

Timer &Timer::operator=(const Timer &other)
{
	if (this != &other)
	{
		m_start = other.m_start;
		m_stop = other.m_stop;
	}

	return *this;
}

void Timer::start(void)
{
#ifdef HAVE_GETTIMEOFDAY
	gettimeofday(&m_start, NULL);
#endif
}

suseconds_t Timer::stop(void)
{
#ifdef HAVE_GETTIMEOFDAY
	gettimeofday(&m_stop, NULL);

	suseconds_t timeDiff = (((m_stop.tv_sec - m_start.tv_sec) * 1000) + ((m_stop.tv_usec - m_start.tv_usec) / 1000));

	return timeDiff;
#else
	return 0;
#endif
}

