#pragma once

#include <string>
#include "json/json.hpp"
#include "axond_module_info.hpp"

class Daemon
{
public:
	Daemon();
	~Daemon();

	std::map<std::string, std::map<std::string, std::string>> modulesInfo();
	bool isModuleLoaded(std::string module);

	bool safeLoadModule(std::string module);
	bool safeUnloadModule(std::string module);
protected:
	static void terminate(std::string message);

	bool loadModule(nlohmann::json& options);
	bool unloadModule(std::string module);

	void registerBasicActions();
	void addServices(CapidModuleInfo& moduleInfo, std::map<CapidModuleRest::Method, CapidModuleSharedAccess>& permissions);

	std::map<CapidModuleRest::Method, CapidModuleSharedAccess> parsePermission(nlohmann::json& input);
	CapidModuleSharedAccess _parsePermission(nlohmann::json& input, bool allowSingle, bool defaultAllowRemote, bool defaultAllowGuests);

	std::string _modulepath;
	std::string _listen;
	uint16_t _port;
	double _timeout;
	std::string _database;
	std::string _username;
	std::string _group;
};
