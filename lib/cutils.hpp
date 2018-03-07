#pragma once

#include <string>
#include <map>
#include <list>

/* Capi Utils */
namespace CUtils
{
	bool file_exists (const std::string& name);
	std::string replace(std::string source, std::map<std::string,std::string>& strMap, int offset=0, int times=0, bool delimiters=true, std::string before=":", std::string after="");
	bool findFile(std::string &file, const std::list<std::string>& paths, const std::map<std::string, std::string>& replacements);
	std::string getHomeDir();

	uint64_t fatoi(std::string& str);
};
