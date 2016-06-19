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
#include <time.h>
#include <libintl.h>
#include <iostream>
#include <fstream>
#include <algorithm>

#include "config.h"
#include "AuthSQL.h"
#include "Base64.h"
#include "ConfigurationFile.h"
#include "HMAC.h"
#include "MessageDetails.h"
#include "MySQLBase.h"
#include "TimeConverter.h"

using namespace std;

static bool g_mustQuit = false;

static struct option g_longOptions[] = {
	{"configuration-file", required_argument, NULL, 'c'},
	{"delete", no_argument, NULL, 'd'},
	{"help", no_argument, NULL, 'h'},
	{"new", required_argument, NULL, 'n'},
	{"sign", required_argument, NULL, 's'},
	{"version", no_argument, NULL, 'v'},
	{0, 0, 0, 0}
};

/// Prints an help message.
static void printHelp(void)
{
	// Help
	cout << "webapi-key-manager - Manage WebAPI keys\n\n"
		<< "Usage: webapi-key-manager [OPTIONS] [ID]\n\n"
		<< "Options:\n"
		<< "  -c, --configuration-file FILENAME load configuration from the given file\n"
		<< "  -d, --delete                      delete the specified key ID\n"
		<< "  -h, --help                        display this help and exit\n"
		<< "  -n, --new DAYS                    generate a new key valid for the given number of days\n"
		<< "  -s, --sign FILENAME               sign the request XML found in the given file with the specified key ID\n"
		<< "  -v, --version                     output version information and exit" << endl;
}

/// Catch signals and take the appropriate action.
static void catchSignals(int sigNum)
{
	if (sigNum != SIGCHLD)
	{
		g_mustQuit = true;
	}
}

/// Get some random bytes.
static char *getRandomBytes(unsigned long bytesCount)
{
	char *pBuffer = NULL;
	off_t bytesRead = 0;

	if (bytesCount == 0)
	{
		return NULL;
	}

	ifstream randomFile("/dev/random");

	if (randomFile.is_open() == true)
	{
		pBuffer = new char[bytesCount + 1];

		while (bytesRead < bytesCount)
		{
			randomFile.read(pBuffer, bytesCount - bytesRead);
			bytesRead += randomFile.gcount();
#ifdef DEBUG
			cout << "Read " << bytesRead << " bytes" << endl;
#endif
		}
		pBuffer[bytesCount] = '\0';

		randomFile.close();
	}

	return pBuffer;
}

int main(int argc, char **argv)
{
	struct sigaction quitAction, ignAction;
	string configFileName("/etc/givemail/conf.d/givemail.conf"), requestFileName, keyId;
	int longOptionIndex = 0, minimumArgsCount = 0;
	int returnCode = EXIT_FAILURE;
	unsigned int daysCount = 30;
	bool generateKey = false, deleteKey = false, signRequest = false;

	// Look at the options
	int optionChar = getopt_long(argc, argv, "c:dhn:s:v", g_longOptions, &longOptionIndex);
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
			case 'd':
				minimumArgsCount = 1;
				deleteKey = true;
				break;
			case 'h':
				printHelp();
				return EXIT_SUCCESS;
			case 'n':
				if (optarg != NULL)
				{
					daysCount = (unsigned int)atoi(optarg);
					generateKey = true;
				}
				break;
			case 's':
				if (optarg != NULL)
				{
					requestFileName = optarg;
					minimumArgsCount = 1;
					signRequest = true;
				}
				break;
			case 'v':
				cout << "webapi-key-manager - " << PACKAGE_STRING << "\n\n"
					<< "This is free software.  You may redistribute copies of it under the terms of\n"
					<< "the GNU Lesser General Public License <http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html>.\n"
					<< "There is NO WARRANTY, to the extent permitted by law." << endl;
				return EXIT_SUCCESS;
			default:
				return EXIT_FAILURE;
		}

		// Next option
		optionChar = getopt_long(argc, argv, "c:dhn:s:v", g_longOptions, &longOptionIndex);
	}

	if ((argc - optind < minimumArgsCount) ||
		((generateKey == false) && (deleteKey == false) && (signRequest == false)))
	{
		printHelp();
		return EXIT_FAILURE;
	}

	if (minimumArgsCount >= 1)
	{
		keyId = argv[optind];
	}

#if defined(ENABLE_NLS)
	bindtextdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);
#endif

	// Catch SIGINT, QUIT and CHLD
	sigemptyset(&quitAction.sa_mask);
	quitAction.sa_flags = 0;
	quitAction.sa_handler = catchSignals;
	sigaction(SIGINT, &quitAction, NULL);
	sigaction(SIGQUIT, &quitAction, NULL);
	sigaction(SIGCHLD, &quitAction, NULL);
	sigemptyset(&ignAction.sa_mask);
	// Ignore SIGPIPE
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

	// Open the database
	MySQLBase db(pConfig->m_hostName, pConfig->m_databaseName,
		pConfig->m_userName, pConfig->m_password);
	if (db.isOpen() == false)
	{
		return EXIT_FAILURE;
	}

	AuthSQL authData(&db);

	if (generateKey == true)
	{
		Key key;
		unsigned long keyLength = 64;

		key.m_creationTime = time(NULL);
		key.m_expiryTime = key.m_creationTime + (daysCount * 24 * 3600);

		char *pRandomKey = getRandomBytes(keyLength);
		if (pRandomKey != NULL)
		{
			char *pEncodedKey = Base64::encode(pRandomKey, keyLength);
			if (pEncodedKey != NULL)
			{
				key.m_appId = db.getUniversalUniqueId();
				key.m_value.append(pEncodedKey, keyLength);

				if (authData.createNewKey(key) == true)
				{
					cout << "Generated a new HMAC key" << endl;
					cout << "Key ID:\t\t" << key.m_appId << endl;
					cout << "Key value:\t" << pEncodedKey << endl;
					cout << "Creation time:\t" << key.m_creationTime << endl;
					cout << "Expiry time:\t" << key.m_expiryTime << endl;
					cout << "Forward this information to customer through an appropriately secure channel" << endl;

					returnCode = EXIT_SUCCESS;
				}
				else
				{
					cerr << "Couldn't generate a new HMAC key" << endl;
				}
				delete[] pEncodedKey;
			}

			delete[] pRandomKey;
		}
	}
	else if (deleteKey == true)
	{
		Key *pKey = authData.getKey(keyId);
		if (pKey == NULL)
		{
			cout << "No such HMAC key ID" << endl;
		}
		else
		{
			if (authData.deleteKey(pKey, true) == true)
			{
				cout << "Deleted HMAC key ID " << keyId << endl;

				returnCode = EXIT_SUCCESS;
			}
			else
			{
				cerr << "Couldn't delete HMAC key" << endl;
			}

			delete pKey;
		}
	}
	else if (signRequest == true)
	{
		time_t timeNow = time(NULL);

		Key *pKey = authData.getKey(keyId);
		if (pKey == NULL)
		{
			cout << "No such HMAC key ID" << endl;
		}
		else
		{
			off_t fileSize = 0;

			double keyDiff = pKey->getTimeDiff(timeNow);
			if (keyDiff < 0)
			{
				// Warn and proceed
				cout << "Warning: key has expired" << endl;
			}

			char *pRequest = MessageDetails::loadRaw(requestFileName, fileSize);
			if (pRequest == NULL)
			{
				cout << "Couldn't open file " << requestFileName << endl;
			}
			else
			{
				string dateHeaderValue(TimeConverter::toTimestamp(timeNow, false));
				string payload(dateHeaderValue);

				payload += "\n";
				payload += pRequest;

				off_t dataLength = (off_t)payload.length();

				cout << "Signing " << dataLength << " bytes" << endl;

				char *pEncodedSignature = HMAC::sign(pKey, payload.c_str(), dataLength);
				if (pEncodedSignature != NULL)
				{
					cout << "Prepend the following headers:" << endl;
					cout << "Date: " << dateHeaderValue << endl;
					cout << "Set-Cookie: GMAuth=" << keyId << ":" << pEncodedSignature
						<< "; expires=" << TimeConverter::toTimestamp(pKey->m_expiryTime, true)
						<< "; path=/" << endl << endl;
					cout << "Post to the WebAPI with:" << endl;
					cout << "curl --data-binary @" << requestFileName
						<< " http://localhost/cgi-bin/webapi.fcgi --cookie \"GMAuth="
						<< keyId << ":" << pEncodedSignature << "\"" << endl;

					delete[] pEncodedSignature;
				}

				delete[] pRequest;
			}

			delete pKey;
		}
	}

	return returnCode;
}

