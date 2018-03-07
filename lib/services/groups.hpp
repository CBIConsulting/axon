#pragma once

#include "restservice.hpp"
#include "../data/dbengine.hpp"

class GroupService : public RestService
{
public:
	GroupService(DbEngine& dbEngine);
	~GroupService();

	CapidModuleRestOutput _get(CapidModuleRestInput& input);
	CapidModuleRestOutput _post(CapidModuleRestInput& input);
	CapidModuleRestOutput _put(CapidModuleRestInput& input);
	CapidModuleRestOutput _delete(CapidModuleRestInput& input);
private:
	DbEngine& dbEngine;
};
