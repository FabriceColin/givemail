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

#include <iostream>

#include "Substitute.h"

#define MAX_FIELD_LENGTH	32

using std::clog;
using std::endl;
using std::map;
using std::set;
using std::string;
using namespace ctemplate;

/// Escape common HTML entities.
static string escapeHtmlEntities(const string &recipientsList)
{
	string recipientsString(recipientsList);

	string::size_type pos = recipientsString.find_first_of("\"<>");
	while (pos != string::npos)
	{
		char thisChar = recipientsString[pos];
		string rep;

		if (thisChar == '\"')
		{
			rep = "&quot;";
		}
		else if (thisChar == '<')
		{
			rep = "&lt;";
		}
		else if (thisChar == '>')
		{
			rep = "&gt;";
		}

		recipientsString.replace(pos, 1, rep.c_str());

		// Next
		pos = recipientsString.find_first_of("\"<>", pos + 1);
	}

	return recipientsString;
}

Substitute::Substitute(const string &dictionaryId,
	const string &contentTemplate, bool escapeEntities) :
	m_contentTemplate(contentTemplate),
	m_escapeEntities(escapeEntities)
{
}

Substitute::~Substitute()
{
}

void Substitute::findFields(void)
{
	if (m_contentTemplate.empty() == true)
	{
		return;
	}

	string::size_type currentPos = 0;

	// Find all possible custom fields
	string::size_type fieldPos = m_contentTemplate.find("{");
	while (fieldPos != string::npos)
	{
		string::size_type endFieldPos = m_contentTemplate.find("}", fieldPos);

		if ((endFieldPos != string::npos) &&
			(endFieldPos - fieldPos + 1 <= MAX_FIELD_LENGTH))
		{
			string fieldName(m_contentTemplate.substr(fieldPos + 1, endFieldPos - fieldPos - 1));

			m_fieldPos[fieldPos] = fieldName;
#ifdef DEBUG
			clog << "Substitute::findFields: found possible custom field " << fieldName << endl;
#endif
			// Move past this field
			currentPos = fieldPos + fieldName.length();
		}
		else
		{
			currentPos = fieldPos + 1;
		}

		fieldPos = m_contentTemplate.find("{", currentPos);
	}
#ifdef DEBUG
	clog << "Substitute::findFields: found " << m_fieldPos.size() << " fields" << endl;
#endif
}

bool Substitute::hasFields(void) const
{
	return !m_fieldPos.empty();
}

bool Substitute::hasField(const string &fieldName) const
{
	for (map<string::size_type, string>::const_iterator posIter = m_fieldPos.begin();
		posIter != m_fieldPos.end(); ++posIter)
	{
		if (fieldName == posIter->second)
		{
			return true;
		}
	}

	return false;
}

void Substitute::substitute(const map<string, string> &fieldValues,
	string &content)
{
	string::size_type currentPos = 0;

	if (m_fieldPos.empty() == true)
	{
		// Nothing to substitute
		content = m_contentTemplate;
		return;
	}

	// Go through the position, field name map
	for (map<string::size_type, string>::const_iterator posIter = m_fieldPos.begin();
		posIter != m_fieldPos.end(); ++posIter)
	{
		string fieldName(posIter->second);
		string fieldValue;

		// What's the value of this field ?
		map<string, string>::const_iterator valueIter = fieldValues.find(fieldName);
		if (valueIter != fieldValues.end())
		{
			fieldValue = valueIter->second;
		}
#ifdef DEBUG
		else clog << "Substitute::substitute: no value for " << fieldName << endl;
#endif

		content += m_contentTemplate.substr(currentPos, posIter->first - currentPos);
		if (fieldValue.empty() == false)
		{
			if (m_escapeEntities == true)
			{
				content += escapeHtmlEntities(fieldValue);
			}
			else
			{
				content += fieldValue;
			}
		}

		// Move past this field
		currentPos = posIter->first + fieldName.length() + 2;
	}

	// Add remainder
	if (currentPos < m_contentTemplate.size())
	{
		content += m_contentTemplate.substr(currentPos);
	}
}

CTemplateSubstitute::CTemplateSubstitute(const string &dictionaryId,
	const string &contentTemplate, bool escapeEntities) :
	Substitute(dictionaryId, contentTemplate, escapeEntities),
	m_dict(dictionaryId)
{
}

CTemplateSubstitute::~CTemplateSubstitute()
{
}

void CTemplateSubstitute::findFields(void)
{
	// Nothing to do
}

bool CTemplateSubstitute::hasFields(void) const
{
	string::size_type startPos = m_contentTemplate.find("{{");
	string::size_type endPos = m_contentTemplate.find("}}");

	// Run a quick check
	if ((startPos != string::npos) &&
		(endPos != string::npos))
	{
		return true;
	}

	return false;
}

bool CTemplateSubstitute::hasField(const string &fieldName) const
{
	string::size_type fieldPos = m_contentTemplate.find(string("{{") + fieldName);

	// Run a quick check
	if (fieldPos != string::npos)
	{
		return true;
	}

	return false;
}

void CTemplateSubstitute::substitute(const map<string, string> &fieldValues,
	string &content)
{
	// Set dictionary values
	for (map<string, string>::const_iterator valueIter = fieldValues.begin();
		valueIter != fieldValues.end(); ++valueIter)
	{
		m_dict.SetValue(valueIter->first, valueIter->second);
		m_dict.ShowSection(valueIter->first + "_section");
	}

	// Create a template named after the dictionary
	Template::StringToTemplateCache(m_dict.name(), m_contentTemplate);
	Template *pTemplate = Template::GetTemplate(m_dict.name(), DO_NOT_STRIP);
	if (pTemplate == NULL)
	{
		content = m_contentTemplate;
		return;
	}

	pTemplate->Expand(&content, &m_dict);
}

