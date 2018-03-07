#include "../lib/axond_module.hpp"
#include "../lib/glove/json.hpp"
#include "../lib/glove/utils.hpp"
#include "module_exception.hpp"
#include <iostream>
#include <memory>

#define MODULE_NAME "sshkeys"
#define MODULE_LONG_NAME "SSH Keys"
#define MODULE_VERSION "0.2"
#define MODULE_AUTHOR "Gaspar Fernandez"

namespace
{
	CapidModuleRestOutput internalError()
	{
		CapidModuleRestOutput cmro;
		cmro.output="{ \"error\": \"Internal error in module sshkeys\" }";
		return cmro;
	}

	CapidModuleRestOutput returnError(int code, std::string what)
	{
		CapidModuleRestOutput cmro;
		cmro.output="{ \"status\": \"error\", \"code\": "+std::to_string(code)+", \"message\": \""+what+"\" }";
		return cmro;
	}
	std::string underline(int len)
	{
		auto s = std::string(len+1, '-');
		s[len] = '\0';
		return s;
	}
	uint64_t fatoi(std::string& str)
	{
		try
			{
				return std::stold(str);
			}
		catch (std::exception &e)
			{
				return 0;
			}
	}

};

namespace SSHKeys
{
	class Keys
	{
	public:
		Keys(CapidModuleSafeDb& db, PluginDataModelFiltered& pd): db(db), pd(pd)
		{
			/* Create table sshkeys_KEYS */
			db.createTable("sshkeys_KEYS", { { "name", "TEXT UNIQUE" }, { "key", "TEXT UNIQUE" }, { "tags", "TEXT" }, { "description", "TEXT" }, { "ctime", "DATETIME" }, { "mtime", "DATETIME" } }, {}, false);

			std::cout <<"PLUGIN VERSION IN DB: "<<pd.getCurrentVersion()<<std::endl;

			/* auto data = db.getData("sshkeys_KEYS KEYS LEFT JOIN sshkeys_KEYGROUP KG ON KEYS.rowid=KG.keyid LEFT JOIN sshkeys_GROUP GRP ON GRP.rowid=KG.groupid", "KEYS.rowid AS keyid, KEYS.key, KEYS.name, KEYS.ctime, KEYS.mtime, GROUP_CONCAT(GRP.name) AS groups", {}, 0,0,"KEYS.rowid", ""); */
			auto data = db.getData("sshkeys_KEYS", "fingerprint", {}, 0,0,"", "");
			for (auto d: data)
				{
					for (auto x: d) {
						std::cout << x.first<< ": "<<x.second<<"\n";
					}
					std::cout << "--------\n\n";
				}


		}

		~Keys()
		{
		}

		uint64_t getKeyId(std::string name)
		{
			auto data = db.getData("sshkeys_KEYS", "rowid", { { "name", name } }, 0,0,"", "");
			if (data.size() != 1)
				return 0;
			else
				return std::stoul(data.front()["rowid"]);
		}

		CapidModuleRestOutput getKeysFromInput(CapidModuleRestInput& input)
		{
			CapidModuleRestOutput out;
			auto format = input.arguments["format"];
			if (format.empty())
				format = "json";

			auto keys = tokenize(input.rawData, "\n", defaultTrim);
			nlohmann::json result;

			for (auto k : keys)
				{
					if (k.empty())
						continue;
					if (k[0]=='#')
						continue;
					auto parts = tokenize(k, " ", defaultTrim);
					std::string extractedKey;
					std::string prev;
					for (auto p : parts)
						{
							if (prev.substr(0,4) == "ssh-")
								{
									extractedKey = prev+" "+p;
									break;
								}
							prev = p;
						}
					if (extractedKey.empty())
						continue;

					CapidModuleSafeDb::DbList conditions({ { "KEYS.key", extractedKey }, { "|KEYS.key LIKE", extractedKey+" %" } });
					auto data = db.getData("sshkeys_KEYS KEYS LEFT JOIN sshkeys_KEYGROUP KG ON KEYS.rowid=KG.keyid LEFT JOIN sshkeys_GROUP GRP ON GRP.rowid=KG.groupid", "KEYS.rowid AS keyid, KEYS.key, KEYS.name, KEYS.ctime, KEYS.mtime, GROUP_CONCAT(GRP.name) AS groups", conditions, 0,0,"KEYS.rowid", "");
					if (data.size()>0)
						result.push_back(data.front());
					else
						result.push_back(std::map<std::string, std::string>({ {"key", extractedKey}}));
				}
			if (format =="json")
				out.output=result.dump(2);
			else if (format =="txt")
				{
					for (auto el : result)
						{
							if (el["name"].is_null())
								continue;
							out.output+="  "+el["name"].get<std::string>()+" ("+el["keyid"].get<std::string>()+")";
							if (!el["groups"].is_null())
								out.output+=" [ groups="+el["groups"].get<std::string>()+"]";
							if (!el["status"].is_null())
								{
									auto status = el["status"].get<std::string>();
									if (status == "disabled")
										out.output+=" DISABLED";
								}
							out.output+="\n";
						}
				}
			return out;

		}

		CapidModuleRestOutput get(CapidModuleRestInput& input)
		{
			CapidModuleRestOutput out;
			try
				{
					auto id = input.special["id"];
					auto format = input.arguments["format"];
					if (format.empty())
						format = "json";

					std::list<std::map<std::string, std::string> > data;
					CapidModuleSafeDb::DbList conditions;
					if ( (!input.rawData.empty()) && (id.empty()) )
						return getKeysFromInput(input);

					if (!id.empty())
						{
							auto idn = fatoi(id);
							if (idn)
								{
									conditions = { { "KEYS.rowid", id } };
								}
							else if (id.front()=='@')
								{
									conditions = { { "GRP.name", id.substr(1) } };
									/* Group */
								}
							else
								{
									conditions = { { "KEYS.name", id } };
								}
						}
					data = db.getData("sshkeys_KEYS KEYS LEFT JOIN sshkeys_KEYGROUP KG ON KEYS.rowid=KG.keyid LEFT JOIN sshkeys_GROUP GRP ON GRP.rowid=KG.groupid", "KEYS.rowid AS keyid, KEYS.key, KEYS.name, KEYS.ctime, KEYS.mtime, GROUP_CONCAT(GRP.name) AS groups", conditions, 0,0,"KEYS.rowid", "");
					//					data = db.getData("sshkeys_KEYS", "rowid, *", conditions, 0,0,"", "");

					if (format == "plain")
						{
							for (auto k : data)
									out.output+=k["key"]+"\n";
						}
					else if (format=="text")
						{
							for (auto k: data)
								out.output+=k["name"]+"\n"+underline(k["name"].length())+"\n"+k["key"]+"\n";
						}
					else if (format=="json")
						{
							nlohmann::json jsout(data);
							out.output=jsout.dump(2);
						}
					else
						throw ModuleException("Unknown output format '"+format+"'", 2);
				}
			catch (ModuleException& e)
				{
					return returnError(e.code(), e.what());
				}
			catch (std::exception& e)
				{
					return returnError(-2, "Internal error: "+std::string(e.what()));
				}
			return out;
		}

		CapidModuleRestOutput post(CapidModuleRestInput& input)
		{
			CapidModuleRestOutput out;
			try
				{
					auto jinput = nlohmann::json::parse(input.rawData);
					auto _name = jinput["name"];
					auto _key = jinput["key"];
					auto _description = jinput["description"];
					auto _tags = jinput["tags"];
					std::string description, tags;

					if (!_description.is_null())
						description = _description.get<std::string>();
					if (!_tags.is_null())
						tags = _tags.get<std::string>();

					if ( (_name.is_null()) || (_key.is_null()) )
						throw ModuleException("Name or Key unspecified.");

					auto name = _name.get<std::string>();
					auto key = _key.get<std::string>();

					auto eid = db.insert("sshkeys_KEYS", {
								{ "name", name },
								{ "key", key },
								{ "description", description },
								{ "tags", tags }
							});
					if (!eid)
						throw ModuleException("Unable to insert key", 1);

					nlohmann::json jsout({ { "module", MODULE_NAME}, { "action", "create" }, { "id", std::to_string(eid) } });
					out.output=jsout.dump(2);
				}
			catch (ModuleException& e)
				{
					return returnError(e.code(), e.what());
				}
			catch (std::exception& e)
				{
					return returnError(-2, "Internal error: "+std::string(e.what()));
				}
			return out;
		}

		CapidModuleRestOutput patch(CapidModuleRestInput& input)
		{
			CapidModuleRestOutput out;
			try
				{
					auto jinput = nlohmann::json::parse(input.rawData);
					auto id = input.special["id"];
					if (id.empty())
						throw ModuleException("No key ID specified", 3);

					auto _name = jinput["name"];
					auto _key = jinput["key"];
					auto _description = jinput["description"];
					auto _tags = jinput["tags"];

					CapidModuleSafeDb::DbList conditions;
					auto idn = fatoi(id);
					if (idn)
						{
							conditions = { { "rowid", id } };
						}
					else
						{
							conditions = { { "name", id } };
						}

					auto current = db.getData("sshkeys_KEYS", "rowid,*", conditions, 0, 0, "", "");
					if (current.size()!=1)
						throw ModuleException("Cant find current key matching criteria", 4);

					CapidModuleSafeDb::DbList fields;
					std::string description, tags, name, key;

					if (!_description.is_null())
						fields.push_back(std::make_pair( "description", _description.get<std::string>()));
					if (!_name.is_null())
						fields.push_back(std::make_pair( "name", _name.get<std::string>()));
					if (!_key.is_null())
						fields.push_back(std::make_pair( "key", _key.get<std::string>()));
					if (!_tags.is_null())
						fields.push_back(std::make_pair( "tags", _tags.get<std::string>()));

					if (!db.update("sshkeys_KEYS", std::stoul(current.front()["rowid"]), fields))
						throw ModuleException("Cant update key");

					nlohmann::json jsout({ { "module", MODULE_NAME}, { "action", "update" }, { "id", current.front()["rowid"] } });
					out.output=jsout.dump(2);
				}
			catch (ModuleException& e)
				{
					return returnError(e.code(), e.what());
				}
			catch (std::exception& e)
				{
					return returnError(-2, "Internal error: "+std::string(e.what()));
				}
			return out;
		}


		CapidModuleRestOutput delet(CapidModuleRestInput& input)
		{
			CapidModuleRestOutput out;
			try
				{
					auto id = input.special["id"];
					if (id.empty())
						throw ModuleException("No key ID specified", 3);

					CapidModuleSafeDb::DbList conditions;
					auto idn = fatoi(id);
					if (idn)
						{
							conditions = { { "rowid", id } };
						}
					else
						{
							conditions = { { "name", id } };
						}

					auto current = db.getData("sshkeys_KEYS", "rowid,*", conditions, 0, 0, "", "");
					if (current.size()!=1)
						throw ModuleException("Cant find current key matching criteria", 4);

					db.destroyMulti("sshkeys_KEYGROUP", { { "keyid", current.front()["rowid"] } });

					if (!db.destroy("sshkeys_KEYS", std::stoul(current.front()["rowid"])))
						throw ModuleException("Cant delete key");

					nlohmann::json jsout({ { "module", MODULE_NAME}, { "action", "delete" }, { "id", current.front()["rowid"] } });
					out.output=jsout.dump(2);
				}
			catch (ModuleException& e)
				{
					return returnError(e.code(), e.what());
				}
			catch (std::exception& e)
				{
					return returnError(-2, "Internal error: "+std::string(e.what()));
				}
			return out;
		}

	private:
		CapidModuleSafeDb& db;
		PluginDataModelFiltered& pd;
	};

	class Groups
	{
	public:
		Groups(CapidModuleSafeDb& db, Keys& keys): db(db), keys(keys)
		{
			/* Create tables sshkeys_GROUP, sshkeys_KEYGROUP */
			db.createTable("sshkeys_GROUP",{ { "name", "TEXT UNIQUE" }, { "tags", "TEXT"}, { "description", "TEXT" }, { "ctime", "DATETIME" }, { "mtime", "DATETIME" } }, {}, false);
			db.createTable("sshkeys_KEYGROUP", { { "keyid", "NUMBER" }, { "groupid", "NUMBER" }, { "ctime", "DATETIME" }, {"UNIQUE(keyid,groupid)", "ON CONFLICT ABORT"} }, {},false);
		}

		~Groups()
		{
		}

		CapidModuleRestOutput get(CapidModuleRestInput& input)
		{
			CapidModuleRestOutput out;
			try
				{
					auto id = input.special["id"];

					std::list<std::map<std::string, std::string> > data;
					CapidModuleSafeDb::DbList conditions;
					if (!id.empty())
						{
							auto idn = fatoi(id);
							if (idn)
								{
									conditions = { { "GRP.rowid", id } };
								}
							else
								{
									conditions = { { "GRP.name", id } };
								}
						}
					data = db.getData("sshkeys_GROUP GRP LEFT JOIN sshkeys_KEYGROUP KG ON GRP.rowid=KG.groupid LEFT JOIN sshkeys_KEYS KEYS ON KEYS.rowid=KG.keyid", "GRP.rowid AS groupid, GRP.name, GRP.ctime, GRP.mtime, GROUP_CONCAT(KEYS.name) AS keys", conditions, 0,0,"GRP.rowid", "");

					nlohmann::json jsout(data);
					out.output=jsout.dump(2);
				}
			catch (ModuleException& e)
				{
					return returnError(e.code(), e.what());
				}
			catch (std::exception& e)
				{
					return returnError(-2, "Internal error: "+std::string(e.what()));
				}
			return out;
		}

		CapidModuleRestOutput post(CapidModuleRestInput& input)
		{
			CapidModuleRestOutput out;
			try
				{
					auto jinput = nlohmann::json::parse(input.rawData);
					auto _name = jinput["name"];
					auto _description = jinput["description"];
					auto _tags = jinput["tags"];
					auto _keys = jinput["keys"];
					std::vector<std::string> keys;

					std::string description, tags;

					if (!_description.is_null())
						description = _description.get<std::string>();
					if (!_tags.is_null())
						tags = _tags.get<std::string>();
					if (!_keys.is_null())
						keys = tokenize(_keys.get<std::string>(), ", ", defaultTrim);

					if (_name.is_null())
						throw ModuleException("Name unspecified.", 5);

					auto name = _name.get<std::string>();

					std::string successfulKeys ="";
					std::string errorKeys="";

					auto eid = db.insert("sshkeys_GROUP", {
								{ "name", name },
								{ "description", description },
								{ "tags", tags }
							});
					if (!eid)
						throw ModuleException("Unable to insert key group", 1);

					for (auto k : keys)
						{
							auto kid = this->keys.getKeyId(k);
							if (kid==0)
								errorKeys+=","+k;
							else
								{
									if (db.insert("sshkeys_KEYGROUP", {
												{"keyid", std::to_string(kid) },
												{ "groupid", std::to_string(eid) }
											}))
										successfulKeys+=","+k;
									else
										errorKeys+=","+k;
								}
						}
					std::map<std::string, std::string> _jsout = { { "module", MODULE_NAME}, { "action", "creategroup" }, { "id", std::to_string(eid) } };
					if (!errorKeys.empty())
						_jsout["errorKeys"]=trim(errorKeys, ", ");
					if (!successfulKeys.empty())
						_jsout["successfulKeys"]=trim(successfulKeys, ", ");

					nlohmann::json jsout(_jsout);
					out.output=jsout.dump(2);
				}
			catch (ModuleException& e)
				{
					return returnError(e.code(), e.what());
				}
			catch (std::exception& e)
				{
					return returnError(-2, "Internal error: "+std::string(e.what()));
				}
			return out;
		}

		CapidModuleRestOutput patch(CapidModuleRestInput& input)
		{
			CapidModuleRestOutput out;
			try
				{
					auto jinput = nlohmann::json::parse(input.rawData);
					auto id = input.special["id"];
					if (id.empty())
						throw ModuleException("No key ID specified", 3);

					CapidModuleSafeDb::DbList conditions;
					auto idn = fatoi(id);
					if (idn)
						{
							conditions = { { "rowid", id } };
						}
					else
						{
							conditions = { { "name", id } };
						}

					auto current = db.getData("sshkeys_GROUP", "rowid,*", conditions, 0, 0, "", "");
					if (current.size()!=1)
						throw ModuleException("Cant find current key matching criteria", 4);

					auto _name = jinput["name"];
					auto _description = jinput["description"];
					auto _tags = jinput["tags"];
					auto _addkeys = jinput["addkeys"];
					auto _delkeys = jinput["delkeys"];
					std::vector<std::string> addkeys, delkeys;
					std::list<std::pair<std::string, std::string>> toUpdate;
					std::string description, tags, name;

					if (!_description.is_null())
						toUpdate.push_back({"description", _description.get<std::string>() });
					if (!_tags.is_null())
						toUpdate.push_back({"tags", _tags.get<std::string>()});
					if (!_name.is_null())
						toUpdate.push_back({"name", _name.get<std::string>()});
					if (!_addkeys.is_null())
						addkeys = tokenize(_addkeys.get<std::string>(), ", ", defaultTrim);
					if (!_delkeys.is_null())
						delkeys = tokenize(_delkeys.get<std::string>(), ", ", defaultTrim);


					std::string successfulKeys_add ="";
					std::string errorKeys_add="";
					std::string successfulKeys_del ="";
					std::string errorKeys_del="";

					auto eid = std::stoul(current.front()["rowid"]);
					if (toUpdate.size()>0)
						{
							if (!db.update("sshkeys_GROUP", eid, toUpdate))
								throw ModuleException("Unable to update group.", 1);
						}
																/* Add keys  */
					for (auto k : addkeys)
						{
							auto kid = this->keys.getKeyId(k);
							if (kid==0)
								errorKeys_add+=","+k;
							else
								{
									if (db.insert("sshkeys_KEYGROUP", {
												{"keyid", std::to_string(kid) },
												{"groupid", std::to_string(eid) }
											}))
										successfulKeys_add+=","+k;
									else
										errorKeys_add+=","+k;
								}
						}
																/* Del keys */
					for (auto k : delkeys)
						{
							auto kid = this->keys.getKeyId(k);
							if (kid==0)
								errorKeys_del+=","+k;
							else
								{
									if (db.destroyMulti("sshkeys_KEYGROUP", {
												{"keyid", std::to_string(kid) },
												{"groupid", std::to_string(eid) }
											}))
										successfulKeys_del+=","+k;
									else
										errorKeys_del+=","+k;
								}
						}
					std::map<std::string, std::string> _jsout = { { "module", MODULE_NAME}, { "action", "updategroup" }, { "id", std::to_string(eid) } };
					if (!errorKeys_add.empty())
						_jsout["addErrorKeys"]=trim(errorKeys_add, ", ");
					if (!successfulKeys_add.empty())
						_jsout["addSuccessfulKeys"]=trim(successfulKeys_add, ", ");
					if (!errorKeys_del.empty())
						_jsout["delErrorKeys"]=trim(errorKeys_del, ", ");
					if (!successfulKeys_del.empty())
						_jsout["delSuccessfulKeys"]=trim(successfulKeys_del, ", ");

					nlohmann::json jsout(_jsout);
					out.output=jsout.dump(2);
				}
			catch (ModuleException& e)
				{
					return returnError(e.code(), e.what());
				}
			catch (std::exception& e)
				{
					return returnError(-2, "Internal error: "+std::string(e.what()));
				}
			return out;
		}

		CapidModuleRestOutput delet(CapidModuleRestInput& input)
		{
			CapidModuleRestOutput out;
			try
				{
					auto id = input.special["id"];
					if (id.empty())
						throw ModuleException("No key ID specified", 3);

					CapidModuleSafeDb::DbList conditions;
					auto idn = fatoi(id);
					if (idn)
						{
							conditions = { { "rowid", id } };
						}
					else
						{
							conditions = { { "name", id } };
						}

					auto current = db.getData("sshkeys_GROUP", "rowid,*", conditions, 0, 0, "", "");
					if (current.size()!=1)
						throw ModuleException("Cant find current key matching criteria", 4);

					auto eid = std::stoul(current.front()["rowid"]);
					db.destroyMulti("sshkeys_KEYGROUP", { { "groupid", current.front()["rowid"] } });
					if (!db.destroy("sshkeys_GROUP", eid))
						throw ModuleException("Unable to delete group.", 1);

					std::map<std::string, std::string> _jsout = { { "module", MODULE_NAME}, { "action", "deletegroup" }, { "id", std::to_string(eid) } };

					nlohmann::json jsout(_jsout);
					out.output=jsout.dump(2);
				}
			catch (ModuleException& e)
				{
					return returnError(e.code(), e.what());
				}
			catch (std::exception& e)
				{
					return returnError(-2, "Internal error: "+std::string(e.what()));
				}
			return out;
		}

	private:
		CapidModuleSafeDb& db;
		Keys& keys;
	};
};

class SSHKeysModule : public CapidModule
{
public:
	SSHKeysModule(): CapidModule(MODULE_NAME, MODULE_LONG_NAME, MODULE_VERSION, MODULE_AUTHOR, MODULE_ENDPOINT)
	{
	}

	bool afterInit()
	{
		_keys = std::make_shared<SSHKeys::Keys>(SSHKeys::Keys(db, plugindata));
		_groups = std::make_shared<SSHKeys::Groups>(SSHKeys::Groups(db, *_keys));
		return true;
	}

	SSHKeys::Keys& keys()
	{
		return *_keys;
	}
	SSHKeys::Groups& groups()
	{
		return *_groups;
	}
private:
	std::shared_ptr<SSHKeys::Keys> _keys;
	std::shared_ptr<SSHKeys::Groups> _groups;

};

/* With a smart pointer, when the lib is unloaded
   the pointer will be freed. */
std::shared_ptr<SSHKeysModule> module;

extern "C"
{
	bool init(CapidModuleShared& shared)
	{
		module = std::make_shared<SSHKeysModule>();

		if (!module->setProps(shared))
			return false;

		shared.addRestService("keys", "keys_get", "keys_post", "", "keys_patch", "keys_delete");
		shared.addRestService("groups", "groups_get", "groups_post", "", "groups_patch", "groups_delete");
		return true;
	}

	CapidModuleRestOutput keys_post(CapidModuleRestInput& input)
	{
		if (module==nullptr)
			return internalError();

		return module->keys().post(input);
	}

	CapidModuleRestOutput keys_patch(CapidModuleRestInput& input)
	{
		if (module==nullptr)
			return internalError();

		return module->keys().patch(input);
	}

	CapidModuleRestOutput keys_delete(CapidModuleRestInput& input)
	{
		if (module==nullptr)
			return internalError();

		return module->keys().delet(input);
	}

	CapidModuleRestOutput keys_get(CapidModuleRestInput& input)
	{
		if (module==nullptr)
			return internalError();

		return module->keys().get(input);
	}

	CapidModuleRestOutput groups_post(CapidModuleRestInput& input)
	{
		if (module==nullptr)
			return internalError();

		return module->groups().post(input);
	}

	CapidModuleRestOutput groups_patch(CapidModuleRestInput& input)
	{
		if (module==nullptr)
			return internalError();

		return module->groups().patch(input);
	}

	CapidModuleRestOutput groups_delete(CapidModuleRestInput& input)
	{
		if (module==nullptr)
			return internalError();

		return module->groups().delet(input);
	}

	CapidModuleRestOutput groups_get(CapidModuleRestInput& input)
	{
		if (module==nullptr)
			return internalError();

		return module->groups().get(input);
	}

}
