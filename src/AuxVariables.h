#ifndef _AUX_VARIABLES_HH
#define _AUX_VARIABLES_HH

#include "SteelType.h"

namespace Steel { 
	class AuxVariables {
	public:
		AuxVariables();
		~AuxVariables();
		
		SteelType lookup(const std::string& id);
		SteelType* lookupLValue(const std::string& id);
		void add(const std::string& id, const SteelType& value);
	private:
		std::map<std::string,SteelType> m_variables;		
	};
};

#endif
