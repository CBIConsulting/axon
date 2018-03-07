#pragma once

#include "glove/utils.hpp"
#include "axond_module.hpp"
#include "glove/glovehttpserver.hpp"
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <iostream>

class CapidModuleRest
{
public:
	enum Method
		{
			_ALL_,
			GET,
			POST,
			UPDATE,
			PUT,
			PATCH,
			DELETE
		};

	/* Must be compatible with CapidModuleSharedBase::_rest_action_callback */
	using _Rest_action_callback = std::function<CapidModuleRestOutput (CapidModuleRestInput&)>;

	CapidModuleRest(std::map<Method, CapidModuleSharedAccess>& permission,
									CapidModuleSharedBase::_rest_action_callback _getCallback,
									CapidModuleSharedBase::_rest_action_callback _postCallback=nullptr,
									CapidModuleSharedBase::_rest_action_callback _putCallback=nullptr,
									CapidModuleSharedBase::_rest_action_callback _patchCallback=nullptr,
									CapidModuleSharedBase::_rest_action_callback _deleteCallback=nullptr
		);
	CapidModuleRest(std::map<Method, CapidModuleSharedAccess>& permission,
									_Rest_action_callback getCallback,
									_Rest_action_callback postCallback=nullptr,
									_Rest_action_callback putCallback=nullptr,
									_Rest_action_callback patchCallback=nullptr,
									_Rest_action_callback deleteCallback=nullptr
	);

	void get(GloveHttpRequest& req, GloveHttpResponse& res);
	void post(GloveHttpRequest& req, GloveHttpResponse& res);
	void put(GloveHttpRequest& req, GloveHttpResponse& res);
	void patch(GloveHttpRequest& req, GloveHttpResponse& res);
	void delete_(GloveHttpRequest& req, GloveHttpResponse& res);

private:
	void requestAction(Method met, _Rest_action_callback action, GloveHttpRequest& req, GloveHttpResponse& res);
	std::map<Method, CapidModuleSharedAccess> permission;
	_Rest_action_callback _get_action;
	_Rest_action_callback _post_action;
	_Rest_action_callback _put_action;
	_Rest_action_callback _patch_action;
	_Rest_action_callback _delete_action;
};

class CapidModuleInfo
{
public:
	CapidModuleInfo(const std::string filename, std::string& config, CapidModuleSafeDb& db, PluginDataModel& pd);
	CapidModuleInfo(CapidModuleInfo&& old);

	~CapidModuleInfo();

	CapidModuleSharedBase& module()
	{
		return *_module;
	}

	bool good()
	{
		return ok;
	}

	CapidModuleRest* getRestFuncs(std::map<std::string, std::string>&settings, std::map<CapidModuleRest::Method, CapidModuleSharedAccess>& permissions);
private:
  std::unique_ptr<CapidModuleSharedBase> _module;
	std::unique_ptr<CapidModuleRest> _rest;
	void* dlfile;
  bool ok;

	void* getFunctionPtr(std::string fname);

};
