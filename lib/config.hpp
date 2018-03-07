#pragma once

#include "json/json.hpp"
#include "json/jsonutils.hpp"
#include <string>

class Config
{
public:
	Config(std::string prog, std::string module);
	~Config();

	bool ok()
	{
		return _ok;
	}
	std::string errorTxt()
	{
		return _errorTxt;
	}
	uint16_t errorCode()
	{
		return _errorCode;
	}

	template <typename _TYPE_>
	bool get(_TYPE_& var, std::string name)
	{
		return jsonutils::get(json, var, name);
	}

	template <typename _TYPE_>
	bool get(_TYPE_& var, std::string name, _TYPE_ defaultValue)
	{
		auto res = get(var, name);
		if (!res)
			var = defaultValue;

		return res;
	}

protected:
	void readConfigFile(std::string& file);
	void setError(uint16_t code, std::string message);
		
private:
	bool _ok;
	std::string _errorTxt;
	uint16_t _errorCode;
  nlohmann::json json;

};
