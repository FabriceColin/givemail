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

#ifndef _THREADS_H_
#define _THREADS_H_

#include <string>

#include "MessageDetails.h"

/// Argument passed to threads.
class ThreadArg
{
	public:
		ThreadArg(const std::string &campaignId,
			MessageDetails *pDetails);
		virtual ~ThreadArg();

		std::string m_campaignId;
		MessageDetails *m_pDetails;

	private:
		// ThreadArg objects cannot be copied
		ThreadArg(const ThreadArg &other);
		ThreadArg &operator=(const ThreadArg &other);

};

#endif // _THREADS_H_
