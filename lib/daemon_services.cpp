#include "daemon_services.hpp"

DaemonServices::DaemonServices(Daemon& d, DbEngine& dbEngine):daemon(d), dbEngine(dbEngine)
{
	_userService = std::make_shared<UserService> (UserService(dbEngine));
	_groupService = std::make_shared<GroupService> (GroupService(dbEngine));
	_moduleService = std::make_shared<ModuleService> (ModuleService(daemon));
}

DaemonServices::~DaemonServices()
{
}

void DaemonServices::index(GloveHttpRequest &request, GloveHttpResponse& response)
{
	std::cout << "HOST: "<<request.getClient()->get_host()<<std::endl;
	std::cout << "IP: "<<request.getClient()->get_address(true)<<std::endl;
	response << " TEST ";
}
