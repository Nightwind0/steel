#ifndef _AUX_VARIABLES_HH
#define _AUX_VARIABLES_HH

#include "SteelType.h"
#include <map>
#include <vector>

namespace Steel { 
	class AuxVariables {
	public:
		AuxVariables();
		~AuxVariables();
		
		SteelType lookup(const std::string& id);
		SteelType* lookupLValue(const std::string& id);
		void add(const std::string& id, SteelType* value);
		bool transferOwnership(const std::map<std::string,SteelType>& file); 
	private:
		std::map<std::string,SteelType*> m_variables;		
		std::vector<SteelType*> m_owned;
	};
};

#endif
