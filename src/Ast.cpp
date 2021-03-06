#include "Ast.h"
#include "SteelInterpreter.h"
#include "SteelType.h"
#include "SteelException.h"
#include <string>
#include <iostream>
#include <cassert>
#include <memory>

namespace Steel { 

ostream & operator<<(ostream &,AstBase&);


AstBase::AstBase(unsigned int line, const std::string &script)
    :m_line(line),m_script_name(script)
{
}

AstBase::~AstBase()
{
}


void AstBase::FindIdentifiers( std::list<AstIdentifier*>& o_ids ) {
  ;
}



AstInteger::AstInteger(unsigned int line,
                       const std::string &script,
                       int value)
    :AstExpression(line,script),m_value(value)
{
}

ostream & AstInteger::print(std::ostream &out)
{
    out << m_value;
    return out;
}

SteelType AstInteger::evaluate(SteelInterpreter *pInterpreter)
{
    SteelType val;
    val.set(m_value);
    return val;
}

AstString::AstString(unsigned int line,
                     const std::string &script)
    : AstExpression(line,script)
{
}


void AstString::addChar(const char c)
{
    m_value += c;
}

void AstString::addString(const std::string &str)
{
    m_value += str;
}

ostream & AstString::print(std::ostream &out)
{
    out << '\"' << m_value << '\"';
    return out;
}


char AstString::getEscapedChar(char c)
{

    switch (c)
    {
    case '0': return '\0';
    case 'a': return '\a';
    case 'b': return '\b';
    case 't': return '\t';
    case 'n': return '\n';
    case 'v': return '\v';
    case 'f': return '\f';
    case 'r': return '\r';
    default : return c;
    }
}


SteelType AstString::evaluate(SteelInterpreter *pInterpreter)
{
    SteelType var;
    std::string value;
    // TODO: Here, we can process substrings/expressions
    std::string subexpr = "return "; // This basically makes the expression into something useful. sort of a hack...
    bool escaping = false;
    bool in_subexpr = false;
    for(int i=0;i<m_value.size();i++){
	char c = m_value[i];

	if(in_subexpr)
	{
	    if(c!='}')
		subexpr+=c;
	    else
	    {
		in_subexpr = false;
		subexpr += ';';
	
#if 0 && defined(DEBUG)
		std::cerr << "Sub expression: " << subexpr << std::endl;
#endif
		SteelParser parser;
		parser.setBuffer(subexpr.c_str(),m_value);
		AstBase * pBase;
		if(parser.Parse(&pBase) == SteelParser::PRC_SUCCESS && !parser.hadError()){
		    AstScript * pScript = dynamic_cast<AstScript*>(pBase);
		    if(pScript){
			pScript->execute(pInterpreter);
			delete pScript;
		    }
            const std::string str = pInterpreter->popReturnStack();
		    value += str;
		}else{
		    value += "%err%";
		}

		subexpr = "return ";
	    }
	}
	else if(escaping)
	{
	    value += getEscapedChar(c);
	    escaping = false;
	}
	else if(c == '\\')
	{
	    escaping = true;
	}
	else if(c == '{')
	{
	    in_subexpr = true;
	}
	else
	{
	    value += c;
	}
    }

    var.set(value);

    return var;
}


AstFloat::AstFloat(unsigned int line,
                   const std::string &script,
                   double value)
    : AstExpression(line,script),m_value(value)
{
    
}

ostream & AstFloat::print(std::ostream &out)
{
    out << m_value;
    return out;
}

SteelType AstFloat::evaluate(SteelInterpreter *pInterpreter)
{
    SteelType var;
    var.set(m_value);
    
    return var;
}

AstBoolean::AstBoolean(unsigned int line,
                       const std::string &script,
                       bool value)
    :AstExpression(line,script),m_bValue(value)
{
}

ostream & AstBoolean::print(std::ostream &out)
{
    if(m_bValue) out << "true";
    else out << "false";

    return out;
}

SteelType AstBoolean::evaluate(SteelInterpreter *pInterpreter)
{
    SteelType var;
    var.set(m_bValue);

    return var;
}




AstIdentifier::AstIdentifier(unsigned int line,
                             const std::string &script,
                             const std::string &value)
  : AstExpression(line,script),m_value(value),m_pLValue(nullptr)
{
    
}

ostream & AstIdentifier::print(std::ostream &out)
{
    out << m_value;
    return out;
}



AstStatement::AstStatement(unsigned int line, const std::string &script)
    :AstBase(line,script)
{
}

AstStatement::~AstStatement()
{
}

ostream & AstStatement::print(std::ostream &out)
{
    out << ";\n";
    return out;
}



AstExpressionStatement::AstExpressionStatement(unsigned int line, const std::string &script, AstExpression *pExp)
    :AstStatement(line,script),m_pExp(pExp)
{
    assert ( pExp );
}
AstExpressionStatement::~AstExpressionStatement()
{
    
}

ostream & AstExpressionStatement::print(std::ostream &out)
{
    out << *m_pExp << ';' << std::endl;
    return out;
}

AstStatement::eStopType AstExpressionStatement::execute(SteelInterpreter *pInterpreter)
{

    // No try/catch because I expect everything to be caught
    // At a lower level
    m_pExp->evaluate(pInterpreter);

    return AstStatement::COMPLETED;
}

void AstExpressionStatement::FindIdentifiers( std::list< AstIdentifier* >& o_ids ) {
	m_pExp->FindIdentifiers(o_ids);
}


AstScript::AstScript(unsigned int line, const std::string &script)
    :AstStatement(line,script),m_pList(nullptr)
{
}
AstScript::~AstScript()
{
    
}

ostream & AstScript::print(std::ostream &out)
{
    if(m_pList) out << *m_pList;

    return out;
}
void AstScript::FindIdentifiers( std::list< AstIdentifier* >& o_ids ) {
	m_pList->FindIdentifiers(o_ids);
}


AstStatement::eStopType AstScript::execute(SteelInterpreter *pInterpreter)
{
    if(m_pList)
    {
        return m_pList->execute(pInterpreter);
    }
    return AstStatement::COMPLETED;
}

void AstScript::SetList(AstStatementList *pList)
{
    m_pList = std::unique_ptr<AstStatementList>(pList);
    m_pList->setTopLevel();
}


AstStatementList::AstStatementList(unsigned int line, const std::string &script)
    :AstStatement(line,script),m_bTopLevel(false)
{
}

AstStatementList::~AstStatementList()
{

}

void AstStatementList::setTopLevel()
{
    m_bTopLevel = true;
}

void AstStatementList::FindIdentifiers( std::list< AstIdentifier* >& o_ids ) {
    for(auto& i : m_list) 
      i->FindIdentifiers(o_ids);
	
}


AstStatement::eStopType AstStatementList::execute(SteelInterpreter *pInterpreter)
{
    eStopType ret = COMPLETED;
    if(!m_bTopLevel)
		pInterpreter->pushScope();
    for(auto& it : m_list)
    {

        eStopType stop = it->execute(pInterpreter);

        if(stop == BREAK || stop == RETURN || stop ==  CONTINUE)
        {
            ret = stop;
            break;
        }
    }
    if(!m_bTopLevel)
		pInterpreter->popScope();

    return ret;
}

ostream & AstStatementList::print(std::ostream &out)
{
    out << "{\n";
    for(const auto& it : m_list)
    {
        out << "\t\t" << *it;
    }
    out << "\t}\n";

    return out;
}


AstWhileStatement::AstWhileStatement(unsigned int line, const std::string &script, AstExpression *pExp, AstStatement *pStmt)
    :AstStatement(line,script),m_pCondition(pExp),m_pStatement(pStmt)
{
}
AstWhileStatement::~AstWhileStatement()
{

}

AstStatement::eStopType AstWhileStatement::execute(SteelInterpreter *pInterpreter)
{
    // I expect it to cast to boolean
    while( m_pCondition->evaluate(pInterpreter) )
    {
        eStopType stop = m_pStatement->execute(pInterpreter);

        if(stop == BREAK) return COMPLETED;
        else if(stop == RETURN) return RETURN;

        // Note: if stop is CONTINUE,
        // Then we just want to keep looping. So no action.
    }

    return COMPLETED;
}

ostream & AstWhileStatement::print(std::ostream &out)
{
    out << "while (" << *m_pCondition << ")\n"
        << '{' << std::endl << *m_pStatement <<  '}' << std::endl;

    return out;
}

void AstWhileStatement::FindIdentifiers( std::list< AstIdentifier* >& o_ids ) {
	m_pCondition->FindIdentifiers(o_ids);
	m_pStatement->FindIdentifiers(o_ids);
}



AstDoStatement::AstDoStatement(unsigned int line, const std::string &script, AstExpression *pExp, AstStatement *pStmt)
    :AstStatement(line,script),m_pCondition(pExp),m_pStatement(pStmt)
{
}
AstDoStatement::~AstDoStatement()
{

}

AstStatement::eStopType AstDoStatement::execute(SteelInterpreter *pInterpreter)
{
    // I expect it to cast to boolean
    do {
        eStopType stop = m_pStatement->execute(pInterpreter);
        if(stop == BREAK) return COMPLETED;
        else if(stop == RETURN) return RETURN;
        // Note: if stop is CONTINUE,
        // Then we just want to keep looping. So no action.
    } while( m_pCondition->evaluate(pInterpreter) );
    
    return COMPLETED;
}

ostream & AstDoStatement::print(std::ostream &out)
{
  out << "do "<< '{' <<*m_pStatement << "}\n while (" << *m_pCondition << ")\n"
        <<  std::endl;

    return out;
}

void AstDoStatement::FindIdentifiers( std::list< AstIdentifier* >& o_ids ) {
	m_pCondition->FindIdentifiers(o_ids);
	m_pStatement->FindIdentifiers(o_ids);
}



AstIfStatement::AstIfStatement(unsigned int line, const std::string &script,
                               AstExpression *pExp, AstStatement *pStmt,
                               AstStatement *pElse)
    :AstStatement(line,script),m_pCondition(pExp),m_pStatement(pStmt),m_pElse(pElse)
{
}
AstIfStatement::~AstIfStatement()
{

}

AstStatement::eStopType AstIfStatement::execute(SteelInterpreter *pInterpreter)
{
    if(m_pCondition->evaluate(pInterpreter))
        return m_pStatement->execute(pInterpreter);
    else if ( m_pElse) return m_pElse->execute(pInterpreter);
    else return COMPLETED;

}

void AstIfStatement::FindIdentifiers( std::list< AstIdentifier* >& o_ids ) {
	m_pCondition->FindIdentifiers(o_ids);
	m_pElse->FindIdentifiers(o_ids);
	m_pStatement->FindIdentifiers(o_ids);
}


ostream & AstIfStatement::print(std::ostream &out)
{
    out << "if (" << *m_pCondition  << ")\n"
        << "{\n" << *m_pStatement << "}\n";
    if(m_pElse) 
    {
        out << " else {\n" << *m_pElse << "\n}\n";
    }

    return out;
}


AstCaseStatement::AstCaseStatement(unsigned int line, const std::string& script,
                                   AstStatement* pStmt)
    :AstStatement(line,script),m_pStatement(pStmt)
{

}

AstCaseStatement::~AstCaseStatement()
{

}

ostream& AstCaseStatement::print(std::ostream& out)
{
    return m_pStatement->print(out);
}

AstStatement::eStopType AstCaseStatement::execute(SteelInterpreter* pInterpreter)
{
    return m_pStatement->execute(pInterpreter);
}

void AstCaseStatement::FindIdentifiers( std::list< AstIdentifier* >& o_ids ) {
  m_pStatement->FindIdentifiers(o_ids);
}


AstCaseStatementList::AstCaseStatementList(unsigned int line, const std::string& script)
    :AstBase(line,script),m_pDefault(nullptr)
{

}

AstCaseStatementList::~AstCaseStatementList()
{
    // TODO: Delete all cases
}

ostream& AstCaseStatementList::print(std::ostream& out)
{
    // TODO: This
    return out;
}

void AstCaseStatementList::add(AstExpression* matchExpression, AstCaseStatement* statement)
{
    Case case_;
    case_.matchExpression = std::shared_ptr<AstExpression>(matchExpression);
    case_.statement = std::shared_ptr<AstCaseStatement>(statement);
    m_cases.emplace_back(case_);
}

bool AstCaseStatementList::setDefault(AstCaseStatement* statement)
{
    if(m_pDefault){
	return false;
    }

    m_pDefault = std::unique_ptr<AstCaseStatement>(statement);
    return true;
}

AstStatement::eStopType AstCaseStatementList::executeCaseMatching(AstExpression* value, SteelInterpreter* pInterpreter)
{
    SteelType val = value->evaluate(pInterpreter);
    bool matched = false;
    // TODO: Later, if we switch to enum/literal ONLY, then we can make this a map
    for(std::list<Case>::iterator iter = m_cases.begin();
	iter != m_cases.end(); iter++)
    {
	if(matched || iter->matchExpression->evaluate(pInterpreter) == val)
	{
	    matched = true;
	    AstStatement::eStopType stopType = iter->statement->execute(pInterpreter);
	    if(stopType == AstStatement::BREAK || stopType == AstStatement::RETURN)
		return stopType;
	    // Otherwise, keep going - this enables fallthroughs;
	}
    }
  
    if(m_pDefault)
	return m_pDefault->execute(pInterpreter);

    return AstStatement::COMPLETED;
}

void AstCaseStatementList::FindIdentifiers( std::list< AstIdentifier* >& o_ids ) {
  for(std::list<Case>::iterator iter = m_cases.begin();
	iter != m_cases.end(); iter++){
	  (*iter).statement->FindIdentifiers(o_ids);
  }	
  m_pDefault->FindIdentifiers(o_ids);
}


AstSwitchStatement::AstSwitchStatement(unsigned int line, const std::string& script,
				       AstExpression* value, AstCaseStatementList* cases)
    :AstStatement(line,script),m_pValue(value),m_pCases(cases)
{
}


std::ostream& AstSwitchStatement::print(std::ostream& out)
{
    return out;
}

AstStatement::eStopType AstSwitchStatement::execute(SteelInterpreter* pInterpreter)
{
    // TODO: Here we look for a matching case and execute it. 
    // We could look for all matching cases, or throw an error if two match
    // Don't forget, if our stop type is complete, to execute the next case as well.
    // If its break, we stop. If it's return, we return .
    // TODO: Future; Add enums to steel. Then, have the C++ be able to inject enums in,
    // and then ONLY accept literal ints or enums in switch statements
    eStopType stopType =  m_pCases->executeCaseMatching(m_pValue.get(),pInterpreter);

    // Eat breaks
    if(stopType == BREAK) 
	stopType = COMPLETED;

    return stopType;
}

void AstSwitchStatement::FindIdentifiers( std::list< AstIdentifier* >& o_ids ) {
	m_pCases->FindIdentifiers(o_ids);
	m_pValue->FindIdentifiers(o_ids);
}



AstReturnStatement::AstReturnStatement(unsigned int line, const std::string &script, AstExpression *pExp)
    :AstStatement(line,script),m_pExpression(pExp)
{
}

AstReturnStatement::~AstReturnStatement()
{
    
}

AstStatement::eStopType AstReturnStatement::execute(SteelInterpreter *pInterpreter)
{
    if( m_pExpression )
      pInterpreter->pushReturnStack(m_pExpression->evaluate(pInterpreter) );
    else pInterpreter->pushReturnStack(SteelType());
    return RETURN;
}

void AstReturnStatement::FindIdentifiers( std::list< AstIdentifier* >& o_ids ) {
	m_pExpression->FindIdentifiers(o_ids);
}


AstImport::AstImport(unsigned int line, 
                     const std::string &script,
                     AstString *pStr):AstStatement(line,script),m_ns(pStr->getString())
{
}

AstImport::~AstImport()
{
}

std::ostream & AstImport::print(ostream &out)
{
    out << "using \"" << m_ns << "\";\n";
    return out;
}
AstStatement::eStopType AstImport::execute(SteelInterpreter *pInterpreter)
{
    pInterpreter->import(m_ns);
    return COMPLETED;
}



ostream & AstReturnStatement::print(std::ostream &out)
{
    out << "return ";
    if(m_pExpression)
    {
        out << '(' << *m_pExpression << ')';
    }
    out << ';' << std::endl;
    return out;

}

AstLoopStatement::AstLoopStatement(unsigned int line, const std::string &script,
                                   AstExpression *pStart, AstExpression *pCondition,
                                   AstExpression *pIteration, AstStatement* pStmt)
  :AstStatement(line,script),m_pStart(pStart),m_pCondition(pCondition),m_pIteration(pIteration),m_pStatement(pStmt),m_pDecl(nullptr)
{
}
AstLoopStatement::AstLoopStatement(unsigned int line, const std::string &script,
                                   AstDeclaration *pDecl, AstExpression *pCondition,
                                   AstExpression *pIteration, AstStatement* pStmt)
  :AstStatement(line,script),m_pStart(nullptr),m_pCondition(pCondition),m_pIteration(pIteration),m_pStatement(pStmt),m_pDecl(pDecl)
{
}

AstLoopStatement::~AstLoopStatement()
{

}

AstStatement::eStopType AstLoopStatement::execute(SteelInterpreter *pInterpreter)
{
    // A for loop has its own scope like in C++
    // (Beyond the scope that the body may have anyway)
    pInterpreter->pushScope();
    if(m_pDecl) m_pDecl->execute(pInterpreter);
    for( m_pStart?m_pStart->evaluate( pInterpreter):1 ;
         m_pCondition->evaluate(pInterpreter) ;
         m_pIteration->evaluate( pInterpreter )
        )
    {
        eStopType stop = m_pStatement->execute(pInterpreter);

        if(stop == BREAK) 
        {
            pInterpreter->popScope();
            return COMPLETED;
        }
        else if (stop == RETURN) 
        {
            pInterpreter->popScope();
            return RETURN;
        }

        // For both CONTINUE and COMPLETED, we just keep going.
    }

    pInterpreter->popScope();
    return COMPLETED;
}

void AstLoopStatement::FindIdentifiers( std::list< AstIdentifier* >& o_ids ) {
  if(m_pDecl)
    m_pDecl->FindIdentifiers(o_ids);

  if(m_pStart)
    m_pStart->FindIdentifiers(o_ids);

  if(m_pCondition)
    m_pCondition->FindIdentifiers(o_ids);
  if(m_pIteration)
    m_pIteration->FindIdentifiers(o_ids);

  if(m_pStatement)
    m_pStatement->FindIdentifiers(o_ids);
}


ostream & AstLoopStatement::print(std::ostream &out)
{
    out << "for (" << *m_pStart << ';' << *m_pCondition << ';' << *m_pIteration << ')' << *m_pStatement;
    
    return out;
}


AstForEachStatement::AstForEachStatement(unsigned int line, const std::string& script,
					 AstDeclaration* decl, AstExpression * array_exp, AstStatement* stmt)
    :AstStatement(line,script),m_pLValue(nullptr),m_pDeclaration(decl),m_pArrayExpression(array_exp),m_pStatement(stmt)
{
}

AstForEachStatement::AstForEachStatement(unsigned int line, const std::string& script,
					 AstIdentifier* lvalue, AstExpression* array_exp, AstStatement* stmt)
    :AstStatement(line,script),m_pDeclaration(nullptr),m_pLValue(lvalue),m_pArrayExpression(array_exp),m_pStatement(stmt)
{
}

AstForEachStatement::~AstForEachStatement()
{

}

void AstForEachStatement::FindIdentifiers( std::list< AstIdentifier* >& o_ids ) {
  //if(m_pDeclaration) m_pDeclaration->FindIdentifiers(o_ids);
	if(m_pArrayExpression) m_pArrayExpression->FindIdentifiers(o_ids);
	if(m_pLValue) m_pLValue->FindIdentifiers(o_ids);
	if(m_pStatement) m_pStatement->FindIdentifiers(o_ids);
}



ostream& AstForEachStatement::print(std::ostream &out)
{
    // TODO: This
	return out;
}

AstStatement::eStopType AstForEachStatement::execute(SteelInterpreter* pInterpreter)
{
	AstStatement::eStopType stoptype;
    pInterpreter->pushScope();
    try{

	SteelType * val = nullptr;
	if(m_pDeclaration)
	{
	  AstDeclaration * vardecl = m_pDeclaration.get();

	  vardecl->execute(pInterpreter);
	  AstIdentifier *pId = vardecl->getIdentifier();

	  val = pId->lvalue(pInterpreter);

	  if(nullptr == val) throw SteelException(SteelException::INVALID_LVALUE, GetLine(),GetScript(),"Invalid lvalue used as iterator in foreach.");
	    
	}
	else if(m_pLValue)
	{
	   val =  m_pLValue->lvalue(pInterpreter);

	   if(nullptr == val) throw SteelException(SteelException::INVALID_LVALUE,
                                              GetLine(),
                                              GetScript(),
                                              "Invalid lvalue used as iterator in foreach.");
	}
	else
	{
	    throw SteelException(SteelException::INVALID_LVALUE, GetLine(),GetScript(), "Invalid lvalue used as iterator in foreach.");
	}

	SteelType array = m_pArrayExpression->evaluate(pInterpreter);

	if(!array.isArray())
	{
	    throw SteelException(SteelException::TYPE_MISMATCH,GetLine(),GetScript(),"foreach on a non-array");
	}

	for(unsigned int i=0;i<array.getArraySize();i++)
	{
	    *val = array.getElement(i);
	    stoptype = m_pStatement->execute(pInterpreter);
		if(stoptype == RETURN || stoptype == BREAK){
			pInterpreter->popScope();
			return stoptype;
		}
	}

    }
    catch (UnknownIdentifier e)
    {
	throw SteelException(SteelException::UNKNOWN_IDENTIFIER, GetLine(),GetScript(),"Unknown identifier in foreach statement: '" + e.identifier + '\'');
    }

    pInterpreter->popScope();

    return COMPLETED;
}




AstIncDec::AstIncDec(unsigned int line,
                     const std::string &script,
                     AstExpression *pLValue,
                     Order order)
    :AstExpression(line,script),m_pLValue(pLValue),m_order(order)
{
}
AstIncDec::~AstIncDec()
{
   
}

void AstIncDec::FindIdentifiers( std::list< AstIdentifier* >& o_ids ) {
	return m_pLValue->FindIdentifiers(o_ids);
}



AstIncrement::AstIncrement(unsigned int line,
                           const std::string &script,
                           AstExpression *pLValue,
                           AstIncDec::Order order)
    :AstIncDec(line,script,pLValue,order)
{
}

AstIncrement::~AstIncrement()
{
}

SteelType AstIncrement::evaluate(SteelInterpreter *pInterpreter)
{
    try
    {
        SteelType *pVar = m_pLValue->lvalue(pInterpreter);

        if(nullptr == pVar) throw SteelException(SteelException::INVALID_LVALUE,
                                              GetLine(),
                                              GetScript(),
                                              "Invalid lvalue before increment (++) operator.");
    
        if(m_order == PRE)
            return ++( *pVar );
        else return (*pVar)++;

    }
    catch(OperationMismatch)
    {
        throw SteelException(SteelException::TYPE_MISMATCH,
                             GetLine(),
                             GetScript(),
                             "Invalid type before increment (++) operator.");
    }
    catch(UnknownIdentifier id)
    {
        throw SteelException(SteelException::UNKNOWN_IDENTIFIER,
                             GetLine(),
                             GetScript(),
                             "Unknown identifier before increment: '" + id.identifier + '\'');
    }

    return SteelType();
}

SteelType * AstIncrement::lvalue(SteelInterpreter *pInterpreter)
{
    return nullptr;
}


AstDecrement::AstDecrement(unsigned int line,
                           const std::string &script,
                           AstExpression *pLValue,
                           AstIncDec::Order order)
    :AstIncDec(line,script,pLValue,order)
{
}

AstDecrement::~AstDecrement()
{
}

SteelType AstDecrement::evaluate(SteelInterpreter *pInterpreter)
{
    try
    {
        SteelType *pVar = m_pLValue->lvalue(pInterpreter);

        if(nullptr == pVar) throw SteelException(SteelException::INVALID_LVALUE,
                                              GetLine(),
                                              GetScript(),
                                              "Invalid lvalue before decrement (--) operator.");
    
        if(m_order == PRE)
            return --( *pVar );
        else return (*pVar)--;

    }
    catch(OperationMismatch)
    {
        throw SteelException(SteelException::TYPE_MISMATCH,
                             GetLine(),
                             GetScript(),
                             "Invalid type before decrement (--) operator.");
    }
    catch(UnknownIdentifier id)
    {
        throw SteelException(SteelException::UNKNOWN_IDENTIFIER,
                             GetLine(),
                             GetScript(),
                             "Unknown identifier before decrement: '" + id.identifier + '\'');
    }

    return SteelType();
}

SteelType * AstDecrement::lvalue(SteelInterpreter *pInterpreter)
{
    return nullptr;
}




AstBinOp::AstBinOp(unsigned int line,
                   const std::string &script,
                   Op op, AstExpression *left, AstExpression *right)
    :AstExpression(line,script),m_op(op),m_right(right),m_left(left)
{
}
AstBinOp::~AstBinOp()
{

}

std::string AstBinOp::ToString(Op op)
{
    switch(op)
    {
    case ADD:
        return "+";
    case SUB:
        return "-";
    case MULT:
        return "*";
    case DIV:
        return "/";
    case MOD:
        return "%";
    case AND:
        return " and ";
    case OR:
        return " or ";
    case D:
        return " d ";
    case POW:
        return "^";
    case ADD_ASSIGN:
      return "+=";
    case SUB_ASSIGN:
      return "-=";
    case MULT_ASSIGN:
      return "*=";
    case DIV_ASSIGN:
      return "/=";
    case MOD_ASSIGN:
      return "%=";
    case EQ:
        return " == ";
    case NE:
        return " != ";
    case LT:
        return " < ";
    case GT:
        return " > ";
    case LTE:
        return " <= ";
    case GTE:
        return " >=";
    default:
        assert(0);
        return "";
    }
}

SteelType *AstBinOp::lvalue(SteelInterpreter *pInterpreter)
{
    return nullptr;
}

SteelType AstBinOp::evaluate(SteelInterpreter *pInterpreter)
{
    try{

        switch( m_op )
        {
        case ADD:

            return m_left->evaluate(pInterpreter) 
                + m_right->evaluate(pInterpreter);
        
        case SUB:
            return m_left->evaluate(pInterpreter) 
                - m_right->evaluate(pInterpreter);
        case MULT:
            return m_left->evaluate(pInterpreter) 
                * m_right->evaluate(pInterpreter);
        case DIV:
            return m_left->evaluate(pInterpreter)
                / m_right->evaluate(pInterpreter);
        case MOD:
            return m_left->evaluate(pInterpreter)
                % m_right->evaluate(pInterpreter);
	case ASSIGN:{
	  SteelType *lv = m_left->lvalue(pInterpreter);
	  if(!lv)
	    throw SteelException(SteelException::INVALID_LVALUE,
				 GetLine(),GetScript(), "Invalid lvalue in assignment.");
	  return *lv = m_right->evaluate(pInterpreter);
	}
	case ADD_ASSIGN:{
	  SteelType *lv =  m_left->lvalue(pInterpreter);
	  if(!lv)
	    throw SteelException(SteelException::INVALID_LVALUE,
				 GetLine(),GetScript(), "Invalid lvalue in += statement.");
	  return *lv += m_right->evaluate(pInterpreter);
	}case SUB_ASSIGN:{
	  SteelType *lv =  m_left->lvalue(pInterpreter);
	  if(!lv)
	    throw SteelException(SteelException::INVALID_LVALUE,
				 GetLine(),GetScript(), "Invalid lvalue in -= statement.");
	  return *lv -= m_right->evaluate(pInterpreter);
	 }case MULT_ASSIGN:{
	  SteelType *lv =  m_left->lvalue(pInterpreter);
	  if(!lv)
	    throw SteelException(SteelException::INVALID_LVALUE,
				 GetLine(),GetScript(), "Invalid lvalue in *= statement.");
	  return *lv *= m_right->evaluate(pInterpreter);
	  }case DIV_ASSIGN:{
	  SteelType *lv =  m_left->lvalue(pInterpreter);
	  if(!lv)
	    throw SteelException(SteelException::INVALID_LVALUE,
				 GetLine(),GetScript(), "Invalid lvalue in /= statement.");
	  return *lv /= m_right->evaluate(pInterpreter);
	   }case MOD_ASSIGN:{
	  SteelType *lv =  m_left->lvalue(pInterpreter);
	  if(!lv)
	    throw SteelException(SteelException::INVALID_LVALUE,
				 GetLine(),GetScript(), "Invalid lvalue in %= statement.");
	  return *lv %= m_right->evaluate(pInterpreter);

	    }case AND:
        {
            SteelType var;
            var.set((bool)m_left->evaluate(pInterpreter)
                    && (bool)m_right->evaluate(pInterpreter));

            return var;
        }
        case OR:
        {
            SteelType var;
            var.set((bool)m_left->evaluate(pInterpreter)
                    || (bool)m_right->evaluate(pInterpreter));

            return var;
        }
        case D:
            return m_left->evaluate(pInterpreter).d( m_right->evaluate(pInterpreter));
        case POW:
            return m_left->evaluate(pInterpreter)
                ^ m_right->evaluate(pInterpreter);
        case EQ:
        {
            SteelType var;
            var.set( m_left->evaluate(pInterpreter)
                     == m_right->evaluate(pInterpreter));
            return var;
        }
        case NE:
        {
            SteelType var;
            var.set ( m_left->evaluate(pInterpreter)
                      != m_right->evaluate(pInterpreter));
            return var;
        }
        case LT:
            return m_left->evaluate(pInterpreter)
                < m_right->evaluate(pInterpreter);
        case GT:
            return m_left->evaluate(pInterpreter)
                > m_right->evaluate(pInterpreter);
        case LTE:
            return m_left->evaluate(pInterpreter)
                <= m_right->evaluate(pInterpreter);
        case GTE:
            return m_left->evaluate(pInterpreter)
                >= m_right->evaluate(pInterpreter);
	case BIN_AND:{
	  SteelType var;
	  var.set ( (int)m_left->evaluate(pInterpreter) 
		    & (int)m_right->evaluate(pInterpreter));
	  return var;
	}
	case BIN_OR:{
	  SteelType var;
	  var.set((int)m_left->evaluate(pInterpreter) | (int)m_right->evaluate(pInterpreter));
	  return var;
	}
        default:
            assert(0);
    
        }
    }
    catch(OperationMismatch m)
    {
        throw SteelException(SteelException::TYPE_MISMATCH,
                             GetLine(),GetScript(),
                             "Binary operation '" + ToString(m_op) + "' disallowed between these types");
    }
    catch(DivideByZero z)
    {
	throw SteelException(SteelException::RUNTIME,
			     GetLine(),GetScript(),
			     "Divide by zero error on operation '" + ToString(m_op) +"'");
    }

    return SteelType();
}

void AstBinOp::FindIdentifiers( std::list< AstIdentifier* >& o_ids ) {
	m_left->FindIdentifiers(o_ids);
	m_right->FindIdentifiers(o_ids);
}


ostream & AstBinOp::print(std::ostream &out)
{
    out << *m_left <<  ToString(m_op) << *m_right;

    return out;
}

AstUnaryOp::AstUnaryOp(unsigned int line,
                       const std::string &script, Op op,
                       AstExpression *operand)
    :AstExpression(line,script),m_op(op),m_operand(operand)
{

}

AstUnaryOp::~AstUnaryOp()
{

}

std::string AstUnaryOp::ToString(Op op)
{
    switch(op)
    {
    case MINUS:
        return "-";
    case PLUS:
        return "+";
    case NOT:
        return " not ";
    default:
        assert ( 0 );
        return "";
    }
}

SteelType *AstUnaryOp::lvalue(SteelInterpreter *pInterpreter)
{
    return nullptr;
}

SteelType AstUnaryOp::evaluate(SteelInterpreter *pInterpreter)
{
    try{
    
        switch(m_op)
        {
        case MINUS:
            return - m_operand->evaluate(pInterpreter);
        case PLUS:
            return m_operand->evaluate(pInterpreter);
	case BIN_NOT:{
	  SteelType var;
	  var.set(~int(m_operand->evaluate(pInterpreter)));
	  return var;
	}
        case NOT:
            return ! m_operand->evaluate(pInterpreter);
#if 0 // We no longer allow unary cat. Use array() 
        case CAT:
        {
            SteelType var;
            var.set ( SteelArray() );
            var.add( m_operand->evaluate(pInterpreter) );
            return var;
        }
#endif
        }
    }
    catch(OperationMismatch m)
    {
        throw SteelException(SteelException::TYPE_MISMATCH,
                             GetLine(),GetScript(),
                             "Unary operation '" + ToString(m_op) + "' disallowed on this type");
    }
    catch(TypeMismatch)
    {
        throw SteelException(SteelException::TYPE_MISMATCH,
                             GetLine(),GetScript(),
                             "Unary operation '" + ToString(m_op) + "' disallowed on this type");
    }

    assert ( 0 );
    return SteelType();
}

ostream & AstUnaryOp::print(std::ostream &out)
{
    out << ToString(m_op) << *m_operand;

    return out;
}

void AstUnaryOp::FindIdentifiers( std::list< AstIdentifier* >& o_ids ) {
	m_operand->FindIdentifiers(o_ids);
}



AstPop::AstPop(unsigned int line,
               const std::string &script,
               AstExpression *pLValue,
	       bool popBack)
    :AstExpression(line,script),m_pLValue(pLValue),m_bPopBack(popBack)
{
}

AstPop::~AstPop()
{

}

SteelType AstPop::evaluate(SteelInterpreter *pInterpreter)
{
    
    SteelType *pL = m_pLValue->lvalue(pInterpreter);
    
    if(nullptr == pL) 
    {
        throw SteelException(SteelException::INVALID_LVALUE,
                             GetLine(),
                             GetScript(),
                             "Invalid lvalue after pop.");
    }
    try {
      if(m_bPopBack)
        return pL->pop_back();
    else
      return pL->pop();
    }catch(TypeMismatch){
      throw SteelException(SteelException::TYPE_MISMATCH, GetLine(), GetScript(), "Pop operand was not an array.");
    }
    
}

void AstPop::FindIdentifiers( std::list< AstIdentifier* >& o_ids ) {
	m_pLValue->FindIdentifiers(o_ids);
}



AstPush::AstPush(unsigned int line,
               const std::string &script,
		 AstExpression *pLValue,
		 AstExpression *pExp,
		 bool pushFront)
    :AstExpression(line,script),m_pLValue(pLValue),m_pExp(pExp),m_bPushFront(pushFront)
{
}

AstPush::~AstPush()
{

}

SteelType AstPush::evaluate(SteelInterpreter *pInterpreter)
{
    
    SteelType *pL = m_pLValue->lvalue(pInterpreter);
    
    if(nullptr == pL) 
    {
        throw SteelException(SteelException::INVALID_LVALUE,
                             GetLine(),
                             GetScript(),
                             "Invalid lvalue after push.");
    }
    try {
      if(!m_bPushFront){
        pL->pushb(m_pExp->evaluate(pInterpreter));
      }else{
        pL->push(m_pExp->evaluate(pInterpreter));
      }
    }catch(TypeMismatch){
      throw SteelException(SteelException::TYPE_MISMATCH, GetLine(), GetScript(), "Push operand was not an array");
    }

    return *pL;
}

void AstPush::FindIdentifiers( std::list< AstIdentifier* >& o_ids ) {
	m_pLValue->FindIdentifiers(o_ids);
}


AstRemove::AstRemove(unsigned int line,
               const std::string &script,
                 AstExpression *pLValue,
                 AstExpression *pExp)
    :AstExpression(line,script),m_pLValue(pLValue),m_pExp(pExp)
{
}

AstRemove::~AstRemove()
{
 
}

SteelType AstRemove::evaluate(SteelInterpreter *pInterpreter)
{
    
    SteelType *pL = m_pLValue->lvalue(pInterpreter);
    
    if(nullptr == pL) 
    {
        throw SteelException(SteelException::INVALID_LVALUE,
                             GetLine(),
                             GetScript(),
                             "Invalid lvalue in remove.");
    }
    
    return pL->removeElement(m_pExp->evaluate(pInterpreter));
}

void AstRemove::FindIdentifiers( std::list< AstIdentifier* >& o_ids ) {
	m_pLValue->FindIdentifiers(o_ids);
}



AstCallExpression::AstCallExpression(unsigned int line,
                                     const std::string &script, AstExpression *pExp, AstParamList *pList)
    :AstExpression(line,script),m_pExp(pExp),m_pParams(pList)
{
    assert ( m_pExp );
}

AstCallExpression::~AstCallExpression()
{

}

SteelType AstCallExpression::evaluate(SteelInterpreter *pInterpreter)
{
    SteelType ret;
    std::shared_ptr<SteelFunctor> pFunctor;
    try{

	m_functor = m_pExp->evaluate(pInterpreter);
	pFunctor = m_functor.getFunctor();
    }catch(TypeMismatch){
        throw SteelException(SteelException::TYPE_MISMATCH,
                             GetLine(), GetScript(),
                             "Function call expression was not a valid function");
    }
    catch(ValueNotFunction v){
        throw SteelException(SteelException::VALUE_NOT_FUNCTION,
                             GetLine(), GetScript(),
                             "Call expression is not a function.");
    }
    catch(UnknownIdentifier id)
    {
        throw SteelException(SteelException::UNKNOWN_IDENTIFIER,
                             GetLine(), GetScript(),
                             "Unknown function '" + id.identifier + '\'');
    }

	try{

        if(m_pParams)
            ret = pFunctor->Call(pInterpreter,m_pParams->getParamList(pInterpreter));
        else 
            ret = pFunctor->Call(pInterpreter,SteelType::Container());
        
#if 0
        if(m_pParams)
            ret = pInterpreter->call( m_pId->getValue(), m_pId->GetNamespace(),m_pParams->getParamList(pInterpreter) );
        else ret = pInterpreter->call( m_pId->getValue(), m_pId->GetNamespace(),SteelType::Container() );
#endif
    }
    catch(ParamMismatch )
    {
        throw SteelException(SteelException::PARAM_MISMATCH,
                             GetLine(),GetScript(),
                             "Function '"+ pFunctor->getIdentifier() + "' called with incorrect number of parameters.");
    }
    catch(TypeMismatch)
    {
        throw SteelException(SteelException::TYPE_MISMATCH,
                             GetLine(), GetScript(),
                             "Type mismatch in parameter to function '" + pFunctor->getIdentifier() + '\'');
    }
    catch(FileNotFound)
    {
      throw SteelException(SteelException::FILE_NOT_FOUND,
			   GetLine(),GetScript(),
			   "Couldn't open file in function '" + pFunctor->getIdentifier() + '\'');
    }


    return ret;
}

void AstCallExpression::FindIdentifiers( std::list< AstIdentifier* >& o_ids ) {
	if(m_pParams) m_pParams->FindIdentifiers(o_ids);
	if(m_pExp) m_pExp->FindIdentifiers(o_ids);
}


ostream & AstCallExpression::print(std::ostream &out)
{
    if(m_pParams)
    {
        out << *m_pExp 
            << '(' << *m_pParams << ')';
    }
    else
    {
        out << *m_pExp << "()";
    }
    
    return out;
}

AstArrayLiteral::AstArrayLiteral(unsigned int line, const std::string& script):AstExpression(line,script)
{
}

AstArrayLiteral::~AstArrayLiteral()
{

}

void AstArrayLiteral::add(AstExpression* pExp)
{
  m_list.emplace_back(std::unique_ptr<AstExpression>(pExp));
}

SteelType AstArrayLiteral::evaluate(SteelInterpreter* pInterpreter)
{
  SteelType array;
  array.set(SteelType::Container());

  for(auto& it : m_list){
    array.pushb(it->evaluate(pInterpreter));
  }

  return array;  
}

void AstArrayLiteral::FindIdentifiers( std::list< AstIdentifier* >& o_ids ) {
 for(auto& it : m_list){
	 it->FindIdentifiers(o_ids);
  }	
}



/*
  AstArrayExpression::AstArrayExpression(unsigned int line,
  const std::string &script,
  AstArrayIdentifier *pId,
  AstExpression * pExp)
  :AstExpression(line,script),m_pId(pId),m_pExpression(pExp)
  {
  }

  AstArrayExpression::~AstArrayExpression()
  {
  delete m_pId;
  delete m_pExpression;
  }

  ostream & AstArrayExpression::print(std::ostream &out)
  {
  out << *m_pId << '[' << *m_pExpression 
  << ']';
  return out;
  }
*/

AstArrayElement::AstArrayElement(unsigned int line,
                                 const std::string &script,
                                 AstExpression *pLValue,
                                 AstExpression *pExp)
    :AstExpression(line,script),m_pLValue(pLValue),m_pExp(pExp)
{
    
}

AstArrayElement::~AstArrayElement()
{

}

int AstArrayElement::getArrayIndex(SteelInterpreter *pInterpreter) const
{
    return m_pExp->evaluate(pInterpreter);
}


SteelType * AstArrayElement::lvalue(SteelInterpreter *pInterpreter)
{
    try
    {
    
        SteelType * pArray = m_pLValue->lvalue(pInterpreter);
        if(nullptr == pArray)
        {
            throw SteelException(SteelException::INVALID_LVALUE,
                                 GetLine(),
                                 GetScript(),
                                 "Invalid lvalue before subscript.");
        }

        if(!pArray->isArray() && !pArray->isHashMap())
        {
            throw SteelException(SteelException::TYPE_MISMATCH,
                                 GetLine(),
                                 GetScript(),
                                 "Lvalue is not an array or hash map.");
        }

	if(pArray->isArray())
	{
	  int index = m_pExp->evaluate(pInterpreter);
	  return pArray->getLValue(index);
	}
	else if(pArray->isHashMap())
	{
	  std::string index = m_pExp->evaluate(pInterpreter);
	  return pArray->getLValue(index);
	}
        // It will throw out of bounds, or what not.
        
    }
    catch(UnknownIdentifier id)
    {
        // TODO: Make these exceptions carry a god damn string.
        throw SteelException(SteelException::UNKNOWN_IDENTIFIER,
                             GetLine(),
                             GetScript(),
                             "Unknown identifier in array or hashmap lvalue: '" + id.identifier + '\'');
                 
    }
    catch(OutOfBounds)
    {
        throw SteelException(SteelException::OUT_OF_BOUNDS,
                             GetLine(),
                             GetScript(),
                             "lvalue subscript was out of bounds.");
    }

    return nullptr;
}

SteelType AstArrayElement::evaluate(SteelInterpreter *pInterpreter)
{
    try
    {

            SteelType val = m_pLValue->evaluate(pInterpreter);
        
            try
            {
              if(val.isArray())
                return val.getElement((int)m_pExp->evaluate(pInterpreter)); 
              else if(val.isHashMap())
                return val.getElement((const std::string&)m_pExp->evaluate(pInterpreter));
	      else throw TypeMismatch();
            }
            catch(TypeMismatch)
            {
                throw SteelException(SteelException::TYPE_MISMATCH,
                                     GetLine(),
                                     GetScript(),
                                     "Left of array subscript (aka '[' ']') was not an array.");
            }

            // The rest of the exceptions should be cought by the outer try.
            // Such as out of bounds.. and anything that goes wrong inside the index expression
      
    }
    catch(UnknownIdentifier id)
    {
        throw SteelException(SteelException::UNKNOWN_IDENTIFIER,
                             GetLine(),
                             GetScript(),
                             "Unknown identifier: '" + id.identifier + '\'');
    }
    catch(OutOfBounds)
    {
        throw SteelException(SteelException::OUT_OF_BOUNDS,
                             GetLine(),
                             GetScript(),
                             "Array index out of bounds.");
    }
	return SteelType();
}


void AstArrayElement::FindIdentifiers( std::list< AstIdentifier* >& o_ids ) {
	m_pExp->FindIdentifiers(o_ids);
	m_pLValue->FindIdentifiers(o_ids);
}

ostream & AstArrayElement::print(std::ostream &out)
{
    out << *m_pLValue << '[' << *m_pExp << ']' ;
    return out;
}


void AstIdentifier::FindIdentifiers( std::list< AstIdentifier* >& o_ids ) {
	o_ids.emplace_back(this);
}


AstPair::AstPair(unsigned int line, const std::string& script, AstExpression* pKey, AstExpression* pValue)
  :AstBase(line,script),m_key(pKey),m_value(pValue){
}

AstPair::~AstPair(){
}

std::string AstPair::GetKey(SteelInterpreter* pInterpreter){
  return  m_key->evaluate(pInterpreter);
}

SteelType AstPair::GetValue(SteelInterpreter* pInterpreter){
  return m_value->evaluate(pInterpreter);
}

void AstPair::FindIdentifiers( std::list< AstIdentifier* >& o_ids ) {
	m_key->FindIdentifiers(o_ids);
	m_value->FindIdentifiers(o_ids);
}


AstPairList::AstPairList(unsigned int line, const std::string& script):AstExpression(line,script)
{
}

AstPairList::~AstPairList()
{
}

void AstPairList::add(AstPair* pair)
{
  m_list.emplace_back(std::unique_ptr<AstPair>(pair));
}

SteelType AstPairList::evaluate(SteelInterpreter *pInterpreter)
{
  SteelType hashmap;
  hashmap.set(SteelType::Map());
  for(auto & it : m_list){
    hashmap.setElement(it->GetKey(pInterpreter),it->GetValue(pInterpreter));
  }
  return hashmap;
}

SteelType* AstPairList::lvalue(SteelInterpreter *pInterpreter)
{
  return nullptr;
}

void AstPairList::FindIdentifiers( std::list< AstIdentifier* >& o_ids ) {
 for(auto & it : m_list){
    it->FindIdentifiers(o_ids);
  }	
}


AstVarAssignmentExpression::AstVarAssignmentExpression(unsigned int line,
                                                       const std::string &script,
                                                       AstExpression *pLValue,
                                                       AstExpression *pExp)
    :AstExpression(line,script),m_pLValue(pLValue),m_pExpression(pExp)
{

}

AstVarAssignmentExpression::~AstVarAssignmentExpression()
{

}

SteelType *AstVarAssignmentExpression::lvalue(SteelInterpreter *pInterpreter)
{
    // Not an lvalue. I found out.
    return nullptr;
}

SteelType AstVarAssignmentExpression::evaluate(SteelInterpreter *pInterpreter)
{
    SteelType exp = m_pExpression->evaluate(pInterpreter);
    try
    {
        SteelType *pL = m_pLValue->lvalue(pInterpreter);
        if(pL == nullptr)
            throw SteelException(SteelException::INVALID_LVALUE,
                                 GetLine(),
                                 GetScript(),
                                 "Left of '=' is not a valid lvalue.");

        pInterpreter->assign ( pL, exp );
    }
    catch(UnknownIdentifier id)
    {
        throw SteelException(SteelException::UNKNOWN_IDENTIFIER,
                             GetLine(),
                             GetScript(),
                             "Unknown identifier: '" + id.identifier + '\'');
    }
    catch(TypeMismatch)
    {
        throw SteelException(SteelException::TYPE_MISMATCH,
                             GetLine(),
                             GetScript(),
                             "Illegal assignment due to type mismatch.");
    }

    return exp;
}

void AstVarAssignmentExpression::FindIdentifiers( std::list< AstIdentifier* >& o_ids ) {
	m_pLValue->FindIdentifiers(o_ids);
	m_pExpression->FindIdentifiers(o_ids);
}


ostream & AstVarAssignmentExpression::print(std::ostream &out)
{
    out << *m_pLValue << '=' << *m_pExpression;
    return out;
}



AstParamList::AstParamList(unsigned int line,
                           const std::string &script)
    :AstBase(line,script)
{
}
AstParamList::~AstParamList()
{
// 
}

SteelType::Container AstParamList::getParamList(SteelInterpreter *pInterpreter) const 
{
    SteelType::Container params;

    for(auto & i : m_params)
    {
    
        SteelType var = i->evaluate(pInterpreter);
        params.emplace_back ( var );
    }
    

    return params;
}

void AstParamList::FindIdentifiers( std::list< AstIdentifier* >& o_ids ) {
 for(auto& i : m_params)
    {
		i->FindIdentifiers(o_ids);
      
    }
}


void AstParamList::add(AstExpression *pExp)
{
    m_params.emplace_back(std::unique_ptr<AstExpression>(pExp));
}
ostream & AstParamList::print(std::ostream &out)
{
    

    return out;
}
#if 1 
SteelType * AstIdentifier::lvalue(SteelInterpreter *pInterpreter)
{
  // Note: Seems like it should be okay to cache but its not, due to closures (AuxVariables) 
  // New note: In previous implementations of closures that may have been true, maybe its ok now?
  //if(m_pLValue) return m_pLValue; 

    try
    {
        m_pLValue = pInterpreter->lookup_lvalue ( getValue() );
        return m_pLValue; 
    }
    catch(ConstViolation)
    {
        throw SteelException(SteelException::VALUE_IS_CONSTANT,
                             GetLine(),
                             GetScript(),
                             "Cannot modify constant value '" + getValue() + '\'');
    }
    catch(UnknownIdentifier id)
    {
        throw SteelException(SteelException::UNKNOWN_IDENTIFIER,
                             GetLine(),
                             GetScript(),
                             "Unknown identifier:'" + getValue() + '\'');
    }

    return nullptr;
};



SteelType AstIdentifier::evaluate(SteelInterpreter *pInterpreter)
{
    try
    {
        return pInterpreter->lookup ( getValue() );
    }
    catch(UnknownIdentifier)
    {
        throw SteelException(SteelException::UNKNOWN_IDENTIFIER,
                             GetLine(),
                             GetScript(),
                             "Unknown identifier:'" + getValue() + '\'');
    }
}
#endif

AstDeclaration::AstDeclaration(unsigned int line,
                                     const std::string &script,
                                     AstIdentifier *pId,
                                     AstExpression *pExp)
    :AstStatement(line,script),m_pId(pId),m_bConst(false),m_pExp(pExp)
{
}

AstDeclaration::AstDeclaration(unsigned int line,
                                     const std::string &script,
                                     AstIdentifier *pId,
                                     bool bConst,
                                     AstExpression *pExp)
    :AstStatement(line,script),m_pId(pId),m_bConst(bConst),m_pExp(pExp)
{
}


AstDeclaration::~AstDeclaration()
{

}

AstStatement::eStopType AstDeclaration::execute(SteelInterpreter *pInterpreter)
{
    try{
        if(m_bConst)
        {
            if(!m_pExp)
                throw SteelException(SteelException::ASSIGNMENT_REQUIRED,
                                     GetLine(),
                                     GetScript(),
                                     "Assignment missing from const declaration of " + m_pId->getValue());

            pInterpreter->declare_const(m_pId->getValue(), m_pExp->evaluate(pInterpreter));
        }
        else
        {
            pInterpreter->declare(m_pId->getValue());
        
            SteelType * pVar = pInterpreter->lookup_lvalue( m_pId->getValue() );
	    
            // If this is null, its crazy, because we JUST declared its ass.
            assert ( nullptr != pVar);
            
	
	    if(!pInterpreter->paramStackEmpty()){
	      // If there is a value, it overrides whatever the expression was.
	      pInterpreter->assign ( pVar, pInterpreter->popParamStack() );
	    }else if(m_pExp) {
	      pInterpreter->assign( pVar, m_pExp->evaluate(pInterpreter) );
	    }else{
              const std::string id = m_pId->getValue();
              SteelType val;
              if(id[0] == '@'){
                val.set(SteelType::Container());
              }else if(id[0] == '#'){
                val.set(SteelType::Map());
              }
              pInterpreter->assign( pVar, val );
	    }
     
        }
    }
    catch(TypeMismatch)
    {
        throw SteelException(SteelException::TYPE_MISMATCH,
                             GetLine(),
                             GetScript(),
                             "Type mismatch in assignment part of declaration of '" + m_pId->getValue() + '\'');
    }
    catch(UnknownIdentifier id)
    {
        throw SteelException(SteelException::UNKNOWN_IDENTIFIER,
                             GetLine(),
                             GetScript(),
                             "Unknown identifier: '" + m_pId->getValue() + '\'');
    }
    catch(AlreadyDefined)
    {
        throw SteelException(SteelException::VARIABLE_DEFINED,
                             GetLine(),
                             GetScript(),
                             "Variable: '" + m_pId->getValue() + "' is already defined.");
    }

    return COMPLETED;
}

void AstDeclaration::FindIdentifiers( std::list< AstIdentifier* >& o_ids ) {
	//m_pId->FindIdentifiers(o_ids); // wait... no, this is local and we want nonlocal
	if(m_pExp) m_pExp->FindIdentifiers(o_ids);
}


ostream & AstDeclaration::print(std::ostream &out)
{
    if(m_bConst)
        out << "const " << *m_pId;
    else
        out << "var " << *m_pId;

    if( m_pExp ) out << '=' << *m_pExp << ';' << std::endl;
    return out;
}


AstArrayDeclaration::AstArrayDeclaration(unsigned int line,
                                         const std::string &script,
                                         AstIdentifier *pId,
                                         AstExpression *pInt)
  :AstDeclaration(line,script,pId,pInt),m_pId(pId),m_pIndex(pInt),m_pExp(nullptr)
{
}

AstArrayDeclaration::~AstArrayDeclaration()
{

}

void AstArrayDeclaration::assign(AstExpression *pExp)
{
    // Todo: Actually evaluate this here and toss it
    m_pExp = std::unique_ptr<AstExpression>(pExp);
}

void AstArrayDeclaration::FindIdentifiers( std::list< AstIdentifier* >& o_ids ) {
	// Don't include THIS identifier because we want non-locals only
	if(m_pExp)
		m_pExp->FindIdentifiers(o_ids);
}



AstStatement::eStopType AstArrayDeclaration::execute(SteelInterpreter *pInterpreter)
{
    try
    {
        if(m_pIndex)
            pInterpreter->declare_array( m_pId->getValue(), m_pIndex->evaluate(pInterpreter));
        else pInterpreter->declare_array( m_pId->getValue(), 0);
    }
    catch(AlreadyDefined)
    {
        throw SteelException(SteelException::VARIABLE_DEFINED,
                             GetLine(),
                             GetScript(),
                             "Array: '" + m_pId->getValue() + "' was previously defined.");
    }

    try{
        SteelType * pVar = pInterpreter->lookup_lvalue( m_pId->getValue() );
        // If this is null here, we're in a BAD way. Programming error.
        assert ( nullptr != pVar);
	
	if(!pInterpreter->paramStackEmpty()){
	  pInterpreter->assign ( pVar, pInterpreter->popParamStack() );
	}else if(m_pExp){
	  pInterpreter->assign( pVar, m_pExp->evaluate(pInterpreter) );
	}
    }
    catch(TypeMismatch)
    {
        throw SteelException(SteelException::TYPE_MISMATCH,
                             GetLine(),
                             GetScript(),
                             "Attempt to assign scalar to array in declaration of :'" + m_pId->getValue() + '\'');
    }
    catch(UnknownIdentifier id)
    {
        throw SteelException(SteelException::UNKNOWN_IDENTIFIER,
                             GetLine(),
                             GetScript(),
                             "Unknown identifier in assignment:" + id.identifier + '\'');
    }

    return COMPLETED;
}

ostream & AstArrayDeclaration::print(std::ostream &out)
{
    out << "var " << *m_pId ;
    if(m_pIndex) out << '[' << *m_pIndex << ']';
    if(m_pExp) out << '=' << *m_pExp;
    out << ';' << std::endl;
    return out;
}



AstAnonymousFunctionDefinition::AstAnonymousFunctionDefinition(unsigned int line, const std::string &script, AstParamDefinitionList* params, AstStatementList * statements)
  :AstExpression(line,script),m_pParamList(params),m_pStatements(statements)
{
}
  
AstAnonymousFunctionDefinition::~AstAnonymousFunctionDefinition()
{
}
  
SteelType AstAnonymousFunctionDefinition::evaluate(SteelInterpreter* pInterpreter)
{
  std::shared_ptr<SteelFunctor> pFunctor(new SteelUserFunction(m_pParamList,m_pStatements));
	
  std::shared_ptr<SteelUserFunction> userFunc = std::dynamic_pointer_cast<SteelUserFunction>(pFunctor);
    if(userFunc) // should be!
	userFunc->bindNonLocals(pInterpreter);
    pFunctor->setIdentifier("Anonymous");
    SteelType functor;
    functor.set(pFunctor);

    return functor;
}


AstParamDefinitionList::AstParamDefinitionList(unsigned int line,
                                               const std::string &script)
    :AstBase(line,script),mnDefaults(0)
{
}

AstParamDefinitionList::~AstParamDefinitionList()
{

}

void AstParamDefinitionList::add(AstDeclaration *pDef)
{
    if(pDef->hasInitializer())
    {
        ++mnDefaults;
    }
    else
    {
        // Are there defaults already? If so... we can't have one without defaults now.
        if(mnDefaults != 0)
        {
            throw SteelException(SteelException::DEFAULTS_MISMATCH, GetLine(), GetScript(), "Default parameter values may only be on the last parameters.");
        }
    }

    m_params.emplace_back( std::unique_ptr<AstDeclaration>(pDef) );
}


int AstParamDefinitionList::size() const
{
    return m_params.size();
}

int AstParamDefinitionList::defaultCount() const
{
    return mnDefaults;
}


void AstParamDefinitionList::executeDeclarations(SteelInterpreter *pInterpreter)
{
  for(auto & i : m_params)
    {
       i->execute(pInterpreter);
    }
}


bool AstParamDefinitionList::containsName( const std::string& id ) const {
	for(auto& i : m_params){
		AstIdentifier * ident = i->getIdentifier();
		if(ident->getValue() == id)
			return true;
	}
	
	return false;
}



ostream & AstParamDefinitionList::print (std::ostream &out)
{

    return out;
}    


SteelType AstFuncIdentifier::evaluate(SteelInterpreter *pInterpreter)
{
  return pInterpreter->lookup_function(GetNamespace(),getValue());
}

std::string AstFuncIdentifier::GetNamespace(void) const
{
    if(m_ns.size())
    {
        return m_ns;
    }
    else
    {
      return SteelInterpreter::kszUnspecifiedNamespace;
    }
}



AstFunctionDefinition::AstFunctionDefinition(unsigned int line,
                                             const std::string &script,
                                             AstIdentifier *pId,
                                             AstParamDefinitionList *pParams,
                                             AstStatementList * pStmts)
    :AstStatement(line,script),m_pId(pId),m_pParams(pParams),m_pStatements(pStmts)
{

}
AstFunctionDefinition::~AstFunctionDefinition()
{

  /*
    delete m_pId;
    delete m_pParams;
    delete m_pStatements;
    We use shared pointers here because
    SteelUserFunction actually keeps these around
    and can outlive the Ast. Though, sometimes the
    Ast outlives the user function too. Hence, ref counting.
    */
}

AstStatement::eStopType AstFunctionDefinition::execute(SteelInterpreter *pInterpreter)
{
  std::string ns = SteelInterpreter::kszGlobalNamespace;
  AstFuncIdentifier * fid = dynamic_cast<AstFuncIdentifier*>(m_pId.get());
  if(fid){
    ns = fid->GetNamespace();
  }
    try{
	// For user functions, if they don't specify a namespace, its global (Not unspecified, thats for calling..)
      pInterpreter->registerFunction( m_pId->getValue(), ns , m_pParams , m_pStatements);
    }
    catch(AlreadyDefined)
    {
        throw SteelException(SteelException::FUNCTION_DEFINED,
                             GetLine(),
                             GetScript(),
                             "Function '" + m_pId->getValue() + "'already defined!");
    }

    return COMPLETED;
}

void AstFunctionDefinition::FindIdentifiers( std::list< AstIdentifier* >& o_ids ) {
	// Um... we don't look here, do we?
}


ostream & AstFunctionDefinition::print (std::ostream &out)
{
    out << "function " << *m_pId  << '(' ;
    if(m_pParams) out << *m_pParams;
    out << ')' << *m_pStatements << std::endl;
    return out;
}


ostream & operator<<(ostream & out,AstBase & ast)
{
    return ast.print(out);
}



}
