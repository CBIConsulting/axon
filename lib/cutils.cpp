/**
*************************************************************
* @file cutils.cpp
* @brief Capi - Utils
*
*
* @author Gaspar Fernández <blakeyed@totaki.com>
* @version
* @date 01 oct 2016
* Historial de cambios:
*
*
*************************************************************/

#include "cutils.hpp"
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

namespace CUtils
{
	bool file_exists (const std::string& name) 
	{
		std::ifstream f(name.c_str());
		return f.good();
	}

	std::string replace(std::string source, std::map<std::string,std::string>& strMap, int offset, int times, bool delimiters, std::string before, std::string after)
	{
		int total = 0;
		std::string::size_type pos=offset;
		std::string::size_type newPos;
		std::string::size_type lowerPos;
		std::string::size_type delsize;

		if (strMap.size() == 0)
			return source;

		if (delimiters)
			delsize = before.length() + after.length();

		do
			{
				std::string rep;
				for (auto i=strMap.begin(); i!=strMap.end(); ++i)
					{
						auto fromStr = i->first;
						newPos = (delimiters)?
							source.find(before + fromStr + after, pos):
							source.find(fromStr, pos);

						if ( (i==strMap.begin()) || (newPos<lowerPos) )
							{
								rep = fromStr;
								lowerPos = newPos;
							}
					}

				pos = lowerPos;
				if (pos == std::string::npos)
					break;

				auto toStr = strMap.find(rep)->second;
				source.replace(pos, rep.length()+((delimiters)?delsize:0), toStr);
				pos+=toStr.size();

			} while ( (times==0) || (++total<times) );

		return source;
	}	

	bool findFile(std::string &file, const std::list<std::string>& paths, const std::map<std::string, std::string>& replacements)
	{
		for (auto p : paths)
			{
				p = replace(p, const_cast<std::map<std::string, std::string>&>(replacements));
				if (file_exists(p)==1)
					{
						file = p;
						return true;
					}
			}
		return false;
	}

	/* Para ver $HOME */
	std::string getHomeDir()
	{
		static char *home = NULL;

		if (!home)
			{
				home = getenv("HOME") ;
			}
		if (!home)
			{
				struct passwd *pw = getpwuid(getuid());
				if (pw)
					home = pw->pw_dir ;
			}
		
		return (home)?home:"";
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
