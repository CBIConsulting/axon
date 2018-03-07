#pragma once

#include "glove/glovehttpserver.hpp"
#include "data/dbengine.hpp"
#include "services/users.hpp"
#include "services/groups.hpp"
#include "services/modules.hpp"
#include "daemon.hpp"
#include <memory>

class DaemonServices
{
public:
	DaemonServices(Daemon& d, DbEngine& dbEngine);
	~DaemonServices();

	UserService& users()
	{
		return *_userService;
	}

	GroupService& groups()
	{
		return *_groupService;
	}
	
	ModuleService& modules()
	{
		return *_moduleService;
	}
	
	void index(GloveHttpRequest &request, GloveHttpResponse& response);
protected:
	DbEngine& dbEngine;
	Daemon& daemon;
	std::shared_ptr<UserService> _userService;
	std::shared_ptr<GroupService> _groupService;
	std::shared_ptr<ModuleService> _moduleService;
};
