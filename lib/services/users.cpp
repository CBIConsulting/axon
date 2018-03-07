#include "users.hpp"
#include "../cutils.hpp"
#include "../data/models/user.hpp"
#include "../glove/json.hpp"
#include "../glove/gloveexception.hpp"
#include "../basic_exception.hpp"

UserService::UserService(DbEngine& dbEngine): dbEngine(dbEngine)
{
}

UserService::~UserService()
{
}

CapidModuleRestOutput UserService::_get(CapidModuleRestInput& input)
{
	auto usr = CapidModel::User::getModel();	

	CapidModuleRestOutput output;
	auto username = input.special["id"];
	auto usersdata = (username.empty())?usr.getAll():usr.getAll(username);

	nlohmann::json jsout(usersdata);
	output.output=jsout.dump(2);
	return output;
}

CapidModuleRestOutput UserService::_post(CapidModuleRestInput& input) 
{
	CapidModuleRestOutput output;

	auto inputJson = nlohmann::json::parse(input.rawData);
	auto username = inputJson["username"];
	auto password = inputJson["password"];
	auto allowed = inputJson["allowed"];
	std::string _username, _password;
	uint32_t _allowed=1;
			
	if (username.is_null())
		throw GloveApiException(1, "No username given");
	else
		_username = username.get<std::string>();
			
	if (password.is_null())
		throw GloveApiException(2, "No password given");
	else
		_password = password.get<std::string>();

	if (!allowed.is_null())
		_allowed = allowed.get<int>();

	auto usr = CapidModel::User::getModel();	
	auto current = usr.get(_username);
	if (current.good())
		throw GloveApiException(3, "User already exists");

	auto userid = usr.create(_username, _password, (_allowed));
	if (!userid)
		throw GloveApiException(4, "There was a problem creating user "+_username);

	nlohmann::json jsout({ { "module", "users"}, { "action", "create" }, { "userid", std::to_string(userid) } });
	output.output=jsout.dump(2);

	/* New USER */
	return output;
}

CapidModuleRestOutput UserService::_put(CapidModuleRestInput& input)
{
	CapidModuleRestOutput output;

	auto inputJson = nlohmann::json::parse(input.rawData);
	auto username = inputJson["username"];
	auto password = inputJson["password"];
	auto allowed = inputJson["allowed"];
	std::string _username, _password;
	int _allowed=-1;

	if (username.is_null())
		throw GloveApiException(1, "No username given");
	else
		_username = username.get<std::string>();
			
	if (!password.is_null())
		_password = password.get<std::string>();

	if (!allowed.is_null())
		_allowed = allowed.get<unsigned>();

	if ( (_password.empty()) && (_allowed<0) )
		throw GloveApiException(5, "No data given");
	
	auto usr = CapidModel::User::getModel();	
	auto current = usr.get(_username);
	if (!current.good())
		throw GloveApiException(6, "User does not exists");

	if (!_password.empty())
		{
			if (!usr.update(_username, _password))
				throw GloveApiException(7, "There was a problem updating password");
		}

	if (!_allowed>=0)
		{
			if (!usr.update(_username, (_allowed>0)))
				throw GloveApiException(8, "There was a problem updating permission");
		}

	nlohmann::json jsout({ { "module", "users"}, { "action", "update" }, { "userid", std::to_string(current.userid()) } });
	output.output=jsout.dump(2);

	/* UPDATE USER */
	return output;

}

CapidModuleRestOutput UserService::_delete(CapidModuleRestInput& input)
{
	CapidModuleRestOutput output;

	auto inputJson = nlohmann::json::parse(input.rawData);
	auto username = inputJson["username"];
	std::string _username;

	if (username.is_null())
		throw GloveApiException(1, "No username given");
	else
		_username = username.get<std::string>();

	if (_username == "admin")
		throw GloveApiException(9, "Admin cannot be deleted");
	if (_username == input.user)
		throw GloveApiException(10, "You cannot delete yourself");
	
	auto usr = CapidModel::User::getModel();	
	auto current = usr.get(_username);
	if (!current.good())
		throw GloveApiException(6, "User does not exists");

	if (!usr.destroy(_username))
		throw GloveApiException(11, "User "+_username+" could not be deleted");

	nlohmann::json jsout({ { "module", "users"}, { "action", "delete" }, { "userid", std::to_string(current.userid()) } });
	output.output=jsout.dump(2);

	/* DELETE USER */
	return output;
}
