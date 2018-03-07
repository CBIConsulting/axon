/**
*************************************************************
* @file sqlitebase.cpp
* @brief Breve descripción
* Pequeña documentación del archivo
*
*
*
*
*
* @author Gaspar Fernández <blakeyed@totaki.com>
* @version
* @date 31 mar 2015
* Historial de cambios:
*
*
*
*
*
*
*
*************************************************************/

#include "sqlitebase.hpp"
#include "../basic_exception.hpp"

#include <iostream>

namespace
{
	enum FieldType
		{
			NONE,
			NUMBER,
			FLOAT,
			DATETIME,
			TEXT
		};
	struct FieldDescription
	{
		FieldType type;
		bool notNull;
	};

	std::map<std::string, std::map<std::string, FieldDescription>> fieldsCache;

	FieldType parseType(std::string & type)
	{
		if (type == "NUMBER")
			return NUMBER;
		else if (type == "DATETIME")
			return DATETIME;
		else if (type == "FLOAT")
			return FLOAT;
		else
			return TEXT;
	}

	void tragicError(std::string error)
	{
		std::cerr << "TRAGIC SQLITE ERROR: "<<error<<std::endl;
		exit(-1);
	}

	bool ends_with(std::string const & value, const std::string & ending)
	{
    if (ending.size() > value.size()) return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
	}

	FieldDescription _fieldType(sqlite3* db, std::string table, std::string field)
	{
		std::string querystr = "PRAGMA table_info("+table+")";
		sqlite3_stmt *query;

		if (sqlite3_prepare_v2(db, querystr.c_str(), -1, &query, NULL) != SQLITE_OK)
			tragicError("Could not prepare SQLite statement: "+querystr);

		int res;
		std::map<std::string, FieldDescription> fieldsdata;
		while ( (res = sqlite3_step(query)) == SQLITE_ROW)
			{
				std::string fieldName;
				FieldDescription fieldDescription;
				for (int i=0; i< sqlite3_column_count(query); ++i)
					{
						std::string colname = sqlite3_column_name(query, i);
						auto ptr = sqlite3_column_text(query,i);
						std::string colval = (ptr!=NULL)?(char*)ptr:"";
						if (colname == "name")
							fieldName = colval;
						else if (colname =="type")
							fieldDescription.type = parseType(colval);
						else if (colname =="notnull")
							fieldDescription.notNull = (colval=="1");
					}
				if (!fieldName.empty())
					fieldsdata.insert({fieldName, fieldDescription});
			}

		if (res==SQLITE_ERROR)
			tragicError("Error executing query: "+querystr);

		fieldsCache.insert({table, fieldsdata});

		sqlite3_finalize(query);

		return fieldsCache[table][field];
	}

	FieldDescription fieldType(sqlite3* db, std::string table, std::string field)
	{
		auto tableData = fieldsCache.find(table);
		if (tableData == fieldsCache.end())
			return _fieldType(db, table, field);

		auto fieldData = tableData->second.find(field);
		if (fieldData == tableData->second.end())
			return {NONE};
			/* tragicError("Field "+field+" not found"); */

		return fieldData->second;
	}

	bool isnum (std::string& str)
	{
		char *endptr;
		auto num = strtoll(str.c_str(), &endptr, 10);

		return (*endptr ==0);
	}

	std::string parseConditions(std::list <std::pair < std::string, std::string> >& conditions)
	{
		std::string querystr;
		if (conditions.size()>0)
			{
				querystr+="WHERE ";
				bool first=true;
				for (auto &c : conditions)
					{
						std::string condType="=";
						auto lastc = c.first.back();
						switch (lastc)
							{
							case '=':
								c.first.pop_back();
								lastc = c.first.back();
								if ( (lastc=='>') || (lastc=='>') )
									{
										c.first.pop_back();
										condType.insert(0, 1, lastc);
									}
								break;
							case '>':
								c.first.pop_back();
								condType = lastc;
								break;
							case '<':
								c.first.pop_back();
								condType = lastc;

								lastc = c.first.back();
								if (lastc=='>')
									{
										c.first.pop_back();
										condType="<>";
									}
								break;
							}
						if (ends_with(c.first, " LIKE"))
							{
								condType=" LIKE ";
								c.first.erase(c.first.find(" LIKE"));
							}

						if (c.first[0] == '|')
							querystr+="OR "+c.first.substr(1);
						else
							{
								if (!first)
									querystr+="AND ";
								querystr+=c.first;
							}
						querystr+=condType;
						if ( (c.second.length()>2) && (c.second.substr(0,2)=="F(") )
							querystr+=c.second.substr(2);
						else if (isnum(c.second))
							{
								querystr+=c.second+" ";
								c.second="F(";	/* Workaround */
							}
						else
							querystr+="? ";


						first=false;
					}
			}
		return querystr;
	}
};

SqliteBase::SqliteBase(std::string filename):SqliteBase()
{
  this->filename = filename;
}

SqliteBase::~SqliteBase()
{
}

SqliteBase::SqliteBase():initialized(false)
{
}

void SqliteBase::setFileName(std::string filename)
{
  this->filename = filename;
}

void SqliteBase::initialize()
{
  if (sqlite3_initialize() != SQLITE_OK)
    tragic("Sqlite3 can't be initialized");

  if (sqlite3_open_v2(filename.c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL) != SQLITE_OK)
    tragic("Sqlite3 file "+std::string(filename.c_str())+" can't be opened");

  initialized = true;
}

void SqliteBase::shutdown()
{
  if (!initialized)
    return;

  sqlite3_close_v2(db);
}

void SqliteBase::tragic(std::string error)
{
	tragicError(error);
}

void SqliteBase::error(int code, int level, std::string error)
{
  std::cerr << "SQLITE ERROR ("<<level<<"): "<<error<<std::endl;
}

void SqliteBase::createTable(std::string name, std::list<std::pair<std::string, std::string> > fields, std::list<std::pair<std::string, std::string> > options, bool ifNotExists)
{
  char *error = NULL;
  std::string query = "CREATE TABLE ";
  if (ifNotExists)
    query+="IF NOT EXISTS ";

  query+=name+" (";
  bool first=true;
  for (auto v: fields)
    {
      if (!first)
				query+=",\n";
      query+=v.first+" "+v.second;
      first=false;
    }
  query+=")";

  if (sqlite3_exec(db, query.c_str(), NULL, 0, &error) != SQLITE_OK)
    throw SQLiteException(1, "Error creating table: "+std::string(error));
}

long unsigned SqliteBase::update(std::string table, long unsigned rowid, std::map<std::string, std::string> fields, bool clean)
{
  std::string querystr, values;
  querystr = "UPDATE "+table+" SET ";
	bool first = true;
  for (auto f : fields)
    {
      if (!first)
				{
					querystr+=", ";
				}

			first = false;

      querystr=querystr+f.first+"=";
			/* Functions and so */
			if ( (f.second.length()>2) && (f.second.substr(0,2)=="F(") )
				querystr+=f.second.substr(2);
			else
				querystr+="?";
    }
	if ( (fieldType(db, table, "mtime").type == DATETIME) && (fields.find("mtime")==fields.end()) )
		{
			querystr+=", mtime=DATETIME('now')";
		}

  querystr+=" WHERE ROWID="+std::to_string(rowid);
  sqlite3_stmt *query;

  if (sqlite3_prepare_v2(db, querystr.c_str(), -1, &query, NULL) != SQLITE_OK)
    throw SQLiteException(2, "Could not prepare SQLite statement: "+querystr);

  unsigned order=1;
  for (auto f : fields)
    {
      int res;
			if ( (f.second.length()>2) && (f.second.substr(0,2)=="F(") )
				continue;
			try
				{
					if (fieldType(db, table, f.first).type == NUMBER)
						{
							res = sqlite3_bind_int(query, order++, std::stoi(f.second));
						}
					else
						if (fieldType(db, table, f.first).type == FLOAT)
						{
							res = sqlite3_bind_double(query, order++, std::stod(f.second));
						}
					else
						{
							res = sqlite3_bind_text(query, order++, f.second.c_str(), f.second.length(), SQLITE_TRANSIENT);
						}
				}
			catch (std::exception& e)
				{
					/* Invalid conversion */
					return 0;
				}
      if (res != SQLITE_OK)
				throw SQLiteException(3, "Failed binding value into prepared statement. Query: "+querystr);
    }

  int res = res = sqlite3_step(query);

  if (res!=SQLITE_DONE)
    throw SQLiteException(4, "Error executing query: "+querystr);

  sqlite3_finalize(query);

  return rowid;
}

long unsigned SqliteBase::insert(std::string table, std::map<std::string, std::string> fields, bool clean)
{
  std::string querystr, values;
  querystr = "INSERT INTO "+table+" (";

  for (auto f : fields)
    {
      if (!values.empty())
				{
					querystr+=", ";
					values+=", ";
				}
      querystr+=f.first;
			/* Functions and so */
			if ( (f.second.length()>2) && (f.second.substr(0,2)=="F(") )
				values+=f.second.substr(2);
			else
				values+="?";
    }
	/* Auto control fields */
	if ( (fieldType(db, table, "ctime").type == DATETIME) && (fields.find("ctime")==fields.end()) )
		{
			querystr+=", ctime";
			values+=", DATETIME('now')";
		}
	if ( (fieldType(db, table, "mtime").type == DATETIME) && (fields.find("mtime")==fields.end()) )
		{
			querystr+=", mtime";
			values+=", DATETIME('now')";
		}
	/* Auto control fields */
  querystr+=") VALUES ("+values+")";
  sqlite3_stmt *query;

  if (sqlite3_prepare_v2(db, querystr.c_str(), -1, &query, NULL) != SQLITE_OK)
    throw SQLiteException(5, "Could not prepare SQLite statement: "+querystr);

  unsigned order=1;
  for (auto f : fields)
    {
      int res;
			if ( (f.second.length()>2) && (f.second.substr(0,2)=="F(") )
				continue;
			try
				{
					if (fieldType(db, table, f.first).type == NUMBER)
						{
							res = sqlite3_bind_int(query, order++, std::stoi(f.second));
						}
					else
						if (fieldType(db, table, f.first).type == FLOAT)
						{
							res = sqlite3_bind_double(query, order++, std::stod(f.second));
						}
					else
						{
							res = sqlite3_bind_text(query, order++, f.second.c_str(), f.second.length(), SQLITE_TRANSIENT);
						}
				}
			catch (std::exception& e)
				{
					/* Invalid conversion */
					return 0;
				}
      if (res != SQLITE_OK)
				throw SQLiteException(6, "Failed binding value into prepared statement. Query: "+querystr);
    }

  int res = res = sqlite3_step(query);
  if (res!=SQLITE_DONE)
    throw SQLiteException(7, "Error executing query: "+querystr);

  long unsigned insertId = sqlite3_last_insert_rowid(db);
  sqlite3_finalize(query);

  return insertId;
}

long unsigned SqliteBase::getRowid(std::string table, std::list <std::pair < std::string, std::string> > conditions)
{
	auto res = getData(table, "rowid", conditions, "LIMIT 1");
	if (res.empty())
		return 0;

	auto total = res.front().find("rowid");
	if (total == res.front().end())
		return 0;

	return std::stoi(total->second);
}

unsigned SqliteBase::tableCount(std::string table, std::list <std::pair < std::string, std::string> > conditions, std::string extra)
{
	auto res = getData(table, "COUNT(ROWID) AS total", conditions, extra);
	if (res.empty())
		return 0;

	auto total = res.front().find("total");
	if (total == res.front().end())
		return 0;

	return std::stoi(total->second);
}

long unsigned SqliteBase::destroy(std::string table, long unsigned rowid)
{
  std::string querystr = "DELETE FROM "+table+" WHERE rowid="+std::to_string(rowid);
  sqlite3_stmt *query;

  if (sqlite3_prepare_v2(db, querystr.c_str(), -1, &query, NULL) != SQLITE_OK)
    throw SQLiteException(8, "Could not prepare SQLite statement: "+querystr);

  int res = res = sqlite3_step(query);
  if (res!=SQLITE_DONE)
    throw SQLiteException(9, "Error executing query: "+querystr);

  sqlite3_finalize(query);
	return 1;
}

long unsigned SqliteBase::destroy(std::string table, std::list <std::pair < std::string, std::string> > conditions, std::string extra)
{
  std::string querystr = "DELETE FROM "+table+" ";

	querystr+=::parseConditions(conditions);

  querystr+=extra;

  sqlite3_stmt *query;

  if (sqlite3_prepare_v2(db, querystr.c_str(), -1, &query, NULL) != SQLITE_OK)
    throw SQLiteException(10, "Could not prepare SQLite statement: "+querystr);

	unsigned order=1;
  for (auto f : conditions)
    {
      int res;

			if ( (f.second.length()>=2) && (f.second.substr(0,2)=="F(") )
				continue;

			res = sqlite3_bind_text(query, order++, f.second.c_str(), -1, SQLITE_TRANSIENT);
      if (res != SQLITE_OK)
				throw SQLiteException(14, "Failed binding value into prepared statement. Query: "+querystr);
    }

  int res = res = sqlite3_step(query);
  if (res!=SQLITE_DONE)
    throw SQLiteException(12, "Error executing query: "+querystr);

  sqlite3_finalize(query);
	return 1;
}

std::list <std::map < std::string, std::string > > SqliteBase::getData(std::string table, std::string fields, std::list <std::pair < std::string, std::string> > conditions, std::string extra)
{
  std::string querystr = "SELECT "+fields+" FROM "+table+" ";

	querystr+=::parseConditions(conditions);

  querystr+=extra;
  sqlite3_stmt *query;

  if (sqlite3_prepare_v2(db, querystr.c_str(), -1, &query, NULL) != SQLITE_OK)
		throw SQLiteException(13, "Could not prepare SQLite statement: "+querystr);

  unsigned order=1;
  for (auto f : conditions)
    {
      int res;

			if ( (f.second.length()>=2) && (f.second.substr(0,2)=="F(") )
				continue;

			res = sqlite3_bind_text(query, order++, f.second.c_str(), -1, SQLITE_TRANSIENT);
      if (res != SQLITE_OK)
				throw SQLiteException(14, "Failed binding value into prepared statement. Query: "+querystr);
    }

  int res;
  std::list <std::map < std::string, std::string > > resdata;

  while ( (res = sqlite3_step(query)) == SQLITE_ROW)
    {
      std::map < std::string, std::string > row;

      for (int i=0; i< sqlite3_column_count(query); ++i)
				{
					switch (sqlite3_column_type(query, i))
						{
						case SQLITE_INTEGER:
						case SQLITE_FLOAT:
							row.insert({sqlite3_column_name(query, i), std::to_string(sqlite3_column_double(query,i))});
							break;
						case SQLITE_TEXT:
							row.insert({sqlite3_column_name(query, i), std::string((const char*)sqlite3_column_text(query,i))});
							break;
						}
				}
      resdata.push_back(row);
    }

  if (res==SQLITE_ERROR)
    throw SQLiteException(15, "Error executing query: "+querystr);

	sqlite3_finalize(query);

  return resdata;
}

std::list <std::map < std::string, std::string > > SqliteBase::tableInfo(std::string table)
{
  std::string querystr = "PRAGMA table_info("+table+")";

  sqlite3_stmt *query;

  if (sqlite3_prepare_v2(db, querystr.c_str(), -1, &query, NULL) != SQLITE_OK)
		throw SQLiteException(13, "Could not prepare SQLite statement: "+querystr);

  int res;
  std::list <std::map < std::string, std::string > > resdata;

  while ( (res = sqlite3_step(query)) == SQLITE_ROW)
    {
      std::map < std::string, std::string > row;

      for (int i=0; i< sqlite3_column_count(query); ++i)
				{
					switch (sqlite3_column_type(query, i))
						{
						case SQLITE_INTEGER:
						case SQLITE_FLOAT:
							row.insert({sqlite3_column_name(query, i), std::to_string(sqlite3_column_double(query,i))});
							break;
						case SQLITE_TEXT:
							row.insert({sqlite3_column_name(query, i), std::string((const char*)sqlite3_column_text(query,i))});
							break;
						}
				}
      resdata.push_back(row);
    }

  if (res==SQLITE_ERROR)
    throw SQLiteException(15, "Error executing query: "+querystr);

	sqlite3_finalize(query);

  return resdata;
}

long unsigned SqliteBase::alterTable(std::string table, std::list<std::pair<std::string, std::string>> settings)
{
  std::string querystr, values;

	for (auto s: settings)
		{
			querystr = "ALTER TABLE "+table+" "+s.first+" "+s.second;
			sqlite3_stmt *query;

			if (sqlite3_prepare_v2(db, querystr.c_str(), -1, &query, NULL) != SQLITE_OK)
				throw SQLiteException(5, "Could not prepare SQLite statement: "+querystr);

			int res = res = sqlite3_step(query);
			if (res!=SQLITE_DONE)
				throw SQLiteException(7, "Error executing query: "+querystr);
			sqlite3_finalize(query);
		}


  return 1;
}
