#pragma once

#include "datamodel.hpp"
#include "../../glove/utils.hpp"

namespace CapidModel
{
	class PluginData : public CapidModel::DataModel<PluginData>
	{
	public:
		class Data
		{
		public:
			Data(std::string pluginname,
					 std::string key,
					 std::string value,
					 std::string ctime,
					 std::string mtime):
				_pluginname(pluginname),
				_key(key),
				_value(value),
				_ctime(ctime),
				_mtime(mtime)
			{

			}
			std::string pluginname()
			{
				return _pluginname;
			}

			std::string key()
			{
				return _key;
			}

			std::string value()
			{
				return _value;
			}

			std::string ctime()
			{
				return _ctime;
			}

			std::string mtime()
			{
				return _mtime;
			}
		private:
			std::string _pluginname, _key, _value, _ctime, _mtime;
		};

		~PluginData()
		{
		}

		std::string get(std::string plugin, std::string key)
		{
			return "123";
		}

		bool set(std::string plugin, std::string key, std::string value)
		{
			return true;
		}
	private:
		friend DataModel<PluginData>;
		PluginData(DbEngine& engine):DataModel(engine)
		{
			init();
		}

		void init()
		{
			engine.createTable("PLUGINDATA", {
					{ "pluginname", "TEXT" },
					{ "key", "TEXT" },
					{ "value", "TEXT" },
					{ "mtime", "DATETIME" },
					{ "ctime", "DATETIME" }
				}, {
					{ "unique", "pluginname, key" },
					{ "onconflict", "replace" }
				}, true);
		}
	};
};
