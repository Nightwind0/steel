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




SteelUserFunction::SteelUserFunction(shared_ptr<AstParamDefinitionList> pParams, shared_ptr<AstStatementList> pList)
    :m_pParams(pParams),m_pList(pList)
{
}

SteelUserFunction::~SteelUserFunction()
{
/*  delete m_pParams;
  delete m_pList;
  */
}

SteelType SteelUserFunction::Call(SteelInterpreter * pInterpreter,const SteelType::Container &supplied_params)
{
    SteelType ret;
    
    pInterpreter->pushScope();

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
        m_pParams->executeDeclarations(pInterpreter, supplied_params);

    if(m_pList)
    {
       
        if(m_pList->execute(pInterpreter) == AstStatement::RETURN)
            ret = pInterpreter->getReturn();
		else
			pInterpreter->setReturn(SteelType()); // If they didn't return anything, we return a blank one for them
    }

    pInterpreter->popScope();
    return ret;
}


}
