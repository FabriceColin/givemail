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

#include "config.h"
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#include <signal.h>
#include <stdlib.h>
#include <strings.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>
#include <libintl.h>
#include <fnmatch.h>
#include <map>
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>

#include "Campaign.h"
#include "ConfigurationFile.h"
#include "CSVParser.h"
#include "OpenDKIM.h"
#include "Process.h"
#include "Recipient.h"
#include "SMTPSession.h"
#include "StatusUpdater.h"
#include "Substituter.h"
#include "Threads.h"
#include "Timer.h"
#include "XmlMessageDetails.h"

// Custom fields
#define CAMPAIGN_NAME_CUSTOM_FIELD	"customfield1"
#define ATTACHMENT_NAME_CUSTOM_FIELD	"customfield2"
#define TIMESTAMP_CUSTOM_FIELD		"customfield3"
#define RECIPIENTS_LIST_CUSTOM_FIELD	"customfield4"

using namespace std;

/**
  * A simple wrapper around a campaign list of recipients.
  * This mimics CampaignSQL.
  */
class RecipientsMap
{
	public:
		RecipientsMap(const string &campaignName, const string &attachmentName) :
			m_campaignName(campaignName),
			m_attachmentName(attachmentName)
		{
		}

		~RecipientsMap()
		{
		}

		/// Lists all domains by number of recipients.
		off_t listDomains(multimap<off_t, string> &domainsBreakdown)
		{
			map<string, off_t> allDomains;

			for (map<string, Recipient>::const_iterator recipIter = m_recipients.begin();
				recipIter != m_recipients.end(); ++recipIter)
			{
				string domainName;

				string::size_type atPos = recipIter->first.find('@');
				if ((atPos != string::npos) &&
					(atPos + 1 < recipIter->first.length()))
				{
					domainName = recipIter->first.substr(atPos + 1);
				}

				map<string, off_t>::iterator domainIter = allDomains.find(domainName);
				if (domainIter != allDomains.end())
				{
					domainIter->second = domainIter->second + 1;	
				}
				else
				{
					allDomains[domainName] = 1;	
				}
			}

			ConfigurationFile *pConfig = ConfigurationFile::getInstance("");
			if ((pConfig != NULL) &&
				(pConfig->m_options.m_mailRelayAddress.empty() == false))
			{
				off_t internalRecipientsCount = 0;

				// The only domains we need to consider are the internal domain and the relay
				// Any recipient on this domain ?
				map<string, off_t>::const_iterator domainIter = allDomains.find(pConfig->m_options.m_internalDomain);
				if (domainIter != allDomains.end())
				{
					internalRecipientsCount = domainIter->second;
					if (pConfig->m_options.m_mailRelayAddress == pConfig->m_options.m_internalDomain)
					{
						// If relay and internal domain are the same, they will deal with all recipients
						internalRecipientsCount = m_recipients.size();
					}

					domainsBreakdown.insert(pair<off_t, string>(internalRecipientsCount, pConfig->m_options.m_internalDomain));
				}
				// Don't add a domain if relay and internal domain are the same
				if (pConfig->m_options.m_mailRelayAddress != pConfig->m_options.m_internalDomain)
				{
					domainsBreakdown.insert(pair<off_t, string>(m_recipients.size() - internalRecipientsCount,
						pConfig->m_options.m_mailRelayAddress));
				}

				return (off_t)domainsBreakdown.size();
			}

			for (map<string, off_t>::const_iterator domainIter = allDomains.begin();
				domainIter != allDomains.end(); ++domainIter)
			{
				domainsBreakdown.insert(pair<off_t, string>(domainIter->second, domainIter->first));
			}

			return (off_t)allDomains.size();
		}

		/// Gets all recipients belonging to a domain.
		bool getRecipients(const std::string &domainName,
			map<string, Recipient> &recipients)
		{
			ConfigurationFile *pConfig = ConfigurationFile::getInstance("");
			bool foundDomains = false;

#ifdef DEBUG
			cout << "RecipientsMap::getRecipients: domain " << domainName << endl;
#endif
			for (map<string, Recipient>::const_iterator recipIter = m_recipients.begin();
				recipIter != m_recipients.end(); ++recipIter)
			{
				string::size_type atPos = recipIter->first.find('@');
				if ((atPos != string::npos) &&
					(atPos + 1 < recipIter->first.length()))
				{
					Recipient thisRecipient(recipIter->second);
					string recipientDomain(recipIter->first.substr(atPos + 1));

					// Add substitution fields for campaign and attachment 
					thisRecipient.m_customFields[CAMPAIGN_NAME_CUSTOM_FIELD] = m_campaignName;
					thisRecipient.m_customFields[ATTACHMENT_NAME_CUSTOM_FIELD] = m_attachmentName;

					// When requesting recipients for the relay, provide all recipients not on the internal domain
					// or all recipients if the internal domain and the relay are one and the same
					if ((pConfig != NULL) &&
						(pConfig->m_options.m_internalDomain.empty() == false) &&
						(pConfig->m_options.m_mailRelayAddress == domainName) &&
						((pConfig->m_options.m_internalDomain != recipientDomain) || (pConfig->m_options.m_mailRelayAddress == pConfig->m_options.m_internalDomain)))
					{
						recipients[recipIter->first] = thisRecipient;
						foundDomains = true;
#ifdef DEBUG
						cout << "RecipientsMap::getRecipients: relaying to recipient " << recipIter->first << endl;
#endif
					}
					else if (domainName == recipientDomain)
					{
						recipients[recipIter->first] = thisRecipient;
						foundDomains = true;
#ifdef DEBUG
						cout << "RecipientsMap::getRecipients: recipient " << recipIter->first << endl;
#endif
					}
				}
			}

			return foundDomains;
		}

		map<string, Recipient> m_recipients;
		string m_campaignName;
		string m_attachmentName;

};

/// Argument passed to run units.
class UnitArg : public ThreadArg
{
	public:
		UnitArg(const std::string &campaignId,
			MessageDetails *pDetails,
			RecipientsMap *pRecipientsMap) :
			ThreadArg(campaignId, pDetails),
			m_pRecipientsMap(pRecipientsMap)
		{
		}
		virtual ~UnitArg()
		{
		}

		RecipientsMap *m_pRecipientsMap;

	private:
		// UnitArg objects cannot be copied
		UnitArg(const UnitArg &other);
		UnitArg &operator=(const UnitArg &other);

};

/// A class that manages starting/stopping threads or processes.
class RunUnits
{
	public:
		RunUnits() :
			m_isChild(false)
		{
		}
		~RunUnits()
		{
		}

		static string getId(void)
		{
			ConfigurationFile *pConfig = ConfigurationFile::getInstance("");
			stringstream idStr;

			if (pConfig->m_threaded == true)
			{
				// FIXME: pthread_t may not always be a numerical ID
				idStr << "Thread " << pthread_self();
			}
			else
			{
				idStr << "Process " << getpid();
			}

			return idStr.str();
		}

		static void cleanup(void)
		{
			ConfigurationFile *pConfig = ConfigurationFile::getInstance("");

			if (pConfig->m_threaded == true)
			{
				OpenDKIM::cleanupThread();
			}
		}

		bool create(void *(*startRoutine)(void*), UnitArg *pArg)
		{
			ConfigurationFile *pConfig = ConfigurationFile::getInstance("");
			bool created = false;

			if (pConfig->m_threaded == true)
			{
				created = createThread(startRoutine, pArg);
			}
			else
			{
				created = createChild(startRoutine, pArg);
			}

			if (created == true)
			{
				m_unitArgs.push_back(pArg);
			}

			return created;
		}

		off_t getCount(void) const
		{
			ConfigurationFile *pConfig = ConfigurationFile::getInstance("");

			if (pConfig->m_threaded == true)
			{
				return (off_t)m_threadIds.size();
			}

			return (off_t)m_childIds.size();
		}

		bool isChild(void) const
		{
			return m_isChild;
		}

		void join(void)
		{
			ConfigurationFile *pConfig = ConfigurationFile::getInstance("");

			if (pConfig->m_threaded == true)
			{
				for (set<pthread_t>::const_iterator idIter = m_threadIds.begin();
					idIter != m_threadIds.end(); ++idIter)
				{
					if (pthread_join(*idIter, NULL) != 0)
					{
						cerr << "Failed to join thread " << *idIter << endl;
					}

					cout << "Joined thread " << *idIter << endl;
				}
				m_threadIds.clear();
			}
			else
			{
				for (set<pid_t>::const_iterator idIter = m_childIds.begin();
					idIter != m_childIds.end(); ++idIter)
				{
					if (waitpid(*idIter, NULL, 0) != *idIter)
					{
						cerr << "Failed to wait for child " << *idIter << endl;
					}

					cout << "Waited for child " << *idIter << endl;
				}
				m_childIds.clear();
			}

			// Delete all argument objects
			for (vector<UnitArg *>::iterator argIter = m_unitArgs.begin();
				argIter != m_unitArgs.end(); ++argIter)
			{
				if (*argIter == NULL)
				{
					continue;
				}

				// Delete the whole thing
				delete (*argIter)->m_pDetails;
				delete (*argIter)->m_pRecipientsMap;
				delete (*argIter);
			}
			m_unitArgs.clear();
		}

	protected:
		vector<UnitArg *> m_unitArgs;
		set<pthread_t> m_threadIds;
		set<pid_t> m_childIds;
		bool m_isChild;

		bool createThread(void *(*startRoutine)(void*), UnitArg *pArg)
		{
			pthread_t threadId;

			// Start it up
			if (pthread_create(&threadId, NULL, startRoutine, (void*)pArg) != 0)
			{
				return false;
			}
			m_threadIds.insert(threadId);

			cout << "Created thread " << threadId << endl;

			return true;
		}

		bool createChild(void *(*startRoutine)(void*), UnitArg *pArg)
		{
			pid_t childId = fork();
			if (childId == -1)
			{
				return false;
			}
			else if (childId > 0)
			{
				cout << "Created child " << childId << endl;

				m_childIds.insert(childId);
				return true;
			}

			// Child
			m_isChild = true;
			startRoutine((void*)pArg);

			return false;
		}

};

static bool g_mustQuit = false;

#ifdef HAVE_GETOPT_H
static struct option g_longOptions[] = {
	{"base-directory", required_argument, NULL, 'b'},
	{"campaign", required_argument, NULL, 'a'},
	{"configuration-file", required_argument, NULL, 'c'},
	{"file-name", required_argument, NULL, 'f'},
	{"help", no_argument, NULL, 'h'},
	{"log-file", required_argument, NULL, 'l'},
	{"priority", required_argument, NULL, 'y'},
	{"timestamp", required_argument, NULL, 's'},
	{"to", required_argument, NULL, 't'},
	{"version", no_argument, NULL, 'v'},
	{"xml-file", required_argument, NULL, 'x'},
	{0, 0, 0, 0}
};
#endif

/// Prints an help message.
static void printHelp(void)
{
	// Help
	cout << "csv2givemail - Extracts message and recipient from a CSV file\n\n"
#ifdef HAVE_GETOPT_H
		<< "Usage: csv2givemail [OPTIONS] CSV_FILE\n\n"
		<< "Options:\n"
		<< "  -a, --campaign COLUMNNUM          column number that holds the campaign name\n"
		<< "  -b, --base-directory DIRNAME      directory that holds files to attach\n"
		<< "  -c, --configuration-file FILENAME load configuration from the given file\n"
		<< "  -f, --file-name COLUMNNUN         column number that holds the file name pattern\n"
		<< "  -h, --help                        display this help and exit\n"
		<< "  -l, --log-file FILENAME           redirect output to the specified log file\n"
		<< "  -s, --timestamp COLUMNNUM         column number that holds the timestamp\n"
		<< "  -t, --to COLUMNNUM                column number that holds the list of To recipients\n"
		<< "  -v, --version                     output version information and exit\n"
		<< "  -x, --xml-file FILENAME           load email details from the given file\n"
		<< "  -y, --priority                    set the scheduling priority (default 15)\n";
#else
		<< "Usage: csv2givemail CONFIGURATION_FILE XML_FILE LOG_FILE CSV_FILE ATTACHMENTS_DIRECTORY CAMPAIGN_COLNUM FILE_COLNUM TIMESTAMP_COLUMN TO_COLUMN\n";
#endif
	cout << "Substitution fields are :\n" << CAMPAIGN_NAME_CUSTOM_FIELD << " campaign name\n"
		<< ATTACHMENT_NAME_CUSTOM_FIELD << " file name pattern\n"
		<< TIMESTAMP_CUSTOM_FIELD << " timestamp\n"
		<< RECIPIENTS_LIST_CUSTOM_FIELD << " list of recipients" << endl;
}

/// Catch signals and take the appropriate action.
static void catchSignals(int sigNum)
{
	if (sigNum != SIGPIPE)
	{
		cout << "Received signal " << sigNum << ". Quitting..." << endl;
		g_mustQuit = true;
	}
}

/// Find recipients for the given domain name and email them.
static bool sendToDomain(const DomainLimits &domainLimits,
	MessageDetails *pDetails, RecipientsMap *pRecipientsMap,
	DomainAuth &domainAuth, StatusUpdater &statusUpdater)
{
	ConfigurationFile *pConfig = ConfigurationFile::getInstance("");
	SMTPSession session(domainLimits, pConfig->m_options);
	map<string, Recipient> recipients;
	set<string> fieldNames;

	if ((pDetails == NULL) ||
		(pRecipientsMap == NULL))
	{
		return false;
	}

	// Get all waiting recipients for this domain
	if (pRecipientsMap->getRecipients(domainLimits.m_domainName, recipients) == false)
	{
		return false;
	}

	Timer domainTimer;

	cout << RunUnits::getId() << " serving " << recipients.size()
		<< " recipients on " << domainLimits.m_domainName << endl;

	// Send to all recipients
	if (session.generateMessages(domainAuth, pDetails, recipients, &statusUpdater) == false)
	{
		// All recipients failed
		for (map<string, Recipient>::iterator recipIter = recipients.begin();
			recipIter != recipients.end(); ++recipIter)
		{
			statusUpdater.updateRecipientStatus(recipIter->first, 0, NULL);
		}
	}

	cout << RunUnits::getId() << " emailed " << recipients.size()
		<< " recipients on " << domainLimits.m_domainName << " in "
		<< domainTimer.stop() / 1000 << " seconds" << endl;

	return true;
}

/// Entry point for worker units.
void *workerUnitFunc(void *pArg)
{
	ConfigurationFile *pConfig = ConfigurationFile::getInstance("");
	multimap<off_t, string> domainsBreakdown;

	if (pArg == NULL)
	{
		cerr << RunUnits::getId() << " has no argument" << endl;
		return NULL;
	}

	UnitArg *pUnitArg = (UnitArg *)pArg;
	MessageDetails *pDetails = pUnitArg->m_pDetails;
	RecipientsMap *pRecipientsMap = pUnitArg->m_pRecipientsMap;

	// Look for fields in content
	if ((pDetails == NULL) ||
		(pRecipientsMap == NULL))
	{
		cerr << RunUnits::getId() << " couldn't get substitution objects" << endl;
		return NULL;
	}
	// FIXME: make sure pDetails' properties are all set correctly

	// Get a list of all domains
	domainsBreakdown.clear();
	off_t domainsCount = pRecipientsMap->listDomains(domainsBreakdown);
	if (domainsCount == 0)
	{
		cerr << RunUnits::getId() << " couldn't break recipients down by domain" << endl;
		return NULL;
	}

	OpenDKIM domainKeys;
	StatusUpdater statusUpdater;
	Timer campaignTimer;

	cout << RunUnits::getId() << " has " << domainsCount << " domains" << endl;

	// Go through the list of domains
	for (multimap<off_t, string>::const_iterator domainIter = domainsBreakdown.begin();
		(g_mustQuit == false) && (domainIter != domainsBreakdown.end()); ++domainIter)
	{
		string domainName(domainIter->second);
		DomainLimits domainLimits(domainName);

		if (pConfig->findDomainLimits(domainLimits, true) == true)
		{
			domainKeys.loadPrivateKey(pConfig);

			if (sendToDomain(domainLimits, pDetails, pRecipientsMap,
				domainKeys, statusUpdater) == false)
			{
				statusUpdater.updateRecipientsStatus(domainName, 0, "No recipient");
			}
		}
		else
		{
			cout << RunUnits::getId() << " skipping domain " << domainName << endl;

			statusUpdater.updateRecipientsStatus(domainName, 0, "No MX record");
		}
	}

	cout << RunUnits::getId() << " processed " << domainsCount
		<< " domains in " << campaignTimer.stop() / 1000 << " seconds" << endl;

	// List failed recipients
	const map<string, int> &status = statusUpdater.getStatus();
	for (map<string, int>::const_iterator statusIter = status.begin();
		statusIter != status.end(); ++statusIter)
	{
		if (statusIter->second == 250)
		{
			continue;
		}

		if (statusIter->first.find('@') == string::npos)
		{
			cout << "Domain ";
		}
		else
		{
			cout << "Recipient ";
		}
		cout << statusIter->first << " failed with status code " << statusIter->second << endl;
	}
 
	RunUnits::cleanup();

	return NULL;
}

/// Scans a directory for files matching the supplied pattern.
static bool scanFiles(const string &entryName, const string &attachmentPattern,
	vector<string> &filesToAttach, bool scanSubDirs)
{
	struct stat fileStat;
	bool foundMatch = false;

	int entryStatus = stat(entryName.c_str(), &fileStat);
	if (entryStatus == -1)
	{
		return false;
	}

	// What type of entry is this ?
	if (S_ISREG(fileStat.st_mode))
	{
		if (fnmatch(attachmentPattern.c_str(), entryName.c_str(), FNM_NOESCAPE) == 0)
		{
			// This is what we are looking for
			filesToAttach.push_back(entryName);
			foundMatch = true;
		}
	}
	else if ((S_ISDIR(fileStat.st_mode)) &&
		(scanSubDirs == true))
	{
		// Open the directory
		DIR *pDir = opendir(entryName.c_str());
		if (pDir == NULL)
		{
			return false;
		}

		// Iterate through this directory's entries
		struct dirent64 *pDirEntry = readdir64(pDir);
		while (pDirEntry != NULL)
		{
			char *pEntryName = pDirEntry->d_name;

			// Skip . .. and dotfiles
			if ((pEntryName != NULL) &&
				(pEntryName[0] != '.'))
			{
				string subEntryName(entryName);

				if (entryName[entryName.length() - 1] != '/')
				{
					subEntryName += "/";
				}
				subEntryName += pEntryName;

				// Scan this
				if (scanFiles(subEntryName, attachmentPattern, filesToAttach, false) == true)
				{
					foundMatch = true;
				}
			}

			// Next entry
			pDirEntry = readdir64(pDir);
		}

		// Close the directory
		closedir(pDir);
	}

#ifdef DEBUG
	cout << "scanFiles: " << entryName << " status " << foundMatch << ", " << filesToAttach.size() << " matches" << endl;
#endif
	return foundMatch;
}

/// Parse the recipients list column.
static bool parseRecipientsList(const string &recipientsList,
	map<string, Recipient> &recipients, Recipient::RecipientType type,
	const string &timestamp)
{
	char sepChar = ';';
	bool haveRecipients = false;
 
	if ((recipientsList.empty() == true) ||
		(recipientsList.find('@') == string::npos))
	{
		return false;
	}

	// Parse the list of recipients
	// Look for semi-colons first, or commas
	string::size_type startPos = 0, sepPos = recipientsList.find(sepChar);
	if (sepPos == string::npos)
	{
		sepChar = ',';
		sepPos = recipientsList.find(sepChar);
	}
	if (sepPos == string::npos)
	{
		Recipient recipientObj = Recipient::extractNameAndEmailAddress(recipientsList);
		if (recipientObj.m_emailAddress.empty() == false)
		{
			recipientObj.m_type = type;
			recipientObj.m_customFields[TIMESTAMP_CUSTOM_FIELD] = timestamp;
			recipientObj.m_customFields[RECIPIENTS_LIST_CUSTOM_FIELD] = recipientsList;
			recipients.insert(pair<string, Recipient>(recipientObj.m_emailAddress, recipientObj));
			haveRecipients = true;
		}
	}
	else
	{
		while (sepPos != string::npos)
		{
			string recipientDetails(recipientsList.substr(startPos, sepPos - startPos));

			Recipient recipientObj = Recipient::extractNameAndEmailAddress(recipientDetails);
			if (recipientObj.m_emailAddress.empty() == false)
			{
				recipientObj.m_type = type;
				recipientObj.m_customFields[TIMESTAMP_CUSTOM_FIELD] = timestamp;
				recipientObj.m_customFields[RECIPIENTS_LIST_CUSTOM_FIELD] = recipientsList;
				recipients.insert(pair<string, Recipient>(recipientObj.m_emailAddress, recipientObj));
				haveRecipients = true;
			}

			// Next
			startPos = sepPos + 1;
			sepPos = recipientsList.find(sepChar, startPos);
		}

		if (startPos < recipientsList.length())
		{
			Recipient recipientObj = Recipient::extractNameAndEmailAddress(recipientsList.substr(startPos));
			if (recipientObj.m_emailAddress.empty() == false)
			{
				recipientObj.m_type = type;
				recipientObj.m_customFields[TIMESTAMP_CUSTOM_FIELD] = timestamp;
				recipientObj.m_customFields[RECIPIENTS_LIST_CUSTOM_FIELD] = recipientsList;
				recipients.insert(pair<string, Recipient>(recipientObj.m_emailAddress, recipientObj));
				haveRecipients = true;
			}
		}
	}
#ifdef DEBUG
	cout << "parseRecipientsList: inserted " << recipients.size()
		<< " recipients of type " << type << endl;
#endif

	return haveRecipients;
}

/// Run a mailing.
static int runMailing(const string &emailFileName, const string &csvFileName,
	const string &baseDirectory,
	unsigned int campaignColumn, unsigned int attachmentColumn,
	unsigned int timestampColumn, unsigned int toColumn)
{
	ConfigurationFile *pConfig = ConfigurationFile::getInstance("");
	RunUnits runUnits;
	Timer mailingTimer;
	bool haveCampaigns = false;
	int finalSuccess = EXIT_SUCCESS;

	if ((emailFileName.empty() == true) ||
		(csvFileName.empty() == true))
	{
		return EXIT_FAILURE;
	}

	ifstream csvStream(csvFileName.c_str());
	if (csvStream.is_open() == false)
	{
		return EXIT_FAILURE;
	}

	CSVParser parser(csvStream);

	// How many columns do we need to look at ?
	unsigned int maxColumn = max(campaignColumn, attachmentColumn);
	maxColumn = max(maxColumn, timestampColumn);
	maxColumn = max(maxColumn, toColumn + 2);

	// Consume the first line, it's only headers
	parser.nextLine();
	while (parser.nextLine() == true)
	{
		string campaignName, attachmentName, timestamp, toRecipientsList, ccRecipientsList, bccRecipientsList;
		bool haveRecipients = false;

		for (unsigned int columnNum = 0; columnNum <= maxColumn; ++columnNum)
		{
			string column;

			if (parser.nextColumn(column) == false)
			{
				break;
			}

			if (columnNum == campaignColumn)
			{
				campaignName = CSVParser::unescapeColumn(column);
			}
			else if (columnNum == attachmentColumn)
			{
				attachmentName = CSVParser::unescapeColumn(column);
			}
			else if (columnNum == timestampColumn)
			{
				timestamp = CSVParser::unescapeColumn(column);
			}
			else if (columnNum == toColumn)
			{
				toRecipientsList = CSVParser::unescapeColumn(column);
			}
			// Assume the CC column is right after the To column
			else if (columnNum == toColumn + 1)
			{
				ccRecipientsList = CSVParser::unescapeColumn(column);
			}
			// ...and the BCC column is right after the CC column
			else if (columnNum == toColumn + 2)
			{
				bccRecipientsList = CSVParser::unescapeColumn(column);
			}
#ifdef DEBUG
			cout << "runMailing: column " << columnNum << " " << column << endl;
#endif
		}

		if (campaignName.empty() == true)
		{
			cerr << "Incomplete campaign details" << endl;
			continue;
		}

		cout << "Campaign " << campaignName << "/" << attachmentName << endl;
		haveCampaigns = true;

		// For each campaign, we'll need a MessageDetails object (here, a XmlMessageDetails)
		XmlMessageDetails *pXmlMessageDetails = new XmlMessageDetails();
		if (pXmlMessageDetails->parse(emailFileName) == false)
		{
			cerr << "Couldn't load template " << emailFileName << endl;

			delete pXmlMessageDetails;

			// Abort
			finalSuccess = EXIT_FAILURE;
			break;
		}
		// ... and a RecipientsMap object
		RecipientsMap *pRecipientsMap = new RecipientsMap(campaignName, attachmentName);

		// Get the recipients for this campaign
		pXmlMessageDetails->m_to.clear();
		pXmlMessageDetails->m_cc.clear();
		if (parseRecipientsList(toRecipientsList, pRecipientsMap->m_recipients, Recipient::AS_TO, timestamp) == true) 
		{
			if (!((pConfig != NULL) &&
				(pConfig->m_hideRecipients == true)))
			{
				pXmlMessageDetails->m_to = toRecipientsList;
#ifdef DEBUG
				cout << "runMailing: To " << toRecipientsList << endl;
#endif
			}
			haveRecipients = true;
		}
		if (parseRecipientsList(ccRecipientsList, pRecipientsMap->m_recipients, Recipient::AS_CC, timestamp) == true)
		{
			if (!((pConfig != NULL) &&
				(pConfig->m_hideRecipients == true)))
			{
				pXmlMessageDetails->m_cc = ccRecipientsList;
#ifdef DEBUG
				cout << "runMailing: CC " << ccRecipientsList << endl;
#endif
			}
			haveRecipients = true;
		}
		if (parseRecipientsList(bccRecipientsList, pRecipientsMap->m_recipients, Recipient::AS_BCC, timestamp) == true)
		{
			haveRecipients = true;
		}

		if (haveRecipients == false)
		{
			cerr << "No recipients for campaign " << campaignName << "/" << attachmentName << endl;

			delete pRecipientsMap;
			delete pXmlMessageDetails;

			continue;
		}

		if (attachmentName.empty() == false)
		{
			vector<string> filePaths;

			// Find what file(s) we will need to attach
			// FIXME: the format of the pattern should be configurable
			if ((scanFiles(baseDirectory, string("*") + attachmentName + string("-*"), filePaths, true) == false) ||
				(filePaths.empty() == true))
			{
				cerr << "No attachment matching *" << attachmentName << "-* for campaign "
					<< campaignName << "/" << attachmentName << " in " << baseDirectory << endl;

				delete pRecipientsMap;
				delete pXmlMessageDetails;

				continue;
			}
			pXmlMessageDetails->setAttachments(filePaths);
		}

		// Have we reached the maximum number of units ?
		if (runUnits.getCount() >= pConfig->m_maxSlaves)
		{
			// Wait for all units to return
			runUnits.join();
		}

		// Use the attachment as pseudo campaign Id
		UnitArg *pUnitArg = new UnitArg(attachmentName, pXmlMessageDetails, pRecipientsMap);
		if (runUnits.create(workerUnitFunc, pUnitArg) == true)
		{
			cout << "Processing campaign " << campaignName << "/" << attachmentName << endl;
		}
		else if (runUnits.isChild() == true)
		{
			delete pUnitArg;
			delete pRecipientsMap;
			delete pXmlMessageDetails;

			return finalSuccess;
		}
		else
		{
			cerr << "Couldn't process campaign " << campaignName << "/" << attachmentName << endl;

			delete pUnitArg;
			delete pRecipientsMap;
			delete pXmlMessageDetails;

			// Abort
			finalSuccess = EXIT_FAILURE;
			break;
		}
	}

	if (haveCampaigns == true)
	{
		cout << "Waiting for campaigns to finish" << endl;
	}
	else
	{
		cout << "No campaigns were found" << endl;
	}

	// Wait for all units to return
	runUnits.join();

	cout << "Processed mailing in " << mailingTimer.stop() / 1000 << " seconds" << endl;

	return finalSuccess;
}

int main(int argc, char **argv)
{
	struct sigaction quitAction;
	string baseDirectory, csvFileName, emailFileName;
	string configFileName("/etc/givemail/conf.d/givemail.conf"), logFileName;
	streambuf *coutBuff = NULL;
	streambuf *clogBuff = NULL;
	streambuf *cerrBuff = NULL;
	unsigned int campaignColumn = 0, attachmentColumn = 1, timestampColumn = 2, toColumn = 3;
	int longOptionIndex = 0;
	int priority = 15, returnCode = EXIT_SUCCESS;

#ifdef HAVE_GETOPT_H
	// Look at the options
	int optionChar = getopt_long(argc, argv, "a:b:c:f:hl:s:t:vx:y:", g_longOptions, &longOptionIndex);
	while (optionChar != -1)
	{
		switch (optionChar)
		{
			case 'a':
				if (optarg != NULL)
				{
					campaignColumn = (unsigned int)atoi(optarg);
				}
				break;
			case 'b':
				if (optarg != NULL)
				{
					baseDirectory = optarg;
				}
				break;
			case 'c':
				if (optarg != NULL)
				{
					configFileName = optarg;
				}
				break;
			case 'f':
				if (optarg != NULL)
				{
					attachmentColumn = (unsigned int)atoi(optarg);
				}
				break;
			case 'h':
				printHelp();
				return EXIT_SUCCESS;
			case 'l':
				if (optarg != NULL)
				{
					logFileName = optarg;
				}
				break;
			case 's':
				if (optarg != NULL)
				{
					timestampColumn = (unsigned int)atoi(optarg);
				}
				break;
			case 't':
				if (optarg != NULL)
				{
					toColumn = (unsigned int)atoi(optarg);
				}
				break;
			case 'v':
				cout << "csv2givemail - " << PACKAGE_STRING << "\n\n"
					<< "This is free software.  You may redistribute copies of it under the terms of\n"
					<< "the GNU Lesser General Public License <http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html>.\n"
					<< "There is NO WARRANTY, to the extent permitted by law." << endl;
				return EXIT_SUCCESS;
			case 'x':
				if (optarg != NULL)
				{
					emailFileName = optarg;
				}
				break;
			case 'y':
				if (optarg != NULL)
				{
					int newPriority = atoi(optarg);
					if ((newPriority >= -20) &&
						(newPriority < 20))
					{
						priority = newPriority;
					}
				}
				break;
			default:
				return EXIT_FAILURE;
		}

		// Next option
		optionChar = getopt_long(argc, argv, "a:b:c:f:hl:s:t:vx:y:", g_longOptions, &longOptionIndex);
	}

	if (argc - optind != 1)
	{
		printHelp();
		return EXIT_FAILURE;
	}
	csvFileName = argv[optind];
#else
	if (argc < 10)
	{
		printHelp();
		return EXIT_FAILURE;
	}
	configFileName = argv[1];
	emailFileName = argv[2];
	logFileName = argv[3];
	csvFileName = argv[4];
	baseDirectory = argv[5];
	campaignColumn = (unsigned int)atoi(argv[6]);
	attachmentColumn = (unsigned int)atoi(argv[7]);
	timestampColumn = (unsigned int)atoi(argv[8]);
	toColumn = (unsigned int)atoi(argv[9]);
#endif

#if defined(ENABLE_NLS)
	bindtextdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);
#endif

	// Catch SIGINT, QUIT, TERM and PIPE
	sigemptyset(&quitAction.sa_mask);
	quitAction.sa_flags = 0;
	quitAction.sa_handler = catchSignals;
	sigaction(SIGINT, &quitAction, NULL);
	sigaction(SIGQUIT, &quitAction, NULL);
	sigaction(SIGTERM, &quitAction, NULL);
	sigaction(SIGPIPE, &quitAction, NULL);
	// Ignore SIGCHLD
	sigaction(SIGCHLD, NULL, NULL);

	// Change the scheduling priority
	if (setpriority(PRIO_PROCESS, 0, priority) == -1)
	{
		cerr << "Couldn't set scheduling priority to " << priority << endl;
	}

	// Load configuration
	ConfigurationFile *pConfig = ConfigurationFile::getInstance(configFileName);
	if ((pConfig == NULL) ||
		(pConfig->parse() == false))
	{
		cerr << "Couldn't open configuration file " << configFileName << endl;
		return EXIT_FAILURE;
	}

	// Initialize the PRNG
	unsigned short mySeed[3];
	mySeed[1] = (unsigned short)time(NULL);
	seed48(mySeed);

	OpenDKIM::initialize();

	// Redirect output ?
	ofstream logFile;
	if ((logFileName.empty() == false) &&
		(logFileName != "stdout"))
	{
		logFile.open(logFileName.c_str(), ios::trunc);
		if (logFile.is_open() == false)
		{
			cerr << "Couldn't open log file " << logFileName << endl;
			return EXIT_FAILURE;
		}

		coutBuff = cout.rdbuf();
		clogBuff = clog.rdbuf();
		cerrBuff = cerr.rdbuf();
		cout.rdbuf(logFile.rdbuf());
		clog.rdbuf(logFile.rdbuf());
		cerr.rdbuf(logFile.rdbuf());
	}

	try
	{
		if (baseDirectory.empty() == true)
		{
			// Get the current working directory
			char *pCurrentDir = get_current_dir_name();
			if (pCurrentDir != NULL)
			{
				baseDirectory = pCurrentDir;

				free(pCurrentDir);
			}
		}

		returnCode = runMailing(emailFileName, csvFileName, baseDirectory,
			campaignColumn, attachmentColumn, timestampColumn, toColumn);
	}
	catch (const exception &e)
	{
		cerr << "Caught exception ! " << e.what() << endl;
		returnCode = EXIT_FAILURE;
	}
	catch (...)
	{
		cerr << "Caught unknown exception !" << endl;
		returnCode = EXIT_FAILURE;
	}

	OpenDKIM::shutdown();

	// Close the log file
	cout.rdbuf(coutBuff);
	clog.rdbuf(clogBuff);
	cerr.rdbuf(cerrBuff);
	logFile.close();

	return returnCode;
}

