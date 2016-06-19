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

#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <pthread.h>
#include <fcgio.h>
#include <libintl.h>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

#include "config.h"
#include "AuthSQL.h"
#include "CampaignSQL.h"
#include "ConfigurationFile.h"
#include "DBUsageLogger.h"
#include "HMAC.h"
#include "MySQLBase.h"
#include "TimeConverter.h"
#include "URLEncoding.h"
#include "WebAPI.h"

#define AUTH_ERROR	10
#define POST_ERROR	30
#define GET_ERROR	50

using namespace std;

static bool g_mustQuit = false;
static ofstream g_outputFile;
static streambuf *g_clogBuff = NULL;

/// Catch signals and take the appropriate action.
static void catchSignals(int sigNum)
{
	if (sigNum != SIGCHLD)
	{
		g_mustQuit = true;
	}
}

/// Close everything at exit.
static void closeAll(void)
{
	if (g_clogBuff != NULL)
	{
		// Restore log output
		clog.rdbuf(g_clogBuff);
		g_outputFile.close();
	}
}

static char *getContent(unsigned int contentLength)
{
	char *pContent = NULL;

	if (contentLength > 0)
	{
		// Read the whole input
		contentLength = min(contentLength, (unsigned int)1000000);
		if (contentLength > 0)
		{
			pContent = new char[contentLength + 1];
			cin.read(pContent, contentLength);
			contentLength = cin.gcount();
			pContent[contentLength] = '\0';
		}
	}

	// Chew up any remaining stdin - this shouldn't be necessary
	// but is because mod_fastcgi doesn't handle it correctly.
	// ignore() doesn't set the eof bit in some versions of glibc++
	// so use gcount() instead of eof()...
	do
	{
		cin.ignore(1024);
	} while (cin.gcount() == 1024);

	return pContent;
}

static string getQueryParameter(const string &queryString, const string &parameter)
{
	string value;
	string::size_type pos = queryString.find(parameter.c_str());

	if (pos != string::npos)
	{
		string::size_type endPos = queryString.find("&", pos);
		if (endPos == string::npos)
		{
			value = queryString.substr(pos + parameter.length());
		}
		else
		{
			value = queryString.substr(pos + parameter.length(),
				endPos - (pos + parameter.length()));
		}
	}

	return value;
}

static string getHeaderValue(const char *pContent, const char *pHeadersEnd,
	const string &headerName)
{
	if ((pContent == NULL) ||
		(headerName.empty() == true))
	{
		return "";
	}

	const char *pHeaderStart = strstr(pContent, headerName.c_str());
	if ((pHeaderStart != NULL) &&
		(pHeaderStart < pHeadersEnd))
	{
		const char *pHeaderEnd = strstr(pHeaderStart, "\r\n");

		if (pHeaderEnd == NULL)
		{
			pHeaderEnd = strstr(pHeaderStart, "\n");
		}

		if ((pHeaderEnd != NULL) &&
			(pHeaderEnd <= pHeadersEnd))
		{
			return string(pHeaderStart + headerName.length(),
				pHeaderEnd - pHeaderStart - headerName.length());
		}
	}

	return "";
}

static string extractAuthCookie(const string &cookies)
{
	string cookieValue;

	string::size_type startPos = cookies.find("GMAuth=");
	if (startPos != string::npos)
	{
		startPos += 7;

		string::size_type endPos = cookies.find(";", startPos);
		if (endPos != string::npos)
		{
			cookieValue = cookies.substr(startPos, endPos - startPos);
		}
		else
		{
			cookieValue = cookies.substr(startPos);
		}
	}

	return cookieValue;
}

static void parseAuth(const string &authValue, string &appKeyId, string &signature)
{
	// Expected format is "appKeyId:signature"
	string::size_type colonPos = authValue.find(':');
	if (colonPos == string::npos)
	{
		return;
	}

	appKeyId = authValue.substr(0, colonPos);
	signature = authValue.substr(colonPos + 1);
}

static void outputError(const string &errorMsg, bool fullDoc = false)
{
	if (fullDoc == true)
	{
		cout << "Content-Type: text/xml\r\n\r\n";
		cout << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n<GiveMail>\r\n";
	}
	cout << "<Error>" << errorMsg << "</Error>\r\n";
	if (fullDoc == true)
	{
		cout << "</GiveMail>\r\n";
	}
	cout.flush();
}

static bool verifyAuthentication(MySQLBase *pDb, DBUsageLogger &usageLogger,
	const string &dateHeaderValue, const string &authValue,
	time_t timeNow, char *pXml, unsigned int xmlLength)
{
	AuthSQL authData(pDb);
	time_t requestTime = TimeConverter::fromTimestamp(dateHeaderValue);
	bool goodAuth = false;

	if (dateHeaderValue.empty() == true)
	{
		outputError("No date");
		usageLogger.logRequest("", AUTH_ERROR + 1);
		return false;
	}
	else if (authValue.empty() == true)
	{
		outputError("No authentication");
		usageLogger.logRequest("", AUTH_ERROR + 2);
		return false;
	}

#ifdef DEBUG
	cout << "<RequestDate>" << dateHeaderValue << "</RequestDate>\r\n";
	cout << "<RequestTime>" << requestTime << "</RequestTime>\r\n";
	cout << "<CurrentTime>" << timeNow << "</CurrentTime>\r\n";
	string xmlOnly(pXml, xmlLength);
	cout << "<XmlInput>" << WebAPI::encodeEntities(xmlOnly) << "</XmlInput>\r\n";
	cout << "<XmlInputLength>" << xmlLength << "</XmlInputLength>\r\n";
#endif

	// Protect against replay attacks and check the request date is +/- 15 minutes the local time
	double timeDiff = difftime(max(requestTime, timeNow), min(requestTime, timeNow));
	if (timeDiff > 15 * 60)
	{
		outputError("Request date too skewed");
		usageLogger.logRequest("", AUTH_ERROR + 3);
	}
	else
	{
		string appKeyId, signature;

		parseAuth(authValue, appKeyId, signature);
#ifdef DEBUG
		cout << "<KeyId>" << appKeyId << "</KeyId>\r\n";
		cout << "<UserSignature>" << signature << "</UserSignature>\r\n";
#endif

		if (appKeyId.empty() == true)
		{
			outputError("No key ID");
			usageLogger.logRequest("", AUTH_ERROR + 4);
		}
		else if (signature.empty() == true)
		{
			outputError("No signature");
			usageLogger.logRequest("", AUTH_ERROR + 5);
		}
		else
		{
			Key *pKey = authData.getKey(appKeyId);
			if (pKey == NULL)
			{
				outputError("No such key ID");
				usageLogger.logRequest("", AUTH_ERROR + 6);
			}
			else
			{
				// Tell the UsageLogger what key this is
				usageLogger.setKeyId(pKey->m_dbId);
				// ...and what hash identifies the request
				usageLogger.setHash(signature);

				// Has the key expired ?
				double keyDiff = pKey->getTimeDiff(timeNow);
				if (keyDiff < 0)
				{
					outputError("Key has expired");
					usageLogger.logRequest("", AUTH_ERROR + 7);

					delete pKey;
					pKey = NULL;
				}
				else
				{
					cout << "<KeyExpiry>" << (time_t)keyDiff << "</KeyExpiry>\r\n";

					// Was the same request issued recently ?
					time_t lastRequestTime = usageLogger.findLastRequestTime();
					if (lastRequestTime > 0)
					{
#ifdef DEBUG
						cout << "<LastRequestTime>" << lastRequestTime << "</LastRequestTime>\r\n";
#endif

						timeDiff = difftime(timeNow, lastRequestTime);
						if (timeDiff < 1 * 60)
						{
							outputError("Request reissued too soon");
							// Better not to log this to the database to prevent DoS attacks
							// usageLogger.logRequest("", AUTH_ERROR + 9);

							delete pKey;
							pKey = NULL;
						}
					}
				}
			}

			if (pKey != NULL)
			{
				string payload(dateHeaderValue);

				payload += "\n"; 
				payload += pXml;

				off_t dataLength = (off_t)payload.length();

				// Sign the XML
				char *pEncodedSignature = HMAC::sign(pKey, payload.c_str(), dataLength);
				if ((pEncodedSignature != NULL) &&
					(dataLength > 0))
				{
					string actualSignature(pEncodedSignature, dataLength);

#ifdef DEBUG
					cout << "<ActualSignature>" << actualSignature << "</ActualSignature>\r\n";
#endif

					// Verify the signature
					if (actualSignature != signature)
					{
						outputError("HMAC verification failed");
						usageLogger.logRequest("", AUTH_ERROR + 9);
					}
					else
					{
						goodAuth = true;
					}

					delete[] pEncodedSignature;
				}

				delete pKey;
			}
		}
	}

	return goodAuth;
}

static bool processXml(MySQLBase *pDb, DBUsageLogger &usageLogger,
	bool checkAuth, const string &authValue,
	unsigned int contentLength, const string &contentType)
{
	char *pContent = getContent(contentLength);
	if (pContent == NULL)
	{
		outputError("Empty request", true);
		return false;
	}

	string unescapedContent;
	bool deleteContent = true;

	if (strncasecmp(contentType.c_str(),
		"application/x-www-form-urlencoded", 33) == 0)
	{
		unescapedContent = URLEncoding::decode(pContent, contentLength);

		delete[] pContent;
		deleteContent = false;

		pContent = const_cast<char*>(unescapedContent.c_str());
		contentLength = unescapedContent.length();
	}
	else if (strncasecmp(contentType.c_str(),
		"text/xml", 8) != 0)
	{
		delete[] pContent;

		outputError("Unsupported request format", true);
		return false;
	}

	CampaignSQL campaignData(pDb);
	WebAPI api(&campaignData, &usageLogger, cout);
	string dateHeaderValue;
	char *pXml = NULL;
	unsigned int xmlLength = 0;
	time_t timeNow = time(NULL);
	bool parsedOk = false;

	cout << "Content-Type: text/xml\r\n\r\n";
	cout << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n";
	cout << "<GiveMail>\r\n";

	// Skip headers and ignore trailing data, eg boundary
	const char *pGiveMailBlockStart = NULL;
	if (pContent != NULL)
	{
		pGiveMailBlockStart = strstr(pContent, "<GiveMail>");
	}
	if (pGiveMailBlockStart == NULL)
	{
		outputError("No GiveMail block");
		usageLogger.logRequest("", POST_ERROR + 1);
	}
	else
	{
		const char *pHeadersEnd = strstr(pContent, "\r\n\r\n");
		unsigned int headersEndLength = 4;

		// Any headers ?
		if (pHeadersEnd == NULL)
		{
			pHeadersEnd = strstr(pContent, "\n\n");
			headersEndLength = 2;
		}

		if ((pHeadersEnd != NULL) &&
			(pHeadersEnd < pGiveMailBlockStart))
		{
			unsigned int shavedAtStart = (pHeadersEnd - pContent) + headersEndLength;

			// Look for the Date header
			dateHeaderValue = getHeaderValue(pContent, pHeadersEnd, "Date: ");

			const char *pGiveMailBlockEnd = strstr(pContent, "</GiveMail>");
			if (pGiveMailBlockEnd != NULL)
			{
				unsigned int shavedAtEnd = (pContent + contentLength) - (pGiveMailBlockEnd + 11);

				pXml = const_cast<char*>(pHeadersEnd) + headersEndLength;
				xmlLength = (unsigned int)(contentLength - shavedAtStart - shavedAtEnd);
			}
			else
			{
				pXml = NULL;
				xmlLength = 0;
			}
		}
		else
		{
			pXml = pContent;
			xmlLength = contentLength;
		}
	}

	if ((pXml == NULL) ||
		(xmlLength == 0))
	{
		outputError("Empty or malformed request");
		usageLogger.logRequest("", POST_ERROR + 2);
	}
	else if ((checkAuth == false) ||
		(verifyAuthentication(pDb, usageLogger,
			dateHeaderValue, authValue,
			timeNow, pXml, xmlLength) == true))
	{
		parsedOk = api.parse(pXml, xmlLength);
	}

	if (deleteContent == true)
	{
		delete[] pContent;
	}
	cout << "</GiveMail>\r\n";
	cout.flush();

	return parsedOk;
}

static bool processFormData(MySQLBase *pDb, DBUsageLogger &usageLogger,
	const string &queryString, unsigned int contentLength,
	const string &contentType)
{
	ConfigurationFile *pConfig = ConfigurationFile::getInstance("");
	string actionParam(getQueryParameter(queryString, "action="));
	string whatParam(getQueryParameter(queryString, "what="));
	bool uploadSuccess = false;

	char *pContent = getContent(contentLength);
	if ((pContent == NULL) ||
		(pConfig == NULL) ||
		(actionParam != "ImportRecipients") ||
		(whatParam.empty() == true))
	{
		if (pContent != NULL)
		{
			delete[] pContent;
		}
		cout << "Content-Type: text/html\r\n\r\n";
		if (pConfig != NULL)
		{
			cout << pConfig->m_uploadFailureSnippet;
		}
		else
		{
			cout << "<b>Upload failed</b>";
		}

		return false;
	}

	string::size_type boundaryPos = contentType.find("boundary=");
	if ((boundaryPos != string::npos) &&
		(contentType.length() > boundaryPos + 9))
	{
		CampaignSQL campaignData(pDb);
		Campaign campaign;
		WebAPI api(&campaignData, &usageLogger, cout);
		map<unsigned int, WebAPI::CSVColumn> columns;
		string boundary("--");
		boundary += contentType.substr(boundaryPos + 9);

		// Assume "what" is a campaign ID 
		campaign.m_id = whatParam;

		string unescapedContent(pContent, contentLength);
#ifdef DEBUG
		clog << "processFormData: boundary " << boundary << endl;
		clog << "processFormData: posted " << unescapedContent << endl;
#endif
		boundaryPos = unescapedContent.find(boundary + string("\r\n"));
		while (boundaryPos != string::npos)
		{
			bool lastPart = false;

			string::size_type nextBoundaryPos = unescapedContent.find(string("\r\n") + boundary + string("\r\n"), boundaryPos);
			if (nextBoundaryPos == string::npos)
			{
				nextBoundaryPos = unescapedContent.find(string("\r\n") + boundary + string("--"), boundaryPos);
				lastPart = true;
			}

			if (nextBoundaryPos == string::npos)
			{
				break;
			}

			string contentPart(unescapedContent.substr(boundaryPos + boundary.length(),
				nextBoundaryPos - (boundaryPos + boundary.length())));

			// Any headers ?
			string::size_type headersEnd = contentPart.find("\r\n\r\n");
			unsigned int headersEndLength = 4;

			if (headersEnd == string::npos)
			{
				headersEnd = contentPart.find("\n\n");
				headersEndLength = 2;
			}
			if (headersEnd != string::npos)
			{
				// Look for headers
				string fieldName(getHeaderValue(contentPart.c_str(),
                                        contentPart.c_str() + headersEnd, "Content-Disposition: form-data; name=\""));
				if (fieldName.empty() == false)
				{
					// Is this a column specifier ?
					// See if it says where name, email address or status are found
					string::size_type quotePos = fieldName.find('"');
					if (quotePos != string::npos)
					{
						fieldName.erase(quotePos);
						if (strncasecmp(fieldName.c_str(), "column_", 7) == 0)
						{
							string columnName(contentPart.substr(headersEnd + headersEndLength));
							unsigned int index = (unsigned int)atoi(fieldName.c_str() + 7);

#ifdef DEBUG
							clog << "processFormData: " << columnName << " in column " << index << endl;
#endif
							// A field may span several columns
							if (columnName == "Name")
							{
								columns[index] = WebAPI::CSV_NAME;
							}
							else if (columnName == "EmailAddress")
							{
								columns[index] = WebAPI::CSV_EMAIL_ADDRESS;
							}
							else if (columnName == "Status")
							{
								columns[index] = WebAPI::CSV_STATUS;
							}
							else if (columnName == "ReturnPath")
							{
								columns[index] = WebAPI::CSV_RETURN_PATH;
							}
							else if (columnName == "TimeSent")
							{
								columns[index] = WebAPI::CSV_TIME_SENT;
							}
							else if (columnName == "NumAttempts")
							{
								columns[index] = WebAPI::CSV_NUM_ATTEMPTS;
							}
							else if (columnName == "Custom1")
							{
								columns[index] = WebAPI::CSV_CUSTOM_FIELD1;
							}
							else if (columnName == "Custom2")
							{
								columns[index] = WebAPI::CSV_CUSTOM_FIELD2;
							}
							else if (columnName == "Custom3")
							{
								columns[index] = WebAPI::CSV_CUSTOM_FIELD3;
							}
							else if (columnName == "Custom4")
							{
								columns[index] = WebAPI::CSV_CUSTOM_FIELD4;
							}
							else if (columnName == "Custom5")
							{
								columns[index] = WebAPI::CSV_CUSTOM_FIELD5;
							}
							else if (columnName == "Custom6")
							{
								columns[index] = WebAPI::CSV_CUSTOM_FIELD6;
							}
						}
					}
				}

				string contentTypeHeaderValue(getHeaderValue(contentPart.c_str(),
					contentPart.c_str() + headersEnd, "Content-Type: "));
				if ((strncasecmp(contentTypeHeaderValue.c_str(), "text/plain", 10) == 0) ||
					(strncasecmp(contentTypeHeaderValue.c_str(), "text/csv", 8) == 0))
				{
					stringstream contentStream;

					contentStream << contentPart.substr(headersEnd + headersEndLength);

					// Import this
					CSVParser parser(contentStream);
					off_t totalCount = api.importCSV(campaign,
						&parser, columns, true);
#ifdef DEBUG
					clog << "processFormData: imported " << totalCount << " recipients" << endl;
#endif
					uploadSuccess = true;
				}
			}

			if (lastPart == true)
			{
				break;
			}

			// Next
			boundaryPos = nextBoundaryPos + 2;
		}
	}

	delete[] pContent;

	cout << "Content-Type: text/html\r\n\r\n";
	if (uploadSuccess == true)
	{
#ifdef DEBUG
		clog << "processFormData: success snippet is " << pConfig->m_uploadSuccessSnippet << endl;
#endif
		cout << pConfig->m_uploadSuccessSnippet << "\r\n";
	}
	else
	{
#ifdef DEBUG
		clog << "processFormData: failure snippet is " << pConfig->m_uploadFailureSnippet << endl;
#endif
		cout << pConfig->m_uploadFailureSnippet << "\r\n";
	}
	cout.flush();

	return uploadSuccess;
}

static bool processRequest(MySQLBase *pDb, DBUsageLogger &usageLogger,
	const string &queryString)
{
	off_t startOffset = 0, maxCount = 100;
	bool parsedOk = false;

	string actionParam(getQueryParameter(queryString, "action="));
	string whatParam(getQueryParameter(queryString, "what="));
	string startParam(getQueryParameter(queryString, "start="));
	if (startParam.empty() == false)
	{
		startOffset = (off_t)atoll(startParam.c_str());
	}
	string endParam(getQueryParameter(queryString, "end="));
	if (endParam.empty() == false)
	{
		off_t endOffset = (off_t)atoll(endParam.c_str());
		if (endOffset > startOffset)
		{
			maxCount = endOffset - startOffset;
		}
	}

	CampaignSQL campaignData(pDb);
	WebAPI api(&campaignData, &usageLogger, cout);
	Campaign campaign;
	bool xmlOutput = true;

	if ((actionParam == "ExportRecipients") &&
		(whatParam.empty() == false))
	{
		// This document is intended for download
		// Here, "what" is supposed to be a campaign ID 
		cout << "Content-Disposition: attachment; filename=Campaign"
			<< whatParam << ".csv" << "\r\nContent-Type: text/csv\r\n\r\n";
		xmlOutput = false;
	}
	else
	{
		cout << "Content-Type: text/xml\r\n\r\n";
		cout << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n";
		cout << "<GiveMail>\r\n";
	}

	if (actionParam == "ListCampaigns")
	{
		parsedOk = true;

		if (whatParam != "All")
		{
			// Assume "what" is a valid campaign status
			campaign.m_status = whatParam;
			if (campaign.hasValidStatus() == false)
			{
				outputError("Unsupported campaign status");
				usageLogger.logRequest(actionParam, GET_ERROR + 1);
				parsedOk = false;
			}
		}
		else
		{
			// Clear the default status so that it's not filtered on
			campaign.m_status.clear();
		}

		if (parsedOk == true)
		{
			api.listCampaigns(campaign, "Min", maxCount, startOffset);
			usageLogger.logRequest(actionParam, 0);
		}
	}
	else if ((actionParam == "ListRecipients") ||
		(actionParam == "ExportRecipients"))
	{
		if (whatParam.empty() == true)
		{
			outputError("No what parameter");
			usageLogger.logRequest(actionParam, GET_ERROR + 2);
		}
		else
		{
			RecipientsPrinter *pPrinter = NULL;

			// Assume "what" is a campaign ID 
			campaign.m_id = whatParam;

			if (actionParam == "ExportRecipients")
			{
				pPrinter = new RecipientsCSVPrinter(cout);
			}
			else
			{
				pPrinter = new RecipientsXMLPrinter(&api);
			}

			api.listRecipients(campaign, false, maxCount, startOffset, *pPrinter);
			usageLogger.logRequest(actionParam, 0);

			delete pPrinter;
			parsedOk = true;
		}
	}
	else
	{
		outputError("Unsupported action");
		usageLogger.logRequest("", GET_ERROR + 3);
	}
	if (xmlOutput == true)
	{
		cout << "</GiveMail>\r\n";
	}
	cout.flush();

	return parsedOk;
}

int main(int argc, char **argv)
{
	struct sigaction quitAction, ignAction;
	string configFileName("/etc/givemail/conf.d/givemail.conf");
	streambuf *cinBuff  = cin.rdbuf();
	g_clogBuff = clog.rdbuf();
	streambuf *coutBuff = cout.rdbuf();
	streambuf *cerrBuff = cerr.rdbuf();
	bool openedConfiguration = false, openedDatabase = false;

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

	atexit(closeAll);

	// Redirect log output
	stringstream logFileNameStream;
	logFileNameStream << "/tmp/givemail-webapi.log.";
	logFileNameStream << getpid();
	g_outputFile.open(logFileNameStream.str().c_str());
	clog.rdbuf(g_outputFile.rdbuf());

	// This environment variable should be declared in /etc/httpd/conf.d/fcgid.conf with
	// DefaultInitEnv WEBAPI_CONFIGURATION_FILE /etc/givemail/conf.d/myconfig.conf
	char *pConfigFile = getenv("WEBAPI_CONFIGURATION_FILE");
	if (pConfigFile != NULL)
	{
		configFileName = pConfigFile;
	}

	// Load configuration
	ConfigurationFile *pConfig = ConfigurationFile::getInstance(configFileName);
	if ((pConfig == NULL) ||
		(pConfig->parse() == false))
	{
		clog << "Couldn't open configuration file " << configFileName << endl;
	}
	else
	{
		openedConfiguration = true;
	}

	// Open the database
	MySQLBase db(pConfig->m_hostName, pConfig->m_databaseName,
		pConfig->m_userName, pConfig->m_password);
	openedDatabase = db.isOpen();
#if 0
	DBUsageLogger usageLogger(&db, "", 0);
	processXml(&db, usageLogger, false, "", 623, "");
	return EXIT_SUCCESS;
#endif

	FCGX_Request request;
	FCGX_Init();
	FCGX_InitRequest(&request, 0, 0);

	// Loop until we have to exit
	while ((g_mustQuit == false) &&
		(FCGX_Accept_r(&request) == 0))
	{
		fcgi_streambuf cin_fcgi_streambuf(request.in);
		fcgi_streambuf cout_fcgi_streambuf(request.out);
		fcgi_streambuf cerr_fcgi_streambuf(request.err);
		string contentType, cookies, queryString, remoteAddress, requestMethod;
		unsigned int contentLength = 0, remotePort = 0;

		// Redirect standard input and outputs
		cin.rdbuf(&cin_fcgi_streambuf);
		cout.rdbuf(&cout_fcgi_streambuf);
		cerr.rdbuf(&cerr_fcgi_streambuf);

		char *pContentLength = FCGX_GetParam("CONTENT_LENGTH", request.envp);
		if (pContentLength != NULL)
		{
			contentLength = (unsigned int)atoi(pContentLength);
		}

		unsigned int envNum = 0;
		for (char *pEnv = request.envp[envNum]; pEnv != NULL; pEnv = request.envp[++envNum])
		{
			string envVar(pEnv);

			string::size_type pos = envVar.find("CONTENT_TYPE=");
			if (pos != string::npos)
			{
				contentType = envVar.substr(pos + 13);
			}
			pos = envVar.find("HTTP_COOKIE=");
			if (pos != string::npos)
			{
				cookies = envVar.substr(pos + 12);
			}
			pos = envVar.find("QUERY_STRING=");
			if (pos != string::npos)
			{
				queryString = envVar.substr(pos + 13);
			}
			pos = envVar.find("REMOTE_ADDR=");
			if (pos != string::npos)
			{
				remoteAddress = envVar.substr(pos + 12);
			}
			pos = envVar.find("REMOTE_PORT=");
			if (pos != string::npos)
			{
				remotePort = (unsigned int)atoi(envVar.substr(pos + 12).c_str());
			}
			pos = envVar.find("REQUEST_METHOD=");
			if (pos != string::npos)
			{
				requestMethod = envVar.substr(pos + 15);
			}
#ifdef DEBUG
			clog << "<Env>" << WebAPI::encodeEntities(pEnv) << "</Env>" << endl;
#endif
		}

		try
		{
			DBUsageLogger usageLogger(&db, remoteAddress, remotePort);
			string authValue(extractAuthCookie(cookies));
			bool checkAuth = true;

			// For some hosts, requests don't have to be authenticated
			if (pConfig->m_skipAuth.find(remoteAddress) != pConfig->m_skipAuth.end())
			{
				checkAuth = false;
			}

			if (openedConfiguration == false)
			{
				outputError("Couldn't open configuration file", true);
			}
			else if (openedDatabase == false)
			{
				outputError("Couldn't open database", true);
			}
			else if ((requestMethod == "POST") &&
				(strncasecmp(contentType.c_str(), "multipart/form-data", 19) != 0))
			{
				processXml(&db, usageLogger, checkAuth, authValue,
					contentLength, contentType);
			}
			// Only allow POST'ed form data and GET requests from localhost
			else if ((requestMethod == "POST") &&
				(checkAuth == false))
			{
				processFormData(&db, usageLogger, queryString,
					contentLength, contentType);
			}
			else if ((requestMethod == "GET") &&
				(checkAuth == false))
			{
				processRequest(&db, usageLogger, queryString);
			}
			else
			{
				outputError("Unsupported request method", true);
			}
		}
		catch (const exception &e)
		{
			clog << "Caught exception ! " << e.what() << endl;
		}
		catch (...)
		{
			clog << "Caught unknown exception !" << endl;
		}

		// Restore standard input and outputs
		cin.rdbuf(cinBuff);
		cout.rdbuf(coutBuff);
		cerr.rdbuf(cerrBuff);
	}

	// FIXME: delete the ConfigurationFile instance

	return EXIT_SUCCESS;
}

