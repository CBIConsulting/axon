#include "modules.hpp"
#include "../cutils.hpp"
#include "../data/models/user.hpp"
#include "../glove/json.hpp"
#include "../glove/gloveexception.hpp"
#include "../basic_exception.hpp"

ModuleService::ModuleService(Daemon& daemon): daemon(daemon)
{
}

ModuleService::~ModuleService()
{
}

CapidModuleRestOutput ModuleService::_get(CapidModuleRestInput& input)
{
	CapidModuleRestOutput output;
	auto modules = daemon.modulesInfo();

	nlohmann::json info(modules);
	output.output = info.dump(2);
	return output;
}

CapidModuleRestOutput ModuleService::_put(CapidModuleRestInput& input)
{
	CapidModuleRestOutput output;

		auto modulename = input.special["id"];

	if (modulename.empty())
		throw GloveApiException(1, "No module specified");

	if (daemon.isModuleLoaded(modulename))
		throw GloveApiException(5, "Module already loaded");

	if (!daemon.safeLoadModule(modulename))
		throw GloveApiException(4, "Unable load module");

	nlohmann::json jsout({ { "module", "modules"}, { "action", "load" }, { "module", modulename } });
	output.output = jsout.dump(2);

	return output;
}

CapidModuleRestOutput ModuleService::_delete(CapidModuleRestInput& input)
{
	CapidModuleRestOutput output;
	auto modulename = input.special["id"];

	if (modulename.empty())
		throw GloveApiException(1, "No module specified");

	if (!daemon.isModuleLoaded(modulename))
		throw GloveApiException(2, "Module not loaded");

	if (!daemon.safeUnloadModule(modulename))
		throw GloveApiException(3, "Unable to unload module");

	nlohmann::json jsout({ { "module", "modules"}, { "action", "unload" }, { "module", modulename } });
	output.output = jsout.dump(2);

	return output;
}
