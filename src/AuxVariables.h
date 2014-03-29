#ifndef _AUX_VARIABLES_HH
#define _AUX_VARIABLES_HH

#include "SteelType.h"
#include <map>
#include <unordered_map>
#include <vector>

namespace Steel { 
	class AuxVariables {
	public:
		AuxVariables();
		~AuxVariables();

                typedef std::map<std::string,SteelType*> VarPtrFile;
                typedef std::map<std::string,SteelType> VariableFile;
		
		SteelType lookup(const std::string& id);
		SteelType* lookupLValue(const std::string& id);
		void add(const std::string& id, SteelType* value);
		bool transferOwnership(const VariableFile& file); 
	private:
        VarPtrFile m_variables;		
		std::vector<SteelType*> m_owned;
	};
};

#endif
