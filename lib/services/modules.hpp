#pragma once

#include "restservice.hpp"
#include "../daemon.hpp"

class ModuleService : public RestService
{
public:
	ModuleService(Daemon& dbEngine);
	~ModuleService();

	CapidModuleRestOutput _get(CapidModuleRestInput& input);
	CapidModuleRestOutput _put(CapidModuleRestInput& input);
	CapidModuleRestOutput _delete(CapidModuleRestInput& input);
private:
	Daemon& daemon;
};
