/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 *  Copyright 2008 Global Sign In
 *  Copyright 2009-2020 Fabrice Colin
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

#include <time.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <strings.h>
#include <sstream>
#include <iostream>
#include <algorithm>

#include "config.h"
#include "SMTPSession.h"
#include "Timer.h"

using std::clog;
using std::endl;
using std::map;
using std::set;
using std::string;
using std::stringstream;
using std::min;

static bool isIPAddress(const string &hostName)
{
	// Is the domain really a domain ?
	for (string::size_type i = 0; i < hostName.length(); ++i)
	{
		if ((isdigit(hostName[i]) == 0) &&
			(hostName[i] != '.'))
		{
			return false;
		}
	}

	return true;
}

pthread_mutex_t SMTPSession::m_mutex = PTHREAD_MUTEX_INITIALIZER;

SMTPSession::SMTPSession(const DomainLimits &domainLimits,
	const SMTPOptions &options) :
	m_domainLimits(domainLimits),
	m_options(options),
	m_msgsCount(0),
	m_msgsDataSize(0),
	m_pProvider(SMTPProviderFactory::getProvider()),
	m_topPriority(1000000),
	m_verifySignatures(false),
	m_dontSend(false),
	m_mutexSessions(false),
	m_errorNum(-1)
{
	char *pEnvVar = getenv("GIVEMAIL_VERIFY_SIGNATURES");

	// This enables signature verifications
	if ((pEnvVar != NULL) &&
		(strncasecmp(pEnvVar, "Y", 1) == 0))
	{
		m_verifySignatures = true;
	}

	pEnvVar = getenv("GIVEMAIL_DONT_SEND");

	// This deactivates sending
	if ((pEnvVar != NULL) &&
		(strncasecmp(pEnvVar, "Y", 1) == 0))
	{
		m_dontSend = true;
	}

	pEnvVar = getenv("GIVEMAIL_MUTEX_SESSIONS");

	// This protects libesmtp sessions with a mutex
	if ((pEnvVar != NULL) &&
		(strncasecmp(pEnvVar, "Y", 1) == 0))
	{
		m_mutexSessions = true;
	}
}

SMTPSession::~SMTPSession()
{
	destroySession();
	if (m_pProvider != NULL)
	{
		delete m_pProvider;
		m_pProvider = NULL;
	}
}

string SMTPSession::appendValueAndPath(const string &value, const string &path)
{
	stringstream headerStr;

	if ((value.empty() == true) &&
		(path.empty() == true))
	{
		return "";
	}

	if (value.empty() == false)
	{
		headerStr << value;
		if (path.empty() == false)
		{
			headerStr << " ";
		}
	}
	if (path.empty() == false)
	{
		headerStr << "<" << path << ">";
	}

	return headerStr.str();
}

bool SMTPSession::createSession(void)
{
	bool enableAuth = false, enableTLS = false;

	if (m_options.m_mailRelayAddress == m_domainLimits.m_domainName)
	{
		if (m_options.m_mailRelayUserName.empty() == false)
		{
			enableAuth = true;
		}
		enableTLS = m_options.m_mailRelayTLS;
	}

	if (m_pProvider == NULL)
	{
		return false;
	}

	if (m_pProvider->hasSession() == false)
	{
		// Cycle to the next server
		if ((m_pProvider->createSession(this) == false) ||
			(cycleServers() == false))
		{
			return false;
		}

		if (enableAuth == true)
		{
			// Assume the mail relay is the realm
			m_pProvider->enableAuthentication(m_options.m_mailRelayAddress,
				m_options.m_mailRelayUserName, m_options.m_mailRelayPassword);
		}
		else
		{
			m_pProvider->enableAuthentication("", "", "");
		}
		if (enableTLS == true)
		{
			m_pProvider->enableStartTLS();
		}
	}

	return true;
}

void SMTPSession::destroySession(void)
{
	if (m_pProvider != NULL)
	{
		m_pProvider->destroySession();
	}
}

bool SMTPSession::initializeAddresses(ResourceRecord &mxRecord)
{
	set<ResourceRecord> aRecords;

	if (isIPAddress(mxRecord.m_hostName) == true)
	{
		ResourceRecord aRecord(mxRecord.m_domainName,
			10, mxRecord.m_hostName, 36000, time(NULL));

		aRecords.insert(aRecord);
	}
	else if (Resolver::queryARecords(mxRecord.m_hostName, aRecords) == false)
	{
		return false;
	}

	// Empty the list of A records
	while (mxRecord.m_addresses.empty() == false)
	{
		mxRecord.m_addresses.pop();
	}

	for (set<ResourceRecord>::const_iterator addressIter = aRecords.begin();
		addressIter != aRecords.end(); ++addressIter)
	{
		// Don't put discarded records back in
		if (isDiscarded(*addressIter) == false)
		{
			clog << "Host " << mxRecord.m_hostName
				<< " has IP address " << addressIter->m_hostName << endl;

			mxRecord.m_addresses.push(*addressIter);
		}
	}

	return !mxRecord.m_addresses.empty();
}

bool SMTPSession::initializeTopQueue(void)
{
	bool isIP = isIPAddress(m_domainLimits.m_domainName);

#ifdef DEBUG
	clog << "SMTPSession::initializeTopQueue: " << m_domainLimits.m_domainName << " " << isIP << endl;
#endif
	if (isIP == true)
	{
		ResourceRecord mxRecord(m_domainLimits.m_domainName,
			10, m_domainLimits.m_domainName, 36000, time(NULL));
		ResourceRecord aRecord(m_domainLimits.m_domainName,
			10, m_domainLimits.m_domainName, 36000, time(NULL));

		mxRecord.m_addresses.push(aRecord);
		m_topQueue.push(mxRecord);

		return true;
	}

	time_t timeNow = time(NULL);

	// Go through MX records
	// They are sorted in order of increasing priority
	for (set<ResourceRecord>::const_reverse_iterator serverIter = m_domainLimits.m_mxRecords.rbegin();
		serverIter != m_domainLimits.m_mxRecords.rend(); ++serverIter)
	{
		// Set priority ?
		if ((m_topPriority == 1000000) &&
			(m_topPriority > serverIter->m_priority))
		{
			m_topPriority = serverIter->m_priority;
#ifdef DEBUG
			clog << "SMTPSession::initializeTopQueue: top priority " << m_topPriority << endl;
#endif
		}

		if (serverIter->m_priority > m_topPriority)
		{
			// This record and all those that follow are lower priority
			break;
		}

		ResourceRecord mxRecord(*serverIter);
		if ((mxRecord.hasExpired(timeNow) == false) &&
			(initializeAddresses(mxRecord) == true))
		{
			m_topQueue.push(mxRecord);
		}
	}
#ifdef DEBUG
	clog << "SMTPSession::initializeTopQueue: " << m_topQueue.size()
		<< " servers of priority " << m_topPriority << endl;
#endif

	return !m_topQueue.empty();
}

bool SMTPSession::queueNextPriorityRecords(void)
{
	time_t timeNow = time(NULL);
	int currentPriority = 1000000;

	// Go through MX records
	// They are sorted in order of increasing priority
	for (set<ResourceRecord>::const_reverse_iterator serverIter = m_domainLimits.m_mxRecords.rbegin();
		serverIter != m_domainLimits.m_mxRecords.rend(); ++serverIter)
	{
		if (serverIter->m_priority <= m_topPriority)
		{
			// Keep looking for lower priority records
			continue;
		}

		// Set priority ?
		if ((currentPriority == 1000000) &&
			(currentPriority > serverIter->m_priority))
		{
			currentPriority = serverIter->m_priority;
		}

		if (serverIter->m_priority < currentPriority)
		{
			// This record and all those that follow are lower priority
			break;
		}

		if (serverIter->hasExpired(timeNow) == false)
		{
			// Push records of the same priority
			m_topQueue.push(*serverIter);
		}
	}
	m_topPriority = currentPriority;
#ifdef DEBUG
	clog << "SMTPSession::queueNextPriorityRecords: top priority " << m_topPriority << endl;
#endif

	return !m_topQueue.empty();
}

bool SMTPSession::setServer(ResourceRecord &mxRecord)
{
	if ((m_pProvider == NULL) ||
		(mxRecord.m_hostName.empty() == true))
	{
		return false;
	}

	if (mxRecord.m_addresses.empty() == true)
	{
		// We have run out of A records to try...
		// Get them all again
		if (initializeAddresses(mxRecord) == false)
		{
			return false;
		}
	}

	time_t timeNow = time(NULL);

	// Cycle through A records
	while (mxRecord.m_addresses.empty() == false)
	{
		ResourceRecord frontRecord(mxRecord.m_addresses.front());
		int port = 25;

		mxRecord.m_addresses.pop();

		// Skip discarded records
		if (isDiscarded(frontRecord) == true)
		{
			continue;
		}

		// Is this record still alive ?
		if (frontRecord.hasExpired(timeNow) == true)
		{
			// No, it's not
#ifdef DEBUG
			clog << "SMTPSession::setServer: A record " << frontRecord.m_hostName
				<< " has expired" << endl;
#endif
			continue;
		}

		// The mail relay may be set as host name or IP address
		if ((m_options.m_mailRelayAddress == mxRecord.m_domainName) ||
			(m_options.m_mailRelayAddress == frontRecord.m_domainName) ||
			(m_options.m_mailRelayAddress == frontRecord.m_hostName))
		{
			port = m_options.m_mailRelayPort;
		}
		// Use port 25 smtp or 587 submission (default) ?
		else if (m_domainLimits.m_useSubmissionPort == true)
		{
			port = 587;
		}

		if (m_pProvider->setServer(frontRecord.m_hostName, port) == false)
		{
			clog << "Couldn't set server to " << frontRecord.m_hostName << endl;

			// Give up on this A record
			continue;
		}
		clog << "Set server for " << m_domainLimits.m_domainName << " to "
			<< frontRecord.m_hostName << ":" << port << endl;

		// Push it back
		mxRecord.m_addresses.push(frontRecord);

		return true;
	}

	return false;
}

void SMTPSession::discardCurrentServer(void)
{
	if (m_topQueue.empty() == true)
	{
		return;
	}

	// Currently-used records were pushed to the back of their respective queues
	ResourceRecord currentMXRecord(m_topQueue.back());
	if (currentMXRecord.m_addresses.empty() == false)
	{
		ResourceRecord &currentARecord = currentMXRecord.m_addresses.back();

		clog << "Discarding " << currentARecord.m_hostName << endl;

		// Discard this particular A record
		m_discarded.insert(currentARecord.m_hostName);
	}
}

bool SMTPSession::isDiscarded(const ResourceRecord &aRecord)
{
	string hostName(aRecord.m_hostName);

	// Was this record previously discarded for some reason ?
	if (m_discarded.find(hostName) != m_discarded.end())
	{
		// Yes, it was
#ifdef DEBUG
		clog << "SMTPSession::isDiscarded: A record " << hostName
			<< " was discarded" << endl;
#endif
		return true;
	}

	return false;
}

bool SMTPSession::signMessage(SMTPMessage *pMsg, DomainAuth &domainAuth)
{
	if ((pMsg == NULL) ||
		(m_pProvider == NULL))
	{
		return false;
	}

	// Proceed if keys etc are not set and signing isn't possible
	if (domainAuth.canSign() == false)
	{
		return true;
	}

	string fullMessage(m_pProvider->getMessageData(pMsg));

	// Sign the message
	if ((domainAuth.sign(fullMessage, pMsg, false) == true) &&
		((m_verifySignatures == false) || (domainAuth.verify(fullMessage, pMsg) == true)))
	{
		string signatureHeader(pMsg->getSignatureHeader());

#ifdef DEBUG
		clog << "SMTPSession::signMessage: signature is '" << signatureHeader << endl;
#endif
		m_msgsDataSize += fullMessage.length() + signatureHeader.length();

		return true;
	}

	return false;
}

string SMTPSession::getDomainName(void) const
{
	return m_domainLimits.m_domainName;
}

unsigned int SMTPSession::getTopMXServersCount(void) const
{
	return m_topQueue.size();
}

bool SMTPSession::cycleServers(void)
{
	if (m_topQueue.empty() == true)
	{
		// No more records in this priority, queue the next bunch
		if (queueNextPriorityRecords() == false)
		{
			// We have run out of MX records to try...
			// Get them all again
			if (initializeTopQueue() == false)
			{
				return false;
			}
		}
	}

	time_t timeNow = time(NULL);

	// Cycle through MX records
	while (m_topQueue.empty() == false)
	{
		ResourceRecord frontRecord(m_topQueue.front());

		m_topQueue.pop();

		// Is this record still alive ? Set it as server
		if ((frontRecord.hasExpired(timeNow) == false) &&
			(setServer(frontRecord) == true))
		{
			// Push it back in the queue
			m_topQueue.push(frontRecord);

			return true;
		}
		// Either it's expired, or can't be used as server

#ifdef DEBUG
		clog << "SMTPSession::cycleServers: MX record " << frontRecord.m_hostName
			<< " has expired or has no usable address" << endl;
#endif
		if (m_topQueue.empty() == true)
		{
			// Try the next priority
			if (queueNextPriorityRecords() == false)
			{
				// ...or get all MX records again
				if (initializeTopQueue() == false)
				{
					break;
				}
			}
		}
	}

	return false;
}

bool SMTPSession::generateMessages(DomainAuth &domainAuth,
	MessageDetails *pDetails, map<string, Recipient> &destinations,
	StatusUpdater *pUpdater)
{
	map<string, string> fieldValues;
	set<SMTPMessage *> messages;
	bool messageOk = true;

	if ((pDetails == NULL) ||
		(m_pProvider == NULL))
	{
		return false; 
	}

	if (destinations.empty() == true)
	{
		return true;
	}
	clog << destinations.size() << " recipients on domain " << m_domainLimits.m_domainName << endl;

	Timer generationTimer;
	SMTPMessage::DSNNotification dsnNotify = SMTPMessage::NEVER;

	// Activate DSN ?
	if (m_options.m_dsnNotify == "SUCCESS")
	{
		dsnNotify = SMTPMessage::SUCCESS;
	}
	else if (m_options.m_dsnNotify == "FAILURE")
	{
		dsnNotify = SMTPMessage::FAILURE;
	}
#ifdef DEBUG
	clog << "SMTPSession::generateMessages: DSN notification " << m_options.m_dsnNotify << " " << dsnNotify << endl;
#endif

	// Can we send the same message to all recipients ?
	if (pDetails->isRecipientPersonalized() == false)
	{
		// Yes, we can !
		map<string, Recipient>::iterator destIter = destinations.begin();
		if (destIter != destinations.end())
		{
			// Get non-recipient related (hopefully) custom fields from the first recipient
			fieldValues = destIter->second.m_customFields;
		}

		SMTPMessage *pMessage = m_pProvider->newMessage(fieldValues, pDetails,
			dsnNotify, false, m_options.m_msgIdSuffix, m_options.m_complaints);
		messages.insert(pMessage);

		// Queue the message
		if (queueMessage(pMessage, domainAuth,
			destinations, pUpdater) == false)
		{
			messageOk = false;
		}
	}
	else
	{
		// Each destination requires its own message unfortunately
		for (map<string, Recipient>::iterator destIter = destinations.begin();
			destIter != destinations.end(); ++destIter)
		{
			map<string, Recipient> recipients;
			string name(destIter->second.m_name);
			string emailAddress(destIter->second.m_emailAddress);

			// Fields should be substituted with these values
			fieldValues["Name"] = name;
			fieldValues["emailaddress"] = emailAddress;
			fieldValues["recipientId"] = MessageDetails::encodeRecipientId(destIter->second.m_id);
			for (map<string, string>::iterator customIter = destIter->second.m_customFields.begin();
				customIter != destIter->second.m_customFields.end(); ++customIter)
			{
				fieldValues[customIter->first] = customIter->second;
#ifdef DEBUG
				clog << "SMTPSession::generateMessages: field " << customIter->first << "=" << customIter->second << endl;
#endif
			}

			recipients[emailAddress] = destIter->second;

			SMTPMessage *pMessage = m_pProvider->newMessage(fieldValues, pDetails,
				dsnNotify, false, m_options.m_msgIdSuffix, m_options.m_complaints);
			messages.insert(pMessage);

			// Queue the message
			// FIXME: it might not be a good idea to queue too many messages, especially if content is huge
			if (queueMessage(pMessage, domainAuth,
				recipients, pUpdater) == false)
			{
				messageOk = false;
			}
		}
	}
	clog << "Queued " << messages.size() << " messages (" << m_msgsDataSize
		<< " signed bytes) for domain " << m_domainLimits.m_domainName
		<< " in " << generationTimer.stop() / 1000 << " seconds" << endl;

	generationTimer.start();

	// Force a send
	if ((m_msgsDataSize == 0) ||
		(dispatchMessages(pUpdater, true) == false))
	{
		messageOk = false;
	}

	clog << "Dispatched " << messages.size() << " messages to " << m_domainLimits.m_domainName
		<< " in " << generationTimer.stop() / 1000 << " seconds" << endl;

	generationTimer.start();

	// Delete all messages
	for (set<SMTPMessage *>::const_iterator msgIter = messages.begin();
		msgIter != messages.end(); ++msgIter)
	{
		SMTPMessage *pMsg = (*msgIter);

#ifdef DEBUG
		if ((messageOk == false) &&
			(m_pProvider->isInternalError(m_errorNum) == true))
		{
			clog << "SMTPSession::generateMessages: SMTP provider failed on below message" << endl
				<< dumpMessage(pMsg, "", "") << endl;
		}
#endif
		if (m_options.m_dumpFileBaseName.empty() == false)
		{
			pMsg->dumpToFile(m_options.m_dumpFileBaseName);
		}
		delete pMsg;
	}

	clog << "Deleted messages in " << generationTimer.stop() / 1000 << " seconds" << endl;

	return messageOk;
}

bool SMTPSession::queueMessage(SMTPMessage *pMsg,
	DomainAuth &domainAuth, map<string, Recipient> &recipients,
	StatusUpdater *pUpdater)
{
	string defaultReturnPath;
	bool setRecipientSpecificHeaders = false;
	bool setReturnPathHeader = false;
	bool serverOk = true;

	// Create, if necessary
	if ((m_pProvider == NULL) ||
		(createSession() == false))
	{
		return false;
	}

	if ((m_pProvider->hasSession() == false) ||
		(pMsg == NULL))
	{
		return false;
	}

	// Default Return-Path header
	if (pMsg->getSenderEmailAddress().empty() == false)
	{
		defaultReturnPath = pMsg->getSenderEmailAddress();
	}
	else if (pMsg->getFromEmailAddress().empty() == false)
	{
		defaultReturnPath = pMsg->getFromEmailAddress();
	}

	// If only one recipient, set To and Return-Path here
	if ((pMsg->m_pDetails != NULL) &&
		(recipients.size() == 1))
	{
		setRecipientSpecificHeaders = true;
		setReturnPathHeader = true;
	}

	m_pProvider->queueMessage(pMsg, defaultReturnPath);

	map<string, Recipient>::const_iterator recipIter = recipients.begin();
	while (recipIter != recipients.end())
	{
		string toHeader;
		string ccHeader;

		if (pMsg->m_pDetails != NULL)
		{
			toHeader = pMsg->m_pDetails->m_to;
			ccHeader = pMsg->m_pDetails->m_cc;
		}

		// Add as many recipients as allowed
		while ((m_msgsCount < m_domainLimits.m_maxMsgsPerServer) && (recipIter != recipients.end()))
		{
			string name(recipIter->second.m_name), emailAddress(recipIter->second.m_emailAddress);
			string dsnEnvId, reversePath;

			if (setRecipientSpecificHeaders == true)
			{
				if ((pMsg->m_pDetails != NULL) &&
					(pMsg->m_pDetails->m_to.empty() == true))
				{
					toHeader = appendValueAndPath(name, emailAddress);
				}

				// Use the recipient ID as envelope notifier
				if (pMsg->m_dsnFlags != SMTPMessage::NEVER)
				{
					dsnEnvId = MessageDetails::encodeRecipientId(recipIter->second.m_id);
				}

				setRecipientSpecificHeaders = false;
			}

			if (setReturnPathHeader == true)
			{
				// Return-Path header
				if (recipIter->second.m_returnPathEmailAddress.empty() == false)
				{
					reversePath = recipIter->second.m_returnPathEmailAddress;
#ifdef DEBUG
					clog << "SMTPSession::queueMessage: overrode Return-Path to "
						<< recipIter->second.m_returnPathEmailAddress << endl;
#endif
				}
				else if (defaultReturnPath.empty() == false)
				{
					reversePath = defaultReturnPath;
#ifdef DEBUG
					clog << "SMTPSession::queueMessage: reverted to default Return-Path" << endl;
#endif
				}

				setReturnPathHeader = false;
			}

			pMsg->setEnvId(dsnEnvId);
			pMsg->setReversePath(reversePath);

			if ((pMsg->m_pDetails != NULL) &&
				(pMsg->m_pDetails->m_unsubscribeLink.empty() == false))
			{
				string encodedId(MessageDetails::encodeRecipientId(recipIter->second.m_id));
				string::size_type eqPos = encodedId.find('=');

				// Base64 encoding may bring in = characters
				while (eqPos != string::npos)
				{
					encodedId.erase(eqPos, 1);

					if (eqPos >= encodedId.length())
					{
						break;
					}
					eqPos = encodedId.find('=', eqPos);
				}
				pMsg->addHeader("List-Unsubscribe", pMsg->m_pDetails->m_unsubscribeLink + encodedId, "");
			}

			pMsg->addRecipient(emailAddress);

			++recipIter;
			++m_msgsCount;
		}

		// Set a CC only if there's a To
		// If there's no To, use CC
		// Both may need substitution
		if (toHeader.empty() == false)
		{
			pMsg->addHeader("To",
				pMsg->m_pDetails->substitute(toHeader, pMsg->m_fieldValues), "");
			if (ccHeader.empty() == false)
			{
				pMsg->addHeader("CC",
					pMsg->m_pDetails->substitute(ccHeader, pMsg->m_fieldValues), "");
			}
		}
		else if (ccHeader.empty() == false)
		{
			pMsg->addHeader("To",
				pMsg->m_pDetails->substitute(ccHeader, pMsg->m_fieldValues), "");
		}

		// Sign the message
		if (signMessage(pMsg, domainAuth) == false)
		{
			// Signing is broken
			serverOk = false;
			break;
		}
		else
		{
			if (dispatchMessages(pUpdater, false) == false)
			{
				// Chances are this message wasn't delivered to anyone
				serverOk = false;
				break;
			}
		}
	}

	return serverOk;
}

bool SMTPSession::dispatchMessages(StatusUpdater *pUpdater, bool force)
{
	bool serverOk = true;

	if ((m_pProvider == NULL) ||
		(m_msgsCount == 0))
	{
		// No message queued
		return true;
	}

	// Do we have to send messages now ?
	if ((m_msgsCount < m_domainLimits.m_maxMsgsPerServer) &&
		(force == false))
	{
		// No, we don't
#ifdef DEBUG
		clog << "SMTPSession::dispatchMessages: post-poned sending ("
			<< m_msgsCount << "/" << m_domainLimits.m_maxMsgsPerServer << ")" << endl;
#endif
		return true;
	}

	// This should only be true in test mode
	if (m_dontSend == true)
	{
		// Pretend messages were sent out
		m_msgsCount = 0;
		m_msgsDataSize = 0;
		return true;
	}

	// Reset error variables
	recordError(true);

#ifdef DEBUG
	clog << "SMTPSession::dispatchMessages: sending " << m_msgsCount << " messages" << endl;
#endif
	Timer sessionTimer;
	if (m_mutexSessions == true)
	{
		pthread_mutex_lock(&m_mutex);
	}
	if (m_pProvider->startSession(false) == false)
	{
		recordError();
		serverOk = false;
	}
#ifdef DEBUG
	clog << "SMTPSession::dispatchMessages: sent " << m_msgsCount << " messages" << endl;
#endif
	// Did some kind of connection error occur ?
	// FIXME: this is hacky
	if ((m_pProvider->isConnectionError(m_errorNum) == true) ||
		(m_errorMsg.find("onnect") != string::npos))
	{
		// Try again with another server
		discardCurrentServer();

		serverOk = cycleServers();
		if (serverOk == true)
		{
			if (m_pProvider->startSession(true) == false)
			{
				recordError();
			}
			else
			{
				// This time was okay
				recordError(true);
			}
		}
	}
	if (m_mutexSessions == true)
	{
		pthread_mutex_unlock(&m_mutex);
	}

	suseconds_t sessionMilliSecs = sessionTimer.stop();
	clog << "Sent " << m_msgsCount << " messages (" << m_msgsDataSize
		<< " bytes) to " << m_domainLimits.m_domainName
		<< " in " << sessionMilliSecs / 1000 << " seconds: ";
	if (sessionMilliSecs == 0)
	{
		clog << "N/A kb/s" << endl;
	}
	else
	{
		clog << ((m_msgsDataSize * 1000) / (1024 * sessionMilliSecs)) << " kb/s" << endl;
	}

	if (serverOk == true)
	{
		sessionTimer.start();

		m_pProvider->updateRecipientsStatus(pUpdater);

		clog << "Enumerated " << m_msgsCount << " messages in "
			<< sessionTimer.stop() / 1000 << " seconds" << endl;
	}
	m_msgsCount = 0;
	m_msgsDataSize = 0;

	// Destroy the current session
	destroySession();
	// ...and create a new one for the next batch
	if (createSession() == false)
	{
		serverOk = false;
	}

	return serverOk;
}

string SMTPSession::dumpMessage(SMTPMessage *pMsg,
	const string &dummyName, const string &dummyEmailAddress)
{
	if ((pMsg == NULL) ||
		(m_pProvider == NULL))
	{
		return "";
	}

	// To header
	if (dummyEmailAddress.empty() == false)
	{
		pMsg->addHeader("To", dummyName.c_str(), dummyEmailAddress.c_str());
	}

	return m_pProvider->getMessageData(pMsg);
}

void SMTPSession::setError(int errNum)
{
	if (m_pProvider != NULL)
	{
		m_errorNum = errNum;
		m_errorMsg = m_pProvider->getErrorMessage(m_errorNum);
	}
}

void SMTPSession::recordError(bool reset)
{
	m_errorMsg.clear();
	if (reset == true)
	{
		m_errorNum = -1;
		return;
	}

	if (m_pProvider != NULL)
	{
		setError(m_pProvider->getCurrentError());
		clog << "SMTP error " << m_errorNum << ": " << m_errorMsg << endl;
	}
}

int SMTPSession::getError(string &msg) const
{
	msg = m_errorMsg;

	return m_errorNum;
}

bool SMTPSession::isInternalError(int errNum)
{
	if (m_pProvider == NULL)
	{
		return true;
	}

	return m_pProvider->isInternalError(errNum);
}

