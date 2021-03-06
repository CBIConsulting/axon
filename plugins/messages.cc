#include "../lib/axond_module.hpp"
#include "../lib/glove/json.hpp"
#include "../lib/glove/utils.hpp"
#include "../lib/glove/utils.hpp"
#include "../lib/mailer.hpp"
#include "module_exception.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <functional>
#include <memory>
#include <ctime>

#define MODULE_NAME "messages"
#define MODULE_LONG_NAME "Messenger"
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
	time_t parseTime(std::string timeStr)
	{
		std::list<std::pair<struct tm, char*>> times;
		std::list<std::string> formats = { "%d/%b/%Y:%T", "%Y%m%d %H:%M:%S", "%Y/%m/%d %H:%M:%S",
																			 "%d-%b-%Y:%T", "%Y-%m-%d %H:%M:%S", "%Y%m%d" };

		for (auto f : formats)
			{
				struct tm tm;
				std::memset(&tm, 0, sizeof(tm));
				char* p = strptime(timeStr.c_str(), f.c_str(), &tm);
				times.push_back({tm, p});
			}
		auto el = std::max_element(times.begin(), times.end(), [](std::pair<struct tm, char*> a, std::pair<struct tm, char*> b)
															 {
																 return (a.second<b.second);
															 });
		if (el->second == NULL)
			throw ModuleException("Unparsed time string: "+timeStr, 12);

		return mktime(&el->first);
	}

	static std::string put_time( const std::tm* tmb, const char* fmt )
	{
		std::string s( 128, '\0' );
		size_t written;
		while( !(written=strftime( &s[0], s.size(), fmt, tmb ) ) )
			s.resize( s.size() + 128 );
		s[written] = '\0';

		return s.c_str();
	}

};

namespace Messages
{
	#define EVENT_ARGS std::pair<uint64_t, std::string>& type, std::pair<uint64_t, std::string>& category, const std::string& data, uint64_t status, const std::string& message, const std::string& ipAddress
	std::map < std::string, std::function<uint64_t (EVENT_ARGS)> > eventFuncs;

	uint64_t logFile(EVENT_ARGS)
	{
		/* Timeformat may be configured */
		/* data can be cached to avoid re-parsing */
		auto _data = nlohmann::json::parse(data);
		auto _filename = _data["filename"];

		if (!_filename.is_string())
			throw ModuleException ("No filename found to log");
		auto filename = _filename.get<std::string>();

		std::ofstream outfile;

		outfile.open(filename, std::ios::out | std::ios_base::app);
		if (outfile.fail())
			throw ModuleException("Could not write to file");

		time_t timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		std::tm tm = *std::localtime(&timestamp);
		std::string fromTime = put_time(&tm,  "%F %T");

		outfile << "["<<fromTime<<"] ["<<type.second<<";"<<category.second<<";"<<status<<";"<<ipAddress<<"] "<<message<<"\n";

		outfile.close();
		/* Log rotating... compressing... etc */
		return 1;
	}

	uint64_t sendMail(EVENT_ARGS)
	{
		auto _data = nlohmann::json::parse(data);
		auto _to = _data["to"];
		auto _subject = _data["to"];
		std::string subject;

		time_t timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		std::tm tm = *std::localtime(&timestamp);
		std::string fromTime = put_time(&tm,  "%F %T");

		std::string body = "You have a new notification from AXOND\nTimestamp: "+fromTime+"\nType: "+type.second+"\nCategory: "+category.second+"\nStatus: "+std::to_string(status)+"\nOriginating IP: "+ipAddress+"\nMessage: "+message;
		if (!_to.is_string())
			throw ModuleException ("No filename found to log");
		auto to = _to.get<std::string>();
		subject = (!_subject.is_string())?"Axond notification":_subject.get<std::string>();

		auto ress = Mailer::sendmail("axond@test.com", to, subject, body);
		if (ress<0)
			throw ModuleException("Cannot send mail. Error: "+std::to_string(ress));
		return 1;
	}

	#undef EVENT_ARGS

	void initEvents()
	{
		eventFuncs = {
			{ "logFile", logFile },
			{ "sendMail", sendMail }
		};
	}
	/* Funciones que trigean eventos */
	class MessagesMain
	{
	public:
		MessagesMain(CapidModuleSafeDb& db): db(db)
		{
			/* Create table sshkeys_KEYS */
			/* Type (ERROR, WARNING, NOTICE...) */
			/* Category (SECURITY, CONTROL or Application ID */
			/* Status (Error code, Status code...) */
			db.createTable("messages_MESSAGES", {
					{ "type", "NUMBER" },
					{ "category", "NUMBER" },
					{ "status", "NUMBER" },
					{ "userid", "NUMBER" },
					{ "local", "NUMBER" },
					{ "ipAddress", "TEXT" },
					{ "ctime", "DATETIME" },
					{ "message", "TEXT" },
					{ "tags", "TEXT" } }, {}, false);

			db.createTable("messages_TYPES", {
					{ "name", "TEXT" },
					{ "ctime", "DATETIME" },
					{ "UNIQUE(name)", "ON CONFLICT ABORT" }
				}, {}, false);

			db.createTable("messages_CATEGORIES", {
					{ "name", "TEXT" },
					{ "ctime", "DATETIME" },
					{ "UNIQUE(name)", "ON CONFLICT ABORT" }
				}, {}, false);

			db.createTable("messages_EVENTS", {
					{ "type", "NUMBER" },
					{ "category", "NUMBER" },
					{ "ctime", "DATETIME" },
					{ "eventCall", "TEXT" },
					{ "eventData", "TEXT" }
				}, {}, false);
			db.createTable("messages_REMEMBER", {
					{ "name", "TEXT" },
					{ "userid", "NUMBER" },
					{ "lastrow", "NUMBER" },
					{ "ctime", "DATETIME" },
					{ "mtime", "DATETIME" },
					{ "UNIQUE(name, userid)", "ON CONFLICT ABORT" }
				}, {}, false);
		}

		~MessagesMain()
		{
		}

		std::string getFromData(const std::list<std::map<std::string, std::string> >& list, uint64_t id)
		{
			for (auto it : list)
				{
					uint64_t rowid = std::stod(it["rowid"]);
					if (rowid == id)
						return it["name"];
				}
			return "";
		}

		uint64_t findKVs(std::string table, std::string name, bool autocreate=false)
		{
			std::list <std::pair < std::string, std::string> > conditions = { { "name", name } };

			auto data = db.getData(table, "rowid, *", conditions, 0, 1, "", "");
			nlohmann::json jsout(data);

			if (data.size()==0)
				{
					if (autocreate)
						{
							std::list <std::pair < std::string, std::string> > list = {	{ "name", name } };
							return db.insert(table, list);
						}
				}
			else
				return std::stod(data.front()["rowid"]);

			throw ModuleException("Element not found in "+table);
		}

		uint64_t findType(std::string type, bool autocreate=true)
		{
			return findKVs("messages_TYPES", type, autocreate);
		}

		uint64_t findCategory(std::string type, bool autocreate=true)
		{
			return findKVs("messages_CATEGORIES", type, autocreate);
		}

		uint64_t findDataByName(const std::list<std::map<std::string, std::string> >& list, std::string name)
		{
			for (auto it : list)
				{
					if (it["name"] == name)
						return std::stold(it["rowid"]);
				}
			return 0;
		}

		std::string findName(const std::string& table, uint64_t id)
		{
			auto data = db.getData(table, "rowid, *", { { "rowid", std::to_string(id) } }, 0, 0, "", "");
			if (data.size()>0)
				return data.front()["name"];

			return "";
		}

		uint64_t findRemember(const std::string name, uint64_t userid, bool getId=false)
		{
			if (name.empty())
				return 0;

			std::list <std::pair < std::string, std::string> > conditions = { { "name", name }, { "userid", std::to_string(userid) } };

			auto data = db.getData("messages_REMEMBER", "rowid, *", conditions, 0, 1, "", "");
			if (data.size()==0)
				return 0;

			return (getId)?std::stoul(data.front()["rowid"]):std::stoul(data.front()["lastrow"]);
		}

		void createRemember(const std::string name, uint64_t userid, const uint64_t lastrowid)
		{
			if (name.empty())
				return;

			auto remId = findRemember(name, userid, true);

			if (remId)
				{
					db.update("messages_REMEMBER", remId, { { "lastrow", std::to_string(lastrowid) } });
				}
			else
				{
					db.insert("messages_REMEMBER", { { "lastrow", std::to_string(lastrowid) }, { "userid", std::to_string(userid) }, { "name", name } });
				}
		}

		CapidModuleRestOutput get(CapidModuleRestInput& input)
		{
			/* En el futuro habrá permisos para la visualización y creación de mensajes.
			   Ciertas categorías/tipos de mensaje sólo podrán ser vistos o creados
			   por determinados usuarios/grupos.
			*/

			CapidModuleRestOutput out;
			auto typeList = db.getData("messages_TYPES", "rowid, *", {}, 0, 0, "", "");
			auto catList = db.getData("messages_CATEGORIES", "rowid, *", {}, 0, 0, "", "");

			try
				{
					std::list <std::pair < std::string, std::string> > conditions;
					uint64_t count=200;

					auto params = tokenize(input.special["id"],",", defaultTrim);
					auto format = input.arguments["format"];
					auto _remember = input.arguments["remember"];
					auto remember = findRemember(_remember, input.userId);

					if (format.empty())
						format = "json";

					if (params.size()==0)
						{

						}
					for (auto p : params)
						{
							auto _p = tokenize(p, "=", defaultTrim);
							if (_p.size()!=2)
								throw ModuleException("Malformed condition '"+p+"'", 6);

							if (_p[0] == "from")
								{
									time_t timestamp = parseTime(_p[1]);
									std::tm tm = *std::localtime(&timestamp);
									std::string fromTime = put_time(&tm,  "%F %T");
									conditions.push_back({"ctime>=", fromTime});
								}
							else if (_p[0] == "count")
								{
									count = std::stoul(_p[1]);
								}
							else if (_p[0] == "type")
								{
									uint64_t typeId=0;
									if (_p[1].find_first_not_of( "0123456789" ) == std::string::npos)
										typeId = std::stold(_p[1]);
									else
										typeId = this->findDataByName(typeList, _p[1]);

									if (typeId)
										conditions.push_back({"type=", std::to_string(typeId)});
								}
							else if (_p[0] == "category")
								{
									uint64_t catId=0;
									if (_p[1].find_first_not_of( "0123456789" ) == std::string::npos)
										catId = std::stold(_p[1]);
									else
										catId = this->findDataByName(catList, _p[1]);

									if (catId)
										conditions.push_back({"category=", std::to_string(catId)});
								}

						}
					if (remember)
						conditions.push_back({"rowid>", std::to_string(remember)});

					auto data = db.getData("messages_MESSAGES", "rowid, *", conditions, 0, count, "", "ctime ASC");
					if ( (!_remember.empty()) && (data.size()>0) )
						{
							auto lastrowid = std::stoul(data.back()["rowid"]);
							createRemember(_remember, input.userId, lastrowid);
						}
					for (auto& elem : data)
						{
							auto t = std::stold(elem["type"]);
							auto c = std::stold(elem["category"]);
							elem["typeStr"] = this->getFromData(typeList, t);
							elem["categoryStr"] = this->getFromData(catList, t);
						}

					if (format == "json")
						{
							nlohmann::json jsout(data);
							out.output=jsout.dump(2);
						}
					else if (format == "txt")
						{
							for (auto& it : data)
								{
									std::string flags = "";
									if (std::stold(it["local"]))
											flags+="local";

									out.output+=it["ctime"]+" ("+it["ipAddress"]+") type="+it["typeStr"]+" cat="+it["category"]+" status="+std::to_string(std::stold(it["status"]))+" - "+it["message"]+" - Flags: "+((flags.empty())?"none":flags)+"\n";
								}
						}
					else if (format == "csv")
						{
							auto quot = [](std::string input) {
								return quote(input, "\"", "\\");
							};
							out.output=quot("ID")+", "+quot("IP Address")+", "+quot("Type")+", "+quot("Category")+", "+quot("Status")+", "+quot("Flags")+", "+quot("Message")+"\n";
							for (auto& it : data)
								{
									std::string flags = "";
									if (std::stold(it["local"]))
											flags+="local";

									out.output+=quot(it["ctime"])+", "+quot(it["ipAddress"])+", "+quot(it["typeStr"])+", "+quot(it["category"])+", "+quot(std::to_string(std::stold(it["status"])))+", "+quot(flags)+", "+quot(it["message"])+"\n";
								}
						}
					else
						throw ModuleException("Format '"+format+"' not recognised");
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

		void triggerEvents(std::pair<uint64_t, std::string> type, std::pair<uint64_t, std::string> category, uint64_t status, std::string& ipAddress, std::string& message)
		{
			try
				{
					std::list <std::pair < std::string, std::string> > conditions = { { "type", std::to_string(type.first) }, { "category", std::to_string(category.first) },
																																						{ "|type", std::to_string(type.first) }, { "category", "0" },
																																						{ "|type", "0" }, { "category", std::to_string(category.first) }};
					auto events = db.getData("messages_EVENTS", "rowid, *", conditions, 0, 0, "", "ctime ASC");

					for (auto e : events)
						{
							auto ev = eventFuncs.find(e["eventCall"]);
							if (ev == eventFuncs.end())
								throw ModuleException("No event func "+e["eventCall"]+" found");

							if (!ev->second(type, category, e["eventData"], status, message, ipAddress))
								throw ModuleException("Trigger "+ev->first+" failed");
						}
				}
			catch (std::exception& e)
				{
					std::cerr << "Error triggering event: "<< e.what()<<"\n";
				}
		}

		CapidModuleRestOutput post(CapidModuleRestInput& input)
		{
			CapidModuleRestOutput out;
			try
				{
					auto jinput = nlohmann::json::parse(input.rawData);
					auto _message = jinput["message"];
					auto _type = jinput["type"];
					auto _category = jinput["category"];
					auto _status = jinput["status"];
					std::string typeStr = "default";
					std::string categoryStr = "default";

					uint64_t type=0, category=0, status=0;
					std::string message, tags;
					if (_message.is_null())
						throw ModuleException("Please, leave your message");

					message = _message.get<std::string>();

					if (!_type.is_null())
						{
							/* Si son numeros bien, si no, lo metemos en una tabla aparte, a ver que tal  */
							if (_type.is_number())
								{
									type = _type.get<uint64_t>();
									typeStr = findName("messages_TYPES", type);
								}
							else if (_type.is_string())
								{
									typeStr = _type.get<std::string>();
									type = this->findType(typeStr);
								}
							else
								throw ModuleException("Type must be number or string");
						}
					if (!_category.is_null())
						{
							if (_category.is_number())
								{
									category = _category.get<uint64_t>();
									categoryStr = findName("messages_CATEGORIES", category);
								}
							else if (_category.is_string())
								{
									categoryStr = _category.get<std::string>();
									category = this->findCategory(categoryStr);
								}
							else
								throw ModuleException("Category must be number or string");
						}
					if (!_status.is_null())
						{
							if (_status.is_number())
								status = _status.get<uint64_t>();
							else
								throw ModuleException("Status must be a number");
						}
					std::list <std::pair < std::string, std::string> > list = {
						{ "type", std::to_string(type) },
						{ "category", std::to_string(category) },
						{ "status", std::to_string(status) },
						{ "userid", std::to_string(input.userId) },
						{ "local", std::to_string(input.local?1:0) },
						{ "ipAddress", input.ipAddress },
						{ "message", message },
						{ "tags", tags }};
					auto eid = db.insert("messages_MESSAGES", list);
					if (!eid)
						throw ModuleException("There was a problem creating message");

					triggerEvents({type, typeStr}, {category, categoryStr}, status, input.ipAddress, message);
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

		bool isAdmin(CapidModuleRestInput& input)
		{
			return ( (input.user=="admin") ||
							 (std::find(input.groups.begin(), input.groups.end(), "admin") != input.groups.end()) );
		}

		CapidModuleRestOutput eventsget(CapidModuleRestInput& input)
		{
			CapidModuleRestOutput out;
			try
				{
					if (!isAdmin(input))
						throw ModuleException("You must be admin to handle exceptions");

					auto data = db.getData("messages_EVENTS", "rowid, *", {}, 0, 0, "", "ctime ASC");
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

		CapidModuleRestOutput eventspost(CapidModuleRestInput& input)
		{
			CapidModuleRestOutput out;
			try
				{
					if (!isAdmin(input))
						throw ModuleException("You must be admin to handle exceptions");

					auto jinput = nlohmann::json::parse(input.rawData);
					auto _call = jinput["event"];
					auto _type = jinput["type"];
					auto _category = jinput["category"];
					auto _data = jinput["data"];

					uint64_t type=0, category=0, status=0;
					std::string message, tags;

					if (!_type.is_null())
						{
							/* Si son numeros bien, si no, lo metemos en una tabla aparte, a ver que tal  */
							if (_type.is_number())
								type = _type.get<uint64_t>();
							else if (_type.is_string())
								type = this->findType(_type.get<std::string>());
							else
								throw ModuleException("Type must be number or string");
						}
					if (!_category.is_null())
						{
							if (_category.is_number())
								category = _category.get<uint64_t>();
							else if (_category.is_string())
								category = this->findCategory(_category.get<std::string>());
							else
								throw ModuleException("Category must be number or string");
						}
					if ( (_call.is_null()) || (!_call.is_string()) )
						throw ModuleException("Call must be a string");
					std::string evData;
					if (_data.is_string())
						evData = _data.get<std::string>();
					else if (_data.is_object())
						evData = _data.dump(2);
					else
						throw ModuleException("Data must be a string or object");

					std::list <std::pair < std::string, std::string> > list = {
						{ "type", std::to_string(type) },
						{ "category", std::to_string(category) },
						{ "eventCall", _call.get<std::string>() },
						{ "eventData", evData }};
					auto eid = db.insert("messages_EVENTS", list);
					if (!eid)
						throw ModuleException("There was a problem creating event");

					nlohmann::json jsout({ { "module", MODULE_NAME}, { "action", "createevent" }, { "id", std::to_string(eid) } });
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
		};
	};

	class MessagesModule : public CapidModule
{
public:
	MessagesModule(): CapidModule(MODULE_NAME, MODULE_LONG_NAME, MODULE_VERSION, MODULE_AUTHOR, MODULE_ENDPOINT)
	{
		Messages::initEvents();
	}

	bool afterInit()
	{
		_messages = std::make_shared<Messages::MessagesMain>(Messages::MessagesMain(db));

		return true;
	}

	Messages::MessagesMain& messages()
	{
		return *_messages;
	}
private:
	std::shared_ptr<Messages::MessagesMain> _messages;
};

/* With a smart pointer, when the lib is unloaded
   the pointer will be freed. */
	std::shared_ptr<MessagesModule> module;

	extern "C"
{
	bool init(CapidModuleShared& shared)
	{
		module = std::make_shared<MessagesModule>();
		if (!module->setProps(shared))
			return false;

		shared.addRestService("messages", "messages_get", "messages_post", "", "", "");
		shared.addRestService("events", "events_get", "events_post", "", "", "");
		return true;
	}

	CapidModuleRestOutput messages_post(CapidModuleRestInput& input)
	{
		if (module==nullptr)
			return internalError();

		return module->messages().post(input);
	}

	CapidModuleRestOutput messages_get(CapidModuleRestInput& input)
	{
		if (module==nullptr)
			return internalError();

		return module->messages().get(input);
	}
	CapidModuleRestOutput events_post(CapidModuleRestInput& input)
	{
		if (module==nullptr)
			return internalError();

		return module->messages().eventspost(input);
	}

	CapidModuleRestOutput events_get(CapidModuleRestInput& input)
	{
		if (module==nullptr)
			return internalError();

		return module->messages().eventsget(input);
	}

}
