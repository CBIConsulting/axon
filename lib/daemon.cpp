#include "daemon.hpp"
#include "daemon_services.hpp"
#include "json/jsonutils.hpp"
#include "basic_exception.hpp"
#include "axond_module_info.hpp"
#include "data/dbengine.hpp"
#include "glove/utils.hpp"
#include "data/safedb.hpp"
#include "data/models/user.hpp"
#include "data/models/group.hpp"
#include "data/models/usergroup.hpp"
#include "config.hpp"
#include "cutils.hpp"
#include "os.hpp"
#include <iostream>
#include <memory>
#include <map>
#include <dlfcn.h>
#include <pwd.h>
#include <sys/types.h>

#define PROGRAM_NAME "axon"
#define MODULE_NAME "axond"
#define LONG_PROGRAM_NAME "Axon Daemon"
#define PROGRAM_VERSION 1
#define PROGRAM_VERSION_LONG "0.0.1"

namespace
{
	std::unique_ptr<Config> configFile = nullptr;
	std::unique_ptr<GloveHttpServer> server = nullptr;
	std::map<std::string, CapidModuleInfo> modules;
	std::unique_ptr<SafeDb> safedb = nullptr;

	CapidModuleSafeDb _safedb;
	PluginDataModel _pdmodel;
	DbEngine dbEngine;
	std::shared_ptr<DaemonServices> services;
};

Daemon::Daemon()
{
	std::unique_ptr<Config> _config(new Config(PROGRAM_NAME, MODULE_NAME));
	configFile = std::move(_config);

	services = std::make_shared<DaemonServices>(DaemonServices(*this, dbEngine));
	std::unique_ptr<GloveHttpServer> _server(new GloveHttpServer());
	server = std::move(_server);

	if (!configFile->ok())
		terminate("Config file error ("+std::to_string(configFile->errorCode())+"): "+configFile->errorTxt());

	if (!configFile->get(_listen, "listen"))
		terminate("Config error: listen not found");
	if (!configFile->get(_port, "port"))
		terminate("Config error: port not found");
	if (!configFile->get(_database, "database"))
		terminate("Config error: database not found");
	configFile->get(_modulepath, "module_path", std::string("./"));

	configFile->get(_username, "username");
	configFile->get(_group, "group");
	_timeout = 0;
	configFile->get(_timeout, "timeout");
	std::cout << "TIMEOUT: "<<_timeout<<"\n";
	dbEngine.setFileName(_database);
	dbEngine.initialize();
	/* Initialize DB function export */
	std::unique_ptr<SafeDb> _safedb(new SafeDb(dbEngine));
	safedb = std::move(_safedb);
	safedb->safeDbExport(::_safedb, ::_pdmodel);

	std::cout << "Loading database models... ";
	auto usr = CapidModel::User::getModel(dbEngine);
	auto grp = CapidModel::Group::getModel(dbEngine);
	auto usrgrp = CapidModel::UserGroup::getModel(dbEngine);
	std::cout << "PLUGINDATA INIT\n";
	auto plugin = CapidModel::PluginData::getModel(dbEngine);
	std::cout << " PLUGINDATA 2\n";
	/* auto user = usr.get("admin"); */
	/* if (user.good()) */
	/* 	{ */
	/* 		std::cout << user.username()<<":"<<user.passhash()<<"--"<<user.ctime()<<"--"<<user.mtime()<<user.userid()<<std::endl; */
	/* 	} */

	std::vector<nlohmann::json> cfgServices;
	if (configFile->get(cfgServices, "services"))
		{
			for (auto serv : cfgServices)
				{

					loadModule(serv);
				}
		}

	auto sshkeys = modules.find("sshkeys");

	registerBasicActions();
	server->addRoute("/", std::bind(&DaemonServices::index, services, std::placeholders::_1, std::placeholders::_2));
	server->listen(_port, _listen);

	if (_timeout > 0)
		server->timeout(_timeout);
	if (!_username.empty())
		OS::setuser(_username);
	if (!_group.empty())
		OS::setgroup(_group);

	/* std::cout << "WEEEJEE\n"; */
	/* auto program = OS::execute("./scriptillo", { "/root/kakapedo" }, "alga\nolga\nelga\nulga\nilga\n"); */
	/* std::cout << "STATUS: "<<program.status<<"\n EXITCODE: "<<program.exitCode<<"\nSTDOUT: "<<program.stdout<<"\nSTDERR: "<<program.stderr<<"\n"; */
	/* std::terminate(); */
}

Daemon::~Daemon()
{
}

bool Daemon::loadModule(nlohmann::json& options)
{
	auto load = options["load"];
	if (!load.is_string())
		terminate ("Config error: Unable to read module 'load' field");
	auto soFile = load.get<std::string>();
	auto filename = _modulepath+soFile;
	std::string moduleConfig;
	auto _config = options["config"];
	if (!_config.is_null())
		moduleConfig = _config.dump(2);

	if (!CUtils::file_exists(filename))
		terminate("Module error: Unable to find "+filename);
	try {
		CapidModuleInfo moduleInfo(filename, moduleConfig, _safedb, _pdmodel);
		auto& module = static_cast<CapidModuleShared&>(moduleInfo.module());

		std:: cout << "Loading "<<module.name()<<" v."<<module.version()<< " by "<< module.author()<< std::endl;
		if (!moduleInfo.good())
			throw BasicException("There was a problem enabling module", 1002);

		auto permissions = parsePermission(options);

		addServices(moduleInfo, permissions);
		modules.insert(std::make_pair(module.name(), std::move(moduleInfo)));

		return true;
	} catch (BasicException& e) {
		terminate("Module error: "+std::string(e.what()));
	} catch (std::exception& e)	{
		terminate("Fatal error: "+std::string(e.what()));
	}
	return false;
}

void Daemon::registerBasicActions()
{
	std::map<CapidModuleRest::Method, CapidModuleSharedAccess> permi;

	permi[CapidModuleRest::GET] = { false, false, { "@admin" } };
	permi[CapidModuleRest::POST] = { false, false, { "@admin" } };
	permi[CapidModuleRest::PUT] = { false, false, { "@admin" } };
	permi[CapidModuleRest::DELETE] = { false, false, { "@admin" } };

	/* We SHOULD give up simple pointers here !! Smart pointers? */
	/* Users */
	CapidModuleRest* userrest = new CapidModuleRest(permi,
																									std::bind(&RestService::_get, &services->users(), std::placeholders::_1),
																									std::bind(&RestService::_post, &services->users(), std::placeholders::_1),
																									std::bind(&RestService::_put, &services->users(), std::placeholders::_1),
																									nullptr, /* patch */
																									std::bind(&RestService::_delete, &services->users(), std::placeholders::_1)
	);
	server->addRest("/users/$id/$extra",
									0,
									GloveHttpServer::jsonApiErrorCall,
									(GloveHttpServer::url_callback)std::bind(&CapidModuleRest::get, userrest, std::placeholders::_1, std::placeholders::_2),
									(GloveHttpServer::url_callback)std::bind(&CapidModuleRest::post, userrest, std::placeholders::_1, std::placeholders::_2),
									(GloveHttpServer::url_callback)std::bind(&CapidModuleRest::put, userrest, std::placeholders::_1, std::placeholders::_2),
									(GloveHttpServer::url_callback)std::bind(&CapidModuleRest::patch, userrest, std::placeholders::_1, std::placeholders::_2),
									(GloveHttpServer::url_callback)std::bind(&CapidModuleRest::delete_, userrest, std::placeholders::_1, std::placeholders::_2)
	);

	/* Groups */
	CapidModuleRest* grouprest = new CapidModuleRest(permi,
																									 std::bind(&RestService::_get, &services->groups(), std::placeholders::_1),
																									 std::bind(&RestService::_post, &services->groups(), std::placeholders::_1),
																									 std::bind(&RestService::_put, &services->groups(), std::placeholders::_1),
																									 nullptr, /* patch */
																									 std::bind(&RestService::_delete, &services->groups(), std::placeholders::_1)
	);
	server->addRest("/groups/$id/$extra",
									0,
									GloveHttpServer::jsonApiErrorCall,
									(GloveHttpServer::url_callback)std::bind(&CapidModuleRest::get, grouprest, std::placeholders::_1, std::placeholders::_2),
									(GloveHttpServer::url_callback)std::bind(&CapidModuleRest::post, grouprest, std::placeholders::_1, std::placeholders::_2),
									(GloveHttpServer::url_callback)std::bind(&CapidModuleRest::put, grouprest, std::placeholders::_1, std::placeholders::_2),
									(GloveHttpServer::url_callback)std::bind(&CapidModuleRest::patch, grouprest, std::placeholders::_1, std::placeholders::_2),
									(GloveHttpServer::url_callback)std::bind(&CapidModuleRest::delete_, grouprest, std::placeholders::_1, std::placeholders::_2)
	);

	/* Modules */
	CapidModuleRest* modulerest = new CapidModuleRest(permi,
																									 std::bind(&RestService::_get, &services->modules(), std::placeholders::_1),
																										nullptr, /* post */
																									 std::bind(&RestService::_put, &services->modules(), std::placeholders::_1),
																									 nullptr, /* patch */
																									 std::bind(&RestService::_delete, &services->modules(), std::placeholders::_1)
	);
	server->addRest("/modules/$id/$extra",
									0,
									GloveHttpServer::jsonApiErrorCall,
									(GloveHttpServer::url_callback)std::bind(&CapidModuleRest::get, modulerest, std::placeholders::_1, std::placeholders::_2),
									(GloveHttpServer::url_callback)std::bind(&CapidModuleRest::post, modulerest, std::placeholders::_1, std::placeholders::_2),
									(GloveHttpServer::url_callback)std::bind(&CapidModuleRest::put, modulerest, std::placeholders::_1, std::placeholders::_2),
									(GloveHttpServer::url_callback)std::bind(&CapidModuleRest::patch, modulerest, std::placeholders::_1, std::placeholders::_2),
									(GloveHttpServer::url_callback)std::bind(&CapidModuleRest::delete_, modulerest, std::placeholders::_1, std::placeholders::_2)
	);

}

bool Daemon::unloadModule(std::string module)
{
	auto modpost = modules.find(module);
	if (modpost == modules.end())
		return false;

	modules.erase(modpost);
	return true;
}

void Daemon::terminate(std::string message)
{
	std::cout << message << std::endl;
	std::terminate();
}

std::map<std::string, std::map<std::string, std::string> > Daemon::modulesInfo()
{
	std::map<std::string, std::map<std::string, std::string>> res;

	for (auto m=modules.begin(); m!=modules.end(); ++m)
		{
			std::map < std::string, std::string> moduleInfo;
			moduleInfo.insert({"Author", m->second.module().author() });
			moduleInfo.insert({"Version", m->second.module().version() });
			moduleInfo.insert({"Name", m->second.module().longname() });
			res.insert({m->first, moduleInfo});
		}
	return res;
}

bool Daemon::safeLoadModule(std::string module)
{
	std::vector<nlohmann::json> cfgServices;
	if (configFile->get(cfgServices, "services"))
		{
			for (auto serv : cfgServices)
				{
					auto load = serv["load"];
					if (!load.is_string())
						terminate ("Config error: Unable to read module 'load' field");

					if (load.get<std::string>() == module)
						{
							return loadModule(serv);
						}
				}
		}
	return false;
}

bool Daemon::safeUnloadModule(std::string module)
{
	return unloadModule(module);
}

bool Daemon::isModuleLoaded(std::string module)
{
	return (modules.find(module)!= modules.end());
}

void Daemon::addServices(CapidModuleInfo& moduleInfo, std::map<CapidModuleRest::Method, CapidModuleSharedAccess>& permissions)
{
	auto& module = static_cast<CapidModuleShared&>(moduleInfo.module());
	auto moduleName = moduleInfo.module().name();
	auto moduleServices = module.getServices();
	for (auto s : moduleServices)
		{
			if (s.second.type == REST)
				{
					if (permissions.find(CapidModuleRest::GET) == permissions.end())
						permissions[CapidModuleRest::GET] = permissions[CapidModuleRest::_ALL_];
					if (permissions.find(CapidModuleRest::POST) == permissions.end())
						permissions[CapidModuleRest::POST] = permissions[CapidModuleRest::_ALL_];
					if (permissions.find(CapidModuleRest::PUT) == permissions.end())
						permissions[CapidModuleRest::PUT] = permissions[CapidModuleRest::_ALL_];
					if (permissions.find(CapidModuleRest::PATCH) == permissions.end())
						permissions[CapidModuleRest::PATCH] = permissions[CapidModuleRest::_ALL_];
					if (permissions.find(CapidModuleRest::DELETE) == permissions.end())
						permissions[CapidModuleRest::DELETE] = permissions[CapidModuleRest::_ALL_];
					/* DEBUG */
					/* std::cout << "SERVICE "<<s.first<<"\n"; */
					/* for (auto d : s.second.settings) */
					/* 	{ */
					/* 		std::cout << d.first<< " => "<<d.second<<"\n"; */
					/* 	} */


					CapidModuleRest* servrest = moduleInfo.getRestFuncs(s.second.settings, permissions);
					server->addRest("/"+moduleName+"/"+s.first+"/$id/$extra",
													2,
													GloveHttpServer::jsonApiErrorCall,
													(GloveHttpServer::url_callback)std::bind(&CapidModuleRest::get, servrest, std::placeholders::_1, std::placeholders::_2),
													(GloveHttpServer::url_callback)std::bind(&CapidModuleRest::post, servrest, std::placeholders::_1, std::placeholders::_2),
													(GloveHttpServer::url_callback)std::bind(&CapidModuleRest::put, servrest, std::placeholders::_1, std::placeholders::_2),
													(GloveHttpServer::url_callback)std::bind(&CapidModuleRest::patch, servrest, std::placeholders::_1, std::placeholders::_2),
													(GloveHttpServer::url_callback)std::bind(&CapidModuleRest::delete_, servrest, std::placeholders::_1, std::placeholders::_2)
					);

				}
			else
				throw BasicException("Service type not implemented\n");
		}

}

std::map<CapidModuleRest::Method, CapidModuleSharedAccess> Daemon::parsePermission(nlohmann::json& input)
{
	std::map<CapidModuleRest::Method, CapidModuleSharedAccess> res;
	/* Allow remote, Disallow guests (by default)  */
	res[CapidModuleRest::_ALL_] = _parsePermission(input, true, true, false);
	auto _apipermission = input["apipermission"];
	if (!_apipermission.is_null())
		{
			if (!_apipermission["get"].is_null())
				res[CapidModuleRest::GET] = _parsePermission(_apipermission["get"], true, res[CapidModuleRest::_ALL_].allowRemote, res[CapidModuleRest::_ALL_].allowGuests);
			if (!_apipermission["post"].is_null())
				res[CapidModuleRest::POST] = _parsePermission(_apipermission["post"], true, res[CapidModuleRest::_ALL_].allowRemote, res[CapidModuleRest::_ALL_].allowGuests);
			if (!_apipermission["patch"].is_null())
				res[CapidModuleRest::PATCH] = _parsePermission(_apipermission["patch"], true, res[CapidModuleRest::_ALL_].allowRemote, res[CapidModuleRest::_ALL_].allowGuests);
			if (!_apipermission["put"].is_null())
				res[CapidModuleRest::PUT] = _parsePermission(_apipermission["put"], true, res[CapidModuleRest::_ALL_].allowRemote, res[CapidModuleRest::_ALL_].allowGuests);
			if (!_apipermission["delete"].is_null())
				res[CapidModuleRest::DELETE] = _parsePermission(_apipermission["delete"], true, res[CapidModuleRest::_ALL_].allowRemote, res[CapidModuleRest::_ALL_].allowGuests);
		}
	return res;
}

/* allowSingle: input json may be a string with users */
CapidModuleSharedAccess Daemon::_parsePermission(nlohmann::json& input, bool allowSingle, bool defaultAllowRemote, bool defaultAllowGuests)
{
	if (input.is_string())
		{
			if (!allowSingle)
				terminate("Config error: Malformed services JSON");
			std::string users = input.get<std::string>();

			return {defaultAllowRemote, defaultAllowGuests, tokenize(users, ", ", defaultTrim)};
		}
	else if (input.is_object())
		{
			bool allowRemote = defaultAllowRemote;
			bool allowGuests = defaultAllowGuests;
			std::string users;
			auto _allowRemote = input["allowremote"];
			auto _allowGuests = input["allowguests"];
			auto _users = input["users"];

			if (!_allowRemote.is_null())
				{
					if (_allowRemote.is_boolean())
						allowRemote = _allowRemote.get<bool>();
					else
						allowRemote = (_allowRemote.get<uint32_t>() != 0);
				}
			if (!_allowGuests.is_null())
				{
					if (_allowGuests.is_boolean())
						allowGuests = _allowGuests.get<bool>();
					else
						allowGuests = (_allowGuests.get<uint32_t>() != 0);
				}
			if (!_users.is_null())
				users = _users.get<std::string>();

			return {allowRemote, allowGuests, tokenize(users, ", ", defaultTrim)};
		}
	else
		terminate("Config error: malformed config JSON");
}
