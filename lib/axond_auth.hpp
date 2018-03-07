#pragma once

#include "glove/glovehttpserver.hpp"
#include "axond_module.hpp"
#include <list>
#include <string>

class CapidAuth
{
public:
	CapidAuth(GloveHttpRequest& req);
	~CapidAuth();

	bool granted(CapidModuleSharedAccess& permission);
	bool isLocal()
	{
		return local;
	}
	bool isGuest()
	{
		return _guest;
	}

	std::string getUser() const
	{
		return user;
	}

	uint64_t getUserId() const
	{
		return userId;
	}
	
	std::list<std::string> getGroups() const
	{
		return groups;
	}

protected:
	int gloveAuth(GloveHttpRequest& request, GloveHttpResponse& response);
	
private:
	std::string user;
	uint64_t userId;
	std::list<std::string> groups;
	bool _guest;
	bool local;
};
