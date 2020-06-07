/*
 *  Copyright 2009-2020 Fabrice Colin
 *
 *  The MySQL FLOSS License Exception allows us to license this code under
 *  the LGPL even though MySQL is under the GPL.
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

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <set>
#include <algorithm>
#include <iostream>
#include <sstream>

#include "config.h"
#include "TimeConverter.h"
#include "MySQLBase.h"

using std::clog;
using std::endl;
using std::string;
using std::stringstream;
using std::set;
using std::map;
using std::vector;
using std::pair;
using std::for_each;

// A function object to close statements with for_each()
struct CloseStatementsFunc
{
	public:
		void operator()(map<string, MYSQL_STMT*>::value_type &p)
		{
			if (p.second != NULL)
			{
				 mysql_stmt_close(p.second);
			}
		}
};

static string trimSpaces(const string &strWithSpaces)
{
	string str(strWithSpaces);
	string::size_type pos = 0;

	while ((str.empty() == false) && (pos < str.length()))
	{
		if (isspace(str[pos]) == 0)
		{
			++pos;
			break;
		}

		str.erase(pos, 1);
	}

	for (pos = str.length() - 1;
			(str.empty() == false) && (pos >= 0); --pos)
	{
		if (isspace(str[pos]) == 0)
		{
			break;
		}

		str.erase(pos, 1);
	}

	return str;
}

static string extractField(const string &str,
	const string &start, const string &end,
	string::size_type &endPos)
{
	string fieldValue;
	string::size_type startPos = string::npos;

	if (start.empty() == true)
	{
		startPos = 0;
	}
	else
	{
		startPos = str.find(start, endPos);
	}

	if (startPos != string::npos)
	{
		startPos += start.length();

		if (end.empty() == true)
		{
			fieldValue = str.substr(startPos);
		}
		else
		{
			endPos = str.find(end, startPos);

			if (endPos != string::npos)
			{
				fieldValue = str.substr(startPos, endPos - startPos);
			}
		}
	}

	return fieldValue;
}

static void parseSeparatedList(const string &listValue,
	const string &separator, vector<string> &elements)
{
	if ((listValue.empty() == true) ||
		(separator.empty() == true))
	{
		return;
	}

	string::size_type endPos = 0;
	string element(extractField(listValue, "", separator, endPos));

	if (element.empty() == false)
	{
		elements.push_back(element);
	}
	else
	{
		elements.push_back(listValue);
		return;
	}

	element = extractField(listValue, separator, separator, endPos);
	while (element.empty() == false)
	{
		elements.push_back(element);

		element = extractField(listValue, separator, separator, endPos);
	}
}

MySQLRow::MySQLRow(MYSQL_ROW row, unsigned int nColumns) :
	SQLRow(nColumns),
	m_row(row),
	m_pStatement(NULL),
	m_pBindValues(NULL)
{
}

MySQLRow::MySQLRow(MYSQL_STMT *pStatement,
	MYSQL_BIND *pBindValues,
	unsigned int nColumns) :
	SQLRow(nColumns),
	m_pStatement(pStatement),
	m_pBindValues(pBindValues)
{
	// Ensure columns are retrieved in the right order
	for (unsigned int colIndex = 0; colIndex < m_nColumns; ++colIndex)
	{
		m_columnValues.insert(pair<unsigned int, string>(colIndex, getColumnValue(colIndex)));
	}
}

MySQLRow::~MySQLRow()
{
	if (m_pBindValues != NULL)
	{
		// Free buffers
		for (unsigned int colIndex = 0; colIndex < m_nColumns; ++colIndex)
		{
			switch (m_pBindValues[colIndex].buffer_type)
			{
				case MYSQL_TYPE_TINY_BLOB:
				case MYSQL_TYPE_MEDIUM_BLOB:
				case MYSQL_TYPE_LONG_BLOB:
				case MYSQL_TYPE_BLOB:
				case MYSQL_TYPE_VAR_STRING:
				case MYSQL_TYPE_STRING:
					if (m_pBindValues[colIndex].buffer != NULL)
					{
						free(m_pBindValues[colIndex].buffer);
						m_pBindValues[colIndex].buffer = NULL;
						m_pBindValues[colIndex].buffer_length = 0;
					}
					break;
				default:
					// Everything else will be freed with the results
					break;
			}
		}
	}
}

string MySQLRow::getColumnValue(unsigned int colIndex)
{
	// What's the actual length ?
	if ((m_pBindValues == NULL) ||
		(m_pBindValues[colIndex].buffer_length == 0))
	{
		return "";
	}

	string value;
	bool isString = false, isTime = false;

	switch (m_pBindValues[colIndex].buffer_type)
	{
		case MYSQL_TYPE_TINY_BLOB:
		case MYSQL_TYPE_MEDIUM_BLOB:
		case MYSQL_TYPE_LONG_BLOB:
		case MYSQL_TYPE_BLOB:
		case MYSQL_TYPE_VAR_STRING:
		case MYSQL_TYPE_STRING:
			m_pBindValues[colIndex].buffer = malloc(m_pBindValues[colIndex].buffer_length);
			memset(m_pBindValues[colIndex].buffer, 0, m_pBindValues[colIndex].buffer_length);
			isString = true;
			break;
		case MYSQL_TYPE_TIME:
		case MYSQL_TYPE_DATE:
		case MYSQL_TYPE_DATETIME:
		case MYSQL_TYPE_TIMESTAMP:
		case MYSQL_TYPE_NEWDATE:
			isTime = true;
			break;
		default:
			break;
	}

	if (mysql_stmt_fetch_column(m_pStatement, &m_pBindValues[colIndex], colIndex, 0) == 0)
	{
		stringstream valueStr;

		if (isString == true)
		{
			if (m_pBindValues[colIndex].buffer != NULL)
			{
				value = string((const char*)m_pBindValues[colIndex].buffer,
					m_pBindValues[colIndex].buffer_length - 1);
			}
		}
		else if (isTime == true)
		{
			MYSQL_TIME *pTimeValue = (MYSQL_TIME*)m_pBindValues[colIndex].buffer;
			valueStr << MySQLBase::fromMySQLTime(*pTimeValue);
			value = valueStr.str();
		}
		else if (m_pBindValues[colIndex].buffer_type == MYSQL_TYPE_LONG)
		{
			valueStr << *((off_t*)m_pBindValues[colIndex].buffer);
			value = valueStr.str();
		}
		else if (m_pBindValues[colIndex].buffer_type == MYSQL_TYPE_DOUBLE)
		{
			valueStr << *((double*)m_pBindValues[colIndex].buffer);
			value = valueStr.str();
		}
		else
		{
			clog << "Failed to read unknown value type for column " << colIndex << endl;
		}
	}
	else
	{
		clog << "Failed to fetch column " << colIndex << " with error "
			<< mysql_stmt_error(m_pStatement) << endl;
	}

	return value;
}

string MySQLRow::getColumn(unsigned int nColumn) const
{
	if (m_pStatement == NULL)
	{
		if ((nColumn < m_nColumns) &&
			(m_row[nColumn] != NULL))
		{
			return m_row[nColumn];
		}

		return "";
	}

	map<unsigned int, string>::const_iterator valueIter = m_columnValues.find(nColumn);

	if (valueIter == m_columnValues.end())
	{
		return "";
	}

	return valueIter->second;
}

MySQLResults::MySQLResults(MYSQL_RES *results) :
	SQLResults(mysql_num_rows(results), mysql_num_fields(results)),
	m_results(results),
	m_pStatement(NULL),
 	m_pBindValues(NULL),
 	m_pValuesLength(NULL)
{
	// Check we actually have results
	if ((m_results == NULL) ||
		(m_nRows <= 0))
	{
		m_nRows = m_nCurrentRow = 0;
		m_nColumns = 0;
	}
}

MySQLResults::MySQLResults(const string &statementId,
	MYSQL_STMT *pStatement) :
	SQLResults(0, 0),
	m_results(NULL),
	m_pStatement(pStatement),
 	m_pBindValues(NULL),
 	m_pValuesLength(NULL)
{
	bool hasResultSet = false;

	m_results = mysql_stmt_result_metadata(m_pStatement);
	if (m_results != NULL)
	{
		// This will generate a result set
		m_nColumns = mysql_num_fields(m_results);
		hasResultSet = true;
	}

	if (mysql_stmt_execute(m_pStatement) != 0)
	{
		clog << "Statement " << statementId << " failed to execute with error "
			<< mysql_stmt_error(m_pStatement) << endl;
		return;
	}

	if (hasResultSet == false)
	{
		return;
	}
	if (m_nColumns == 0)
	{
		clog << "Statement " << statementId << " has no result column" << endl;
		return;
	}

	my_bool is_not_null = 0;

	m_pBindValues = new MYSQL_BIND[m_nColumns];
	m_pValuesLength = new unsigned long[m_nColumns];

	// Bind results
	for (unsigned int colIndex = 0; colIndex < m_nColumns; ++colIndex)
	{
		MYSQL_FIELD *pField = mysql_fetch_field_direct(m_results, colIndex);

		memset(&m_pBindValues[colIndex], 0, sizeof(MYSQL_BIND));
		m_pValuesLength[colIndex] = 0;

		// Bind should indicate how long this should be
		m_pBindValues[colIndex].buffer = NULL;
		m_pBindValues[colIndex].buffer_length = 0;

		if (pField != NULL)
		{
			m_pBindValues[colIndex].buffer_type = pField->type;

			switch (pField->type)
			{
				case MYSQL_TYPE_FLOAT:
				case MYSQL_TYPE_DOUBLE:
					m_pBindValues[colIndex].buffer_type = MYSQL_TYPE_DOUBLE;
					m_pValuesLength[colIndex] = sizeof(double);
					m_pBindValues[colIndex].buffer = malloc(m_pValuesLength[colIndex]);
					break;
				case MYSQL_TYPE_TIME:
				case MYSQL_TYPE_DATE:
				case MYSQL_TYPE_DATETIME:
				case MYSQL_TYPE_TIMESTAMP:
				case MYSQL_TYPE_NEWDATE:
					m_pValuesLength[colIndex] = sizeof(MYSQL_TIME);
					m_pBindValues[colIndex].buffer = malloc(m_pValuesLength[colIndex]);
					break;
				case MYSQL_TYPE_TINY_BLOB:
				case MYSQL_TYPE_MEDIUM_BLOB:
				case MYSQL_TYPE_LONG_BLOB:
				case MYSQL_TYPE_BLOB:
				case MYSQL_TYPE_VAR_STRING:
				case MYSQL_TYPE_STRING:
					// These are allocated on fetch
					m_pBindValues[colIndex].buffer_type = MYSQL_TYPE_STRING;
					break;
				case MYSQL_TYPE_NULL:
					m_pBindValues[colIndex].buffer = NULL;
					m_pValuesLength[colIndex] = 0;
					break;
				default:
					m_pBindValues[colIndex].buffer_type = MYSQL_TYPE_LONG;
					m_pValuesLength[colIndex] = sizeof(off_t);
					m_pBindValues[colIndex].buffer = malloc(m_pValuesLength[colIndex]);
					break;
			}

			if (m_pValuesLength[colIndex] > 0)
			{
				memset(m_pBindValues[colIndex].buffer, 0, m_pValuesLength[colIndex]);
			}
		}
		else
		{
			clog << "Statement " << statementId
				<< " has unknown type for column " << colIndex << endl;
		}

		m_pBindValues[colIndex].is_null = &is_not_null;
		m_pBindValues[colIndex].length = &m_pValuesLength[colIndex];
		m_pBindValues[colIndex].error = NULL;
	}

	// FIXME: record errors
	if (mysql_stmt_bind_result(m_pStatement, m_pBindValues) != 0)
	{
		clog << "Statement " << statementId << " failed to bind results with error "
			<< mysql_stmt_error(m_pStatement) << endl;
		return;
	}
}

MySQLResults::~MySQLResults()
{
	if (m_results != NULL)
	{
		mysql_free_result(m_results);
	}
	if (m_pStatement != NULL)
	{
		mysql_stmt_reset(m_pStatement);
	}

	if (m_pBindValues != NULL)
	{
		// Free buffers
		for (unsigned int colIndex = 0; colIndex < m_nColumns; ++colIndex)
		{
			switch (m_pBindValues[colIndex].buffer_type)
			{
				case MYSQL_TYPE_FLOAT:
				case MYSQL_TYPE_DOUBLE:
				case MYSQL_TYPE_TIME:
				case MYSQL_TYPE_DATE:
				case MYSQL_TYPE_DATETIME:
				case MYSQL_TYPE_TIMESTAMP:
				case MYSQL_TYPE_NEWDATE:
					if (m_pBindValues[colIndex].buffer != NULL)
					{
						free(m_pBindValues[colIndex].buffer);
					}
					break;
				case MYSQL_TYPE_TINY_BLOB:
				case MYSQL_TYPE_MEDIUM_BLOB:
				case MYSQL_TYPE_LONG_BLOB:
				case MYSQL_TYPE_BLOB:
				case MYSQL_TYPE_VAR_STRING:
				case MYSQL_TYPE_STRING:
					// These are freed with the row
					break;
				case MYSQL_TYPE_NULL:
					// Nothing to free
					break;
				default:
					if (m_pBindValues[colIndex].buffer != NULL)
					{
						free(m_pBindValues[colIndex].buffer);
					}
					break;
			}

			m_pBindValues[colIndex].buffer_length = 0;
			m_pBindValues[colIndex].buffer = NULL;
		}

		delete[] m_pBindValues;
	}
	/*if (m_pValuesLength != NULL)
	{
		delete[] m_pValuesLength;
	}*/
}

bool MySQLResults::hasMoreRows(void) const
{
	bool hasRows = SQLResults::hasMoreRows();

	if (m_pStatement != NULL)
	{
		// We can't really tell
		return true;
	}

	return hasRows;
}

string MySQLResults::getColumnName(unsigned int nColumn) const
{
	if ((nColumn < m_nColumns) &&
		(m_results != NULL))
	{
		MYSQL_FIELD *pField = mysql_fetch_field_direct(m_results, nColumn);
		if ((pField != NULL) &&
			(pField->name != NULL))
		{
			return pField->name;
		}
	}

	return "";
}

SQLRow *MySQLResults::nextRow(void)
{
	if ((m_pStatement == NULL) &&
		(m_results != NULL))
	{
		if (m_nCurrentRow >= m_nRows)
		{
			return NULL;
		}

		MYSQL_ROW row = mysql_fetch_row(m_results);
		if (row == NULL)
		{
			return NULL;
		}
		++m_nCurrentRow;

		return new MySQLRow(row, m_nColumns);
	}

	if ((m_pStatement == NULL) ||
		(m_pBindValues == NULL))
	{
		return NULL;
	}

	int fetchStatus = mysql_stmt_fetch(m_pStatement);

	if ((fetchStatus == 0) ||
		(fetchStatus == MYSQL_DATA_TRUNCATED))
	{
		for (unsigned int colIndex = 0; colIndex < m_nColumns; ++colIndex)
		{
			// What's the actual length ?
			m_pBindValues[colIndex].buffer_length = m_pValuesLength[colIndex] + 1;
		}

		return new MySQLRow(m_pStatement,
			m_pBindValues, m_nColumns);
	}
	else if (fetchStatus != MYSQL_NO_DATA)
	{
		clog << "Statement results fetch failed with error "
			<< mysql_stmt_error(m_pStatement) << endl;
	}

	return NULL;
}

bool MySQLResults::rewind(void)
{
	if (m_pStatement != NULL)
	{
		mysql_stmt_data_seek(m_pStatement, 0);
	}
	m_nCurrentRow = 0;

	return true;
}

pthread_mutex_t MySQLBase::m_initMutex = PTHREAD_MUTEX_INITIALIZER;

MySQLBase::MySQLBase(const string &hostName, const string &databaseName,
	const string &userName, const string &password, bool readOnly) :
	SQLDB(databaseName, readOnly),
	m_hostName(hostName),
	m_userName(userName),
	m_password(password),
	m_isOpen(false)
{
	pthread_mutex_init(&m_mutex, 0);
	open();
}

MySQLBase::~MySQLBase()
{
	close();
	pthread_mutex_destroy(&m_mutex);
}

void MySQLBase::open(void)
{
	if ((m_hostName.empty() == true) ||
		(m_userName.empty() == true) ||
		(m_databaseName.empty() == true))
	{
		return;
	}

	// Initialize
	if (mysql_init(&m_database) == NULL)
	{
		clog << "Couldn't initialize MySQL API" << endl;
		return;
	}

	// Enable automatic reconnections
	my_bool reconnect = 1;
	if (mysql_options(&m_database, MYSQL_OPT_RECONNECT, (const char*)&reconnect) != 0)
	{
		clog << "Couldn't enable reconnections" << endl;
	}

	// Connect to the database
	if (mysql_real_connect(&m_database, m_hostName.c_str(),
		m_userName.c_str(), m_password.c_str(), m_databaseName.c_str(),
		0, "/var/lib/mysql/mysql.sock", 0) == NULL)
	{
		clog << "MySQL error " << mysql_errno(&m_database)
			<< ": " << mysql_error(&m_database) << endl;
		return;
	}

	m_isOpen = true;
}

void MySQLBase::close(void)
{
	if (m_isOpen == true)
	{
		m_isOpen = false;
		for_each(m_statements.begin(), m_statements.end(),
			CloseStatementsFunc());
		m_statements.clear();

		mysql_close(&m_database);
	}
}

string MySQLBase::getUniversalUniqueId(void)
{
	SQLResults *pResults = executeStatement("SELECT UUID();");
	if (pResults == NULL)
	{
		return "";
	}

	// There should only be one row
	SQLRow *pRow = pResults->nextRow();
	if (pRow == NULL)
	{
		delete pResults;
		return "";
	}

	string uuid(pRow->getColumn(0));

	delete pRow;
	delete pResults;

	return uuid;
}

string MySQLBase::escapeString(const string &text)
{
	if ((text.empty() == true) ||
		(m_isOpen == false))
	{
		return "";
	}

	string modText(text);

	// Double up % wildcards so that they are not mistaken for format specifiers in executeStatement()
	string::size_type percentPos = modText.find('%');
	while (percentPos != string::npos)
	{
		modText.replace(percentPos, 1, "%%");

		// Next
		if (percentPos + 2 < modText.length())
		{
			percentPos = modText.find('%', percentPos + 2);
		}
		else
		{
			break;
		}
	}

	char *pEscapedText = new char[(modText.length() * 2) + 1];

	off_t escapedLen = mysql_real_escape_string(&m_database,
		pEscapedText, modText.c_str(), modText.length());

	string escapedText(pEscapedText, escapedLen);

	delete[] pEscapedText;

	return escapedText;
}

bool MySQLBase::isOpen(void) const
{
	return m_isOpen;
}

bool MySQLBase::alterTable(const string &tableName,
	const string &columns, const string &newDefinition)
{
	if ((tableName.empty() == true) ||
		(columns.empty() == true) ||
		(newDefinition.empty() == true))
	{
		return false;
	}

	string sql("BEGIN TRANSACTION; CREATE TEMPORARY TABLE ");
	sql += tableName;
	sql += "_backup (";
	sql += columns;
	sql += "); INSERT INTO ";
	sql += tableName;
	sql += "_backup SELECT ";
	sql += columns;
	sql += " FROM ";
	sql += tableName;
	sql += "; DROP TABLE ";
	sql += tableName;
	sql += "; CREATE TABLE ";
	sql += tableName;
	sql += " (";
	sql += newDefinition;
	sql += "); INSERT INTO ";
	sql += tableName;
	sql += " SELECT ";
	sql += columns;
	sql += " FROM ";
	sql += tableName;
	sql += "_backup; DROP TABLE ";
	sql += tableName;
	sql += "_backup; COMMIT;";
#ifdef DEBUG
	clog << "MySQLBase::alterTable: " << sql << endl;
#endif

	return executeSimpleStatement(sql);
}

bool MySQLBase::beginTransaction(void)
{
	if (m_isOpen == false)
	{
		return false;
	}

	if (mysql_autocommit(&m_database, 0) == 0)
	{
		return true;
	}

	clog << m_databaseName << ": failed to begin transaction" << endl;

	return false;
}

bool MySQLBase::rollbackTransaction(void)
{
	if (m_isOpen == false)
	{
		return false;
	}

	if (mysql_rollback(&m_database) == 0)
	{
		mysql_autocommit(&m_database, 1);

		return true;
	}

	clog << m_databaseName << ": failed to rollback transaction" << endl;

	mysql_autocommit(&m_database, 1);

	return false;
}

bool MySQLBase::endTransaction(void)
{
	if (m_isOpen == false)
	{
		return false;
	}

	if (mysql_commit(&m_database) == 0)
	{
		mysql_autocommit(&m_database, 1);

		return true;
	}

	clog << m_databaseName << ": failed to end transaction" << endl;

	mysql_autocommit(&m_database, 1);

	return false;
}

void MySQLBase::initialize(void)
{
	pthread_mutex_lock(&m_initMutex);

	if (mysql_library_init(0, NULL, NULL) != 0)
	{
		clog << "Failed to initialize MySQL" << endl;
	}

	pthread_mutex_unlock(&m_initMutex);
}

MYSQL_TIME *MySQLBase::toMySQLTime(const string &value,
	enum_mysql_timestamp_type type, bool inGMTime)
{
	if (value.empty() == false)
	{
		time_t aTime = atoi(value.c_str());

		return toMySQLTime(aTime, type, inGMTime);
	}

	return NULL;
}

MYSQL_TIME *MySQLBase::toMySQLTime(time_t aTime,
	enum_mysql_timestamp_type type, bool inGMTime)
{
	struct tm *pTimeTm = new struct tm;

	if (((inGMTime == true) &&
#ifdef HAVE_GMTIME_R
		(gmtime_r(&aTime, pTimeTm) != NULL)
#else
		((pTimeTm = gmtime(&aTime)) != NULL)
#endif
		) ||
#ifdef HAVE_LOCALTIME_R
		(localtime_r(&aTime, pTimeTm) != NULL)
#else
		((pTimeTm = localtime(&aTime)) != NULL)
#endif
		)
	{
		MYSQL_TIME *pMySQLTime = new MYSQL_TIME;

		pMySQLTime->year = pTimeTm->tm_year + 1900;
		pMySQLTime->month = pTimeTm->tm_mon + 1;
		pMySQLTime->day = pTimeTm->tm_mday;
		pMySQLTime->hour = pTimeTm->tm_hour;
		pMySQLTime->minute = pTimeTm->tm_min;
		pMySQLTime->second = pTimeTm->tm_sec;
		pMySQLTime->second_part = 0;
		pMySQLTime->neg = 0;
		pMySQLTime->time_type = type;

		delete pTimeTm;

		return pMySQLTime;
	}
	delete pTimeTm;

	return NULL;
}

time_t MySQLBase::fromMySQLTime(MYSQL_TIME time, bool inGMTime)
{
	struct tm timeTm;
	time_t epoch;

        timeTm.tm_sec = time.second;
	timeTm.tm_min = time.minute;
	timeTm.tm_hour = time.hour;
	timeTm.tm_mday = time.day;
        timeTm.tm_mon = time.month - 1;
	timeTm.tm_year = time.year - 1900;
	timeTm.tm_wday = timeTm.tm_yday = timeTm.tm_isdst = 0;

        if (inGMTime == true)
        {
                epoch = TimeConverter::timegm(&timeTm);
        }
        else
        {
                epoch = mktime(&timeTm);
        }

        return epoch;
}

bool MySQLBase::executeSimpleStatement(const string &sql)
{
	if ((sql.empty() == true) ||
		(m_isOpen == false))
	{
		return false;
	}

	vector<string> statements;

	parseSeparatedList(sql, ";", statements);

	for (vector<string>::const_iterator statIter = statements.begin();
		statIter != statements.end(); ++statIter)
	{
		string statement(trimSpaces(*statIter));

		clog << "Simple SQL statement <" << statement << ">" << endl;

		pthread_mutex_lock(&m_mutex);
		if (mysql_query(&m_database, statement.c_str()))
		{
			unsigned int errorCode = mysql_errno(&m_database);

			clog << "SQL statement <" << statement << "> failed with error "
				<< errorCode << ": " << mysql_error(&m_database) << endl;
			pthread_mutex_unlock(&m_mutex);

			return false;
		}

		MYSQL_RES *pResult = mysql_store_result(&m_database);
		if (pResult != NULL)
		{
			mysql_free_result(pResult);
		}
		pthread_mutex_unlock(&m_mutex);
	}

	return true;
}

SQLResults *MySQLBase::executeStatement(const char *sqlFormat, ...)
{
	MySQLResults *pResults = NULL;
	char stringBuff[2048];
	va_list ap;

	if ((sqlFormat == NULL) ||
		(m_isOpen == false))
	{
		return NULL;
	}

	va_start(ap, sqlFormat);
	int numChars = vsnprintf(stringBuff, 2048, sqlFormat, ap);
	if (numChars <= 0)
	{
#ifdef DEBUG
		clog << "MySQLBase::executeStatement: couldn't format statement" << endl;
#endif
		return NULL;
	}
	if (numChars >= 2048)
	{
		// Not enough space
#ifdef DEBUG
		clog << "MySQLBase::executeStatement: not enough space (" << numChars << ")" << endl;
#endif
		return NULL;
	}
	stringBuff[numChars] = '\0';
	va_end(ap);

	pthread_mutex_lock(&m_mutex);
	if (mysql_query(&m_database, stringBuff))
	{
		unsigned int errorCode = mysql_errno(&m_database);

		clog << "SQL statement <" << stringBuff << "> failed with error "
			<< errorCode << ": " << mysql_error(&m_database) << endl;
		pthread_mutex_unlock(&m_mutex);

		return NULL;
	}

	MYSQL_RES *pResult = mysql_store_result(&m_database);
	if (pResult != NULL)
	{
		pResults = new MySQLResults(pResult);
	}
	pthread_mutex_unlock(&m_mutex);

	return pResults;
}

SQLResults *MySQLBase::executeStatement(const string &sqlFormat,
	off_t min, off_t max)
{
	if (sqlFormat.empty() == true)
	{
		return NULL;
	}

	stringstream paginationStatement;
	paginationStatement << sqlFormat;
	paginationStatement << " LIMIT ";
	paginationStatement << max - min;
	paginationStatement << " OFFSET ";
	paginationStatement << min;
	paginationStatement << ";";

	// Call overload
	return executeStatement(paginationStatement.str().c_str());
}

bool MySQLBase::prepareStatement(const string &statementId,
	const string &sqlFormat)
{
	if ((sqlFormat.empty() == true) ||
		(m_isOpen == false))
	{
		return false;
	}

	map<string, MYSQL_STMT*>::iterator statIter = m_statements.find(statementId);
	if (statIter != m_statements.end())
	{
		return true;
	}

	MYSQL_STMT *pStatement = mysql_stmt_init(&m_database);

	if (pStatement == NULL)
	{
		return false;
	}

	if (mysql_stmt_prepare(pStatement,
		sqlFormat.c_str(),
		(unsigned long)sqlFormat.length()) == 0)
	{
		m_statements.insert(pair<string, MYSQL_STMT*>(statementId, pStatement));

		return true;
	}

	clog << m_databaseName << ": failed to compile SQL statement " << statementId << endl;

	return false;
}

SQLResults *MySQLBase::executePreparedStatement(const string &statementId,
	const vector<string> &values)
{
	vector<pair<string, SQLRow::SQLType> > typedValues;

	for(vector<string>::const_iterator valueIter = values.begin();
		valueIter != values.end(); ++valueIter)
	{
		typedValues.push_back(pair<string, SQLRow::SQLType>(*valueIter,
			SQLRow::SQL_TYPE_STRING));
	}

	return executePreparedStatement(statementId, typedValues);
}

SQLResults *MySQLBase::executePreparedStatement(const string &statementId,
	const vector<pair<string, SQLRow::SQLType> > &values)
{
	map<string, MYSQL_STMT*>::iterator statIter = m_statements.find(statementId);
	if ((statIter == m_statements.end()) ||
		(statIter->second == NULL))
	{
#ifdef DEBUG
		clog << "MySQLBase::executePreparedStatement: invalid SQL statement ID " << statementId << endl;
#endif
		return NULL;
	}

	unsigned long paramCount = mysql_stmt_param_count(statIter->second);
	if (paramCount != (unsigned long)values.size())
	{
		clog << "Statement " << statementId << " expected " << paramCount
			<< " values, got " << values.size() << endl;
		return NULL;
	}

	MYSQL_BIND bindValues[values.size()];
	unsigned long valuesLength[values.size()];
	unsigned int paramIndex = 0;
	my_bool is_not_null = 0;

	memset(bindValues, 0, sizeof(bindValues));
	memset(valuesLength, 0, sizeof(valuesLength));

	// Bind parameters
	for (vector<pair<string, SQLRow::SQLType> >::const_iterator valueIter = values.begin();
		valueIter != values.end(); ++valueIter, ++paramIndex)
	{
		enum_field_types type;

		switch (valueIter->second)
		{
			case SQLRow::SQL_TYPE_INT:
				type = MYSQL_TYPE_LONG;
				valuesLength[paramIndex] = sizeof(off_t);
				bindValues[paramIndex].buffer = new off_t((off_t)atoll(valueIter->first.c_str()));
				break;
			case SQLRow::SQL_TYPE_DOUBLE:
				type = MYSQL_TYPE_DOUBLE;
				valuesLength[paramIndex] = 8;
				bindValues[paramIndex].buffer = new double(atof(valueIter->first.c_str()));
				break;
			case SQLRow::SQL_TYPE_TIME:
				type = MYSQL_TYPE_TIME;
				valuesLength[paramIndex] = sizeof(MYSQL_TIME);
				bindValues[paramIndex].buffer = (void*)toMySQLTime(valueIter->first, MYSQL_TIMESTAMP_TIME);
				break;
			case SQLRow::SQL_TYPE_DATE:
				type = MYSQL_TYPE_DATE;
				valuesLength[paramIndex] = sizeof(MYSQL_TIME);
				bindValues[paramIndex].buffer = (void*)toMySQLTime(valueIter->first, MYSQL_TIMESTAMP_DATE);
				break;
			case SQLRow::SQL_TYPE_DATETIME:
				type = MYSQL_TYPE_DATETIME;
				valuesLength[paramIndex] = sizeof(MYSQL_TIME);
				bindValues[paramIndex].buffer = (void*)toMySQLTime(valueIter->first, MYSQL_TIMESTAMP_DATETIME);
				break;
			case SQLRow::SQL_TYPE_TIMESTAMP:
				type = MYSQL_TYPE_TIMESTAMP;
				valuesLength[paramIndex] = sizeof(MYSQL_TIME);
				bindValues[paramIndex].buffer = (void*)toMySQLTime(valueIter->first, MYSQL_TIMESTAMP_DATE);
				break;
			case SQLRow::SQL_TYPE_BLOB:
				type = MYSQL_TYPE_BLOB;
				valuesLength[paramIndex] = valueIter->first.length();
				bindValues[paramIndex].buffer = (void*)strdup(valueIter->first.c_str());
				break;
			case SQLRow::SQL_TYPE_NULL:
				type = MYSQL_TYPE_NULL;
				valuesLength[paramIndex] = 0;
				bindValues[paramIndex].buffer = NULL;
				break;
			default:
				type = MYSQL_TYPE_STRING;
				valuesLength[paramIndex] = valueIter->first.length();
				bindValues[paramIndex].buffer = (void*)strdup(valueIter->first.c_str());
				break;
		}

		bindValues[paramIndex].buffer_type = type;
		bindValues[paramIndex].is_null = &is_not_null;
		bindValues[paramIndex].length = &valuesLength[paramIndex];
		bindValues[paramIndex].error = NULL;
	}

	MySQLResults *pResults = NULL;

	if (mysql_stmt_bind_param(statIter->second, bindValues) != 0)
	{
		clog << m_databaseName << ": failed to bind parameter to statement "
			<< statementId << " with error "
			<< mysql_stmt_error(statIter->second) << endl;
	}
	else
	{
		pResults = new MySQLResults(statIter->first,
			statIter->second);
	}

	paramIndex = 0;

	// Free buffers
	for (vector<pair<string, SQLRow::SQLType> >::const_iterator valueIter = values.begin();
		valueIter != values.end(); ++valueIter, ++paramIndex)
	{
		if (bindValues[paramIndex].buffer == NULL)
		{
			continue;
		}

		switch (valueIter->second)
		{
			case SQLRow::SQL_TYPE_INT:
				delete (off_t*)bindValues[paramIndex].buffer;
				break;
			case SQLRow::SQL_TYPE_DOUBLE:
				delete (double*)bindValues[paramIndex].buffer;
				break;
			case SQLRow::SQL_TYPE_TIME:
				delete (MYSQL_TIME*)bindValues[paramIndex].buffer;
				break;
			case SQLRow::SQL_TYPE_DATE:
				delete (MYSQL_TIME*)bindValues[paramIndex].buffer;
				break;
			case SQLRow::SQL_TYPE_DATETIME:
				delete (MYSQL_TIME*)bindValues[paramIndex].buffer;
				break;
			case SQLRow::SQL_TYPE_TIMESTAMP:
				delete (MYSQL_TIME*)bindValues[paramIndex].buffer;
				break;
			case SQLRow::SQL_TYPE_BLOB:
			case SQLRow::SQL_TYPE_STRING:
				if (bindValues[paramIndex].buffer != NULL)
				{
					free(bindValues[paramIndex].buffer);
				}
				break;
			case SQLRow::SQL_TYPE_NULL:
				break;
		}
	}

	return pResults;
}

