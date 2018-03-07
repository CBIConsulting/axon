#include "axond_auth.hpp"
#include "glove/glovecoding.hpp"
#include "glove/utils.hpp"
#include "data/models/user.hpp"
#include "data/models/usergroup.hpp"
#include "basic_exception.hpp"
#include <algorithm>

CapidAuth::CapidAuth(GloveHttpRequest& req): _guest(false)
{
	local = req.isLocal();
	auto authorization = req.getHeader("Authorization");
	auto space=authorization.find(' ');

	if (authorization.empty())
		{
			userId = 0;
			_guest = true;
			return;
		}
	if (space == std::string::npos)
		throw CapidAuthException(1001, "Malformed auth request");

	/* Only support Basic Auth, a new object here is ok */
	GloveHttpResponse response("");
	
	if (authorization.substr(0, space)== "Hashed")
		{
			auto hash = GloveCoding::base64_decode(authorization.substr(space+1));
			auto toks = tokenize(trim(hash), ":");
			if (toks.size()!=3)
				throw CapidAuthException(1003, "Invalid auth request");

			auto usr = CapidModel::User::getModel();
			auto usrgrp = CapidModel::UserGroup::getModel();
			auto rawpath = req.getUri().rawpath;
			if (rawpath.back()=='/')
				rawpath.pop_back();
			auto user = usr.match (toks[0], toks[1]+".[PASSHASH]."+rawpath, toks[2]);

			if (!user.good())
				throw CapidAuthException(1004, "Invalid user");
			this->user = user.username();
			this->userId = user.userid();
			/* Debemos sacar el allowed de los grupos */
			this->groups = usrgrp.getGroups(this->user, true);
		}
	else if (req.auth(response, std::bind(&CapidAuth::gloveAuth, this, std::placeholders::_1, std::placeholders::_2), "Basic"))
		{
		}
	/* else */
		/* throw CapidAuthException(1002, "Invalid auth request"); */
}

CapidAuth::~CapidAuth()
{
}

bool CapidAuth::granted(CapidModuleSharedAccess& permission)
{
	if ( (!permission.allowRemote) && (!local) )
		return false;
	if ( (permission.allowGuests) && (_guest) )
		return true;
	for (auto p: permission.users)
		{
			if (p.empty())
				continue;
			if (p == "@guests")
				return true;
			else if ( (p.front()=='@') && (std::find(groups.begin(), groups.end(), p) != groups.end()) )
				return true;
			else if (p == user)
				return true;
			
		}
	return false;
}

int CapidAuth::gloveAuth(GloveHttpRequest& request, GloveHttpResponse& response)
{
	auto user = request.getAuthUser();

	auto usr = CapidModel::User::getModel();
	auto _user = usr.get(user);
	if (!_user.good())
		return 0;

	if (request.checkHashedPassword(_user.passhash(), usr.getHashMethod()))
		{
			this->user = _user.username();
			this->userId = _user.userid();
			/* Debemos sacar el allowed de los grupos */
			auto usrgrp = CapidModel::UserGroup::getModel();
			this->groups = usrgrp.getGroups(this->user, true);
			return 1;
		}
	else
		throw CapidAuthException(1004, "Invalid user");
	return 0;
}
