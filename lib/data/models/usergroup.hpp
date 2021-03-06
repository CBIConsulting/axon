#pragma once

#include "datamodel.hpp"
#include "../../glove/utils.hpp"
#include "user.hpp"
#include "group.hpp"

namespace CapidModel
{
	class UserGroup : public CapidModel::DataModel<UserGroup>
	{
	public:
		~UserGroup()
		{
		}

		bool userInGroup(std::string username, std::string groupname)
		{
			auto userdata = user().get(username);
			auto groupdata= group().get(groupname);

			if ( (!userdata.good()) || (!groupdata.good()) )
				return false;

			auto groups = getGroups(username);
			for (auto g : groups)
				{
					if (g == groupname)
						return true;
				}
			return false;
		}

		bool create(std::string username, std::string groupname)
		{
			auto userdata = user().get(username);
			auto groupdata= group().get(groupname);

			if ( (!userdata.good()) || (!groupdata.good()) )
				return false;

			if (userInGroup(username, groupname))
				return false;

			return (engine.insert("USERGROUP", {
						{ "userid", std::to_string(userdata.userid()) },
						{ "groupid", std::to_string(groupdata.groupid()) },
						{ "ctime", "F(DATETIME('now')" }}));
		};

		std::list<std::string> getGroups(std::string username, bool addatsign=false)
		{
			auto usr = user().get(username);
			if (!usr.good())
				return {};

			auto res = engine.getData("USERGROUP USG JOIN GROUPS GRP ON GRP.rowid=USG.groupid",
																"USG.userid, USG.groupid, GRP.name AS groupname",
																{
																	{ "USG.userid", std::to_string(usr.userid()) }
																}, "GROUP BY GRP.rowid");
			std::list<std::string> result;
			for (auto el : res)
				{
					result.push_back((addatsign)?("@"+el["groupname"]):el["groupname"]);
				}

			return result;
		}

		std::list<std::string> getUsers(std::string groupname)
		{
			auto grp = group().get(groupname);
			if (!grp.good())
				return {};

			auto res = engine.getData("USERGROUP USG JOIN USER USR ON USR.rowid=USG.userid",
																"USG.userid, USG.groupid, USR.name AS username",
																{
																	{ "USG.groupid", std::to_string(grp.groupid()) }
																}, "GROUP BY USR.rowid");
			std::list<std::string> result;
			for (auto el : res)
				{
					result.push_back(el["username"]);
				}

			return result;
		}
		/* Users: xxxx, Groups: @xxxx */
		bool destroy(std::string entity)
		{
			if (entity.empty())
				return false;
			else if (entity[0]=='@')
				return destroyGroup(entity.substr(1));
			else
				return destroyUser(entity);
		}

		bool destroy(std::string username, std::string groupname)
		{
			auto grp = group().get(groupname);
			if (!grp.good())
				return false;
			auto usr = user().get(username);
			if (!usr.good())
				return false;

			return engine.destroy("USERGROUP", {  { "userid", std::to_string(usr.userid()) }, { "groupid", std::to_string(grp.groupid()) } });
		}

private:
		friend DataModel<UserGroup>;

		bool destroyGroup(std::string groupname)
		{
			auto grp = group().get(groupname);
			if (!grp.good())
				return false;

			return engine.destroy("USERGROUP", { { "groupid", std::to_string(grp.groupid()) } });
		}

		bool destroyUser(std::string username)
		{
			auto usr = user().get(username);
			if (!usr.good())
				return false;

			return engine.destroy("USERGROUP", { { "userid", std::to_string(usr.userid()) } });
		}

		UserGroup(DbEngine& engine):DataModel(engine)
		{
			init();
		}

		CapidModel::User& user()
		{
			static auto _user = CapidModel::User::getModel();
			return _user;
		}

		CapidModel::Group& group()
		{
			static auto _group = CapidModel::Group::getModel();
			return _group;
		}

		void init()
		{
			engine.createTable("USERGROUP", {
					{ "userid", "NUMBER" },
					{ "groupid", "NUMBER" },
					{ "ctime", "DATETIME" }
				}, {}, true);
			if (engine.tableCount("USERGROUP")==0)
				{
					/* Create users */
					create("admin", "admin");
				}
		}
	};
};
