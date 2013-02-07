#include "AuxVariables.h"
#include "SteelException.h"

namespace Steel { 

AuxVariables::AuxVariables() {

}

AuxVariables::~AuxVariables() {

}


SteelType AuxVariables::lookup( const std::string& id )  {
    // if strict, throw Unknown Identifier
    SteelType * pVar = lookupLValue(id);
    return *pVar;	
}


SteelType* AuxVariables::lookupLValue( const std::string& id ) {
	std::map<std::string,SteelType>::iterator it = m_variables.find(id);
	if( it != m_variables.end()  )	{
		// Found a match.
		return &(it->second);
	}else{
		return NULL;
	}
}


void AuxVariables::add( const std::string& id, const SteelType& value ) {
	m_variables[id] = value;
}


	
	
}
