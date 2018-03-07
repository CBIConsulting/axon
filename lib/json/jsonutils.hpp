#pragma once

#include "json.hpp"

namespace
{
	template <typename _TYPE_>
	static bool _get(nlohmann::json& _json, _TYPE_& var, std::string name)
	{		
		if (_json.find(name) == _json.end())
			return false;
		else
			var = _json[name].get<_TYPE_>();
		return true;
	}
};

namespace jsonutils
{
	template <typename _TYPE_>
	static bool get(nlohmann::json& _json, _TYPE_& var, std::string name)
	{
		auto dot = name.find('.');
		if (dot == std::string::npos)
			return _get(_json, var, name);
		auto right = _json[name.substr(0,dot)];
		if (right.is_null())
			return false;

		return _get(right, var, name.substr(dot+1));
	}
};	
