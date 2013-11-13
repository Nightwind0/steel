#include "AuxVariables.h"
#include "SteelException.h"
#include <cassert>

namespace Steel { 

AuxVariables::AuxVariables() {

}

AuxVariables::~AuxVariables() {
	for(std::vector<SteelType*>::const_iterator it = m_owned.begin();
		it != m_owned.end(); it++){
		delete *it;
	}
}


SteelType AuxVariables::lookup( const std::string& id )  {
    // if strict, throw Unknown Identifier
    SteelType * pVar = lookupLValue(id);
    return *pVar;	
}


SteelType* AuxVariables::lookupLValue( const std::string& id ) {
	std::map<std::string,SteelType*>::iterator it = m_variables.find(id);
	if( it != m_variables.end()  )	{
		// Found a match.
		assert(it->second);
		return it->second;
	}else{
		return NULL;
	}
}


void AuxVariables::add( const std::string& id, SteelType* value ) {
	m_variables[id] = value;
}

bool AuxVariables::transferOwnership( const std::map< std::string, SteelType >& file ) {
	for(std::map<std::string,SteelType*>::iterator mine_it = m_variables.begin();
		mine_it != m_variables.end(); mine_it++){
		SteelType* mine = mine_it->second;
		for(std::map<std::string,SteelType>::const_iterator it = file.begin();
			it != file.end(); it++){
		  const std::string name = it->first;
		  const SteelType * theirs = &(it->second);
		  if(mine == theirs){
		    SteelType *copy = new SteelType(*theirs);
		    m_owned.push_back(copy);
		    
		    mine_it->second = copy; 
		    break;
		  }
		}
	}
	return true;
}



	
	
}
