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

#ifndef _SMTPSESSION_H_
#define _SMTPSESSION_H_

#include <pthread.h>
#include <map>
#include <queue>
#include <set>
#include <string>

#include "DomainAuth.h"
#include "DomainLimits.h"
#include "MessageDetails.h"
#include "Recipient.h"
#include "Resolver.h"
#include "SMTPOptions.h"
#include "SMTPProvider.h"
#include "StatusUpdater.h"

/// A session is associated to each domain name emails are to be sent to.
class SMTPSession
{
	public:
		SMTPSession(const DomainLimits &domainLimits,
			const SMTPOptions &options);
		virtual ~SMTPSession();

		/// Appends name and email address.
		static std::string appendValueAndPath(const std::string &name, const std::string &emailAddress);

		/// Returns the domain name associated with this session.
		std::string getDomainName(void) const;

		/// Returns the number of top-priority MX servers.
		unsigned int getTopMXServersCount(void) const;

		/// Cycles to the next MX/A record pair.
		bool cycleServers(void);

		/**
		  * Convenience wrapper around queueMessage() and dispatchMessages()
		  * that generates and sends 1 or N messages for a domain's recipients.
		  */
		bool generateMessages(DomainAuth &domainAuth, MessageDetails *pDetails,
			std::map<std::string, Recipient> &destinations,
			StatusUpdater *pUpdater);

		/// Queues, and possibly dispatches, a message to the given recipients.
		bool queueMessage(SMTPMessage *pMsg, DomainAuth &domainAuth,
			std::map<std::string, Recipient> &recipients,
			StatusUpdater *pUpdater);

		/// Dispatches all queued messages.
		bool dispatchMessages(StatusUpdater *pUpdater, bool force);

		/// Dumps a message.
		std::string dumpMessage(SMTPMessage *pMsg, const std::string &dummyName,
			const std::string &dummyEmailAddress);

		/// Sets the error.
		void setError(int errNum);

		/// Records the current error.
		void recordError(bool reset = false);

		/// Gets the latest error.
		int getError(std::string &msg) const;

		/// Returns true if this is an internal error.
		bool isInternalError(int errNum);

	protected:
		static pthread_mutex_t m_mutex;
		DomainLimits m_domainLimits;
		SMTPOptions m_options;
		unsigned int m_msgsCount;
		off_t m_msgsDataSize;
		SMTPProvider *m_pProvider;
		int m_topPriority;
		std::queue<ResourceRecord> m_topQueue;
		std::set<std::string> m_discarded;
		bool m_dontSend;
		bool m_mutexSessions;
		int m_errorNum;
		std::string m_errorMsg;

		/// Creates a new session.
		bool createSession(void);

		/// Destroys the current session.
		void destroySession(void);

		/// Initializes a MX record's addresses by pulling A records.
		bool initializeAddresses(ResourceRecord &mxRecord);

		/// Initializes the queue with top-priority MX records.
		bool initializeTopQueue(void);

		/// Queues MX records of lower priority.
		bool queueNextPriorityRecords(void);

		/// Sets the server to be used to one of the MX record's addresses.
		bool setServer(ResourceRecord &mxRecord);

		/// Discards the current server.
		void discardCurrentServer(void);

		/// Returns true if this record was previously discarded.
		bool isDiscarded(const ResourceRecord &aRecord);

		/// Signs a message prior to sending.
		void signMessage(SMTPMessage *pMsg, DomainAuth &domainAuth);

	private:
		SMTPSession(const SMTPSession &other);
		SMTPSession &operator=(const SMTPSession &other);

};

#endif // _SMTPSESSION_H_
