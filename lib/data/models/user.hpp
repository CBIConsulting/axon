#pragma once

#include "datamodel.hpp"
#include "../../glove/glovecoding.hpp"
#include "../../glove/utils.hpp"

namespace CapidModel
{
	class User : public CapidModel::DataModel<User>
	{
	public:
		class Data
		{
		public:
			Data(uint64_t userid,
					 std::string username,
					 std::string passhash,
					 bool allowed,
					 std::string ctime,
					 std::string mtime):
				_userid(userid), _username(username),
				_passhash(passhash), _allowed(allowed),
				_ctime(ctime), _mtime(mtime)
			{
			}
			Data (): _userid(0)
			{
			}

			bool good()
			{
				return (_userid>0);
			}

			uint64_t userid()
			{
				return _userid;
			}

			std::string username()
			{
				return _username;
			}

			bool allowed()
			{
				return _allowed;
			}

			std::string passhash()
			{
				return _passhash;
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
			uint64_t _userid;
			std::string _username;
			std::string _passhash;
			bool _allowed;
			std::string _ctime;
			std::string _mtime;
		};

		~User() {}

		bool exists(std::string name)
		{
			if (!checkUsername(name))
				return false;

			auto rowid = engine.getRowid("USER", { { "name", name } });
			return (rowid);
		}

		bool create(std::string username, std::string password, bool allowed=true)
		{
			if (!checkUsername(username))
				return false;

			auto passhash = this->hashMethod(password);
			return (engine.insert("USER", {
					{ "name" , username },
					{ "passhash", passhash },
					{	"allowed" , (allowed)?"1":"0" },
					{ "ctime" , "F(DATETIME('now')" },
					{ "mtime" , "F(DATETIME('now')" }}));
		}

		bool update(std::string username, std::string password)
		{
			if (!checkUsername(username))
				return false;
			auto rowid = engine.getRowid("USER", { { "name", username } });
			if (!rowid)
				return false;

			return (engine.update("USER", rowid, {{ "mtime" , "F(DATETIME('now')" }, {"passhash", this->hashMethod(password) }}));
		}

		bool update(std::string username, bool allow)
		{
			if (!checkUsername(username))
				return false;

			auto rowid = engine.getRowid("USER", { { "name", username } });
			if (!rowid)
				return false;

			return (engine.update("USER", rowid, {{ "mtime" , "F(DATETIME('now')" }, {"allowed", (allow)?"1":"0" }}));
		}

		bool destroy(std::string username)
		{
			if (!checkUsername(username))
				return false;

			auto rowid = engine.getRowid("USER", { { "name", username } });
			if (!rowid)
				return false;

			return engine.destroy("USER", rowid);
		}

		std::list <std::map < std::string, std::string > > getAll(std::string username="")
		{
			std::list <std::pair < std::string, std::string> > conditions;
			if (!username.empty())
				conditions = { { "USR.name", username } };

			return engine.getData("USER USR JOIN USERGROUP USG ON USR.rowid=USG.userid JOIN GROUPS GRP ON USG.groupid=GRP.rowid", "USR.rowid AS userId, USR.name, USR.ctime, USR.mtime, USR.allowed, GROUP_CONCAT(GRP.name) AS groups", conditions, "GROUP BY USR.rowid");
		}

		Data get(std::string username)
		{
			if (!checkUsername(username))
				return Data();

			auto data = engine.getData("USER", "rowid,*", { { "name", username } }, "LIMIT 1");
			if (data.size())
				{
					auto row = data.front();
					return Data(std::stoi(row["rowid"]),
											row["name"],
											row["passhash"],
											(row["allowed"] == "1"),
											row["ctime"],
											row["mtime"]);
				}
			return Data();
		}

		/* Returns user data ONLY if user matches the hash.
		 hashFormat = "data.data.data.[PASSHASH].data.data.data
		 the returned hashFormat will be hashed and compared the input hash*/
		Data match(std::string username, std::string hashFormat, std::string hash)
		{
			Data d = get(username);
			if (!d.good())
				return Data();

			auto _hash = this->hashMethod(string_replace(hashFormat, { {"[PASSHASH]", d.passhash() } }));

			if (_hash == hash)
				{
					return d;
				}

			return Data();
		}

		std::function<std::string(std::string)> getHashMethod() const
		{
			return hashMethod;
		}
	private:
		friend DataModel<User>;
		std::function<std::string(std::string)> hashMethod = GloveCoding::sha1_b64;

		User(DbEngine& engine):DataModel(engine)
		{
			init();
		}

		bool checkUsername(std::string name)
		{
			/* User names MUST NOT have blank spaces, NO commas, MUST HAVE >2 characters, MUST NOT START WITH @ or _ */
			if (name.length()<=2)
				return false;
			if (name.find(' ') != std::string::npos)
				return false;
			if (name.find(',') != std::string::npos)
				return false;
			if ( (name[0]=='@') || (name[0]=='_') )
				return false;

			return true;
		}

		void init()
		{
			engine.createTable("USER", {
					{ "name", "TEXT UNIQUE" },
					{ "passhash", "TEXT" },
					{ "ctime", "DATETIME" },
					{ "mtime", "DATETIME" },
					{ "allowed", "NUMBER" }
				}, {}, true);
			if (engine.tableCount("USER")==0)
				{
					/* Create users */
					create("admin", "admin", true);
				}
		}
	};
};
