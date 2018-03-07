#include "../lib/axond_module.hpp"
#include "../lib/glove/json.hpp"
#include "../lib/glove/utils.hpp"
#include "../lib/silicon/silicon.h"
#include "../lib/glove/glovecoding.hpp"
#include "module_exception.hpp"
#include <iostream>
#include <memory>
#include <ctime>

#define MODULE_NAME "files"
#define MODULE_LONG_NAME "Files"
#define MODULE_VERSION "0.1"
#define MODULE_AUTHOR "Gaspar Fernandez"

namespace
{
	CapidModuleRestOutput internalError()
	{
		CapidModuleRestOutput cmro;
		cmro.output="{ \"error\": \"Internal error in module files\" }";
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

namespace Files
{
	class FilesMain
	{
	public:
		FilesMain(CapidModuleSafeDb& db): db(db)
		{
			/* Create table sshkeys_KEYS */
			/* Type (ERROR, WARNING, NOTICE...) */
			/* Category (SECURITY, CONTROL or Application ID */
			/* Status (Error code, Status code...) */
			db.createTable("files_FILES", {
					{ "ctime", "DATETIME" },
					{ "mtime", "DATETIME" },
					{ "name", "TEXT UNIQUE" },
					{ "owner", "NUMBER" },
					{ "description", "TEXT" },
					{ "template", "NUMBER" }, /* This file is a text template*/
					{ "version", "TEXT" },
					{ "content", "TEXT" },
					{ "size", "NUMBER" },
					{ "accessType", "NUMBER"} /* (1) Allow by default, (0) Deny by default*/
				}, {}, false);
			db.createTable("files_FILELOG", {
					{ "ctime", "DATETIME" },
					{ "fileId", "NUMBER" },
					{ "size", "NUMBER" },
					{ "type", "NUMBER" }, /* Message type: (0) internal, (1) upload, (2) change, (3) access */
					{ "description", "TEXT" },
					{ "version", "TEXT" }
				}, {}, false);
			/* Read may be allowed or disables. Write must be allowed */
			db.createTable("files_ACL", {
					{ "ctime", "DATETIME" },
					{ "fileId", "NUMBER" },
					{ "type", "NUMBER" }, /* (1) Allow or (0) deny */
					{ "access", "NUMBER" }, /* (0) Read / (1) Read+Write*/
					{ "data", "TEXT" }, /* user or group. Probably in the future IP */
					{ "validity", "NUMBER" } /* This rule will be valid for N seconds*/
				}, {}, false);
		}

		~FilesMain()
		{
		}

		bool isAdmin(CapidModuleRestInput& input)
		{
			return ( (input.user=="admin") ||
							 (std::find(input.groups.begin(), input.groups.end(), "admin") != input.groups.end()) );

		}

		CapidModuleRestOutput getFullList(CapidModuleRestInput& input)
		{
			CapidModuleRestOutput out;

			auto allowlist="'"+input.user+"'";
			for (auto g : input.groups)
				allowlist+=", '"+g+"'";

			bool admin = isAdmin(input);

			auto table = "files_FILES FILES";
			std::list<std::map<std::string, std::string> > data;

			std::string fields;

			if (!admin)
				{
					fields = "FILES.ctime, FILES.mtime, FILES.name, FILES.owner, FILES.description, FILES.template, FILES.version, "
						"FILES.size, FILES.accessType, "
						"(SELECT COUNT(rowid) FROM files_ACL WHERE data IN("+allowlist+") AND fileId=FILES.rowid AND type=1 AND STRFTIME('%s', DATETIME('now'))-STRFTIME('%s', ctime)<validity) AS allowed, "
						"(SELECT COUNT(rowid) FROM files_ACL WHERE data IN("+allowlist+") AND fileId=FILES.rowid AND type=0 AND STRFTIME('%s', DATETIME('now'))-STRFTIME('%s', ctime)<validity) AS denied, "
						"(SELECT COUNT(rowid) FROM files_FILELOG WHERE fileId=FILES.rowid) AS revisions";
					data = db.getData(table, fields, {
							{ "allowed>=", "0" },
								{ "denied", "0" }
						}, 0, 0, "", "");
				}
			else
				{
					fields = "FILES.ctime, FILES.mtime, FILES.name, FILES.owner, FILES.description, FILES.template, FILES.version, "
						"FILES.size, FILES.accessType, "
						"(SELECT COUNT(rowid) FROM files_FILELOG WHERE fileId=FILES.rowid) AS revisions";
					data = db.getData(table, fields, {
						}, 0, 0, "", "");
				}
			std::list<std::map<std::string, std::string> > data_out;
			/* Make a last check. No explicitly disabled items here */
			for (auto d : data)
				{
					auto at = fatoi(d["accessType"]);
					auto allowed = fatoi(d["allowed"]);
					if ( (admin) || (at) || (allowed) )
						data_out.push_back({
								{ "accessType", (at)?"allow":"deny" },
								{ "ctime", d["ctime"] },
								{ "mtime", d["mtime"] },
								{ "name", d["name"] },
								{ "description", d["description"] },
								{ "owner", std::to_string(fatoi(d["owner"])) },
								{ "revisions", std::to_string(fatoi(d["revisions"])) },
								{ "size", std::to_string(fatoi(d["size"])) },
								{ "template", std::to_string(fatoi(d["template"])) },
								{ "version", d["version"] }
							});
				}

			nlohmann::json jsout(data_out);
			out.output=jsout.dump(2);

			return out;
		}

		void ACL_cleanup()
		{
			/* Permissions cleanup */
			db.destroyMulti("files_ACL", { { "STRFTIME('%s', DATETIME('now'))-STRFTIME('%s', ctime)>", "F(validity" } });
			/* Permissions cleanup */
		}

		std::map<std::string, int> filePermission(CapidModuleRestInput& input, uint64_t fileId, uint64_t permissionType)
		{
			auto _fileId = std::to_string(fileId);
			auto _ptype = std::to_string(permissionType);

			auto allowlist="'"+input.user+"'";
			for (auto g : input.groups)
				allowlist+=", '"+g+"'";

			ACL_cleanup();

			auto fields = "(SELECT COUNT(rowid) FROM files_ACL WHERE data IN("+allowlist+") AND fileId="+_fileId+" AND type=1 AND access="+_ptype+" AND STRFTIME('%s', DATETIME('now'))-STRFTIME('%s', ctime)<validity) AS allowed, "
				"(SELECT COUNT(rowid) FROM files_ACL WHERE data IN("+allowlist+") AND fileId="+_fileId+" AND type=0 AND access="+_ptype+" AND STRFTIME('%s', DATETIME('now'))-STRFTIME('%s', ctime)<validity) AS denied ";
			auto data = db.getData("files_ACL", fields, {
				}, 0, 0, "", "");

			if (data.size()==0)
				{
					return { { "allowed", 0 }, { "denied", 0 } };
				}
			else
				{
					auto row = data.front();
					return { { "allowed", fatoi(row["allowed"]) },
							{ "denied", fatoi(row["denied"]) } };
				}
		}

		bool canWrite(CapidModuleRestInput& input, uint64_t fileId)
		{
			if (isAdmin(input))
				return true;

			auto perms = filePermission(input, fileId, 1);
			return ( (perms["denied"]==0) &&
							 (perms["allowed"]>0) );
		}

		bool canRead(CapidModuleRestInput& input, uint64_t fileId, uint16_t accessType)
		{
			if (isAdmin(input))
				return true;

			auto perms = filePermission(input, fileId, 0);
			if (perms["denied"]>0)
				return false;

			return ( (accessType) || (perms["allowed"]>0) );
		}

		CapidModuleRestOutput extractFileData(std::map<std::string, std::string >& data, std::string& outputType, CapidModuleRestInput& input)
		{
			CapidModuleRestOutput out;

			auto fId = fatoi (data["rowid"]);
			updateFileLog(fId, 0, 3, "Access by "+input.user+" ("+std::to_string(input.userId)+"), ip: "+input.ipAddress, outputType+";"+((fatoi(data["template"]))?"templated":""));
			if ( (outputType == "base64") && (!fatoi(data["template"])) )
				{
					out.output = data["content"];
					return out;
				}

			auto content = GloveCoding::base64_decode(data["content"]);

			if (fatoi(data["template"]))
				{
					auto fill = nlohmann::json::parse(input.rawData);
					auto tpl = Silicon::createFromStr(content, content.length()*2);
					if (fill.is_object())
						{
							for (auto ei = fill.begin(); ei != fill.end(); ei++)
								{
									tpl.setKeyword(ei.key(), ei.value());
								}
						}
					content = tpl.render();
				}
			if (outputType == "base64")
				out.output = GloveCoding::base64_encode((const unsigned char*) content.c_str(), content.length());
			else if (outputType == "raw")
				out.output = content;
			else
				throw ModuleException ("Unexpected output type "+outputType);

			return out;
		}

		CapidModuleRestOutput extractFileLog(std::map<std::string, std::string>& data, std::string& outputType, CapidModuleRestInput& input)
		{
			CapidModuleRestOutput out;

			auto log = db.getData("files_FILELOG", "ctime, size, type, description, version", {
					{ "fileId", std::to_string(fatoi(data["rowid"])) }
				}, 0, 0, "", "ctime ASC");

			nlohmann::json jsout(log);
			out.output=jsout.dump(2);

			return out;
		}

		CapidModuleRestOutput extractFileMeta(std::map<std::string, std::string>& data, std::string& outputType, CapidModuleRestInput& input)
		{
			CapidModuleRestOutput out;

			auto at = fatoi(data["accessType"]);
			auto allowed = fatoi(data["allowed"]);

			nlohmann::json jsout({
					{ "accessType", (at)?"allow":"deny" },
					{ "ctime", data["ctime"] },
					{ "mtime", data["mtime"] },
					{ "name", data["name"] },
					{ "description", data["description"] },
					{ "owner", std::to_string(fatoi(data["owner"])) },
					{ "revisions", std::to_string(fatoi(data["revisions"])) },
					{ "size", std::to_string(fatoi(data["size"])) },
					{ "template", std::to_string(fatoi(data["template"])) },
					{ "version", data["version"] }
				});
			out.output=jsout.dump(2);

			return out;
		}

		CapidModuleRestOutput get(CapidModuleRestInput& input)
		{
			/* Se podrá ver un listado de ficheros, el log de un fichero, o descargar dicho fichero */
			CapidModuleRestOutput out;
			try
				{
					auto id = input.special["id"];
					auto info = input.arguments["info"];
					auto output = input.arguments["output"];

					if (info.empty())
						info = "file";

					if (output.empty())
						output = "raw";

					if (id.empty())
						return getFullList(input);

					std::list <std::pair < std::string, std::string> > conditions;

					if (fatoi(id)>0)
						conditions.push_back({ "rowid", id });
					else
						conditions.push_back({ "name", id });

					auto data = db.getData("FILES_files", "rowid, *, (SELECT COUNT(rowid) FROM files_FILELOG WHERE fileId=files_FILES.rowid) AS revisions", conditions, 0, 0, "", "ctime ASC");
					if (data.size()==0)
						throw ModuleException("File not found!");
					auto file = data.front();
					if (!canRead(input, fatoi(file["rowid"]), fatoi(file["accessType"])))
						throw ModuleException("Not allowed to read this file");

					if (info == "file")
						return extractFileData(file, output, input);
					else if (info == "log")
						return extractFileLog(file, output, input);
					else if (info == "meta")
						return extractFileMeta(file, output, input);
					else
						throw ModuleException("Unknown info type: "+info);

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

		void updateFileLog(uint64_t id, uint64_t size, uint16_t type, std::string description, std::string version)
		{
			std::list <std::pair < std::string, std::string> > data = {
				{ "fileId", std::to_string(id) },
				{ "size", std::to_string(size) },
				{ "type", std::to_string(type) }, /* log entry type */
				{ "description", description },
				{ "version", version }
			};
			db.insert("files_FILELOG", data);
		}

		void updateFileAcl(uint64_t fileId, uint16_t type, uint16_t access, std::string who, uint64_t validity)
		{
			if (who.empty())
				return;

			std::list <std::pair < std::string, std::string> > data = {
				{ "fileId", std::to_string(fileId) },
				{ "type", std::to_string(type) },
				{ "access", std::to_string(access) },
				{ "data", who },
				{ "validity", std::to_string(validity) }
			};

			db.insert("files_ACL", data);
		}

		uint64_t createFile(uint64_t owner, std::string name, std::string description, std::string version, std::string content, uint16_t template_, uint16_t accessType, uint64_t length)
		{
			ACL_cleanup();

			std::list <std::pair < std::string, std::string> > data = {
				{ "name", name },
				{ "owner", std::to_string(owner) },
				{ "description", description  },
				{ "template", std::to_string(template_) },
				{ "accessType", std::to_string(accessType) },
				{ "version", version },
				{ "content", content },
				{ "size", std::to_string(length) }
			};
			auto fId = db.insert("files_FILES", data);
			updateFileLog(fId, length, 1, description, version);

			return fId;
		}

		CapidModuleRestOutput post(CapidModuleRestInput& input)
		{
			/* Se podrán enviar nuevos ficheros, no sobreescribir */
			CapidModuleRestOutput out;
			try
				{
					auto jinput = nlohmann::json::parse(input.rawData);
					auto _name = jinput["name"];
					auto _description = jinput["description"];
					auto _template = jinput["template"];
					auto _version = jinput["version"];
					auto _content = jinput["content"];
					auto _accessType = jinput["accessType"];
					auto _allow = jinput["allow"];
					auto _deny = jinput["deny"];
					auto _for = jinput["for"]; /* seconds */

					auto name = defaultTrim(getStrFromJson("name", _name));
					auto description = defaultTrim(getStrFromJson("description", _description));
					auto template_ = getIntFromJson("template", _template, false, 0);
					auto version = getStrFromJson("version", _version);
					auto content = getStrFromJson("content", _content);
					auto accessType = getIntFromJson("accessType", _accessType, false, 1);
					auto allow = getStrFromJson("allow", _allow, false, "");
					auto deny = getStrFromJson("deny", _deny, false, "");
					auto for_ = getIntFromJson("for", _for, false, 3600);

					auto current = db.getData("files_FILES", "rowid, name, description", {
							{ "name", name }
						}, 0, 0, "", "");
					if (current.size()>0)
						throw ModuleException ("This file name cannot be created");

					if (name.empty())
						throw ModuleException("Please, enter a file name");
					if (description.empty())
						throw ModuleException("Description cannot be empty");
					if (version.empty())
						throw ModuleException("Version cannot be empty");
					if ( (template_<0) || (template_>1) )
						throw ModuleException("Template must be either 0 or 1");
					if ( (accessType<0) || (accessType>1) )
						throw ModuleException("AccessType must be either 0 (to deny by default) or 1 (to allow by default)");

					auto decodedContent = GloveCoding::base64_decode(content);

					auto fId = createFile(input.userId, name, description, version, content, template_, accessType, decodedContent.length());
					if (!allow.empty())
						{
							auto users = tokenize(allow,",", defaultTrim);
							for (auto u: users)
								{
									updateFileAcl(fId, 1, 0, u, for_);
								}
							updateFileLog(fId, 0, 0, "Updated ACL. Allowed "+allow+" for "+std::to_string(for_)+"secs.", "");
						}
					if (!deny.empty())
						{
							auto users = tokenize(deny,",", defaultTrim);
							for (auto u: users)
								{
									updateFileAcl(fId, 0, 0, u, for_);
								}
							updateFileLog(fId, 0, 0, "Updated ACL. Denied "+deny+" for "+std::to_string(for_)+"secs.", "");
						}

					nlohmann::json jsout({ { "module", MODULE_NAME}, { "action", "create" }, { "id", std::to_string(fId) } });
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

		CapidModuleRestOutput put(CapidModuleRestInput& input)
		{
			/* Se podrán modificar ficheros o cambiar características */
			CapidModuleRestOutput out;
			try
				{
					auto id = input.special["id"];
					auto jinput = nlohmann::json::parse(input.rawData);

					auto _name = jinput["name"];
					auto _description = jinput["description"];
					auto _template = jinput["template"];
					auto _version = jinput["version"];
					auto _content = jinput["content"];
					auto _accessType = jinput["accessType"];
					auto _allow = jinput["allow"];
					auto _deny = jinput["deny"];
					auto _for = jinput["for"]; /* seconds */

					auto name = defaultTrim(getStrFromJson("name", _name, false, ""));
					auto description = defaultTrim(getStrFromJson("description", _description, false, ""));
					auto template_ = getIntFromJson("template", _template, false, 0);
					auto version = getStrFromJson("version", _version, false, "");
					auto content = getStrFromJson("content", _content, false, "");
					auto accessType = getIntFromJson("accessType", _accessType, false, 1);
					auto allow = getStrFromJson("allow", _allow, false, "");
					auto deny = getStrFromJson("deny", _deny, false, "");
					auto for_ = getIntFromJson("for", _for, false, 3600);

					if (id.empty())
						throw ModuleException("Id not specified");

					std::list <std::pair < std::string, std::string> > conditions;

					if (fatoi(id)>0)
						conditions.push_back({ "rowid", id });
					else
						conditions.push_back({ "name", id });

					auto data = db.getData("FILES_files", "rowid, *, (SELECT COUNT(rowid) FROM files_FILELOG WHERE fileId=files_FILES.rowid) AS revisions", conditions, 0, 0, "", "ctime ASC");
					if (data.size()==0)
						throw ModuleException("File not found!");
					auto file = data.front();
					if (!canWrite(input, fatoi(file["rowid"])))
						throw ModuleException("Not allowed to edit this file");

					ACL_cleanup();
					/* change basic data? */
					CapidModuleSafeDb::DbList changes;
					if ( (!_name.is_null()) && (!name.empty()) )
						changes.push_back({ "name", name });
					if (!_description.is_null())
						changes.push_back({ "description", description });
					if (!_template.is_null())
						changes.push_back({ "template", std::to_string(template_) });
					if (!_accessType.is_null())
						changes.push_back({ "accessType", std::to_string(accessType) });
					if (!_version.is_null())
						changes.push_back({ "version", version });
					uint64_t changeSize = 0;
					if (!_content.is_null())
						{
							changes.push_back({ "content", content });
							changeSize = GloveCoding::base64_decode(content).length();
							changes.push_back({ "size", std::to_string(changeSize) });
						}

					auto fid = fatoi(file["rowid"]);
					if (changes.size()>0)
						{
							db.update("files_FILES", fid, changes);
							updateFileLog(fid, changeSize, 2, "Changed by "+input.user+" ("+std::to_string(input.userId)+"), ip: "+input.ipAddress+": "+description, version);
						}

					if (!allow.empty())
						{
							auto users = tokenize(allow,",", defaultTrim);
							for (auto u: users)
								{
									updateFileAcl(fid, 1, 0, u, for_);
									updateFileLog(fid, 0, 0, "Updated ACL. Allowed "+allow+" for "+std::to_string(for_)+"secs.", "");
								}
						}
					if (!deny.empty())
						{
							auto users = tokenize(deny,",", defaultTrim);
							for (auto u: users)
								{
									updateFileAcl(fid, 0, 0, u, for_);
									updateFileLog(fid, 0, 0, "Updated ACL. Allowed "+allow+" for "+std::to_string(for_)+"secs.", "");
								}
						}

					nlohmann::json jsout({ { "module", MODULE_NAME}, { "action", "update" }, { "id", std::to_string(fid) } });
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
			/* Se podrán eliminar ficheros */
			CapidModuleRestOutput out;
			try
				{
					auto id = input.special["id"];

					std::list <std::pair < std::string, std::string> > conditions;

					if (fatoi(id)>0)
						conditions.push_back({ "rowid", id });
					else
						conditions.push_back({ "name", id });

					auto data = db.getData("FILES_files", "rowid, *, (SELECT COUNT(rowid) FROM files_FILELOG WHERE fileId=files_FILES.rowid) AS revisions", conditions, 0, 0, "", "ctime ASC");
					if (data.size()==0)
						throw ModuleException("File not found!");
					auto file = data.front();
					if (!canWrite(input, fatoi(file["rowid"])))
						throw ModuleException("Not allowed to read this file");
					auto fid = fatoi(file["rowid"]);

					if (!db.destroy("files_FILES", fid))
						throw ModuleException("There was a problem deleting file");
					db.destroyMulti("files_ACL", { { "fileId", std::to_string(fid) } });
					db.destroyMulti("files_FILELOG", { { "fileId", std::to_string(fid) } });
					nlohmann::json jsout({ { "module", MODULE_NAME}, { "action", "delete" }, { "id", std::to_string(fid) } });
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

	class FilesModule : public CapidModule
{
public:
	FilesModule(): CapidModule(MODULE_NAME, MODULE_LONG_NAME, MODULE_VERSION, MODULE_AUTHOR, MODULE_ENDPOINT)
	{
	}

	bool afterInit()
	{
		_files = std::make_shared<Files::FilesMain>(Files::FilesMain(db));

		return true;
	}

	Files::FilesMain& files()
	{
		return *_files;
	}
private:
	std::shared_ptr<Files::FilesMain> _files;
};

/* With a smart pointer, when the lib is unloaded
   the pointer will be freed. */
	std::shared_ptr<FilesModule> module;

	extern "C"
{
	bool init(CapidModuleShared& shared)
	{
		module = std::make_shared<FilesModule>();
		if (!module->setProps(shared))
			return false;

		shared.addRestService("files", "files_get", "files_post", "files_put", "", "files_delete");
		return true;
	}

	CapidModuleRestOutput files_post(CapidModuleRestInput& input)
	{
		if (module==nullptr)
			return internalError();

		return module->files().post(input);
	}

	CapidModuleRestOutput files_delete(CapidModuleRestInput& input)
	{
		if (module==nullptr)
			return internalError();

		return module->files().delet(input);
	}

	CapidModuleRestOutput files_put(CapidModuleRestInput& input)
	{
		if (module==nullptr)
			return internalError();

		return module->files().put(input);
	}

	CapidModuleRestOutput files_get(CapidModuleRestInput& input)
	{
		if (module==nullptr)
			return internalError();

		return module->files().get(input);
	}
}
