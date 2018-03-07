/* Collection of DB methods.
	 EXCEPTION FREE (to use in C modules)
	 TABLE SAFE (to avoid modules overwriting critic data)
 */
#include "safedb.hpp"
#include "../basic_exception.hpp"
#include <iostream>

namespace
{
	static std::vector<std::string> protectedTables = {
		"USER",
		"GROUPS",
		"USERGROUP",
		"PLUGINDATA"
	};
};

SafeDb::SafeDb(DbEngine& dbEngine):engine(dbEngine)
{
}

SafeDb::~SafeDb()
{
}

void SafeDb::safeDbExport(CapidModuleSafeDb& db, PluginDataModel& pd)
{
	namespace ph = std::placeholders;
	db.createTable = std::bind(&SafeDb::safeCreateTable, this, ph::_1, ph::_2, ph::_3, ph::_4);
	db.tableCount = std::bind(&SafeDb::safeTableCount, this, ph::_1, ph::_2);
	db.getId = std::bind(&SafeDb::safeGetId, this, ph::_1, ph::_2);
	db.insert = std::bind(&SafeDb::safeInsert, this, ph::_1, ph::_2);
	db.update = std::bind(&SafeDb::safeUpdate, this, ph::_1, ph::_2, ph::_3);
	db.updateMulti = std::bind(&SafeDb::safeUpdateMulti, this, ph::_1, ph::_2, ph::_3);
	db.destroy = std::bind(&SafeDb::safeDestroy, this, ph::_1, ph::_2);
	db.destroyMulti = std::bind(&SafeDb::safeDestroyMulti, this, ph::_1, ph::_2);
	db.getData = std::bind(&SafeDb::safeGetData, this, ph::_1, ph::_2, ph::_3, ph::_4, ph::_5, ph::_6, ph::_7);
	db.alterTable = std::bind(&SafeDb::safeAlterTable, this, ph::_1, ph::_2);

	pd.getCurrentVersion = std::bind(&SafeDb::getCurrentPluginVersion, this, ph::_1);
	pd.getValue = std::bind(&SafeDb::getPluginValue, this, ph::_1, ph::_2);
	pd.setValue = std::bind(&SafeDb::setPluginValue, this, ph::_1, ph::_2, ph::_3);
}

bool SafeDb::safeCreateTable(std::string table, CapidModuleSafeDb::DbList fields, CapidModuleSafeDb::DbList options, bool ifExists)
{
	try
		{
			if (std::find(protectedTables.begin(), protectedTables.end(), table)!=protectedTables.end())
				throw BasicException("Table is protected", 1000);

			engine.createTable(table, fields, options, ifExists);
			return true;
		}
	catch (BasicException& e)
		{
			/* We can log exceptions later */
			return false;
		}
	catch (std::exception& e)
		{
			return false;
		}
}

uint64_t SafeDb::safeTableCount(std::string table, CapidModuleSafeDb::DbList conditions)
{
	try
		{
			return engine.tableCount(table, conditions, "");
		}
	catch (std::exception& e)
		{
			return 0;
		}
}

uint64_t SafeDb::safeGetId(std::string table, CapidModuleSafeDb::DbList conditions)
{
		try
		{
			return engine.getRowid(table, conditions);
		}
	catch (std::exception& e)
		{
			return 0;
		}
}

uint64_t SafeDb::safeAlterTable(std::string table, CapidModuleSafeDb::DbList settings)
{
	try
		{
			if (std::find(protectedTables.begin(), protectedTables.end(), table)!=protectedTables.end())
				throw BasicException("Table is protected", 1000);

			engine.alterTable(table, settings);
			return true;
		}
	catch (BasicException& e)
		{
			/* We can log exceptions later */
			return false;
		}
	catch (std::exception& e)
		{
			return false;
		}
}

uint64_t SafeDb::safeInsert(std::string table, CapidModuleSafeDb::DbList fields)
{
	try
		{
			if (std::find(protectedTables.begin(), protectedTables.end(), table)!=protectedTables.end())
				throw BasicException("Table is protected", 1000);

			std::map<std::string, std::string> _fields{ std::begin(fields), std::end(fields) };
			return engine.insert(table, _fields, true);
		}
	catch (BasicException& e)
		{
			/* We can log exceptions later */
			return false;
		}
	catch (std::exception& e)
		{
			return false;
		}
}
uint64_t SafeDb::safeUpdate(std::string table, uint64_t id, CapidModuleSafeDb::DbList fields)
{
	try
		{
			if (std::find(protectedTables.begin(), protectedTables.end(), table)!=protectedTables.end())
				throw BasicException("Table is protected", 1000);

			std::map<std::string, std::string> _fields{ std::begin(fields), std::end(fields) };
			return engine.update(table, id, _fields, true);
		}
	catch (BasicException& e)
		{
			/* We can log exceptions later */
			return false;
		}
	catch (std::exception& e)
		{
			return false;
		}
}

uint64_t SafeDb::safeUpdateMulti(std::string table, CapidModuleSafeDb::DbList fields, CapidModuleSafeDb::DbList conditions)
{
}
uint64_t SafeDb::safeDestroy(std::string table, uint64_t id)
{
	try
		{
			if (std::find(protectedTables.begin(), protectedTables.end(), table)!=protectedTables.end())
				throw BasicException("Table is protected", 1000);

			return engine.destroy(table, id);
		}
	catch (BasicException& e)
		{
			/* We can log exceptions later */
			return false;
		}
	catch (std::exception& e)
		{
			return false;
		}
}
uint64_t SafeDb::safeDestroyMulti(std::string table, CapidModuleSafeDb::DbList conditions)
{
	try
		{
			if (std::find(protectedTables.begin(), protectedTables.end(), table)!=protectedTables.end())
				throw BasicException("Table is protected", 1000);

			return engine.destroy(table, conditions, "");
		}
	catch (BasicException& e)
		{
			/* We can log exceptions later */
			return false;
		}
	catch (std::exception& e)
		{
			return false;
		}
}

std::list<std::map<std::string, std::string> > SafeDb::safeGetData(std::string table, std::string fields, CapidModuleSafeDb::DbList conditions, uint64_t first, uint64_t count, std::string group, std::string order)
{
	std::string extra="";
	if (!group.empty())
		extra+=" GROUP BY "+group;
	if (!order.empty())
		extra+=" ORDER BY "+order;

	if ( (first) || (count) )
		extra+= " LIMIT "+std::to_string(first)+", "+std::to_string(count);

	return engine.getData(table, fields, conditions, extra);
}

std::string SafeDb::getCurrentPluginVersion(std::string plugin)
{

	auto model = CapidModel::PluginData::getModel();

	return model.get(plugin, "_version");
}

std::string SafeDb::getPluginValue(std::string plugin, std::string key)
{
	auto model = CapidModel::PluginData::getModel();

	return model.get(plugin, key);
}

bool SafeDb::setPluginValue(std::string plugin, std::string key, std::string value)
{
	auto model = CapidModel::PluginData::getModel();

	return model.set(plugin, key, value);
}
