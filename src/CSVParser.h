/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 *  Copyright 2008 Global Sign In
 *  Copyright 2009 Fabrice Colin
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

#ifndef _CSVPARSER_H_
#define _CSVPARSER_H_

#include <string>
#include <istream>

/// A basic line-by-line, column-by-column parser for CSVs.
class CSVParser
{
	public:
		CSVParser(std::istream &stream);
		~CSVParser();

		/// Replaces double double-quotes.
		static std::string unescapeColumn(const std::string &column);

		/// Moves to the next line.
		bool nextLine(void);

		/// Moves to the next column and returns its value.
		bool nextColumn(std::string &column);

	protected:
		std::istream &m_stream;
		std::string m_line;
		std::string::size_type m_pos;
 
};

#endif // _CSVPARSER_H_
