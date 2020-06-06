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

#ifndef _CONFIGURATIONFILE_H_
#define _CONFIGURATIONFILE_H_

#include <pthread.h>
#include <libxml/parser.h>
#include <string>
#include <set>

#include "DomainLimits.h"
#include "SMTPOptions.h"

/// A XML-based configuration file.
class ConfigurationFile
{
	public:
		virtual ~ConfigurationFile();

		static ConfigurationFile *getInstance(const std::string &fileName);

		static std::string getNodeContent(xmlNode *pNode);

		/// Returns the file name.
		std::string getFileName(void) const;

		/// Parses the configuration file.
		bool parse(void);

		/// Returns configuration for the given domain.
		bool findDomainLimits(DomainLimits &domainLimits,
			bool fallbackToARecord = false);

		std::string m_hostName;
		std::string m_databaseName;
		std::string m_userName;
		std::string m_password;
		std::string m_dkPrivateKey;
		std::string m_dkPublicKey;
		std::string m_dkDomain;
		std::string m_dkSelector;
		bool m_threaded;
		off_t m_maxSlaves;
		std::string m_endOfCampaignCommand;
		std::string m_spamCheckCommand;
		bool m_hideRecipients;
		std::string m_uploadSuccessSnippet;
		std::string m_uploadFailureSnippet;
		std::set<std::string> m_skipAuth;
		SMTPOptions m_options;

	protected:
		static ConfigurationFile *m_pInstance;
		std::string m_fileName;
		pthread_mutex_t m_mutex;
		std::set<DomainLimits> m_domainLimits;

		ConfigurationFile(const std::string &fileName);

	private:
		// ConfigurationFile objects cannot be copied
		ConfigurationFile(const ConfigurationFile &other);
		ConfigurationFile &operator=(const ConfigurationFile &other);

};

#endif // _CONFIGURATIONFILE_H_
