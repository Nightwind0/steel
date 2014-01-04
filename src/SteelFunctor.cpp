#include <string>
#include <queue>
#include "SteelFunctor.h"
#include "SteelInterpreter.h"
#include "Ast.h"

namespace Steel { 
	

class AutoRemover
{
public:
  AutoRemover(SteelInterpreter* pInterpreter, AuxVariables* pVariables):m_pInterpreter(pInterpreter),m_aux_vars(pVariables){
  }
  ~AutoRemover(){
	m_pInterpreter->removeAuxVariables(m_aux_vars);
  }
private:
	SteelInterpreter* m_pInterpreter;
	AuxVariables * m_aux_vars;
};

SteelFunctor::SteelFunctor()
{
}

SteelFunctor::~SteelFunctor()
{
}




  SteelUserFunction::SteelUserFunction(std::shared_ptr<AstParamDefinitionList> pParams, std::shared_ptr<AstStatementList> pList)
    :m_pParams(pParams),m_pList(pList),m_bound_interpreter(NULL)
{
}

SteelUserFunction::~SteelUserFunction()
{
	// we don't delete these because the statements this function is made up of already deletes them
/*  delete m_pParams;
  delete m_pList;
  */
	if(m_bound_interpreter)
		m_bound_interpreter->unlinkAuxVariables(&m_nonlocals);
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

    if(m_pParams)
        m_pParams->executeDeclarations(pInterpreter);

    if(m_pList)
    {
      AutoRemover remover(pInterpreter,&m_nonlocals);
      pInterpreter->registerAuxVariables(&m_nonlocals);
      if(m_pList->execute(pInterpreter) == AstStatement::RETURN){
	ret = pInterpreter->popReturnStack();
      }//else
      //pInterpreter->getReturnStack().push(SteelType());
    }
    
    pInterpreter->popScope();
    return ret;
}

void SteelUserFunction::bindNonLocals(SteelInterpreter* pInterpreter) {
	// TODO: Here we go through our statements looking for variable identifiers
	// be there arrays, hashes, scalars, doesn't matter. 
	// and we take those id's and look up their values in the interpreter,
	// and copy them into our nonlocal variable file. 
	std::list<AstIdentifier*> ids;
	m_pList->FindIdentifiers(ids);
	
	for(std::list<AstIdentifier*>::const_iterator it = ids.begin(); it != ids.end(); it++){
		const std::string name = (*it)->getValue();
		if(m_pParams->containsName(name))
			continue; 
		try{
		  SteelType *val = pInterpreter->enclose_value(name);
		  m_nonlocals.add(name,val);
		}catch(UnknownIdentifier e){
		}
	}
	
	pInterpreter->linkAuxVariables(&m_nonlocals);
	m_bound_interpreter = pInterpreter;
	//TODO:  by the way, don't forget to set them const if they were declared const
}



}
