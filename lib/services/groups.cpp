#include "groups.hpp"
#include "../cutils.hpp"
#include "../data/models/group.hpp"
#include "../data/models/usergroup.hpp"
#include "../glove/json.hpp"
#include "../glove/gloveexception.hpp"
#include "../basic_exception.hpp"

GroupService::GroupService(DbEngine& dbEngine): dbEngine(dbEngine)
{
}

GroupService::~GroupService()
{
}

CapidModuleRestOutput GroupService::_get(CapidModuleRestInput& input)
{
	auto grp = CapidModel::Group::getModel();	

	CapidModuleRestOutput output;

	auto groupname = input.special["id"];
	auto groupsdata = (groupname.empty())?grp.getAll():grp.getAll(groupname);

	nlohmann::json jsout (groupsdata);
	output.output=jsout.dump(2);
	return output;
}

CapidModuleRestOutput GroupService::_post(CapidModuleRestInput& input) 
{
	CapidModuleRestOutput output;

	auto inputJson = nlohmann::json::parse(input.rawData);
	auto groupname = inputJson["groupname"];
	auto allowed = inputJson["allowed"];
	auto usersIn = inputJson["users"];
	std::string _groupname;
	uint32_t _allowed = 1;

	if (groupname.is_null())
		throw GloveApiException(1, "No groupname given");
	else
		_groupname = groupname.get<std::string>();

	if (!allowed.is_null())
		_allowed = allowed.get<int>();

	auto grp = CapidModel::Group::getModel();
	auto usrgrp = CapidModel::UserGroup::getModel();
	auto current = grp.get(_groupname);
	if (current.good())
		throw GloveApiException(3, "Group already exists");

	auto groupid = grp.create(_groupname, (_allowed));
	if (!groupid)
		throw GloveApiException(4, "There was a problem creating group "+_groupname);

	nlohmann::json jsout({ { "module", "groups"}, { "action", "create" }, { "groupid", std::to_string(groupid) } });

	std::string successUsers;
	std::string failUsers;
	
	if (!usersIn.is_null())
		{			
			auto _usersIn = tokenize(usersIn.get<std::string>(), ",", defaultTrim);
			if (_usersIn.size()>0)
				{
					for (auto u : _usersIn)
						{
							if (usrgrp.create(u, _groupname))
								successUsers+=","+u;
							else
								failUsers+=","+u;
						}
				}
		}
	if (successUsers.length()>0)
		jsout["successfullyAddedUsers"] = trim(successUsers, ",");
	if (successUsers.length()>0)
		jsout["notAddedUsers"] = trim(failUsers, ",");
	
	output.output=jsout.dump(2);

	return output;
}

CapidModuleRestOutput GroupService::_put(CapidModuleRestInput& input)
{
	CapidModuleRestOutput output;
	auto inputJson = nlohmann::json::parse(input.rawData);
	auto groupname = inputJson["groupname"];
	auto allowed = inputJson["allowed"];
	auto usersIn = inputJson["addusers"];
	auto usersOut = inputJson["delusers"];
	std::string _groupname;
	int _allowed = -1;

	if (groupname.is_null())
		throw GloveApiException(1, "No groupname given");
	else
		_groupname = groupname.get<std::string>();

	if (!allowed.is_null())
		_allowed = allowed.get<int>();

	auto grp = CapidModel::Group::getModel();
	auto usrgrp = CapidModel::UserGroup::getModel();
	auto current = grp.get(_groupname);
	if (!current.good())
		throw GloveApiException(5, "Group does not exist");

	if (_allowed>=0)
		{
			if (!grp.update(_groupname, (_allowed>0)))
				throw GloveApiException(6, "There was a problem updating permissions");
		}
	
	nlohmann::json jsout({ { "module", "groups"}, { "action", "update" }, { "groupid", std::to_string(current.groupid()) } });

	std::string successUsers;
	std::string failUsers;
	
	if (!usersIn.is_null())
		{			
			auto _usersIn = tokenize(usersIn.get<std::string>(), ",", defaultTrim);
			if (_usersIn.size()>0)
				{
					for (auto u : _usersIn)
						{
							if (usrgrp.create(u, _groupname))
								successUsers+=","+u;
							else
								failUsers+=","+u;
						}
				}
		}
	if (successUsers.length()>0)
		jsout["successfullyAddedUsers"] = trim(successUsers, ",");
	if (failUsers.length()>0)
		jsout["notAddedUsers"] = trim(failUsers, ",");

	successUsers.clear();
	failUsers.clear();
	
	if (!usersOut.is_null())
		{			
			auto _usersOut = tokenize(usersOut.get<std::string>(), ",", defaultTrim);
			if (_usersOut.size()>0)
				{
					for (auto u : _usersOut)
						{
							if (usrgrp.destroy(u, _groupname))
								successUsers+=","+u;
							else
								failUsers+=","+u;
						}
				}
		}
	if (successUsers.length()>0)
		jsout["successfullyDeletedUsers"] = trim(successUsers, ",");
	if (failUsers.length()>0)
		jsout["notDeletedUsers"] = trim(failUsers, ",");

	output.output=jsout.dump(2);
	
	return output;
}

CapidModuleRestOutput GroupService::_delete(CapidModuleRestInput& input)
{
	CapidModuleRestOutput output;

	auto inputJson = nlohmann::json::parse(input.rawData);
	auto groupname = inputJson["groupname"];
	auto allowed = inputJson["allowed"];
	auto usersIn = inputJson["addusers"];
	auto usersOut = inputJson["delusers"];
	std::string _groupname;

	if (groupname.is_null())
		throw GloveApiException(1, "No groupname given");
	else
		_groupname = groupname.get<std::string>();

	auto usrgrp = CapidModel::UserGroup::getModel();
	auto grp = CapidModel::Group::getModel();
	auto current = grp.get(_groupname);
	if (!current.good())
		throw GloveApiException(5, "Group does not exist");
	
	if (!usrgrp.destroy("@"+_groupname))
		throw GloveApiException(8, "There was a problem kicking users out");
	
	if (!grp.destroy(_groupname))
		throw GloveApiException(7, "There was a problem deleting group");

	nlohmann::json jsout({ { "module", "groups"}, { "action", "delete" }, { "groupid", std::to_string(current.groupid()) } });
	output.output=jsout.dump(2);
	
	return output;
}
