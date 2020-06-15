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

#ifndef _SUBSTITUTE_H_
#define _SUBSTITUTE_H_

#include <map>
#include <set>
#include <string>
#include <ctemplate/template.h>

/// Substitutes fields in content with their actual values.
class Substitute
{
	public:
		Substitute(const std::string &dictionaryId,
			const std::string &contentTemplate,
			bool escapeEntities);
		virtual ~Substitute();

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
		Substitute(const Substitute &other);
		Substitute &operator=(const Substitute &other);

};

/// Substitutes fields in content with their actual values.
class CTemplateSubstitute : public Substitute
{
	public:
		CTemplateSubstitute(const std::string &dictionaryId,
			const std::string &contentTemplate,
			bool escapeEntities);
		virtual ~CTemplateSubstitute();

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
		CTemplateSubstitute(const CTemplateSubstitute &other);
		CTemplateSubstitute &operator=(const CTemplateSubstitute &other);

};
#endif // _SUBSTITUTE_H_
