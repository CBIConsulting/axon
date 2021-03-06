#include "../lib/axond_module.hpp"
#include "../lib/glove/json.hpp"
#include "../lib/glove/utils.hpp"
#include "module_exception.hpp"
#include <iostream>
#include <memory>
#include <ctime>

#define MODULE_NAME "machines"
#define MODULE_LONG_NAME "Machines"
#define MODULE_VERSION "0.1"
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

	time_t parseTime(std::string timeStr, bool utc=false)
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

		char* currentZone = getenv("TZ");
		if (utc)
			{
				setenv("TZ", "UTC", 1);
				tzset();
			}
		time_t res = mktime(&el->first);
		if (utc) {
			if (currentZone)
				setenv("TZ", currentZone,1);
			else
				unsetenv("TZ");
			tzset();
		}
		return res;
	}

	uint64_t timeSince(std::string timestr, bool utc=true)
	{
		auto last = parseTime(timestr,utc);
		time_t now = time(NULL);

		return now-last;
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

namespace Machines
{
	class MachinesMain
	{
	public:
		MachinesMain(CapidModuleSafeDb& db): db(db)
		{
			db.createTable("machines_MACHINES", {
					{ "type", "NUMBER" },
					{ "name", "TEXT" },
					{ "description", "TEXT" },
					{ "fixedIp", "NUMBER" }, /* Ip fija o dinamica */
					{ "maxHeartbeatTime", "NUMBER" }, /* Seconds until machine is declared offline */
					{ "ctime", "DATETIME" },
					{ "mtime", "DATETIME" }
				}, {}, false);
			db.createTable("machines_DATA", {
					{ "machineId", "NUMBER" },
					{ "internalId", "TEXT UNIQUE" },
					{ "location", "TEXT" },
					{ "lastIp", "TEXT" },
					{ "lastHeartbeat", "DATETIME" },
					{ "userid", "NUMBER" },
					{ "ctime", "DATETIME" },
					{ "mtime", "DATETIME" },
					{ "statusval", "NUMBER" },
					{ "statustxt", "TEXT" }}, {}, false);
			db.createTable("machines_LOG", {
					{ "machineId", "NUMBER" },
					{ "machineDataId", "NUMBER" },
					{ "message", "TEXT" },
					{ "userid", "NUMBER" },
					{ "ipAddress", "TEXT" },
					{ "origin", "TEXT" },	/* Medio que origina el mensaje. Puede ser el mismo equipo o puede ser otro */
					{ "level", "NUMBER" },
					{ "ctime", "DATETIME" }}, {}, false);
		}

		~MachinesMain()
		{
		}

		void classify(std::vector<std::map<std::string, std::string> >& fdata,
									std::map <std::string, std::vector<std::map<std::string, std::string> > >& machineNames,
									std::list<std::map<std::string, std::string> >& data,
									CapidModuleRestInput& input)
		{
			for (auto m : data)
				{
					auto muserid = fatoi(m["userid"]);
					if (input.guest)
						continue;

					if ( (input.user=="admin") ||
							 (std::find(input.groups.begin(), input.groups.end(), "admin") != input.groups.end()) ||
							 (input.ipAddress == m["lastIp"]))
						{
							std::map<std::string, std::string> single = {
								{ "cluster_name", m["name"] },
								{ "cluster_type", std::to_string(fatoi(m["type"])) },
								{ "machine_location", m["location"] },
								{ "cluster_ctime", m["mctime"] },
								{ "cluster_description", m["description"] },
								{ "cluster_fixedIp", m["fixedIp"] },
								{ "machine_lastHeartbeat", m["lastHeartbeat"] },
								{ "cluster_id", std::to_string(fatoi(m["machineId"])) },
								{ "cluster_maxHeartbeatTime", std::to_string(fatoi(m["maxHeartbeatTime"])) },
								{ "machine_id", m["internalId"] },
								{ "machine_ip", m["lastIp"] },
								{ "machine_status", m["statustxt"] },
								{ "machine_statuscode", std::to_string(fatoi(m["statusval"])) },
								{ "machine_ctime", m["ctime"] },
								{ "machine_mtime", m["mtime"] }
							};
							fdata.push_back(single);

							machineNames[single["cluster_name"]].push_back(fdata.back());
						}
				}
		}

		std::string machinesTextOutput(std::map <std::string, std::vector<std::map<std::string, std::string> > >& machineNames)
		{
			std::stringstream sst;
			for (auto i : machineNames)
				{
					if (i.second.size()>1)
						{
							auto mach = i.second[0];
							sst << "MACHINE: "+mach["cluster_name"]+"; Type: "+mach["cluster_type"]+"; Heartbeat: "+mach["maxHeartbeatTime"]+"s; Description: "+mach["cluster_description"]+" ("+mach["cluster_ctime"]+")\n";
							sst << "  "<<mach["machine_id"]<<" : "<<mach["machine_location"]<<" ( IP: "<<mach["machine_ip"]<<")\n";
							sst << "  Status: ("<<mach["machine_statuscode"]<<") "<<mach["machine_status"]<<"\n";
							sst << "  Last heartbeat: "<<mach["machine_lastHeartbeat"]<<" ( "<<timeSince(mach["machine_lastHeartbeat"])<<"secs ago)\n";
						}
					else
						{
							auto mach = i.second[0];
							sst << "CLUSTER: "+mach["cluster_name"]+"; Type: "+mach["cluster_type"]+"; Heartbeat: "+mach["maxHeartbeatTime"]+"s; Description: "+mach["cluster_description"]+" ("+mach["cluster_ctime"]+")\n";
							for (auto ss : i.second)
								{
									sst << "   * "<<ss["machine_id"]<<" : "<<ss["machine_location"]<<" ( IP: "<<ss["machine_ip"]<<")\n";
									sst << "     Status: ("<<ss["machine_statuscode"]<<") "<<ss["machine_status"]<<"\n";
									sst << "     Last heartbeat: "<<ss["machine_lastHeartbeat"]<<" ( "<<timeSince(ss["machine_lastHeartbeat"])<<"secs ago)\n";
								}
						}
				}
			return sst.str();
		}

		CapidModuleRestOutput getMachineLogInfo(CapidModuleRestInput& input, std::string select, std::string outputType)
		{
			std::list<std::map<std::string, std::string> > data;
			std::list<std::map<std::string, std::string> > data_out;
			CapidModuleSafeDb::DbList conditions;
			auto tables = "machines_LOG mLOG LEFT JOIN machines_DATA mDATA ON mLOG.machineDataId=mDATA.rowid LEFT JOIN machines_MACHINES mMACH ON mDATA.machineId=mMACH.rowid";
			auto fields = "mLOG.message,mLOG.userid,mLOG.ipAddress,mLOG.origin,mLOG.level,mLOG.ctime AS logtime, mDATA.rowid, mMACH.type, mMACH.name, mMACH.description, mMACH.maxHeartbeatTime, mMACH.ctime AS mctime, mMACH.mtime AS mmtime, mMACH.fixedIp, mDATA.*";
			if (select =="id")
				{
					conditions.push_back( { "internalId", input.special["id"]});
				}
			else if (select == "name")
				{
					conditions.push_back( { "name", input.special["id"]});
				}
			else if (select == "type")
				{
					conditions.push_back( { "type", input.special["id"]});
				}
			else if (select == "all")
				{
					/* No more conditions */
				}
			else
				throw ModuleException("Unexpected "+select+" for select argument");

			data = db.getData(tables, fields, conditions, 0, 0, "", "logtime DESC");
			for (auto d : data)
				{
					if ( (input.user=="admin") ||
							 (std::find(input.groups.begin(), input.groups.end(), "admin") != input.groups.end()) ||
							 (input.ipAddress == d["lastIp"]))
						{

							data_out.push_back({
									{ "time", d["logtime"] },
									{ "message", d["message"] },
									{ "name", d["name"] },
									{ "id", d["internalId"] },
									{ "level", std::to_string(fatoi(d["level"])) },
									{ "origin", d["origin"] },
									{ "userid" , std::to_string(fatoi(d["userid"])) },
									{ "ip" , d["ipAddress"] }
								});
						}
				}
			CapidModuleRestOutput out;
			if (outputType=="json")
				{
					nlohmann::json jsout(data_out);
					out.output=jsout.dump(2);
				}
			else if (outputType=="text")
				{
					std::stringstream ss;
					for (auto l : data_out)
						{
							ss<<l["time"]<<" - "<<l["id"]<<" ("<<l["name"]<<") from "<<l["origin"]<<", "<<l["userid"]<<","<<l["ip"] << " - Level "<<l["level"]<<". "<<l["message"]<<"\n";
						}
					out.output = ss.str();
				}
			return out;
		}

		CapidModuleRestOutput getMachineTimedOut(CapidModuleRestInput& input, std::string select, std::string outputType)
		{
			std::list<std::map<std::string, std::string> > data;
			if (select == "type")
				{
					data = db.getData("machines_DATA mDATA LEFT JOIN machines_MACHINES mMACH ON mDATA.machineId=mMACH.rowid", "mDATA.rowid, mMACH.type, mMACH.name, mMACH.description, mMACH.maxHeartbeatTime, mMACH.ctime AS mctime, mMACH.mtime AS mmtime, mMACH.fixedIp, mDATA.*,strftime('%s', 'now')-strftime('%s',mDATA.lastHeartbeat) AS hbsecs", {
							{ "type", std::to_string(fatoi(input.special["id"])) },
								{ "hbsecs>", "F(mMACH.maxHeartbeatTime" }
						}, 0, 0, "", "");
				}
			else if (select == "all")
				{
					data = db.getData("machines_DATA mDATA LEFT JOIN machines_MACHINES mMACH ON mDATA.machineId=mMACH.rowid", "mDATA.rowid, mMACH.type, mMACH.name, mMACH.description, mMACH.maxHeartbeatTime, mMACH.ctime AS mctime, mMACH.mtime AS mmtime, mMACH.fixedIp, mDATA.*,strftime('%s', 'now')-strftime('%s',mDATA.lastHeartbeat) AS hbsecs", {
							{ "hbsecs>", "F(mMACH.maxHeartbeatTime" }
						}, 0, 0, "", "");
				}
			else
				throw ModuleException("Unexpected "+select+" for select argument");

			std::vector<std::map<std::string, std::string> > fdata;
			std::map <std::string, std::vector<std::map<std::string, std::string> > > machineNames;

			classify(fdata, machineNames, data, input);

			CapidModuleRestOutput out;
			if (outputType == "json")
				{
					nlohmann::json jsout(data);
					out.output=jsout.dump(2);
				}
			else if (outputType == "text")
				{
					out.output = machinesTextOutput(machineNames);
				}
			else
				throw ModuleException("Output type "+outputType+" is not valid.");

			return out;
		}

		CapidModuleRestOutput getMachineBasicInfo(CapidModuleRestInput& input, std::string select, std::string outputType)
		{
			std::list<std::map<std::string, std::string> > data;
			if (select =="id")
				{
					data = db.getData("machines_DATA mDATA LEFT JOIN machines_MACHINES mMACH ON mDATA.machineId=mMACH.rowid", "mDATA.rowid, mMACH.type, mMACH.name, mMACH.description, mMACH.maxHeartbeatTime, mMACH.ctime AS mctime, mMACH.mtime AS mmtime, mMACH.fixedIp, mDATA.*", {
							{ "internalId", input.special["id"] }
						}, 0, 0, "", "");
				}
			else if (select == "name")
				{
					data = db.getData("machines_DATA mDATA LEFT JOIN machines_MACHINES mMACH ON mDATA.machineId=mMACH.rowid", "mDATA.rowid, mMACH.type, mMACH.name, mMACH.description, mMACH.maxHeartbeatTime, mMACH.ctime AS mctime, mMACH.mtime AS mmtime, mMACH.fixedIp, mDATA.*", {
							{ "name", input.special["id"] }
						}, 0, 0, "", "");
				}
			else if (select == "type")
				{
					data = db.getData("machines_DATA mDATA LEFT JOIN machines_MACHINES mMACH ON mDATA.machineId=mMACH.rowid", "mDATA.rowid, mMACH.type, mMACH.name, mMACH.description, mMACH.maxHeartbeatTime, mMACH.ctime AS mctime, mMACH.mtime AS mmtime, mMACH.fixedIp, mDATA.*", {
							{ "type", std::to_string(fatoi(input.special["id"])) }
						}, 0, 0, "", "");
				}
			else
				throw ModuleException("Unexpected "+select+" for select argument");

			std::vector<std::map<std::string, std::string> > fdata;
			std::map <std::string, std::vector<std::map<std::string, std::string> > > machineNames;

			classify(fdata, machineNames, data, input);

			CapidModuleRestOutput out;
			if (outputType == "json")
				{
					nlohmann::json jsout(data);
					out.output=jsout.dump(2);
				}
			else if (outputType == "text")
				{
					out.output = machinesTextOutput(machineNames);
				}
			else
				throw ModuleException("Output type "+outputType+" is not valid.");
			return out;
		}

		CapidModuleRestOutput get(CapidModuleRestInput& input)
		{
			/* Get de status y logs para servidores. Puedo obtener mensajes locales
				 como cualquier usuario, pero para conocer mensajes de otra máquina o de todas,
			   debo ser administrador */
			CapidModuleRestOutput out;
			try
				{
					auto id = input.special["id"];
					auto select = input.arguments["select"];
					auto info = input.arguments["info"];
					auto output = input.arguments["output"];

					if (id.empty())
						throw ModuleException("Must specify target to query machines");

					std::cout << "ID: "<<id<<"\n";
					std::cout << "SLE:"<<select<<"\n";
					std::cout << "INF:"<<info<<"\n";

					if (info.empty())
						info = "basic";			/* basic, log, timedout */
					if (select.empty())
						select = "id";			/* id, name, type */
					if (output.empty())
						output = "json";

					if (info == "basic")
						return getMachineBasicInfo(input, select, output);
					else if (info == "log")
						{
							return getMachineLogInfo(input, select, output);
						}
					else if (info == "timedout")
						{
							return getMachineTimedOut(input, select, output);
						}
					else
						throw ModuleException("Unexpected "+info+" for info argument");
					/* std::list <std::pair < std::string, std::string> > conditions; */
					/* uint64_t count=200; */

					/* auto params = tokenize(input.special["id"],",", defaultTrim); */

					/* if (params.size()==0) */
					/* 	{ */

					/* 	} */
					/* for (auto p : params) */
					/* 	{ */
					/* 		auto _p = tokenize(p, "=", defaultTrim); */
					/* 		if (_p.size()!=2) */
					/* 			throw ModuleException("Malformed condition '"+p+"'", 6); */
					/* 		if (_p[0] == "from") */
					/* 			{ */
					/* 				time_t timestamp = parseTime(_p[1]); */
					/* 				std::tm tm = *std::localtime(&timestamp); */
					/* 				std::string fromTime = put_time(&tm,  "%F %T");									 */
					/* 				conditions.push_back({"ctime>=", fromTime}); */
					/* 			} */
					/* 	} */

					/* auto data = db.getData("messages_MESSAGES", "rowid, *", conditions, 0, count, "", "ctime ASC"); */
					/* nlohmann::json jsout(data); */

					/* out.output=jsout.dump(2); */
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

		int64_t getIntFromJson(std::string strName, nlohmann::json& _val, bool required=true, int64_t defaultValue=0)
		{
			if ( (required) && (_val.is_null()) )
				throw ModuleException(strName+ " not specified", 2);

			if ( (!_val.is_null()) && (!_val.is_number()) )
				throw ModuleException(strName+ " must be a number", 3);

			if (_val.is_null())
				return defaultValue;

			return _val.get<int64_t>();
		}

		int64_t getMachineType(nlohmann::json& _type)
		{
			/* En el futuro puede que exista una string que indique el tipo */
			return getIntFromJson("type", _type);
		}

		std::string getStrFromJson(std::string strName, nlohmann::json& _str, bool required=true, std::string defaultValue="")
		{
			if ( (required) && (_str.is_null()) )
				throw ModuleException(strName+ " not specified", 2);

			if ( (!_str.is_null()) && (!_str.is_string()) )
				throw ModuleException(strName+ " must be a string", 3);

			if (_str.is_null())
				return defaultValue;

			return _str.get<std::string>();
		}

		void updateMachineLog(uint64_t machineId, uint64_t machineSubId, uint64_t userId, std::string lastIp, std::string origin, uint64_t level, std::string message)
		{
			std::list <std::pair < std::string, std::string> > data = {
				{ "machineId", std::to_string(machineId) },
				{ "machineDataId", std::to_string(machineSubId) },
				{ "message", message },
				{ "userid", std::to_string(userId) },
				{ "ipAddress", lastIp },
				{ "origin", origin },
				{ "level", std::to_string(level) }
			};
			auto sid = db.insert("machines_LOG", data);
			if (!sid)
				throw ModuleException("There was a problem updating machine log", 7);
		}

		void updateMachineLog(uint64_t machineId, uint64_t machineSubId, uint64_t userId, std::string lastIp, std::string origin, uint64_t level, uint64_t statusval, std::string statustxt)
		{
			bool writelog = false;
			std::string message;
			if (statusval)
				{
					if (!statustxt.empty())
						message = statustxt+"("+std::to_string(statusval)+")";
					else
						message = "Status changed to: "+std::to_string(statusval);
					writelog=true;
				}
			else if (!statustxt.empty())
				{
					message = statustxt+"(No status code)";
					writelog= true;
				}
			if (writelog)
				updateMachineLog(machineId, machineSubId, userId, lastIp, origin, level, message);
		}

		std::string getOrigin(uint64_t heartbeat)
		{
			/* Por ahora, solo heartbeat en la ecuación */
			std::string origin;
			if (heartbeat)
				origin = "same";
			else
				origin = "user";

			return origin;
		}

		uint64_t updateMachineStatus(uint64_t mdataId, uint64_t statusval, std::string statustxt, uint64_t heartbeat, std::string ipAddress)
		{
			std::list <std::pair < std::string, std::string> > data;

			if (statusval != NO_STATUS)
				data.push_back({ "statusval", std::to_string(statusval) });
			if (!statustxt.empty())
				data.push_back({ "statustxt", statustxt });

			if (heartbeat)
				{
					data.push_back({"lastIp", ipAddress });
					data.push_back({"lastHeartbeat", "F(DATETIME('now')" });
				}
			if (data.size()>0)
				{
					auto mid = db.update("machines_DATA", mdataId, data);
					if (!mid)
						throw ModuleException("There was a problem updating machine status", 6);

					return mid;
				}
			return 0;
		}

		uint64_t updateMachineData(uint64_t mdataId, std::string location, uint64_t userId, uint64_t statusval, std::string statustxt, uint64_t heartbeat, std::string ipAddress)
		{
			std::list <std::pair < std::string, std::string> > data = {
				{ "location", location },
				{ "userid", std::to_string(userId) }
			};
			if (statusval != NO_STATUS)
				data.push_back({ "statusval", std::to_string(statusval) });
			if (!statustxt.empty())
				data.push_back({ "statustxt", statustxt });

			if (heartbeat)
				{
					data.push_back({"lastIp", ipAddress });
					data.push_back({"lastHeartbeat", "F(DATETIME('now')" });
				}
			auto mid = db.update("machines_DATA", mdataId, data);
			if (!mid)
				throw ModuleException("There was a problem updating machine data", 6);

			return mid;
		}
		uint64_t createMachineData(uint64_t machineId, std::string internalId, std::string location, uint64_t userId, uint64_t statusval, std::string statustxt, uint64_t heartbeat, std::string ipAddress)
		{
			std::list <std::pair < std::string, std::string> > data = {
				{ "machineId", std::to_string(machineId) },
				{ "internalId", internalId },
				{ "location", location },
				{ "userid", std::to_string(userId) },
				{ "statusval", std::to_string(statusval) },
				{ "statustxt", statustxt }
			};
			if (heartbeat)
				{
					data.push_back({"lastIp", ipAddress });
					data.push_back({"lastHeartbeat", "F(DATETIME('now')" });
				}
			auto mid = db.insert("machines_DATA", data);
			if (!mid)
				throw ModuleException("There was a problem creating machine data", 6);

			return mid;
		}

		uint64_t createMachineEntry(uint64_t type, std::string name, std::string description, uint64_t fixedIp, uint64_t timeout)
		{
			std::list <std::pair < std::string, std::string> > data = {
				{ "type", std::to_string(type) },
				{ "name", name },
				{ "description", description },
				{ "fixedIp", std::to_string(fixedIp) },
				{ "maxHeartbeattime", std::to_string(timeout) }
			};
			auto eid = db.insert("machines_MACHINES", data);
			if (!eid)
				throw ModuleException("There was a problem inserting machine basic data", 4);

			return eid;
		}


		CapidModuleRestOutput post(CapidModuleRestInput& input)
		{
			/* Enviamos un nuevo equipo a la tabla. Cualquiera en la red
			   puede darse de alta si no está (siempre y cuando sea un usuario
				 del sistema).

			Si su estado es -1, darse de alta actualizará datos y reanudará
			el estado.*/
			CapidModuleRestOutput out;
			try
				{
					auto jinput = nlohmann::json::parse(input.rawData);
					auto _type = jinput["type"];
					auto _id = jinput["id"];
					auto _name = jinput["name"];
					auto _timeout = jinput["timeout"];
					auto _description = jinput["description"];
					auto _location = jinput["location"];
					auto _fixedIp = jinput["fixedip"];
					auto _statusval = jinput["statusval"];
					auto _statustxt = jinput["statustxt"];
					auto _heartbeat = jinput["heartbeat"];
					auto _level = jinput["level"];

					auto id = getStrFromJson("id", _id); /* machineId */
					auto name = getStrFromJson("name", _name);
					auto level = getIntFromJson("level", _level, false, 5);
					auto timeout = getIntFromJson("timeout", _timeout, false, 300);
					auto fixedIp = getIntFromJson("fixedip", _fixedIp, false, 1);
					auto statusval = getIntFromJson("statusval", _statusval, false, 0);
					auto heartbeat = getIntFromJson("heartbeat", _heartbeat, false, 0);
					auto statustxt = getStrFromJson("statustxt", _statustxt, false, "");
					auto location = getStrFromJson("location", _location, false, "");
					auto lastIp = input.ipAddress;
					auto userId = input.userId;

					if (statusval == MACHINE_DISCONNECTED)
						throw ModuleException("You can't create DISCONNECTED machines");

					auto existent = db.getData("machines_MACHINES", "rowid, *", { { "name", name } }, 0, 0, "", "");
					if (existent.size()>0)
						{
							auto eid = std::stoul(existent.front()["rowid"]);
							auto dataids = db.getData("machines_DATA", "rowid, *", { { "machineId", existent.front()["rowid"] } }, 0, 0, "", "");
							if (dataids.size()==0)
								{
									/* Current machine does not exist */
									auto mid = createMachineData(eid, id, location, userId, statusval, statustxt, heartbeat, lastIp);

									auto origin = getOrigin(heartbeat);
									updateMachineLog(eid, mid, userId, lastIp, origin, 100, "Machine creation");
									updateMachineLog(eid, mid, userId, lastIp, origin, level, statusval, statustxt);

								}
							else
								{
									/* Current machine exists. Is it enabled? */
									std::map<std::string, std::string> current;
									for (auto subm : dataids)
										{
											if (subm["internalId"] == id)
												{
													current = subm;
													break;
												}
										}
									if (current.size()>0)
										{
											if (std::stoul(current["statusval"])==MACHINE_DISCONNECTED)
												{
													auto mid = updateMachineData(std::stoul(current["rowid"]), location, userId, statusval, statustxt, heartbeat, lastIp);
													auto origin = getOrigin(heartbeat);

													updateMachineLog(eid, mid, userId, lastIp, origin, 100, "Machine re-launched");
													updateMachineLog(eid, mid, userId, lastIp, origin, level, statusval, statustxt);
													nlohmann::json jsout({ { "module", MODULE_NAME}, { "action", "create" }, { "id", std::to_string(eid) }, { "message", "Machine re-launched" } });
													out.output=jsout.dump(2);
												}
											else
												{
													throw ModuleException("The machine is already created.");
												}
										}
									else
										{
											/* New machine on cluster */
											auto mid = createMachineData(eid, id, location, userId, statusval, statustxt, heartbeat, lastIp);

											auto origin = getOrigin(heartbeat);
											updateMachineLog(eid, mid, userId, lastIp, origin, 100, "Machine creation");
											updateMachineLog(eid, mid, userId, lastIp, origin, level, statusval, statustxt);

											nlohmann::json jsout({ { "module", MODULE_NAME}, { "action", "create" }, { "id", std::to_string(eid) }, { "message", "New machine in cluster" } });
											out.output=jsout.dump(2);

										}

								}
							/* Create or not */
						}
					else
						{
							/* Create everything */
							auto description = getStrFromJson("description", _description);
							auto type = getMachineType(_type);
							auto eid = createMachineEntry(type, name, description, fixedIp, timeout);
							auto mid = createMachineData(eid, id, location, userId, statusval, statustxt, heartbeat, lastIp);

							auto origin = getOrigin(heartbeat);
							updateMachineLog(eid, mid, userId, lastIp, origin, 100, "Machine creation");
							updateMachineLog(eid, mid, userId, lastIp, origin, 100, statusval, statustxt);

							nlohmann::json jsout({ { "module", MODULE_NAME}, { "action", "create" }, { "id", std::to_string(eid) } });
							out.output=jsout.dump(2);
						}

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

		CapidModuleRestOutput put(CapidModuleRestInput& input)
		{
			/* Actualiza información. Puede ser el propio equipo local o un administrador.
			   También se puede usar para escribir un mensaje de log. */
			CapidModuleRestOutput out;
			try
				{
					auto id = input.special["id"];
					auto jinput = nlohmann::json::parse(input.rawData);
					auto _statusval = jinput["statusval"];
					auto _statustxt = jinput["statustxt"];
					auto _heartbeat = jinput["heartbeat"];
					auto _level = jinput["level"];

					auto statusval = getIntFromJson("statusval", _statusval, false, 0);
					auto heartbeat = getIntFromJson("heartbeat", _heartbeat, false, 0);
					auto statustxt = getStrFromJson("statustxt", _statustxt, false, "");
					auto level = getIntFromJson("level", _level, false, 5);

					auto lastIp = input.ipAddress;
					auto userId = input.userId;

					auto machines = db.getData("machines_DATA", "rowid, *", { { "internalId", id } }, 0, 0, "", "");
					if (machines.size()==0)
						throw ModuleException("Could not find machine.");
					auto machine = machines.front();
					if (machine.size()==0)
						throw ModuleException("Could not find machine.");

					if ( (heartbeat) && (!machine["lastIp"].empty()) && (machine["lastIp"] != lastIp) )
						throw ModuleException("Not allowed to heartbeat this machine!");

					uint64_t mid;
					if ( (!statusval) && (statustxt.empty()) && (heartbeat))
						mid = updateMachineStatus(std::stoul(machine["rowid"]), MACHINE_OK, "Heartbeat", heartbeat, lastIp);
					else
						mid = updateMachineStatus(std::stoul(machine["rowid"]), statusval, statustxt, heartbeat, lastIp);

					updateMachineLog(std::stoul(machine["machineId"]), mid, userId, lastIp, getOrigin(heartbeat), level, statusval, statustxt);

					nlohmann::json jsout({ { "module", MODULE_NAME}, { "action", "updateStatus" }, { "id", machine["internalId"] } });
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
			/* Sólo un admin puede eliminar equipos. Será softdelete, su estado figurará como borrado */
			CapidModuleRestOutput out;
			try
				{
					auto id = input.special["id"];
					auto machines = db.getData("machines_DATA", "rowid, *", { { "internalId", id } }, 0, 0, "", "");
					if (machines.size()==0)
						throw ModuleException("Could not find machine.");
					auto mid = std::stoul(machines.front()["rowid"]);
					auto eid = std::stoul(machines.front()["machineId"]);
					if (!db.destroy("machines_DATA", mid))
						throw ModuleException("Could not remove machine");

					db.destroyMulti("machines_LOG", { { "machineDataId", std::to_string(mid) } });

					auto ms = db.getData("machines_DATA", "rowid, *", { { "machineId", std::to_string(eid) } }, 0, 0, "", "");
					if (ms.size()==0)
						{
							if (!db.destroy("machines_MACHINES", eid))
								throw ModuleException("Could not remove some information.");
						}

					nlohmann::json jsout({ { "module", MODULE_NAME}, { "action", "delete" }, { "id", std::to_string(eid) } });
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
		enum MachineStatus:int32_t
			{
				NO_STATUS = 0,
				MACHINE_OK = 1,
				MACHINE_ERROR= 2,
				MACHINE_DISCONNECTED = -1
			};

		std::map < MachineStatus, std::string> statusTxt = {
			{ NO_STATUS, "No status" },
			{ MACHINE_OK, "OK" },
			{ MACHINE_ERROR, "Error" },
			{ MACHINE_DISCONNECTED, "Disconnected" }
		};
		};
	};


	class MachinesModule : public CapidModule
{
public:
	MachinesModule(): CapidModule(MODULE_NAME, MODULE_LONG_NAME, MODULE_VERSION, MODULE_AUTHOR, MODULE_ENDPOINT)
	{
	}

	bool afterInit()
	{
		_machines = std::make_shared<Machines::MachinesMain>(Machines::MachinesMain(db));

		return true;
	}

	Machines::MachinesMain& machines()
	{
		return *_machines;
	}
private:
	std::shared_ptr<Machines::MachinesMain> _machines;
};

/* With a smart pointer, when the lib is unloaded
   the pointer will be freed. */
	std::shared_ptr<MachinesModule> module;

	extern "C"
{
	bool init(CapidModuleShared& shared)
	{
		module = std::make_shared<MachinesModule>();
		if (!module->setProps(shared))
			return false;

		shared.addRestService("machines", "machines_get", "machines_post", "machines_put", "", "machines_delete");
		return true;
	}

	CapidModuleRestOutput machines_post(CapidModuleRestInput& input)
	{
		if (module==nullptr)
			return internalError();

		return module->machines().post(input);
	}

	CapidModuleRestOutput machines_get(CapidModuleRestInput& input)
	{
		if (module==nullptr)
			return internalError();

		return module->machines().get(input);
	}

	CapidModuleRestOutput machines_delete(CapidModuleRestInput& input)
	{
		if (module==nullptr)
			return internalError();

		return module->machines().delet(input);
	}

	CapidModuleRestOutput machines_put(CapidModuleRestInput& input)
	{
		if (module==nullptr)
			return internalError();

		return module->machines().put(input);
	}
}
