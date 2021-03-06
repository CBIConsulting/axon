#pragma once

#include <string>
#include <sqlite3.h>
#include <list>
#include <map>

class SqliteBase
{
 public:
  SqliteBase(std::string filename);
  SqliteBase();
  void setFileName(std::string filename);
  void initialize();
  void shutdown();
  virtual ~SqliteBase();
  virtual void tragic(std::string error);
  virtual void error(int code, int level, std::string error);
  void createTable(std::string name, std::list<std::pair<std::string, std::string> > fields, std::list<std::pair<std::string, std::string> > options = {}, bool ifNotExists=false);
	unsigned tableCount(std::string table, std::list <std::pair < std::string, std::string> > conditions = {}, std::string extra="");
	long unsigned getRowid(std::string table, std::list <std::pair < std::string, std::string> > conditions);
  long unsigned insert(std::string table, std::map<std::string, std::string> fields, bool clean=true);
  long unsigned update(std::string table, long unsigned rowid, std::map<std::string, std::string> fields, bool clean=true);
	long unsigned destroy(std::string table, long unsigned rowid);
  long unsigned destroy(std::string table, std::list <std::pair < std::string, std::string> > conditions, std::string extra="");
  std::list <std::map < std::string, std::string > > getData(std::string table, std::string fields, std::list <std::pair < std::string, std::string> > conditions, std::string extra="");
	long unsigned alterTable(std::string table, std::list<std::pair<std::string, std::string>> settings);
  std::list <std::map < std::string, std::string > > tableInfo(std::string table);
 protected:
  bool initialized;
  std::string filename;

 private:
  sqlite3* db;
};
