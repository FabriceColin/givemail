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

#ifndef _XMLMESSAGEDETAILS_H_
#define _XMLMESSAGEDETAILS_H_

#include <string>
#include <map>

#include "MessageDetails.h"

/// An XML file holding headers and content of an email.
class XmlMessageDetails : public MessageDetails
{
	public:
		XmlMessageDetails();
		virtual ~XmlMessageDetails();

		/// Parses the file and extracts the message's details.
		bool parse(const std::string &fileName);

		/// Parses the supplied memory buffer and extracts the message's details.
		bool parse(const char *pBuffer, int size);

		std::map<std::string, std::string> m_customFields;

	protected:
		void parse(xmlDoc *pDoc);

	private:
		// XmlMessageDetails objects cannot be copied
		XmlMessageDetails(const XmlMessageDetails &other);
		XmlMessageDetails &operator=(const XmlMessageDetails &other);

};

#endif // _XMLMESSAGEDETAILS_H_
