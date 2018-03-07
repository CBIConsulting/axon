#include "../lib/axond_module.hpp"
#include "../lib/glove/json.hpp"
#include "../lib/glove/utils.hpp"
#include "../lib/os.hpp"
#include "module_exception.hpp"
#include <iostream>
#include <memory>
#include <ctime>

#define MODULE_NAME "exec"
#define MODULE_LONG_NAME "Executor"
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

	struct CommandInfo
	{
		std::string name;						/* The name users will call. */
		std::string command;				/* Command to execute. */
		std::vector<std::string> args; /* Arguments */
		bool allowInput;							 /* Allow stdin */
		bool allowRemote;							 /* Allow execution by remote hosts. */
	};

};

namespace Executor
{
	class Execs
	{
	public:
		Execs(CapidModuleSafeDb& db, 	std::map<std::string, std::map<std::string, CommandInfo> >& commands): db(db), commands(commands)
		{
		}

		~Execs()
		{
		}

		CommandInfo* findCommand(std::string command, CapidModuleRestInput& input)
		{
			auto euser = commands.find(input.user);
			if (euser != commands.end())				
				{
					auto ecommand = euser->second.find(command);
					if (ecommand != euser->second.end())
						return &ecommand->second;
				}
			for (auto g : input.groups)
				{
					auto egroup = commands.find(g);
					if (egroup != commands.end())				
						{
							auto ecommand = egroup->second.find(command);
							if (ecommand != egroup->second.end())
								return &ecommand->second;
						}							
				}

			auto eip = commands.find("#"+input.ipAddress);
			if (eip != commands.end())				
				{
					auto ecommand = eip->second.find(command);
					if (ecommand != eip->second.end())
						return &ecommand->second;
				}

			return nullptr;
		}
		std::list<std::string> parseArguments(CommandInfo& ci, nlohmann::json& arguments)
		{
			std::list<std::string> args;
			for (auto a : ci.args)
				{
					auto len = a.length();
					if ( (a.front()=='%') && (a.back()=='%') )
						{
							auto name = a.substr(1, len-2);
							auto _arg = arguments[name];
							if ( (_arg.is_null()) || (!_arg.is_string()) )
								throw ModuleException("Argument "+name+" is mandatory!");
							args.push_back(_arg.get<std::string>());
							/* Mandatory argument */
						}
					else if ( (a.front()=='@') && (a.back()=='@') )
						{
							auto name = a.substr(1, len-2);
							auto _arg = arguments[name];
							if (_arg.is_string())
								args.push_back(_arg.get<std::string>());
							else
								args.push_back(a);
							
							/* Optional argument */
						}
					else
						args.push_back(a);
				}
			return args;
		}
		
		CapidModuleRestOutput post(CapidModuleRestInput& input)
		{
			CapidModuleRestOutput out;
			try
				{
					auto _command = input.special["id"];
					if (_command.empty())
						throw ModuleException("No command specified", 6);
					auto command = findCommand(_command, input);
					if (command == nullptr)
						throw ModuleException("Command not found", 7);
					auto jinput = (input.rawData.empty())?nlohmann::json::parse("{}"):nlohmann::json::parse(input.rawData);;
					auto arguments = parseArguments(*command, jinput);
					std::string stdin;
					if (command->allowInput)
						{
							auto _stdin = jinput["_stdin"];
							if (_stdin.is_string())
								stdin = _stdin.get<std::string>();
						}
					auto ed = OS::execute(command->command, arguments, stdin);
					std::map<std::string, std::string> mout = { { "module", MODULE_NAME},
																											{ "action", "execute" },
					};
					if (ed.status==-1)
						mout["status"] = "error";
					else if (ed.status==-2)
						{
							mout["status"] = "signaled";
							mout["signal"] = std::to_string(ed.exitCode);
						}
					else if (ed.status==0)
						{
							mout["status"] = "ok";
							mout["code"] = std::to_string(ed.exitCode);
							mout["stdout"] = std::string(ed.stdout);
							mout["stderr"] = ed.stderr;
						}		
					nlohmann::json jsout(mout);
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
		std::map<std::string, std::map<std::string, CommandInfo> >& commands;
		};
};

class ExecutorModule : public CapidModule
{
public:
	ExecutorModule(std::string configStr): CapidModule(MODULE_NAME, MODULE_LONG_NAME, MODULE_VERSION, MODULE_AUTHOR, MODULE_ENDPOINT), status(true)
	{
		try
			{
				if (!configStr.empty())
					{
						auto config = nlohmann::json::parse(configStr);
						if (config["users"].is_object())
							parseUsers(config["users"]);
					}
			}
		catch (std::exception& e)
			{
				std::cout << "WH: "<<e.what()<<"\n";
				status = false;
			}
	}

	bool afterInit()
	{
		if (!status)
			return false;
		
		_execs = std::make_shared<Executor::Execs>(Executor::Execs(db, commands));

		return true;
	}

	Executor::Execs& execs()
	{
		return *_execs;
	}
protected:
	void parseUsers(nlohmann::json& users)
	{
		for (auto ju = users.begin(); ju != users.end(); ++ju)
			{
				//				std::cout << "USER: "<<ju.key()<<"\n"<<ju.value().dump(2)<<"\n\n";
				addCommand(ju.key(), ju.value()); 
			}
	}

	void addCommand(std::string user, nlohmann::json& command)
	{
		if (command.is_array())
			{
				for (auto c : command)
					addCommand(user, c);
			}
		else if (command.is_object())
			{
				auto _name = command["name"];
				auto _command = command["command"];
				auto _arguments = command["arguments"];
				auto _allowInput = command["allowInput"];
				auto _allowRemote = command["allowremote"];
				if ( (_name.is_null()) || (!_name.is_string()) )
					throw ModuleException("No name specified", 2);
				if ( (_command.is_null()) || (!_command.is_string()) )
					throw ModuleException("No command specified", 3);
				
				CommandInfo ci;
				ci.name =_name.get<std::string>();
				ci.command = _command.get<std::string>();
				ci.args = _arguments.get<std::vector<std::string>>();
				ci.allowInput = true;
				ci.allowRemote = true;
				auto current = commands.find(user);
				if (current != commands.end())
					current->second.insert({ci.name, ci});
				else
					commands.insert({user, {{ci.name, ci}}});				
			}
		else
			throw ModuleException("Invalid data",1);
	}

private:
	std::shared_ptr<Executor::Execs> _execs;
	std::map<std::string, std::map<std::string, CommandInfo> >commands;
	bool status;
};

/* With a smart pointer, when the lib is unloaded
   the pointer will be freed. */
	std::shared_ptr<ExecutorModule> module;

	extern "C"
{
	bool init(CapidModuleShared& shared)
	{
		module = std::make_shared<ExecutorModule>(shared.config());
		if (!module->setProps(shared))
			return false;

		shared.addRestService("exec", "", "exec_post", "", "", "");
		return true;
	}

	CapidModuleRestOutput exec_post(CapidModuleRestInput& input)
	{
		if (module==nullptr)
			return internalError();

		return module->execs().post(input);
	}
}
