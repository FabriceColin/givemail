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

#include <strings.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <iostream>

#include "ConfigurationFile.h"

using std::clog;
using std::endl;
using std::set;
using std::string;
using std::ios;

ConfigurationFile *ConfigurationFile::m_pInstance = NULL;

ConfigurationFile::ConfigurationFile(const string &fileName) :
	m_threaded(true),
	m_maxSlaves(10),
	m_hideRecipients(true),
	m_fileName(fileName)
{
	pthread_mutex_init(&m_mutex, 0);
}

ConfigurationFile::~ConfigurationFile()
{
	pthread_mutex_destroy(&m_mutex);
}

ConfigurationFile *ConfigurationFile::getInstance(const string &fileName)
{
	if (m_pInstance == NULL)
	{
		m_pInstance = new ConfigurationFile(fileName);
	}

	return m_pInstance;
}

string ConfigurationFile::getNodeContent(xmlNode *pNode)
{
	string content;

	if (pNode == NULL)
	{
		return "";
	}

	char *pContent = (char*)xmlNodeGetContent(pNode);
	if (pContent != NULL)
	{
		content += pContent;

		xmlFree(pContent);
	}
	else for (xmlNode *pSubNode = pNode->children;
		pSubNode != NULL; pSubNode = pSubNode->next)
	{
		if (pSubNode->type == XML_CDATA_SECTION_NODE)
		{
			char *pContent = (char*)xmlNodeGetContent(pSubNode);
			if (pContent != NULL)
			{
				content += pContent;

				xmlFree(pContent);
			}
		}
	}

	return content;
}

string ConfigurationFile::getFileName(void) const
{
	return m_fileName;
}

bool ConfigurationFile::parse(void)
{
	xmlDoc *pDoc = NULL;
	xmlNode *pRootElement = NULL;
	bool parsedFile = false;

	if (m_fileName.empty() == true)
	{
		return false;
	}

	// Initialize the library and check potential ABI mismatches between
	// the version it was compiled for and the actual shared library used
	LIBXML_TEST_VERSION

	// Parse the file and get the document
#if LIBXML_VERSION < 20600
	pDoc = xmlParseFile(m_fileName.c_str());
#else
	pDoc = xmlReadFile(m_fileName.c_str(), NULL, XML_PARSE_NOCDATA);
#endif
	if (pDoc != NULL)
	{
		// Get the root element node
		pRootElement = xmlDocGetRootElement(pDoc);

		for (xmlNode *pCurrentNode = pRootElement->children; pCurrentNode != NULL;
			pCurrentNode = pCurrentNode->next)
		{
			// What type of tag is it ?
			if (pCurrentNode->type != XML_ELEMENT_NODE)
			{
				continue;
			}

			// What tag is it ?
			if (xmlStrncmp(pCurrentNode->name, BAD_CAST"database", 8) == 0)
			{
				for (xmlNode *pCurrentDBNode = pCurrentNode->children;
					pCurrentDBNode != NULL; pCurrentDBNode = pCurrentDBNode->next)
				{
					if (pCurrentDBNode->type != XML_ELEMENT_NODE)
					{
						continue;
					}

					string childNodeContent(getNodeContent(pCurrentDBNode));
					if (childNodeContent.empty() == true)
					{
						continue;
					}

					if (xmlStrncmp(pCurrentDBNode->name, BAD_CAST"hostname", 8) == 0)
					{
						m_hostName = childNodeContent;
					}
					else if (xmlStrncmp(pCurrentDBNode->name, BAD_CAST"databasename", 12) == 0)
					{
						m_databaseName = childNodeContent;
					}
					else if (xmlStrncmp(pCurrentDBNode->name, BAD_CAST"username", 8) == 0)
					{
						m_userName = childNodeContent;
					}
					else if (xmlStrncmp(pCurrentDBNode->name, BAD_CAST"password", 8) == 0)
					{
						m_password = childNodeContent;
					}
				}
			}
			else if (xmlStrncmp(pCurrentNode->name, BAD_CAST"domain", 6) == 0)
			{
				DomainLimits domainLimits("");

				for (xmlNode *pCurrentDomainNode = pCurrentNode->children;
					pCurrentDomainNode != NULL; pCurrentDomainNode = pCurrentDomainNode->next)
				{
					if (pCurrentDomainNode->type != XML_ELEMENT_NODE)
					{
						continue;
					}

					string childNodeContent(getNodeContent(pCurrentDomainNode));
					if (childNodeContent.empty() == true)
					{
						continue;
					}

					if (xmlStrncmp(pCurrentDomainNode->name, BAD_CAST"domainname", 10) == 0)
					{
						domainLimits.m_domainName = childNodeContent;
					}
					else if (xmlStrncmp(pCurrentDomainNode->name, BAD_CAST"maxmsgsperserver", 16) == 0)
					{
						domainLimits.m_maxMsgsPerServer = (unsigned int)atoi(childNodeContent.c_str());
					}
					else if (xmlStrncmp(pCurrentDomainNode->name, BAD_CAST"usesubmission", 13) == 0)
					{
						if (strncasecmp(childNodeContent.c_str(), "YES", 3) == 0)
						{
							domainLimits.m_useSubmissionPort = true;
						}
						else
						{
							domainLimits.m_useSubmissionPort = false;
						}
					}
				}

				if (domainLimits.m_domainName.empty() == false)
				{
					pthread_mutex_lock(&m_mutex);
					m_domainLimits.insert(domainLimits);
					pthread_mutex_unlock(&m_mutex);
				}
			}
			else if (xmlStrncmp(pCurrentNode->name, BAD_CAST"master", 6) == 0)
			{
				for (xmlNode *pCurrentMasterNode = pCurrentNode->children;
					pCurrentMasterNode != NULL; pCurrentMasterNode = pCurrentMasterNode->next)
				{
					if (pCurrentMasterNode->type != XML_ELEMENT_NODE)
					{
						continue;
					}

					string childNodeContent(getNodeContent(pCurrentMasterNode));
					if (childNodeContent.empty() == true)
					{
						continue;
					}

					if (xmlStrncmp(pCurrentMasterNode->name, BAD_CAST"msgidsuffix", 11) == 0)
					{
						m_options.m_msgIdSuffix = childNodeContent;
					}
					else if (xmlStrncmp(pCurrentMasterNode->name, BAD_CAST"complaints", 10) == 0)
					{
						m_options.m_complaints = childNodeContent;
					}
				}
			}
			else if (xmlStrncmp(pCurrentNode->name, BAD_CAST"slave", 5) == 0)
			{
				for (xmlNode *pCurrentSlaveNode = pCurrentNode->children;
					pCurrentSlaveNode != NULL; pCurrentSlaveNode = pCurrentSlaveNode->next)
				{
					if (pCurrentSlaveNode->type != XML_ELEMENT_NODE)
					{
						continue;
					}

					string childNodeContent(getNodeContent(pCurrentSlaveNode));
					if (childNodeContent.empty() == true)
					{
						continue;
					}

					if (xmlStrncmp(pCurrentSlaveNode->name, BAD_CAST"dkprivatekey", 12) == 0)
					{
						m_dkPrivateKey = childNodeContent;
					}
					else if (xmlStrncmp(pCurrentSlaveNode->name, BAD_CAST"dkdomain", 8) == 0)
					{
						m_dkDomain = childNodeContent;
					}
					else if (xmlStrncmp(pCurrentSlaveNode->name, BAD_CAST"threaded", 8) == 0)
					{
						if (strncasecmp(childNodeContent.c_str(), "NO", 2) == 0)
						{
							m_threaded = false;
						}
						else
						{
							m_threaded = true;
						}
					}
					else if ((xmlStrncmp(pCurrentSlaveNode->name, BAD_CAST"maxthreads", 10) == 0) ||
						(xmlStrncmp(pCurrentSlaveNode->name, BAD_CAST"maxslaves", 9) == 0))
					{
						m_maxSlaves = (off_t)atoll(childNodeContent.c_str());
					}
					else if (xmlStrncmp(pCurrentSlaveNode->name, BAD_CAST"dsnnotify", 9) == 0)
					{
						m_options.m_dsnNotify = childNodeContent;
					}
				}
			}
			else if (xmlStrncmp(pCurrentNode->name, BAD_CAST"endofcampaign", 13) == 0)
			{
				for (xmlNode *pCurrentEOCNode = pCurrentNode->children;
					pCurrentEOCNode != NULL; pCurrentEOCNode = pCurrentEOCNode->next)
				{
					if (pCurrentEOCNode->type != XML_ELEMENT_NODE)
					{
						continue;
					}

					string childNodeContent(getNodeContent(pCurrentEOCNode));
					if (childNodeContent.empty() == true)
					{
						continue;
					}

					if (xmlStrncmp(pCurrentEOCNode->name, BAD_CAST"command", 7) == 0)
					{
						m_endOfCampaignCommand = childNodeContent;
					}
				}
			}
			else if (xmlStrncmp(pCurrentNode->name, BAD_CAST"spamcheck", 9) == 0)
			{
				for (xmlNode *pCurrentSPNode = pCurrentNode->children;
					pCurrentSPNode != NULL; pCurrentSPNode = pCurrentSPNode->next)
				{
					if (pCurrentSPNode->type != XML_ELEMENT_NODE)
					{
						continue;
					}

					string childNodeContent(getNodeContent(pCurrentSPNode));
					if (childNodeContent.empty() == true)
					{
						continue;
					}

					if (xmlStrncmp(pCurrentSPNode->name, BAD_CAST"command", 7) == 0)
					{
						m_spamCheckCommand = childNodeContent;
					}
				}
			}
			else if (xmlStrncmp(pCurrentNode->name, BAD_CAST"relay", 5) == 0)
			{
				for (xmlNode *pCurrentRelayNode = pCurrentNode->children;
					pCurrentRelayNode != NULL; pCurrentRelayNode = pCurrentRelayNode->next)
				{
					if (pCurrentRelayNode->type != XML_ELEMENT_NODE)
					{
						continue;
					}

					string childNodeContent(getNodeContent(pCurrentRelayNode));
					if (childNodeContent.empty() == true)
					{
						continue;
					}

					if (xmlStrncmp(pCurrentRelayNode->name, BAD_CAST"address", 7) == 0)
					{
						m_options.m_mailRelayAddress = childNodeContent;
					}
					else if (xmlStrncmp(pCurrentRelayNode->name, BAD_CAST"port", 4) == 0)
					{
						m_options.m_mailRelayPort = atoi(childNodeContent.c_str());
					}
					else if (xmlStrncmp(pCurrentRelayNode->name, BAD_CAST"internaldomain", 14) == 0)
					{
						m_options.m_internalDomain = childNodeContent;
					}
					else if (xmlStrncmp(pCurrentRelayNode->name, BAD_CAST"username", 8) == 0)
					{
						m_options.m_mailRelayUserName = childNodeContent;
					}
					else if (xmlStrncmp(pCurrentRelayNode->name, BAD_CAST"password", 8) == 0)
					{
						m_options.m_mailRelayPassword = childNodeContent;
					}
					else if (xmlStrncmp(pCurrentRelayNode->name, BAD_CAST"starttls", 8) == 0)
					{
						if (strncasecmp(childNodeContent.c_str(), "YES", 3) == 0)
						{
							m_options.m_mailRelayTLS = true;
						}
						else
						{
							m_options.m_mailRelayTLS = false;
						}
					}
				}
			}
			else if (xmlStrncmp(pCurrentNode->name, BAD_CAST"confidentiality", 15) == 0)
			{
				for (xmlNode *pCurrentConfNode = pCurrentNode->children;
					pCurrentConfNode != NULL; pCurrentConfNode = pCurrentConfNode->next)
				{
					if (pCurrentConfNode->type != XML_ELEMENT_NODE)
					{
						continue;
					}

					string childNodeContent(getNodeContent(pCurrentConfNode));
					if (childNodeContent.empty() == true)
					{
						continue;
					}

					if (xmlStrncmp(pCurrentConfNode->name, BAD_CAST"hiderecipients", 14) == 0)
					{
						if (strncasecmp(childNodeContent.c_str(), "YES", 3) == 0)
						{
							m_hideRecipients = true;
						}
						else
						{
							m_hideRecipients = false;
						}
					}
				}
			}
			else if (xmlStrncmp(pCurrentNode->name, BAD_CAST"webapi", 6) == 0)
			{
				for (xmlNode *pCurrentUpNode = pCurrentNode->children;
					pCurrentUpNode != NULL; pCurrentUpNode = pCurrentUpNode->next)
				{
					if (pCurrentUpNode->type != XML_ELEMENT_NODE)
					{
						continue;
					}

					string childNodeContent(getNodeContent(pCurrentUpNode));
					if (childNodeContent.empty() == true)
					{
						continue;
					}

					if (xmlStrncmp(pCurrentUpNode->name, BAD_CAST"successsnippet", 14) == 0)
					{
						m_uploadSuccessSnippet = childNodeContent;
					}
					else if (xmlStrncmp(pCurrentUpNode->name, BAD_CAST"failuresnippet", 14) == 0)
					{
						m_uploadFailureSnippet = childNodeContent;
					}
					else if (xmlStrncmp(pCurrentUpNode->name, BAD_CAST"skipauth", 8) == 0)
					{
						m_skipAuth.insert(childNodeContent);
					}
				}
			}
		}

		char *pEnvVar = NULL;

		// These may be provided through environment variables
		if (m_options.m_mailRelayAddress.empty() == true)
		{
			pEnvVar = getenv("GIVEMAIL_RELAY_ADDRESS");
			if (pEnvVar != NULL)
			{
				m_options.m_mailRelayAddress = pEnvVar;
			}
			pEnvVar = getenv("GIVEMAIL_RELAY_PORT");
			if (pEnvVar != NULL)
			{
				m_options.m_mailRelayPort = atoi(pEnvVar);
			}
			pEnvVar = getenv("GIVEMAIL_RELAY_STARTTLS");
			if ((pEnvVar != NULL) &&
				(strncasecmp(pEnvVar, "Y", 1) == 0))
			{
				m_options.m_mailRelayTLS = true;
			}
		}
		if (m_options.m_internalDomain.empty() == true)
		{
			pEnvVar = getenv("GIVEMAIL_RELAY_INTERNAL_DOMAIN");
			if (pEnvVar != NULL)
			{
				m_options.m_internalDomain = pEnvVar;
			}
		}
		if (m_options.m_mailRelayUserName.empty() == true)
		{
			pEnvVar = getenv("GIVEMAIL_RELAY_USER_NAME");
			if (pEnvVar != NULL)
			{
				m_options.m_mailRelayUserName = pEnvVar;
			}
		}
		if (m_options.m_mailRelayPassword.empty() == true)
		{
			pEnvVar = getenv("GIVEMAIL_RELAY_PASSWORD");
			if (pEnvVar != NULL)
			{
				m_options.m_mailRelayPassword = pEnvVar;
			}
		}

		// Free the document
		xmlFreeDoc(pDoc);

		parsedFile = true;
	}

	// Cleanup
	xmlCleanupParser();

	// If no suffix was provided, default to the hostname
	if (m_options.m_msgIdSuffix.empty() == true)
	{
		char hostName[HOST_NAME_MAX + 1];

		if ((gethostname(hostName, HOST_NAME_MAX) == 0) &&
			(hostName[0] != '\0'))
		{
			m_options.m_msgIdSuffix = string("@") + hostName;
#ifdef DEBUG
			clog << "ConfigurationFile::parse: default message ID suffix is "
				<< m_options.m_msgIdSuffix << endl;
#endif
		}
	}

	return parsedFile;
}

bool ConfigurationFile::findDomainLimits(DomainLimits &domainLimits,
	bool fallbackToARecord)
{
	bool wasFound = false, isUsable = true;

	pthread_mutex_lock(&m_mutex);
	set<DomainLimits>::iterator limitIter = m_domainLimits.find(domainLimits);
	if (limitIter != m_domainLimits.end())
	{
		domainLimits = *limitIter;
		wasFound = true;

#ifdef DEBUG
		clog << "ConfigurationFile::findDomainLimits: " << domainLimits.m_domainName
			<< " is limited to " << limitIter->m_maxMsgsPerServer << endl;
#endif
	}

	// Get the MX records for this domain if necessary
	if (domainLimits.m_mxRecords.empty() == true)
	{
		if (Resolver::queryMXRecords(domainLimits.m_domainName, domainLimits.m_mxRecords) == false)
		{
			if (fallbackToARecord == true)
			{
				// No MX record for this domain, fallback to A records
				isUsable = Resolver::queryARecords(domainLimits.m_domainName,
					domainLimits.m_mxRecords);
			}
			else
			{
				isUsable = false;
			}
		}

		// Insert or update this entry
		if (wasFound == true)
		{
			m_domainLimits.erase(limitIter);
		}
		m_domainLimits.insert(domainLimits);
	}
	pthread_mutex_unlock(&m_mutex);

	return isUsable;
}

