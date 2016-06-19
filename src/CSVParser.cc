/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 *  Copyright 2008 Global Sign In
 *  Copyright 2009-2010 Fabrice Colin
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

#include "CSVParser.h"

using std::string;
using std::istream;

CSVParser::CSVParser(istream &stream) :
	m_stream(stream),
	m_pos(0)
{
}

CSVParser::~CSVParser()
{
}

string CSVParser::unescapeColumn(const string &column)
{
	if (column.empty() == true)
	{
		return "";
	}

	string cleanColumn(column);

	string::size_type startPos = cleanColumn.find("\"\"");
	while (startPos != string::npos)
	{
		cleanColumn.erase(startPos, 1);

		++startPos;
		if (startPos > cleanColumn.length())
		{
			break;
		}

		startPos = cleanColumn.find("\"\"", startPos);
	}

	return cleanColumn;
}

bool CSVParser::nextLine(void)
{
	m_line.clear();
	m_pos = 0;

	if (m_stream.eof())
	{
		return false;
	}

	getline(m_stream, m_line);

	if (m_stream.fail())
	{
		return false;
	}

	return true;
}

bool CSVParser::nextColumn(string &column)
{
	string::size_type pos = 0;
	unsigned int increment = 1;

	if (m_pos == string::npos)
	{
		return false;
	}

	if (m_line[m_pos] != '\"')
	{
		pos = m_line.find(',', m_pos);
	}
	else
	{
		++m_pos;
		pos = m_line.find('\"', m_pos);
		while ((pos != string::npos) &&
			(pos + 1 < m_line.length()) &&
			(m_line[pos + 1] == '\"'))
		{
			// Skip escaped quotes in content
			pos = m_line.find('\"', pos + 2);
		}

		increment = 2;
	}

	if (pos == string::npos)
	{
		// Only one column or the last column
		column = m_line.substr(m_pos);
		m_pos = pos;

		return true;
	}

	column = m_line.substr(m_pos, pos - m_pos);
	m_pos = pos + increment;

	if (m_pos + increment >= m_line.length())
	{
		// The next column is empty
		m_pos = string::npos;
	}

	return true;
}

