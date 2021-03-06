#pragma once

#include "../axond_module.hpp"
#include "dbengine.hpp"

class SafeDb
{
public:
	SafeDb(DbEngine& dbEngine);
	~SafeDb();

	void safeDbExport(CapidModuleSafeDb& db, PluginDataModel& pd);

	bool safeCreateTable(std::string table, CapidModuleSafeDb::DbList fields, CapidModuleSafeDb::DbList options, bool ifExists);
	uint64_t safeTableCount(std::string table, CapidModuleSafeDb::DbList conditions);
	uint64_t safeGetId(std::string table, CapidModuleSafeDb::DbList conditions);
	uint64_t safeInsert(std::string table, CapidModuleSafeDb::DbList fields);
	uint64_t safeUpdate(std::string table, uint64_t id, CapidModuleSafeDb::DbList fields);
	uint64_t safeUpdateMulti(std::string table, CapidModuleSafeDb::DbList fields, CapidModuleSafeDb::DbList conditions);
	uint64_t safeDestroy(std::string table, uint64_t id);
	uint64_t safeDestroyMulti(std::string table, CapidModuleSafeDb::DbList conditions);
	uint64_t safeAlterTable(std::string table, CapidModuleSafeDb::DbList settings);

	std::list<std::map<std::string, std::string> > safeGetData(std::string table, std::string fields, CapidModuleSafeDb::DbList conditions, uint64_t first, uint64_t count, std::string group, std::string order);
	std::string getCurrentPluginVersion(std::string plugin);
	std::string getPluginValue(std::string plugin, std::string key);
	bool setPluginValue(std::string plugin, std::string key, std::string value);
private:
	DbEngine& engine;
};
