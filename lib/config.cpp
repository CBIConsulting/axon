/**
*************************************************************
* @file config.cpp
* @brief Load CAPI Config
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
#include "basic_exception.hpp"
#include "config.hpp"
#include <map>
#include <fstream>

namespace
{
	const std::list<std::string> stdConfigPaths =
		{
			":file.:cfgext",
			":home/.:prog.:cfgext",
			":home/.:prog/:file.:cfgext",
			"/etc/:prog.:cfgext",
			"/etc/:prog/:file.:cfgext",
			"/var/:prog/:file.:cfgext"
		};
};

Config::Config(std::string prog, std::string module):_ok(false),_errorCode(1),_errorTxt("Not ready")
{
	std::string f;
  std::map<std::string, std::string>  kwds =
    {
      { "file", module },
      { "cfgext", "json" },
      { "prog", "prog" },
      { "home", CUtils::getHomeDir() }
    };

	try
		{
			if (CUtils::findFile(f, stdConfigPaths, kwds ))
				readConfigFile(f);
			else
				throw BasicException("Config file not found", 1);
			_ok = true;
			setError(0, "");
		}
	catch (BasicException& e)
		{
			setError(e.code(), e.what());
		}
}

Config::~Config()
{
}

void Config::readConfigFile(std::string& file)
{
  std::ifstream ifs(file);
  if (ifs.fail())
    throw BasicException("Could not open config file "+file);

  try
    {
      json << ifs;
    }
  catch (std::invalid_argument ia)
    {
      throw BasicException ("Invalid argument in JSON ("+std::string(ia.what())+")");
    }
  catch (std::exception e)
    {
      throw BasicException ("Error reading JSON ("+std::string(e.what())+")");
    }
	
}

void Config::setError(uint16_t code, std::string message)
{
	_errorCode = code;
	_errorTxt = message;
}
