#pragma once

#include <string>
#include <vector>
#include <list>
#include <functional>
#include <iostream>
#include "glove/utils.hpp"
#include "data/models/plugindata.hpp"


enum CapidModuleType
	{
		MODULE_ENDPOINT
	};

struct CapidModuleRestInput
{
	std::map<std::string, std::string> special;
	std::map<std::string, std::string> request;
	std::map<std::string, std::string> arguments;
	std::string rawData;
	std::string user;
	std::string ipAddress;
	uint64_t userId;
	bool guest;										/* guest user */
	bool local;										/* local user */
	std::list<std::string> groups;
};

struct CapidModuleRestOutput
{
	std::string output;
	std::string type;
};

enum CapidModuleServiceType
	{
		WEB,												/* Not implemented */
		REST,
		PRODUCER,										/* Not implemented */
		CONSUMER,										/* Not implemented */
	};

/* A Service may be requested by the module. Then, the module
 should read settings map to read all functions of the module.
 type = REST
 settings["GET"] = "mymodule_get"
 settings["POST"] = "mymodule_get"
 ...
*/
struct CapidModuleServiceDef
{
	CapidModuleServiceType type;
	std::map<std::string, std::string> settings;
};

struct CapidModuleSafeDb
{
	typedef std::list <std::pair < std::string, std::string> > DbList;
	std::function<bool(std::string, DbList, DbList, bool)> createTable;
	std::function<uint64_t(std::string table, DbList conditions)> tableCount;
	std::function<uint64_t(std::string table, DbList conditions)> getId;
	std::function<uint64_t(std::string table, DbList fields)> insert;
	std::function<uint64_t(std::string table, uint64_t id, DbList fields)> update;
	std::function<uint64_t(std::string table, DbList fields, DbList conditions)> updateMulti;
	std::function<uint64_t(std::string table, uint64_t id)> destroy;
	std::function<uint64_t(std::string table, DbList conditions)> destroyMulti;
	std::function<std::list<std::map<std::string, std::string> >(std::string table, std::string fields, DbList conditions, uint64_t, uint64_t, std::string, std::string)> getData;
	std::function<uint64_t(std::string table, DbList fields)> alterTable;
};

struct PluginDataModel
{
	std::function<std::string(std::string plugin)> getCurrentVersion;
	std::function<std::string(std::string plugin, std::string key)> getValue;
	std::function<bool(std::string plugin, std::string key, std::string value)> setValue;
};

/* The same as Plugin Data Model but prevents plugins from asking about other plugins */
struct PluginDataModelFiltered
{
	std::function<std::string()> getCurrentVersion;
	std::function<std::string(std::string key)> getValue;
	std::function<bool(std::string key, std::string value)> setValue;
};

class CapidModuleShared;
/* Modules should extend this class */
class CapidModule
{
public:
	CapidModule(std::string name, std::string longname, std::string version, std::string author, CapidModuleType type):
		_name(name), _longname(longname), _version(version),
		_author(author), _type(type)
	{

	}

	~CapidModule()
	{
	}

	inline bool setProps(CapidModuleShared& shared);

protected:
	virtual bool afterInit()
	{
		return true;
	}

	/* Gets database version from  */
	std::string getDbVersion(std::string defaultVersion)
	{
	}
	CapidModuleSafeDb db;
	PluginDataModelFiltered plugindata;

private:
	std::string _name;
	std::string _longname;
	std::string _version;
	std::string _author;
	CapidModuleType _type;

	/*
	void setProps(CapidModuleShared& shared)
	{
	}

	friend CapidModuleShared; */
};

struct CapidModuleSharedAccess
{
	bool allowRemote;
	bool allowGuests;
	std::vector <std::string> users;
};

class CapidModuleSharedBase
{
public:
	typedef CapidModuleRestOutput (*_rest_action_callback)(CapidModuleRestInput&);

	CapidModuleSharedBase(CapidModuleSafeDb& db, PluginDataModel& pd, const std::string& config, bool allowRemote, bool allowGuests, std::string users): db(db), _config(config),_pd(pd)
	{
		access = { allowRemote, allowGuests, tokenize(users, " ", defaultTrim) };
		/* pluginData(); */
	}

	virtual ~CapidModuleSharedBase()
	{
	}

	virtual std::string longname()
	{
		return _longname;
	}

	virtual std::string name()
	{
		return _name;
	}

	virtual std::string version()
	{
		return _version;
	}

	virtual std::string author()
	{
		return _author;
	}

	virtual std::string soFile()=0;

	virtual CapidModuleType type()
	{
		return _type;
	}

	void addRestService(std::string name, std::string get, std::string post="", std::string put="", std::string patch="", std::string _delete="")
	{
		myservices.insert({name,
					{ REST,
							{ { "get", get }, { "post", post }, { "put", put }, { "patch", patch }, {"delete", _delete } }
					} });
	}

	std::map<std::string, CapidModuleServiceDef>& getServices()
	{
		return myservices;
	}

	void clearServices()
	{
		myservices.clear();
	}

	std::string config()
	{
		return _config;
	}
protected:
	CapidModuleSafeDb db;
	std::string _config;
	std::string _name;
	std::string _longname;
	std::string _author;
	std::string _version;
	CapidModuleType _type;
	std::map<std::string, CapidModuleServiceDef> myservices;
	PluginDataModelFiltered pd;
	CapidModuleSharedAccess access;

	void afterInit()
	{
		this->pd.getCurrentVersion = std::bind(_pd.getCurrentVersion, name());
		this->pd.getValue = std::bind(_pd.getValue, name(), std::placeholders::_1);
		this->pd.setValue = std::bind(_pd.setValue, name(), std::placeholders::_1, std::placeholders::_2);
		/* Meter las que faltan */

	}
private:
	PluginDataModel _pd;
};

class CapidInternalModuleShared : public CapidModuleSharedBase
{
public:
	CapidInternalModuleShared(CapidModuleSafeDb& db, PluginDataModel& pd, std::string name, std::string longname, std::string version, std::string author, bool allowRemote, bool allowGuests, std::string users): CapidModuleSharedBase(db, pd, "", allowRemote, allowGuests, users)
	{
		_name = name;
		_version = version;
		_author = author;
		_longname = longname;

		afterInit();
	}
	~CapidInternalModuleShared()
	{
	}

	virtual std::string soFile()
	{
		return "_internal_";
	}
};
/* Shared object class between module and server */
class CapidModuleShared : public CapidModuleSharedBase
{
public:
	typedef bool (*_init_function)(CapidModuleShared&);

	CapidModuleShared(std::string soFile, CapidModuleSafeDb& db, PluginDataModel& pd, const std::string& config, bool allowRemote, bool allowGuests, std::string users): CapidModuleSharedBase(db, pd, config, allowRemote, allowGuests, users), _soFile(soFile)
	{
	}

	~CapidModuleShared()
	{
	}

	std::string soFile()
	{
		return _soFile;
	}
protected:
	std::string _soFile;

	friend bool CapidModule::setProps(CapidModuleShared& shared);
};

bool CapidModule::setProps(CapidModuleShared& shared)
{
	shared._name = _name;
	shared._longname = _longname;
	shared._version = _version;
	shared._author = _author;
	shared._type = _type;
	db = shared.db;
	shared.afterInit();						/* Fills shared.pd variables */
	plugindata = shared.pd;
	return this->afterInit();
}
