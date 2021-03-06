#pragma once

#include "datamodel.hpp"
#include "../../glove/utils.hpp"

namespace CapidModel
{
	class Group : public CapidModel::DataModel<Group>
	{
	public:
		class Data
		{
		public:
			Data(uint64_t groupid,
					 std::string name,
					 bool allowed,
					 std::string ctime,
					 std::string mtime):
				_groupid(groupid), _name(name),
				_allowed(allowed),
				_ctime(ctime), _mtime(mtime)
			{
			}
			Data (): _groupid(0)
			{
			}

			bool good()
			{
				return (_groupid>0);
			}

			uint64_t groupid()
			{
				return _groupid;
			}

			std::string name()
			{
				return _name;
			}

			bool allowed()
			{
				return _allowed;
			}

			std::string ctime()
			{
				return _ctime;
			}
			std::string mtime()
			{
				return _mtime;
			}

		private:
			uint64_t _groupid;
			std::string _name;
			bool _allowed;
			std::string _ctime;
			std::string _mtime;
		};

		~Group() {}

		bool exists(std::string name)
		{
			if (!checkGroupname(name))
				return false;

			auto rowid = engine.getRowid("GROUPS", { { "name", name } });
			return (rowid);
		}

		bool create(std::string groupname, bool allowed=true)
		{
			if (!checkGroupname(groupname))
				return false;

			return (engine.insert("GROUPS", {
					{ "name" , groupname },
					{	"allowed" , (allowed)?"1":"0" },
					{ "ctime" , "F(DATETIME('now')" },
					{ "mtime" , "F(DATETIME('now')" }}));
		}

		bool update(std::string groupname, bool allow)
		{
			if (!checkGroupname(groupname))
				return false;

			auto rowid = engine.getRowid("GROUPS", { { "name", groupname } });
			if (!rowid)
				return false;

			return (engine.update("GROUPS", rowid, {{ "mtime" , "F(DATETIME('now')" }, {"allowed", (allow)?"1":"0" }}));
		}

		bool destroy(std::string groupname)
		{
			if (!checkGroupname(groupname))
				return false;

			auto rowid = engine.getRowid("GROUPS", { { "name", groupname } });
			if (!rowid)
				return false;

			return engine.destroy("GROUPS", rowid);
		}

		Data get(std::string groupname)
		{
			if (!checkGroupname(groupname))
				return Data();

			auto data = engine.getData("GROUPS", "rowid,*", { { "name", groupname } }, "LIMIT 1");
			if (data.size())
				{
					auto row = data.front();
					return Data(std::stoi(row["rowid"]),
											row["name"],
											(row["allowed"] == "1"),
											row["ctime"],
											row["mtime"]);
				}
			return Data();
		}

		std::list <std::map < std::string, std::string > > getAll(std::string groupname="")
		{
			std::list <std::pair < std::string, std::string> > conditions;
			if (!groupname.empty())
				conditions = { { "GRP.name", groupname } };

			return engine.getData("GROUPS GRP JOIN USERGROUP USG ON GRP.rowid=USG.groupid JOIN USER USR ON USG.userid=USR.rowid", "GRP.rowid AS groupid, GRP.name, GRP.ctime, GRP.mtime, GRP.allowed, GROUP_CONCAT(USR.name) AS users", conditions, "GROUP BY GRP.rowid");
		}

	private:
		friend DataModel<Group>;

		Group(DbEngine& engine):DataModel(engine)
		{
			init();
		}

		bool checkGroupname(std::string name)
		{
			/* Group names MUST NOT have blank spaces, MUST HAVE >2 characters, MUST NOT START WITH @ or _ */
			if (name.length()<=2)
				return false;
			if (name.find(' ') != std::string::npos)
				return false;
			if ( (name[0]=='@') || (name[0]=='_') )
				return false;

			return true;
		}

		void init()
		{
			engine.createTable("GROUPS", {
					{ "name", "TEXT UNIQUE" },
					{ "ctime", "DATETIME" },
					{ "mtime", "DATETIME" },
					{ "allowed", "NUMBER" }
				}, {}, true);
			if (engine.tableCount("GROUPS")==0)
				{
					/* Create users */
					create("admin", true);
				}
		}
	};
};
