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

#ifndef _DBSTATUSUPDATER_H_
#define _DBSTATUSUPDATER_H_

#include <string>

#include "SQLDB.h"
#include "StatusUpdater.h"

/// Updates the status of recipients in the database.
class DBStatusUpdater : public StatusUpdater
{
	public:
		DBStatusUpdater(SQLDB *pDb,
			const std::string &campaignId);
		virtual ~DBStatusUpdater();

		/// Returns the number of updated recipients.
		unsigned int getRecipientsCount(void);

		/// Updates the status of a domain recipients.
		virtual void updateRecipientsStatus(const std::string &domainName,
			int statusCode, const char *pText);

		/// Updates the status of a recipient.
		virtual void updateRecipientStatus(const std::string &emailAddress,
			int statusCode, const char *pText,
			const std::string &msgId = std::string(""));

	protected:
		SQLDB *m_pDb;
		std::string m_campaignId;
		unsigned int m_recipientsCount;

	private:
		DBStatusUpdater(const DBStatusUpdater &other);
		DBStatusUpdater &operator=(const DBStatusUpdater &other);

};

#endif // _DBSTATUSUPDATER_H_
