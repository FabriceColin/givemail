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
#include <libintl.h>
#include <map>
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>

#include "ConfigurationFile.h"
#include "DomainsMap.h"
#include "OpenDKIM.h"
#include "Process.h"
#include "Recipient.h"
#include "Resolver.h"
#include "SMTPSession.h"
#include "Substitute.h"
#include "Threads.h"
#include "Timer.h"
#ifdef USE_MYSQL
#include "CampaignSQL.h"
#include "DBStatusUpdater.h"
#include "MySQLBase.h"
#endif
#include "XmlMessageDetails.h"

//#define _TEST_CHILD_ENV
#define EXIT_ASK_FOR_RESTART 10

using namespace std;

#ifdef USE_MYSQL
static MySQLBase *g_pDb = NULL;
#endif
static bool g_mustQuit = false;
static int g_returnCode = EXIT_SUCCESS;

#ifdef HAVE_GETOPT_H
static struct option g_longOptions[] = {
	{"configuration-file", required_argument, NULL, 'c'},
	{"dump-file", required_argument, NULL, 'd'},
	{"reply", no_argument, NULL, 'e'},
	{"fields-file", required_argument, NULL, 'f'},
	{"help", no_argument, NULL, 'h'},
	{"id", required_argument, NULL, 'i'},
	{"log-file", required_argument, NULL, 'l'},
	{"message-id-prefix", required_argument, NULL, 'm'},
	{"priority", required_argument, NULL, 'y'},
	{"random", required_argument, NULL, 'a'},
	{"resolve", required_argument, NULL, 'r'},
	{"slave", no_argument, NULL, 's'},
	{"status-file", required_argument, NULL, 't'},
	{"spam-check", no_argument, NULL, 'p'},
	{"customfield", required_argument, NULL, 'u'},
	{"version", no_argument, NULL, 'v'},
	{"xml-file", required_argument, NULL, 'x'},
	{0, 0, 0, 0}
};
#endif

/// Prints an help message.
static void printHelp(void)
{
	// Help
	cout << "givemail - Send emails out en masse\n\n"
#ifdef HAVE_GETOPT_H
		<< "Usage: givemail [OPTIONS] DOMAIN|CAMPAIGNID [TO_EMAIL_ADDRESS1 TO_NAME1 ...]\n\n"
		<< "Options:\n"
		<< "  -a, --random NUM                  email as many random user names as specified\n"
		<< "  -c, --configuration-file CONFFILE load configuration from the given file\n"
		<< "  -d, --dump-file DUMPFILE          dump message parts loaded with -x to files, using this base name\n"
		<< "  -e, --reply                       set References and In-Reply-To to MSGID\n"
		<< "  -f, --fields-file                 load substitution fields from the given file\n"
		<< "  -h, --help                        display this help and exit\n"
#ifdef USE_MYSQL
		<< "  -i, --id ID                       set a slave ID\n"
#endif
		<< "  -l, --log-file LOGFILE            redirect output to the specified log file\n"
		<< "  -m, --message-id-prefix MSGID     (Resent-)Message-Id prefix, with CONFFILE's master/msgidsuffix as suffix\n"
#ifdef USE_MYSQL
		<< "  -p, --spam-check                  check the specified campaign's spam status\n"
#endif
		<< "  -r, --resolve A|MX|ALL            resolve A and/or MX records and exit\n"
#ifdef USE_MYSQL
		<< "  -s, --slave                       run in slave mode and serve the specified campaign\n"
#endif
		<< "  -t, --status-file                 save recipients SMTP statuses to given file\n"
		<< "  -u, --customfield XYZ:STRING      set substitution field customfieldXYZ\n"
		<< "  -v, --version                     output version information and exit\n"
		<< "  -x, --xml-file XMLFILE            load email details from the given file\n"
		<< "  -y, --priority                    set the scheduling priority (default 15)" << endl;
#else
		<< "Usage: givemail CONFIGURATION_FILE XML_FILE DOMAIN TO_EMAIL_ADDRESS1 TO_NAME1 ...\n"
		<< "Usage: givemail -r A|MX|ALL DOMAIN" << endl;
#endif
}

/// Catch signals and take the appropriate action.
static void catchSignals(int sigNum)
{
	if ((sigNum != SIGCHLD) &&
		(sigNum != SIGPIPE))
	{
		if (sigNum == SIGUSR2)
		{
			// Get restarted by the daemon
			g_returnCode = EXIT_ASK_FOR_RESTART;
		}

		cout << "Received signal " << sigNum << ". Quitting..." << endl;
		g_mustQuit = true;
	}
	else
	{
		cout << "Ignoring signal " << sigNum << endl;
	}
}

/// Applies the test file's custom fields on all destinations.
static void applyCustomFields(map<string, Recipient> &destinations, XmlMessageDetails &testFile)
{
	for (map<string, Recipient>::iterator destIter = destinations.begin();
		destIter != destinations.end(); ++destIter)
	{
		destIter->second.m_customFields = testFile.m_customFields;
	}
}

/// Run in test mode.
static bool runTest(const string &domainName, XmlMessageDetails &testFile,
	const string &statusFileName, map<string, Recipient> &destinations)
{
	ConfigurationFile *pConfig = ConfigurationFile::getInstance("");
	OpenDKIM domainKeys;
	bool testStatus = true;

	if (domainName.empty() == true)
	{
		cerr << "No DOMAIN specified" << endl;
		return false;
	}

	DomainLimits domainLimits(domainName);

	pConfig->findDomainLimits(domainLimits, true);

	SMTPSession session(domainLimits, pConfig->m_options);

	if ((testFile.getPlainSubstituteObj() == NULL) ||
		(testFile.getHtmlSubstituteObj() == NULL))
	{
		cerr << "Couldn't get substitution objects" << endl;
		return false;
	}

	StatusUpdater updater(statusFileName);

	domainKeys.loadPrivateKey(pConfig);

	if (session.generateMessages(domainKeys, &testFile, destinations, &updater) == false)
	{
		testStatus = false;
	}

	return testStatus;
}

#ifdef USE_MYSQL
/// Run in spam check mode.
static bool runSpamCheck(const string &campaignId)
{
	ConfigurationFile *pConfig = ConfigurationFile::getInstance("");
	DomainLimits domainLimits("example.com");

	pConfig->findDomainLimits(domainLimits, true);

	SMTPSession session(domainLimits, pConfig->m_options);
	map<string, string> fieldValues;
	string dummyName("Alfredo Boca Camazans");
	string dummyEmailAddress("abc@example.com");
	string name;

	// Open the database
	g_pDb = new MySQLBase(pConfig->m_hostName, pConfig->m_databaseName,
		pConfig->m_userName, pConfig->m_password);
	if (g_pDb->isOpen() == false)
	{
		return false;
	}

	CampaignSQL campaignData(g_pDb);
	Campaign *pCampaign = campaignData.getCampaign(campaignId);
	if (pCampaign == NULL)
	{
		return false;
	}
	MessageDetails *pDetails = campaignData.getMessage(pCampaign->m_id);
	if (pDetails == NULL)
	{
		delete pCampaign;
		return false;
	}

	// Fields should be substituted with these values
	fieldValues["Name"] = dummyName;
	fieldValues["emailaddress"] = dummyEmailAddress;

	SMTPProvider *pProvider = SMTPProviderFactory::getProvider();
	if (pProvider == NULL)
	{
		delete pCampaign;
		delete pDetails;
		return false;
	}

	// FIXME: make sure pDetails' properties are all set correctly
	SMTPMessage *pMessage = pProvider->newMessage(fieldValues, pDetails,
		SMTPMessage::NEVER, false, pConfig->m_options.m_msgIdSuffix, pConfig->m_options.m_complaints);

	string messageContent(session.dumpMessage(pMessage, dummyName, dummyEmailAddress));
	if (pConfig->m_spamCheckCommand.empty() == false)
	{
#ifdef _TEST_CHILD_ENV
		Process process("/usr/bin/printenv", true);
#else
		Process process(pConfig->m_spamCheckCommand, true);
#endif

		pid_t spamCheckPid = process.launch(messageContent);
		if (spamCheckPid > 0)
		{
#ifdef DEBUG
			cout << "Launched " << pConfig->m_spamCheckCommand << " under PID " << spamCheckPid << endl;
#endif
			// Make sure the child process is still running before and after it's fed input
			if (process.isRunning() == true)
			{
				string spamVerdict(process.readFromOutput());

#ifdef DEBUG
				cout << "Spam verdict is: " << spamVerdict << endl;
#endif
			}
		}
	}

	if (pMessage != NULL)
	{
		delete pMessage;
	}
	delete pProvider;
	delete pCampaign;
	delete pDetails;

	return true;
}

/// Find recipients for the given domain name and email them.
static void sendToDomain(const string &campaignId,
	const DomainLimits &domainLimits, MessageDetails *pDetails,
	DomainAuth &domainAuth, unsigned int recipientsCount)
{
	ConfigurationFile *pConfig = ConfigurationFile::getInstance("");
	SMTPSession session(domainLimits, pConfig->m_options);
	set<string> fieldNames;
	unsigned int apiFailuresCount = 0;

	if (pDetails == NULL)
	{
		return;
	}

	DBStatusUpdater *pUpdater = new DBStatusUpdater(g_pDb, campaignId);

	while ((g_mustQuit == false) &&
		(g_pDb != NULL))
	{
		Timer batchTimer;
		CampaignSQL campaignData(g_pDb);
		map<string, Recipient> recipients;
		// Make sure we get in one go at least as many as "number of MX servers" * "max msgs per server"
		off_t maxRecipientsCount = (off_t)max((unsigned int)100, domainLimits.m_maxMsgsPerServer * session.getTopMXServersCount());

		// Get a group of waiting recipients for this domain
		if (campaignData.getRecipients(campaignId, "Waiting",
			domainLimits.m_domainName, maxRecipientsCount,
			recipients) == false)
		{
			break;
		}

		// Send to all recipients
		if (session.generateMessages(domainAuth, pDetails, recipients, pUpdater) == false)
		{
			const map<string, int> &status = pUpdater->getStatus();
			string errMsg;
			int errNum = session.getError(errMsg);

			cout << "Sending to " << domainLimits.m_domainName << " failed with error code "
				<< errNum << ": " << errMsg << endl;

			// All recipients failed
			for (map<string, Recipient>::iterator recipIter = recipients.begin();
				recipIter != recipients.end(); ++recipIter)
			{
				int statusCode = 0;

				// This recipient's status may have been set by SMTPSession
				if (status.find(recipIter->first) == status.end())
				{
					// Don't record errNum, it's an errno-type number, not an SMTP error code
					pUpdater->updateRecipientStatus(recipIter->first,
						statusCode, errMsg.c_str());
				}
			}

			if (session.isInternalError(errNum) == true)
			{
#ifdef DEBUG
				cout << "Too many API failures, restarting..." << endl;
#endif
				++apiFailuresCount;
				if (apiFailuresCount >= 3)
				{
					// Enough, something is wrong, get restarted
					g_returnCode = EXIT_ASK_FOR_RESTART;
					g_mustQuit = true;
				}
			}
			else
			{
				apiFailuresCount = 0;
			}
		}
		pUpdater->clear();

		cout << "Grabbed and emailed " << recipients.size() << " recipients in "
			<< batchTimer.stop() / 1000 << " seconds" << endl;
	}

	cout << "Processed " << pUpdater->getRecipientsCount() << " recipients out of " << recipientsCount << endl;

	delete pUpdater;
}

/// Entry point for worker threads.
void *workerThreadFunc(void *pArg)
{
	ConfigurationFile *pConfig = ConfigurationFile::getInstance("");
	DomainsMap *pDomainsMap = DomainsMap::getInstance();
	OpenDKIM domainKeys;

	if ((pArg == NULL) ||
		(pDomainsMap == NULL))
	{
		return NULL;
	}

	ThreadArg *pThreadArg = (ThreadArg *)pArg;
	string domainName;
	unsigned int recipientsCount = 0;

	// Get top domains
	bool gotDomain = pDomainsMap->getTopDomain(domainName, recipientsCount);
	while ((g_mustQuit == false) &&
		(gotDomain == true))
	{
		DomainLimits domainLimits(domainName);

		if (pConfig->findDomainLimits(domainLimits, true) == true)
		{
			domainKeys.loadPrivateKey(pConfig);

			sendToDomain(pThreadArg->m_campaignId, domainLimits,
				pThreadArg->m_pDetails, domainKeys, recipientsCount);
		}
		else
		{
			cout << "Skipping domain " << domainName << endl;

			DBStatusUpdater updater(g_pDb, pThreadArg->m_campaignId);
			updater.updateRecipientsStatus(domainName, 0, "No MX record");
		}

		// Next
		gotDomain = pDomainsMap->getTopDomain(domainName, recipientsCount);
	}

	cout << "No more top domains, exiting" << endl;

	// Delete the argument object
	delete pThreadArg;

	OpenDKIM::cleanupThread();

	return NULL;
}

/// Run in slave mode.
static bool runSlave(const string &campaignId, const string &slaveId)
{
	ConfigurationFile *pConfig = ConfigurationFile::getInstance("");
	DomainsMap *pDomainsMap = DomainsMap::getInstance();
	multimap<off_t, string> &domainsBreakdown = pDomainsMap->getMap();
	bool sendStatus = true;

	if (pDomainsMap == NULL)
	{
		return false;
	}

	// Open the database
	g_pDb = new MySQLBase(pConfig->m_hostName, pConfig->m_databaseName,
		pConfig->m_userName, pConfig->m_password);
	if (g_pDb->isOpen() == false)
	{
		return false;
	}

	CampaignSQL campaignData(g_pDb);
	Campaign *pCampaign = campaignData.getCampaign(campaignId);
	if (pCampaign == NULL)
	{
		return false;
	}
	MessageDetails *pDetails = campaignData.getMessage(pCampaign->m_id);
	if (pDetails == NULL)
	{
		return false;
	}

	// Look for fields in content only if necessary
	if ((pDetails->getPlainSubstituteObj() == NULL) ||
		(pDetails->getHtmlSubstituteObj() == NULL))
	{
		cerr << "Couldn't get substitution objects" << endl;
		return false;
	}
	// FIXME: make sure pDetails' properties are all set correctly

	cout << "Processing campaign " << pCampaign->m_name << "(" << campaignId << ")" << endl;

	off_t rowsCount = 0;
	bool multiThreaded = true;

	// Get a list of domains
	if (slaveId.empty() == true)
	{
		rowsCount = campaignData.listDomains(campaignId, "Waiting", domainsBreakdown);
	}
	else
	{
		off_t offset = (off_t)atoll(slaveId.c_str());

		// Partition the domains map based on the number of slave processes
		rowsCount = campaignData.listDomains(campaignId, "Waiting", domainsBreakdown,
			pConfig->m_maxSlaves, offset);
		multiThreaded = false;
	}

	if (rowsCount == 0)
	{
		cerr << "Couldn't break recipients down by domain" << endl;
		sendStatus = false;
	}

	cout << "Campaign has " << rowsCount << " domains" << endl;

	if (multiThreaded == false)
	{
		ThreadArg *pThreadArg = new ThreadArg(campaignId, pDetails);

		// If we were provided a slave ID, other slave processes will take care of the job
		workerThreadFunc((void*)pThreadArg);
	}
	else
	{
		set<pthread_t> workerThreadIds;
		off_t maxSlaves = min(pConfig->m_maxSlaves, rowsCount);

		// Launch worker threads
		for (off_t threadsCount = 0; threadsCount < maxSlaves; ++threadsCount)
		{
			ThreadArg *pThreadArg = new ThreadArg(campaignId, pDetails);
			pthread_t threadId;

			// Start it up
			if (pthread_create(&threadId, NULL, workerThreadFunc, (void*)pThreadArg) != 0)
			{
				cerr << "Couldn't create thread " << threadsCount << endl;
				break;
			}

			workerThreadIds.insert(threadId);
		}

		// Each thread will pick a domain, is there any left ?
		if (rowsCount > maxSlaves)
		{
			OpenDKIM domainKeys;
			string domainName;
			unsigned int recipientsCount = 0;

			// Process least used domains sequentially here
			bool gotDomain = pDomainsMap->getBottomDomain(domainName, recipientsCount);
			while (gotDomain == true)
			{
				DomainLimits domainLimits(domainName);

				if (pConfig->findDomainLimits(domainLimits, true) == true)
				{
					domainKeys.loadPrivateKey(pConfig);

					sendToDomain(campaignId, domainLimits,
						pDetails, domainKeys, recipientsCount);
				}
				else
				{
					cout << "Skipping domain " << domainName << endl;

					DBStatusUpdater updater(g_pDb, campaignId);
					updater.updateRecipientsStatus(domainName, 0, "No MX record");
				}

				// Next
				gotDomain = pDomainsMap->getBottomDomain(domainName, recipientsCount);
			}

			cout << "No more bottom domains, waiting for threads to exit" << endl;
		}

		// Join all worker threads before exiting
		for (set<pthread_t>::const_iterator idIter = workerThreadIds.begin();
			idIter != workerThreadIds.end(); ++idIter)
		{
			if (pthread_join(*idIter, NULL) != 0)
			{
				cerr << "Failed to join thread " << *idIter << endl;
			}
		}
	}

	delete pCampaign;
	delete pDetails;

	return sendStatus;
}
#endif

int main(int argc, char **argv)
{
	XmlMessageDetails testFile;
	struct sigaction quitAction;
	string campaignId, slaveId, domainName, emailFileName, fieldsFileName, resolveWhat, statusFileName;
	string configFileName("/etc/givemail/conf.d/givemail.conf"), dumpFileBaseName, logFileName;
	streambuf *coutBuff = NULL;
	streambuf *clogBuff = NULL;
	streambuf *cerrBuff = NULL;
	int longOptionIndex = 0, minimumArgsCount = 2;
	int randomCount = -1, priority = 15;
	bool checkForSpam = false, isSlave = false;

#ifdef HAVE_GETOPT_H
	// Look at the options
	int optionChar = getopt_long(argc, argv, "a:c:d:ef:hi:l:m:pr:st:u:vx:y:", g_longOptions, &longOptionIndex);
	while (optionChar != -1)
	{
		switch (optionChar)
		{
			case 'a':
				if (optarg != NULL)
				{
					randomCount = atoi(optarg);
					minimumArgsCount = 1;
				}
				break;
			case 'c':
				if (optarg != NULL)
				{
					configFileName = optarg;
				}
				break;
			case 'd':
				if (optarg != NULL)
				{
					dumpFileBaseName = optarg;
				}
				break;
			case 'e':
				testFile.m_isReply = true;
				break;
			case 'f':
				if (optarg != NULL)
				{
					fieldsFileName = optarg;
				}
				break;
			case 'h':
				printHelp();
				return EXIT_SUCCESS;
			case 'i':
				if (optarg != NULL)
				{
					slaveId = optarg;
				}
				break;
			case 'l':
				if (optarg != NULL)
				{
					logFileName = optarg;
				}
				break;
			case 'm':
				if (optarg != NULL)
				{
					testFile.m_msgId = optarg;
				}
				break;
			case 'p':
				checkForSpam = true;
				minimumArgsCount = 1;
				break;
			case 'r':
				minimumArgsCount = 1;
				if (optarg != NULL)
				{
					resolveWhat = optarg;
				}
				break;
			case 's':
				isSlave = true;
				minimumArgsCount = 1;
				break;
			case 't':
				if (optarg != NULL)
				{
					statusFileName = optarg;
				}
				break;
			case 'u':
				if (optarg != NULL)
				{
					string customField(optarg);
					string::size_type colonPos = customField.find(':');

					if ((colonPos != string::npos) &&
						(colonPos + 1 < customField.length()))
					{
						stringstream nameStr;

						nameStr << "customfield" << (unsigned int)atoi(customField.substr(0, colonPos).c_str());

						testFile.m_customFields.insert(pair<string, string>(nameStr.str(), customField.substr(colonPos + 1)));
					}
				}
				break;
			case 'v':
				cout << "givemail - " << PACKAGE_STRING << "\n\n"
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
		optionChar = getopt_long(argc, argv, "a:c:d:ef:hi:l:m:pr:st:u:vx:y:", g_longOptions, &longOptionIndex);
	}

	if (argc - optind < minimumArgsCount)
	{
		printHelp();
		return EXIT_FAILURE;
	}
#else
	int optind = 3;

	minimumArgsCount = 2;
	if (argc < minimumArgsCount)
	{
		printHelp();
		return EXIT_FAILURE;
	}
	configFileName = argv[1];
	if (configFileName == "-r")
	{
		// Resolve and exit mode
		configFileName.clear();
		resolveWhat = argv[2];
	}
	else
	{
		emailFileName = argv[2];
	}
#endif

	// What mode is this ?
	if ((isSlave == true) ||
		(checkForSpam == true))
	{
		campaignId = argv[optind];
#ifdef DEBUG
		cout << "Campaign " << campaignId << endl;
#endif
	}
	else
	{
		domainName = argv[optind];
#ifdef DEBUG
		cout << "Domain name " << domainName << endl;
#endif
	}

#if defined(ENABLE_NLS)
	bindtextdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);
#endif

	// Catch SIGINT, QUIT, TERM, CHLD and PIPE
	sigemptyset(&quitAction.sa_mask);
	quitAction.sa_flags = 0;
	quitAction.sa_handler = catchSignals;
	sigaction(SIGINT, &quitAction, NULL);
	sigaction(SIGQUIT, &quitAction, NULL);
	sigaction(SIGTERM, &quitAction, NULL);
	sigaction(SIGCHLD, &quitAction, NULL);
	sigaction(SIGPIPE, &quitAction, NULL);
	sigaction(SIGUSR2, &quitAction, NULL);

	// Change the scheduling priority
	if (setpriority(PRIO_PROCESS, 0, priority) == -1)
	{
		cerr << "Couldn't set scheduling priority to " << priority << endl;
	}

	// Resolve and exit ?
	if ((domainName.empty() == false) &&
		(resolveWhat.empty() == false))
	{
		set<ResourceRecord> mxResourceRecords, aResourceRecords;

		mxResourceRecords.clear();

		if ((strncasecmp(resolveWhat.c_str(), "MX", 2) == 0) ||
			(strncasecmp(resolveWhat.c_str(), "ALL", 3) == 0))
		{
			Resolver::queryMXRecords(domainName, mxResourceRecords);
		}
		else
		{
			// Assume it's set to A
			Resolver::queryARecords(domainName, aResourceRecords);
		}

		for (set<ResourceRecord>::const_iterator serverIter = mxResourceRecords.begin();
			serverIter != mxResourceRecords.end(); ++serverIter)
		{
			ResourceRecord server(*serverIter);

			cout << "MX record " << server.m_hostName
				<< ", priority " << server.m_priority
				<< ", TTL " << server.m_ttl << endl;

			if (strncasecmp(resolveWhat.c_str(), "ALL", 3) == 0)
			{
				Resolver::queryARecords(server.m_hostName, aResourceRecords);

				cout << "Now " << aResourceRecords.size() << " A records" << endl;
			}
		}

		for (set<ResourceRecord>::const_iterator addressIter = aResourceRecords.begin();
			addressIter != aResourceRecords.end(); ++addressIter)
		{
			cout << "A record #" << addressIter->m_priority
				<< " " << addressIter->m_hostName
				<< ", TTL " << addressIter->m_ttl << endl;
		}

		return EXIT_SUCCESS;
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

	map<string, Recipient> destinations;

	// Get recipients
	if (randomCount > 0)
	{
		for (int randomIndex = 0; randomIndex < randomCount; ++randomIndex)
		{
			stringstream randomNameStr;
			long int lRandom = lrand48();

			randomNameStr << "random" << lRandom;

			string randomName(randomNameStr.str());

			cout << "Random recipient " << randomIndex << ": " << randomName << endl;

			string emailAddress(randomName + string("@") + domainName);

			// Use this random name as ID too
			Recipient recipient(randomName,
				randomName, "", emailAddress, "");
			destinations[emailAddress] = recipient;
		}
	}
	else if ((isSlave == false) &&
		(checkForSpam == false))
	{
		if ((argc - optind - 1) % 2)
		{
			cerr << "An even number of email addresses and names is required, not " << (argc - optind - 1) << endl;
			return EXIT_FAILURE;
		}

		for (int recipIndex = optind + 1; recipIndex < argc; recipIndex = recipIndex + 2)
		{
			string emailAddress(argv[recipIndex]);
			string name(argv[recipIndex]);

#ifdef DEBUG
			cout << "Email address " << emailAddress << " for " << name << endl;
#endif
			if (recipIndex + 1 < argc)
			{
				name = argv[recipIndex + 1];
			}

			// Use the email address as ID too
			Recipient recipient(emailAddress,
				name, "", emailAddress, "");
			destinations[emailAddress] = recipient;

			if (testFile.m_to.empty() == false)
			{
				testFile.m_to += ", ";
			}
			testFile.m_to += SMTPSession::appendValueAndPath(name, emailAddress);

			// Assume all destinations belong to the same domain
			if (domainName.empty() == true)
			{
				string::size_type pos = emailAddress.find('@');

				if ((pos != string::npos) &&
					(emailAddress.length() > pos + 1))
				{
					domainName = emailAddress.substr(pos + 1);
				}
			}
		}
	}

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
		if (isSlave == true)
		{
#ifdef USE_MYSQL
			// Slave mode
			if ((runSlave(campaignId, slaveId) == false) &&
				(g_returnCode == EXIT_SUCCESS))
			{
				g_returnCode = EXIT_FAILURE;
			}
#endif
		}
		else if (checkForSpam == true)
		{
#ifdef USE_MYSQL
			// Spam check mode
			if ((runSpamCheck(campaignId) == false) &&
				(g_returnCode == EXIT_SUCCESS))
			{
				g_returnCode = EXIT_FAILURE;
			}
#endif
		}
		else
		{
			// Test mode
			if (fieldsFileName.empty() == false)
			{
				// Load substitution fields from file ?
				if (fieldsFileName == "stdin")
				{
					stringstream fieldsStream;
					string fieldsInput;

					while (getline(cin, fieldsInput))
					{
						fieldsStream << fieldsInput << endl;
					}

					fieldsInput = fieldsStream.str();
					testFile.parse(fieldsInput.c_str(), fieldsInput.length());
				}
				else
				{
					testFile.parse(fieldsFileName);
				}
			}
			pConfig->m_options.m_dumpFileBaseName = dumpFileBaseName;
			if (emailFileName.empty() == false)
			{
				if (testFile.parse(emailFileName) == false)
				{
					cerr << "Couldn't load template " << emailFileName << endl;
					g_returnCode = EXIT_FAILURE;
				}
				else
				{
					applyCustomFields(destinations, testFile);

					if ((runTest(domainName, testFile, statusFileName, destinations) == false) &&
						(g_returnCode == EXIT_SUCCESS))
					{
						g_returnCode = EXIT_FAILURE;
					}
				}
			}
			else cerr << "No template specified, use the -x/--xml-file option" << endl;
		}
	}
	catch (const exception &e)
	{
		cerr << "Caught exception ! " << e.what() << endl;
		if (g_returnCode == EXIT_SUCCESS)
		{
			g_returnCode = EXIT_FAILURE;
		}
	}
	catch (...)
	{
		cerr << "Caught unknown exception !" << endl;
		if (g_returnCode == EXIT_SUCCESS)
		{
			g_returnCode = EXIT_FAILURE;
		}
	}

	OpenDKIM::shutdown();

	// FIXME: delete g_pDb, as well as DomainsMap and ConfigurationFile instances

	// Close the log file
	cout.rdbuf(coutBuff);
	clog.rdbuf(clogBuff);
	cerr.rdbuf(cerrBuff);
	logFile.close();

	return g_returnCode;
}

