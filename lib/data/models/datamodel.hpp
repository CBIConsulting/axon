#pragma once

#include "../dbengine.hpp"
#include <stdexcept>
#include <iostream>

/* Model singleton */

namespace CapidModel
{
	template <class T>
	class DataModel
	{
	public:
		static T& getModel()
		{
		if (thisref == nullptr)
			throw std::runtime_error("model not initialized");

		return *thisref;
		}
		static T& getModel(DbEngine& engine)
		{
			if (thisref == nullptr)
				thisref = new T(engine);

		return *thisref;
		}

		~DataModel() {}

		virtual void init()=0;
	protected:
		DbEngine& engine;

		DataModel(DbEngine& engine): engine(engine)
		{
		}

		void basicInit()
		{
		}
		static T* thisref;
	};

	template <class T> T* DataModel<T>::thisref = nullptr;

};
