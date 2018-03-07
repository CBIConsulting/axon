#include "axond_module_info.hpp"
#include "axond_auth.hpp"
#include "basic_exception.hpp"
#include <dlfcn.h>

CapidModuleRest::CapidModuleRest(std::map<Method, CapidModuleSharedAccess>& permission,
																 CapidModuleSharedBase::_rest_action_callback _getCallback,
																 CapidModuleSharedBase::_rest_action_callback _postCallback,
																 CapidModuleSharedBase::_rest_action_callback _putCallback,
																 CapidModuleSharedBase::_rest_action_callback _patchCallback,
																 CapidModuleSharedBase::_rest_action_callback _deleteCallback):	permission(permission)
{
	_get_action = nullptr;
	_post_action = nullptr;
	_put_action = nullptr;
	_patch_action = nullptr;
	_delete_action = nullptr;

	if (_getCallback != nullptr)
		_get_action = [_getCallback] (CapidModuleRestInput& input) -> CapidModuleRestOutput
			{
				return _getCallback(input);
			};
	if (_postCallback != nullptr)
		_post_action = [_postCallback] (CapidModuleRestInput& input) -> CapidModuleRestOutput
			{
				return _postCallback(input);
			};
	if (_putCallback != nullptr)
		_put_action = [_putCallback] (CapidModuleRestInput& input) -> CapidModuleRestOutput
			{
				return _putCallback(input);
			};
	if (_patchCallback != nullptr)
		_patch_action = [_patchCallback] (CapidModuleRestInput& input) -> CapidModuleRestOutput
			{
				return _patchCallback(input);
			};
	if (_deleteCallback != nullptr)
		_delete_action = [_deleteCallback] (CapidModuleRestInput& input) -> CapidModuleRestOutput
			{
				return _deleteCallback(input);
			};
}

CapidModuleRest::CapidModuleRest(std::map<Method, CapidModuleSharedAccess>& permission,
																 _Rest_action_callback getCallback,
																 _Rest_action_callback postCallback,
																 _Rest_action_callback putCallback,
																 _Rest_action_callback patchCallback,
																 _Rest_action_callback deleteCallback): permission(permission)
{
	_get_action = getCallback;
	_post_action = postCallback;
	_put_action = putCallback;
	_patch_action = patchCallback;
	_delete_action = deleteCallback;
}

void CapidModuleRest::requestAction(CapidModuleRest::Method met, _Rest_action_callback action, GloveHttpRequest& req, GloveHttpResponse& res)
{
	try
		{
			if (action == nullptr)
				throw CapidModuleException(1000, "Method not allowed");

			CapidAuth auth(req);

			if (!auth.granted(permission[met]))
				throw CapidAuthException(1000, "Access denied");

			CapidModuleRestInput input;
			CapidModuleRestOutput output;
			input.guest = auth.isGuest();
			input.local = auth.isLocal();
			input.user = auth.getUser();
			input.userId = auth.getUserId();
			input.groups = auth.getGroups();
			input.rawData = req.getData();
			input.special = req.special;
			input.ipAddress = req.getClient()->get_address(false);

			for (auto el : req.getDataCol())
				input.request.insert(std::make_pair(el.first, el.second));
			input.arguments = req.getUri().arguments;

			output = action(input);
			res << output.output;
		}
	catch (CapidModuleException& e)
		{
			res << "Module error: "<< ((e.code()>=1000)?e.what():"");
		}
	catch (CapidAuthException& e)
		{
			res << "Auth error: "<< ((e.code()>=1000)?e.what():"");
		}
	catch (BasicException& e)
		{
			res << "Application error: "<< ((e.code()>=10000)?e.what():"");
		}

}

void CapidModuleRest::get(GloveHttpRequest& req, GloveHttpResponse& res)
{
	requestAction(GET, _get_action, req, res);
}

void CapidModuleRest::post(GloveHttpRequest& req, GloveHttpResponse& res)
{
	requestAction(POST, _post_action, req, res);
}

void CapidModuleRest::put(GloveHttpRequest& req, GloveHttpResponse& res)
{
	requestAction(PUT, _put_action, req, res);
}

void CapidModuleRest::patch(GloveHttpRequest& req, GloveHttpResponse& res)
{
	requestAction(PATCH, _patch_action, req, res);
}

void CapidModuleRest::delete_(GloveHttpRequest& req, GloveHttpResponse& res)
{
	requestAction(DELETE, _delete_action, req, res);
}

CapidModuleInfo::CapidModuleInfo(const std::string filename, std::string& config, CapidModuleSafeDb& db, PluginDataModel& pd)
{
	dlfile = NULL;
	/* CapidModuleSafeDb sdb; */
	/* db.safeDbExport(sdb); */
	/* moduleref.db = sdb; */
	auto module = std::unique_ptr<CapidModuleSharedBase>(new CapidModuleShared(filename, db, pd, config, true, true, ""));
	_module = std::move(module);
	_rest = nullptr;
	dlfile = dlopen(filename.c_str(), RTLD_LAZY);
	if (dlerror() != NULL)
		throw BasicException("Unable to load module "+filename, 103);
	auto init = (CapidModuleShared::_init_function)dlsym(dlfile, "init");
	if (dlerror()!=NULL)
		throw BasicException("Unable to init module "+filename, 104);

	auto& moduleref = static_cast<CapidModuleShared&>(*_module);
	//	moduleref.daemonSettings(sdb);
	ok = init(moduleref);
}

CapidModuleInfo::CapidModuleInfo(CapidModuleInfo&& old)
{
	_module = std::move(old._module);
	/* if (old._rest != nullptr) */
	_rest = std::move(old._rest);
	dlfile = old.dlfile;
	old.dlfile = NULL;
}


CapidModuleInfo::~CapidModuleInfo()
{
	if (dlfile)
		{
			dlclose(dlfile);
		}
}

void* CapidModuleInfo::getFunctionPtr(std::string fname)
{
	if (fname.empty())
		return nullptr;

	void *fun = dlsym(dlfile, fname.c_str());
	if (dlerror()!=NULL)
		return nullptr;

	return fun;
}

CapidModuleRest* CapidModuleInfo::getRestFuncs(std::map<std::string, std::string>&settings, std::map<CapidModuleRest::Method, CapidModuleSharedAccess>& permissions)
{
	/* We should have a better way to pass the resource.
		 A pointer? Thats so XX century!! */
	auto fget    = (CapidModuleShared::_rest_action_callback)getFunctionPtr(settings["get"]);
	auto fpost   = (CapidModuleShared::_rest_action_callback)getFunctionPtr(settings["post"]);
	auto fput    = (CapidModuleShared::_rest_action_callback)getFunctionPtr(settings["put"]);
	auto fpatch  = (CapidModuleShared::_rest_action_callback)getFunctionPtr(settings["patch"]);
	auto fdelete = (CapidModuleShared::_rest_action_callback)getFunctionPtr(settings["delete"]);

	auto servrest = new CapidModuleRest(permissions,
																			fget,
																			fpost,
																			fput,
																			fpatch,
																			fdelete);
	return servrest;
}
