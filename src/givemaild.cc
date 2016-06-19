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

#include <getopt.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pthread.h>
#include <libintl.h>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>

#include "config.h"
#include "CampaignSQL.h"
#include "ConfigurationFile.h"
#include "Daemon.h"
#include "MySQLBase.h"
#include "Process.h"
#include "TimeConverter.h"

#define EXIT_ASK_FOR_RESTART 10
#define MAX_CAMPAIGNS 1

using namespace std;

class SlaveInfo
{
	public:
		SlaveInfo()
		{
		}
		SlaveInfo(const string &campaignId,
			const string &slaveId) :
			m_campaignId(campaignId),
			m_slaveId(slaveId)
		{
		}
		SlaveInfo(const SlaveInfo &other) :
			m_campaignId(other.m_campaignId),
			m_slaveId(other.m_slaveId)
		{
		}
		~SlaveInfo()
		{
		}

		SlaveInfo &operator=(const SlaveInfo &other)
		{
			if (this != &other)
			{
				m_campaignId = other.m_campaignId;
				m_slaveId = other.m_slaveId;
			}

			return *this;
		}

		string m_campaignId;
		string m_slaveId;

};

static bool g_mustQuit = false;
static string g_logFileName("givemaild.log");
static bool g_restartSlaves = false;
static map<pid_t, SlaveInfo> g_slaveCampaigns;
static MySQLBase *g_pDb = NULL;

static struct option g_longOptions[] = {
	{"configuration-file", required_argument, NULL, 'c'},
	{"help", no_argument, NULL, 'h'},
	{"log-file", required_argument, NULL, 'l'},
	{"restart-slaves", no_argument, NULL, 'r'},
	{"version", no_argument, NULL, 'v'},
	{0, 0, 0, 0}
};

/// Print an help message.
static void printHelp(void)
{
	// Help
	cout << "givemaild - Monitor the campaigns database\n\n"
		<< "Usage: givemaild\n\n"
		<< "Options:\n"
		<< "  -c, --configuration-file FILENAME load configuration from the given file name\n"
		<< "  -h, --help                        display this help and exit\n"
		<< "  -l, --log-file FILENAME           redirect output to the specified log file\n"
		<< "  -r, --restart-slaves              restart slaves killed by SIGSEGV\n"
		<< "  -v, --version                     output version information and exit" << endl;
}

/// Start a slave program.
static bool startSlave(const SlaveInfo &slaveInfo, 
	const string &configFileName)
{
	string commandLine("givemail --slave ");

	if (slaveInfo.m_campaignId.empty() == true)
	{
		return false;
	}

	commandLine += slaveInfo.m_campaignId;
	commandLine += " --configuration-file ";
	commandLine += configFileName;
	commandLine += " --log-file ";
	if (g_logFileName.empty() == false)
	{
		// Get the slave to log to the same directory
		string::size_type pos = g_logFileName.find_last_of("/");
		if (pos != string::npos)
		{
			commandLine += g_logFileName.substr(0, pos + 1);
		}
	}
	commandLine += "givemail-slave-";
	commandLine += slaveInfo.m_campaignId;
	if (slaveInfo.m_slaveId.empty() == false)
	{
		commandLine += "-";
		commandLine += slaveInfo.m_slaveId;
	}
	commandLine += ".log";
	if (slaveInfo.m_slaveId.empty() == false)
	{
		commandLine += " --id ";
		commandLine += slaveInfo.m_slaveId;
	}

	// Execute the command
	Process process(commandLine, false);
	pid_t childPid = process.launch("");
	if (childPid > 0)
	{
		// Record this slave
		g_slaveCampaigns.insert(pair<pid_t, SlaveInfo>(childPid, slaveInfo));

		cout << "Processing campaign " << slaveInfo.m_campaignId
			<< " (process " << childPid << ")" << endl;
	
		return true;
	}

	return false;
}

/// Start a campaign.
static void startCampaign(Campaign &campaign)
{
	SlaveInfo slaveInfo(campaign.m_id, "");

	ConfigurationFile *pConfig = ConfigurationFile::getInstance("");
	if (pConfig->m_threaded == true)
	{
		// Start one slave
		startSlave(slaveInfo, pConfig->getFileName());
	}
	else for (off_t slaveNum = 0; slaveNum < pConfig->m_maxSlaves; ++slaveNum)
	{
		stringstream idStr;

		idStr << slaveNum;
		slaveInfo.m_slaveId = idStr.str();

		// Start several slave processes
		startSlave(slaveInfo, pConfig->getFileName());
	}
}

/// Catch signals and take the appropriate action.
static void catchSignals(int sigNum)
{
	if (sigNum == SIGCHLD)
	{
		ConfigurationFile *pConfig = ConfigurationFile::getInstance("");
		pid_t childPid = 0;
		int childStatus = 0;
		bool restartSlave = false, signalEndOfCampaign = false;

		while ((childPid = waitpid(-1, &childStatus, WNOHANG)) > 0)
		{
			SlaveInfo slaveInfo;

			if (WIFEXITED(childStatus))
			{
				cout << "Child process " << childPid << " exited with return code "
					<< WEXITSTATUS(childStatus) << endl;

				if ((g_restartSlaves == true) &&
					(WEXITSTATUS(childStatus) == EXIT_ASK_FOR_RESTART))
				{
					restartSlave = true;
				}
			}
			else if (WIFSIGNALED(childStatus))
			{
				cout << "Child process " << childPid << " was killed by signal "
					<< WTERMSIG(childStatus) << endl;

				if ((g_restartSlaves == true) &&
					(WTERMSIG(childStatus) == SIGSEGV))
				{
					restartSlave = true;
				}
			}
			else if (WIFSTOPPED(childStatus))
			{
				cout << "Child process " << childPid << " was stopped by signal "
					<< WSTOPSIG(childStatus) << endl;
			}

			// Deal with childrens' exit
			map<pid_t, SlaveInfo>::iterator slaveIter = g_slaveCampaigns.find(childPid);
			if (slaveIter != g_slaveCampaigns.end())
			{
				slaveInfo = slaveIter->second;

				// Remove from the list
				g_slaveCampaigns.erase(slaveIter);

				// Restart ?
				if (restartSlave == true)
				{
					startSlave(slaveInfo, pConfig->getFileName());
					return;
				}
				else
				{
					signalEndOfCampaign = true;
				}
				// Else, keep going
			}

			if (signalEndOfCampaign == false)
			{
				// The process that quit wasn't a slave
				return;
			}

			// Any other slave left processing this campaign ?
			for (map<pid_t, SlaveInfo>::const_iterator slaveIter = g_slaveCampaigns.begin();
				slaveIter != g_slaveCampaigns.end(); ++slaveIter)
			{
				if (slaveIter->second.m_campaignId == slaveInfo.m_campaignId)
				{
					// Yes, there is
					return;
				}
			}

			if (g_pDb != NULL)
			{
				CampaignSQL campaignData(g_pDb);
				time_t checkTime = time(NULL);
				string checkTimestamp(TimeConverter::toTimestamp(checkTime, false));

				off_t totalCount = campaignData.countRecipients(slaveInfo.m_campaignId,
					"", "", false);
				off_t failedAPICount = campaignData.countRecipients(slaveInfo.m_campaignId,
					"Failed", "0 Invalid API", true);

				if (failedAPICount > (totalCount / 10))
				{
					struct sigaction oldAction, ignAction;

					// Reset recipients that failed because of an API error
					campaignData.resetFailedRecipients(slaveInfo.m_campaignId, "0 Invalid API", true);

					cout << checkTimestamp << ": reprocessing campaign "
						<< slaveInfo.m_campaignId << " (" << failedAPICount
						<< "/" << totalCount << ")" << endl;

					signalEndOfCampaign = false;

					// Ignore SIGCHLD for a while
					sigemptyset(&ignAction.sa_mask);
					ignAction.sa_flags = 0;
					ignAction.sa_handler = SIG_IGN;
					sigaction(SIGCHLD, &ignAction, &oldAction);

					// Reprocess the campaign
					Campaign campaign;
					campaign.m_id = slaveInfo.m_campaignId;
					startCampaign(campaign);

					// Restore the previous signal action
					sigaction(SIGCHLD, &oldAction, NULL);
				}
				else
				{
					cout << checkTimestamp << ": done processing campaign "
						<< slaveInfo.m_campaignId << endl;

					signalEndOfCampaign = true;

					// Update the campaign's status
					Campaign *pCampaign = campaignData.getCampaign(slaveInfo.m_campaignId);
					if (pCampaign != NULL)
					{
						pCampaign->m_status = "Sent";
						if (campaignData.setCampaign(*pCampaign) == false)
						{
							cerr << "Couldn't update campaign " << slaveInfo.m_campaignId << endl;
						}

						delete pCampaign;
					}
				}
			}

			if ((signalEndOfCampaign == true) &&
				(pConfig->m_endOfCampaignCommand.empty() == false))
			{
				string commandStr(pConfig->m_endOfCampaignCommand);

				// We may have to customize parameters
				string::size_type idPos = commandStr.find("{campaignId}");
				if (idPos != string::npos)
				{
					commandStr.replace(idPos, 12, slaveInfo.m_campaignId);
				}

				Process process(commandStr, true);

				pid_t endOfCampaignPid = process.launch("");
				cout << "Launched " << commandStr << " under PID " << endOfCampaignPid << endl;
			}
		}
	}
	else
	{
		cout << "Received signal " << sigNum << ". Quitting..." << endl;
		g_mustQuit = true;
	}
}

/// Run in master mode.
static int runMaster(void)
{
	CampaignSQL campaignData(g_pDb);
	unsigned int iterationsCount = 0;

	// Loop until we have to exit
	while (g_mustQuit == false)
	{
		set<Campaign> campaigns;
		off_t totalCount = 0;
		time_t lookupTime = time(NULL);
		string lookupTimestamp(TimeConverter::toTimestamp(lookupTime, false));
		string newStatus("Sending");
		bool newCampaigns = true;

		if (iterationsCount == 60)
		{
			// Get campaigns with recipients we can try again, between 1 and 3 days old
			campaignData.getCampaignsWithTemporaryFailures(lookupTime - (3600 * 96),
				lookupTime - (3600 * 24), MAX_CAMPAIGNS, 0, totalCount, campaigns);

			// Don't move these campaigns back to Sending
			newStatus = "Resending";
			newCampaigns = false;
			iterationsCount = 0;
		}
		else
		{
			// Get ready campaigns
			campaignData.getCampaigns("Ready", CampaignSQL::STATUS, MAX_CAMPAIGNS, 0, totalCount, campaigns);
		}

		// In practice, campaigns will be few and far between
		for (set<Campaign>::const_iterator campaignIter = campaigns.begin();
			campaignIter != campaigns.end(); ++campaignIter)
		{
			Campaign campaign(*campaignIter);
			pid_t campaignSlave = 0;

			// Double-check we don't already have a slave running for this campaign
			for (map<pid_t, SlaveInfo>::const_iterator slaveIter = g_slaveCampaigns.begin();
				slaveIter != g_slaveCampaigns.end(); ++slaveIter)
			{
				if (slaveIter->second.m_campaignId == campaign.m_id)
				{
					campaignSlave = slaveIter->first;
					break;
				}
			}
			if (campaignSlave != 0)
			{
				cerr << lookupTimestamp << ": campaign " << campaign.m_id
					<< " is already being processed by process " << campaignSlave << endl;
				continue;
			}

			// Update the status
			campaign.m_status = newStatus;
			if (newCampaigns == true)
			{
				cout << lookupTimestamp << ": processing campaign "
					<< campaign.m_name << " (" << campaign.m_id << "), out of "
					<< totalCount << endl;

				// Set the timestamp too
				campaign.m_timestamp = lookupTime;
			}
			else
			{
				cout << lookupTimestamp << ": retrying temporarily failed recipients in campaign "
					<< campaign.m_name << " (" << campaign.m_id << "), out of "
					<< totalCount << endl;

				// Reset recipients that failed with a 4xy
				campaignData.resetFailedRecipients(campaign.m_id, "4", true);
			}
			if (campaignData.setCampaign(campaign) == true)
			{
				startCampaign(campaign);
			}
			else
			{
				cerr << "Couldn't update campaign " << campaign.m_id << endl;
			}
		}

		sleep(60);
		++iterationsCount;
	}

	return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
	struct sigaction quitAction, ignAction;
	string configFileName("/etc/givemail/conf.d/givemail.conf");
	streambuf *coutBuff = NULL;
	streambuf *cerrBuff = NULL;
	int longOptionIndex = 0, returnCode = EXIT_SUCCESS;
	bool redirectOutput = false;

	// Look at the options
	int optionChar = getopt_long(argc, argv, "c:hl:rv", g_longOptions, &longOptionIndex);
	while (optionChar != -1)
	{
		switch (optionChar)
		{
			case 'c':
				if (optarg != NULL)
				{
					configFileName = optarg;
				}
				break;
			case 'h':
				printHelp();
				return EXIT_SUCCESS;
			case 'l':
				if (optarg != NULL)
				{
					g_logFileName = optarg;
				}
				break;
			case 'r':
				g_restartSlaves = true;
				break;
			case 'v':
				cout << "givemaild - " << PACKAGE_STRING << "\n\n"
					<< "This is free software.  You may redistribute copies of it under the terms of\n"
					<< "the GNU Lesser General Public License <http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html>.\n"
					<< "There is NO WARRANTY, to the extent permitted by law." << endl;
				return EXIT_SUCCESS;
			default:
				return EXIT_FAILURE;
		}

		// Next option
		optionChar = getopt_long(argc, argv, "c:hl:rv", g_longOptions, &longOptionIndex);
	}

#if defined(ENABLE_NLS)
	bindtextdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);
#endif

	// Catch SIGINT, QUIT, TERM and CHLD
	sigemptyset(&quitAction.sa_mask);
	quitAction.sa_flags = 0;
	quitAction.sa_handler = catchSignals;
	sigaction(SIGINT, &quitAction, NULL);
	sigaction(SIGQUIT, &quitAction, NULL);
	sigaction(SIGTERM, &quitAction, NULL);
	sigaction(SIGCHLD, &quitAction, NULL);
	// Ignore SIGPIPE
	sigemptyset(&ignAction.sa_mask);
	ignAction.sa_flags = 0;
	ignAction.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &ignAction, NULL);

	// Load configuration
	ConfigurationFile *pConfig = ConfigurationFile::getInstance(configFileName);
	if ((pConfig == NULL) ||
		(pConfig->parse() == false))
	{
		cerr << "Couldn't open configuration file " << configFileName << endl;
		return EXIT_FAILURE;
	}

	// Test whether we can open the log file
	ofstream logFile;
	if (g_logFileName.empty() == false)
	{
		logFile.open(g_logFileName.c_str(), ios::trunc);
		if (logFile.is_open() == false)
		{
			cerr << "Couldn't open log file " << g_logFileName << endl;
			return EXIT_FAILURE;
		}
		redirectOutput = true;

		// Close until we have daemonized
		logFile.close();
	}

	// Run as a daemon
	if (Daemon::daemonize("/") == false)
	{
		// Parent and first child exit here
		return EXIT_SUCCESS;
	}
	// Second, daemonized, child keeps running here

	// Redirect output ?
	if (redirectOutput == true)
	{
		logFile.open(g_logFileName.c_str(), ios::trunc);
		if (logFile.is_open() == true)
		{
			coutBuff = cout.rdbuf();
			cerrBuff = cerr.rdbuf();
			cout.rdbuf(logFile.rdbuf());
			cerr.rdbuf(logFile.rdbuf());
		}
		else
		{
			redirectOutput = false;
		}
	}

	try
	{
		// Open the database
		g_pDb = new MySQLBase(pConfig->m_hostName, pConfig->m_databaseName,
			pConfig->m_userName, pConfig->m_password);
		if (g_pDb->isOpen() == false)
		{
			cerr << "Couldn't open database " << pConfig->m_databaseName << " at " << pConfig->m_hostName << endl;
			returnCode = EXIT_FAILURE;
		}
		else
		{
			returnCode = runMaster();
		}
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

	// Ignore SIGCHLD, close the database
	sigemptyset(&ignAction.sa_mask);
	ignAction.sa_flags = 0;
	ignAction.sa_handler = SIG_IGN;
	sigaction(SIGCHLD, &ignAction, NULL);
	if (g_pDb != NULL)
	{
		delete g_pDb;
		g_pDb = NULL;
	}
	// FIXME: delete the ConfigurationFile instance

	// Close the log file
	if (redirectOutput == true)
	{
		cout.rdbuf(coutBuff);
		cerr.rdbuf(cerrBuff);
		logFile.close();
	}

	return returnCode;
}

