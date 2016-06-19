/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 *  Copyright 2008 Global Sign In
 *  Copyright 2009-2011 Fabrice Colin
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

#ifndef _STATUSUPDATER_H_
#define _STATUSUPDATER_H_

#include <fstream>
#include <string>
#include <map>

/// An interface to update status.
class StatusUpdater
{
	public:
		StatusUpdater(const std::string &statusFileName = std::string(""));
		virtual ~StatusUpdater();

		/// Updates the status of a domain recipients.
		virtual void updateRecipientsStatus(const std::string &domainName,
			int statusCode, const char *pText);

		/// Updates the status of a recipient.
		virtual void updateRecipientStatus(const std::string &emailAddress,
			int statusCode, const char *pText,
			const std::string &msgId = std::string(""));

		/// Returns accumulated statuses.
		const std::map<std::string, int> &getStatus(void) const;

		/// Clear accumulated statuses.
		void clear(void);

	protected:
		std::ofstream m_statusFile;
		std::map<std::string, int> m_status;

	private:
		StatusUpdater(const StatusUpdater &other);
		StatusUpdater &operator=(const StatusUpdater &other);

};

#endif // _STATUSUPDATER_H_
