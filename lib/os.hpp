#pragma once

#include <array>
#include <list>
#include <string>

class OS
{
public:
	struct ExecData
	{
		int status;
		std::string stdout;
		std::string stderr;
		int exitCode;
	};
	static void setuser(std::string userName);
	static void setgroup(std::string groupName);

	static ExecData execute(std::string executable, std::list<std::string> arguments={}, std::string stdin="");
private:
};
