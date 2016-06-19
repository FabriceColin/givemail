/*
 *  Copyright 2009-2014 Fabrice Colin
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

#ifndef _MYSQLBASE_H_
#define _MYSQLBASE_H_

#include <mysql/mysql.h>
#include <pthread.h>
#include <string>
#include <map>
#include <vector>
#include <utility>

#include "SQLDB.h"

/// A row of results.
class MySQLRow : public SQLRow
{
	public:
		MySQLRow(MYSQL_ROW row, unsigned int nColumns);
		MySQLRow(MYSQL_STMT *pStatement,
			MYSQL_BIND *pBindValues,
			unsigned int nColumns);
		virtual ~MySQLRow();

		virtual std::string getColumn(unsigned int nColumn) const;

	protected:
		MYSQL_ROW m_row;
		MYSQL_STMT *m_pStatement;
		MYSQL_BIND *m_pBindValues;
		std::map<unsigned int, std::string> m_columnValues;

		std::string getColumnValue(unsigned int nColumn);

	private:
		MySQLRow(const MySQLRow &other);
		MySQLRow &operator=(const MySQLRow &other);

};

/// Results extracted from a MySQL table.
class MySQLResults : public SQLResults
{
	public:
		MySQLResults(MYSQL_RES *results);
		MySQLResults(const std::string &statementId,
			MYSQL_STMT *pStatement);
		virtual ~MySQLResults();

		virtual bool hasMoreRows(void) const;

		virtual std::string getColumnName(unsigned int nColumn) const;

		virtual SQLRow *nextRow(void);

		virtual bool rewind(void);

	protected:
		MYSQL_RES *m_results;
		MYSQL_STMT *m_pStatement;
		MYSQL_BIND *m_pBindValues;
		unsigned long *m_pValuesLength;

	private:
		MySQLResults(const MySQLResults &other);
		MySQLResults &operator=(const MySQLResults &other);

};

/// Simple C++ wrapper around the MySQL API.
class MySQLBase : public SQLDB
{
	public:
		MySQLBase(const std::string &hostName,
			const std::string &databaseName,
			const std::string &userName,
			const std::string &password,
			bool readOnly = false);
		virtual ~MySQLBase();

		static void initialize(void);

		static MYSQL_TIME *toMySQLTime(const std::string &value,
			enum_mysql_timestamp_type type,
			bool inGMTime = false);

		static MYSQL_TIME *toMySQLTime(time_t aTime,
			enum_mysql_timestamp_type type,
			bool inGMTime = false);

		static time_t fromMySQLTime(MYSQL_TIME time,
			bool inGMTime = false);

		virtual std::string getUniversalUniqueId(void);

		virtual std::string escapeString(const std::string &text);

		virtual bool isOpen(void) const;

		virtual bool alterTable(const std::string &tableName,
			const std::string &columns,
			const std::string &newDefinition);

		virtual bool beginTransaction(void);

		virtual bool rollbackTransaction(void);

		virtual bool endTransaction(void);

		virtual bool executeSimpleStatement(const std::string &sql);

		virtual SQLResults *executeStatement(const char *sqlFormat, ...);

		virtual SQLResults *executeStatement(const std::string &sqlFormat,
			off_t min, off_t max);

		virtual bool prepareStatement(const std::string &statementId,
			const std::string &sqlFormat);

		virtual SQLResults *executePreparedStatement(const std::string &statementId,
			const std::vector<std::string> &values);

		virtual SQLResults *executePreparedStatement(const std::string &statementId,
			const std::vector<std::pair<std::string, SQLRow::SQLType> > &values);

	protected:
		static pthread_mutex_t m_initMutex;
		pthread_mutex_t m_mutex;
		std::string m_hostName;
		std::string m_userName;
		std::string m_password;
		MYSQL m_database;
		bool m_isOpen;
		std::map<std::string, MYSQL_STMT*> m_statements;

		void open(void);

		void close(void);

	private:
		MySQLBase(const MySQLBase &other);
		MySQLBase &operator=(const MySQLBase &other);

};

#endif // _MYSQLBASE_H_
