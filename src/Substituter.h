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

#ifndef _SUBSTITUTER_H_
#define _SUBSTITUTER_H_

#include <map>
#include <set>
#include <string>
#include <ctemplate/template.h>

/// Substitutes fields in content with their actual values.
class Substituter
{
	public:
		Substituter(const std::string &dictionaryId,
			const std::string &contentTemplate,
			bool escapeEntities);
		virtual ~Substituter();

		/// Returns whether there are fields to substitute in the content.
		virtual bool hasFields(void) const = 0;

		/// Returns whether the given field name is to be substituted.
		virtual bool hasField(const std::string &fieldName) const = 0;

		/// Substitutes fields, links and images if applicable.
		virtual void substitute(const std::map<std::string, std::string> &fieldValues,
			std::string &content) = 0;

	protected:
		std::string m_contentTemplate;
		bool m_escapeEntities;

	private:
		Substituter(const Substituter &other);
		Substituter &operator=(const Substituter &other);

};

/// Substitutes fields in content with their actual values.
class CTemplateSubstituter : public Substituter
{
	public:
		CTemplateSubstituter(const std::string &dictionaryId,
			const std::string &contentTemplate,
			bool escapeEntities);
		virtual ~CTemplateSubstituter();

		/// Returns whether there are fields to substitute in the content.
		virtual bool hasFields(void) const;

		/// Returns whether the given field name is to be substituted.
		static bool hasField(const std::string &contentTemplate,
			const std::string &fieldName);

		/// Returns whether the given field name is to be substituted.
		bool hasField(const std::string &fieldName) const;

		/// Substitutes fields, links and images if applicable.
		void substitute(const std::map<std::string, std::string> &fieldValues,
			std::string &content);

	protected:
		ctemplate::TemplateDictionary m_dict;

	private:
		CTemplateSubstituter(const CTemplateSubstituter &other);
		CTemplateSubstituter &operator=(const CTemplateSubstituter &other);

};
#endif // _SUBSTITUTER_H_
