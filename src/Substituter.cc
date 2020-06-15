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

#include "Substituter.h"

using std::clog;
using std::endl;
using std::map;
using std::string;
using namespace ctemplate;

Substituter::Substituter(const string &dictionaryId,
	const string &contentTemplate, bool escapeEntities) :
	m_contentTemplate(contentTemplate),
	m_escapeEntities(escapeEntities)
{
}

Substituter::~Substituter()
{
}

CTemplateSubstituter::CTemplateSubstituter(const string &dictionaryId,
	const string &contentTemplate, bool escapeEntities) :
	Substituter(dictionaryId, contentTemplate, escapeEntities),
	m_dict(dictionaryId)
{
}

CTemplateSubstituter::~CTemplateSubstituter()
{
}

bool CTemplateSubstituter::hasFields(void) const
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

bool CTemplateSubstituter::hasField(const string &contentTemplate,
	const string &fieldName)
{
	string::size_type fieldPos = contentTemplate.find(string("{{") + fieldName);

	// Run a quick check
	if (fieldPos != string::npos)
	{
		return true;
	}

	return false;
}

bool CTemplateSubstituter::hasField(const string &fieldName) const
{
	return hasField(m_contentTemplate, fieldName);
}

void CTemplateSubstituter::substitute(const map<string, string> &fieldValues,
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

