#include <string>
#include <queue>
#include "SteelFunctor.h"
#include "SteelInterpreter.h"
#include "Ast.h"

namespace Steel { 
	

SteelFunctor::SteelFunctor()
{
}

SteelFunctor::~SteelFunctor()
{
}




  SteelUserFunction::SteelUserFunction(std::shared_ptr<AstParamDefinitionList> pParams, std::shared_ptr<AstStatementList> pList)
    :m_pParams(pParams),m_pList(pList)
{
}

SteelUserFunction::~SteelUserFunction()
{
	// we don't delete these because the statements this function is made up of already deletes them
/*  delete m_pParams;
  delete m_pList;
  */

}

SteelType SteelUserFunction::Call(SteelInterpreter * pInterpreter,const SteelType::Container &supplied_params)
{
    SteelType ret;
    
    pInterpreter->pushScope();


    for(SteelType::Container::const_iterator it = supplied_params.begin(); it != supplied_params.end(); it++){
      pInterpreter->pushParamStack(*it);
    }

    if( m_pParams != NULL)
    {
        if( supplied_params.size() > m_pParams->size() ) throw ParamMismatch();
        if( supplied_params.size() < (m_pParams->size() - m_pParams->defaultCount()) ) throw ParamMismatch();
    }
    else 
    {
        if( supplied_params.size() != 0) throw ParamMismatch();
    }

    // Put bound locals into scope. This way, the params can reference them
      for(const auto& bound : m_bound_variables){
	pInterpreter->declare(bound.first);
	*pInterpreter->lookup_lvalue(bound.first) = bound.second; 
      }


    if(m_pParams)
        m_pParams->executeDeclarations(pInterpreter);

    if(m_pList)
    {
      //AutoRemover remover(pInterpreter,&m_nonlocals);
      //pInterpreter->registerAuxVariables(&m_nonlocals);
      if(m_pList->execute(pInterpreter) == AstStatement::RETURN){
	ret = pInterpreter->popReturnStack();
      }//else
      //pInterpreter->getReturnStack().push(SteelType());
      //pInterpreter->removeAuxVariables(&m_nonlocals);
    }

    // Hacky. This syncs up my values. TODO: Fix this hack
    for(auto& bound : m_bound_variables){
      bound.second = pInterpreter->lookup(bound.first);
    }	
    pInterpreter->popScope();
    return ret;
}

void SteelUserFunction::bindNonLocals(SteelInterpreter* pInterpreter) {
	// TODO: Here we go through our statements looking for variable identifiers
	// be there arrays, hashes, scalars, doesn't matter. 
	// and we take those id's and look up their values in the interpreter,
	// and copy them into our nonlocal variable file.
	if(m_pList){
	  std::list<AstIdentifier*> ids;
	  m_pList->FindIdentifiers(ids);

	
	for(std::list<AstIdentifier*>::const_iterator it = ids.begin(); it != ids.end(); it++){
		const std::string name = (*it)->getValue();
		if(m_pParams->containsName(name))
			continue; 
		try{
		  SteelType *val = pInterpreter->enclose_value(name);
		  m_bound_variables[name] = *val;
		}catch(UnknownIdentifier e){
		}
	}
	}
	//pInterpreter->linkAuxVariables(&m_nonlocals);
	//	m_bound_interpreter = pInterpreter;
	//TODO:  by the way, don't forget to set them const if they were declared const
}



}
