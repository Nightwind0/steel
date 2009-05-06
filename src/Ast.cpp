#include "Ast.h"
#include "SteelInterpreter.h"
#include "SteelType.h"
#include "SteelException.h"
#include <string>
#include <iostream>
#include <cassert>


ostream & operator<<(ostream &,AstBase&);


AstBase::AstBase(unsigned int line, const std::string &script)
    :m_line(line),m_script_name(script)
{
}

AstBase::~AstBase()
{
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
    var.set(m_value);

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
    : AstExpression(line,script),m_value(value)
{
    
}

ostream & AstIdentifier::print(std::ostream &out)
{
    out << m_value;
    return out;
}

SteelType AstIdentifier::evaluate(SteelInterpreter *pInterpreter)
{
    // Shouldn't ever be called.
    return SteelType();
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
    delete m_pExp;
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

AstScript::AstScript(unsigned int line, const std::string &script)
    :AstBase(line,script),m_pList(NULL)
{
}
AstScript::~AstScript()
{
    delete m_pList;
}

ostream & AstScript::print(std::ostream &out)
{
    if(m_pList) out << *m_pList;

    return out;
}

void AstScript::executeScript(SteelInterpreter *pInterpreter)
{
    if(m_pList)
    {
        m_pList->execute(pInterpreter);
    }
}

void AstScript::SetList(AstStatementList *pList)
{
    m_pList = pList;
}


AstStatementList::AstStatementList(unsigned int line, const std::string &script)
    :AstStatement(line,script)
{
}

AstStatementList::~AstStatementList()
{
    for(std::list<AstStatement*>::iterator i = m_list.begin();
        i != m_list.end(); i++) delete *i;
}

AstStatement::eStopType AstStatementList::execute(SteelInterpreter *pInterpreter)
{
    eStopType ret = COMPLETED;
    pInterpreter->pushScope();
    for(std::list<AstStatement*>::const_iterator it = m_list.begin();
        it != m_list.end(); it++)
    {
    
        AstStatement * pStatement = *it;
        eStopType stop = pStatement->execute(pInterpreter);

        if(stop == BREAK || stop == RETURN || stop ==  CONTINUE)
        {
            ret = stop;
            break;
        }
    }
    pInterpreter->popScope();

    return ret;
}

ostream & AstStatementList::print(std::ostream &out)
{
    out << "{\n";
    for(std::list<AstStatement*>::const_iterator it = m_list.begin();
        it != m_list.end(); it++)
    {
        AstStatement * pStatement = *it;
        out << "\t\t" << *pStatement;
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
    delete m_pCondition;
    delete m_pStatement;
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
        << '\t' << *m_pStatement<< std::endl;

    return out;
}


AstDoStatement::AstDoStatement(unsigned int line, const std::string &script, AstExpression *pExp, AstStatement *pStmt)
    :AstStatement(line,script),m_pCondition(pExp),m_pStatement(pStmt)
{
}
AstDoStatement::~AstDoStatement()
{
    delete m_pCondition;
    delete m_pStatement;
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
    out << "do "<< *m_pStatement << "\n while (" << *m_pCondition << ")\n"
        <<  std::endl;

    return out;
}


AstIfStatement::AstIfStatement(unsigned int line, const std::string &script,
                               AstExpression *pExp, AstStatement *pStmt,
                               AstStatement *pElse)
    :AstStatement(line,script),m_pCondition(pExp),m_pStatement(pStmt),m_pElse(pElse)
{
}
AstIfStatement::~AstIfStatement()
{
    delete m_pCondition;
    delete m_pElse;
    delete m_pStatement;
}

AstStatement::eStopType AstIfStatement::execute(SteelInterpreter *pInterpreter)
{
    if(m_pCondition->evaluate(pInterpreter))
        return m_pStatement->execute(pInterpreter);
    else if ( m_pElse) return m_pElse->execute(pInterpreter);
    else return COMPLETED;

}

ostream & AstIfStatement::print(std::ostream &out)
{
    out << "if (" << *m_pCondition  << ")\n"
        << '\t' << *m_pStatement << '\n';
    if(m_pElse) 
    {
        out << " else " << *m_pElse << '\n';
    }

    return out;
}

AstReturnStatement::AstReturnStatement(unsigned int line, const std::string &script, AstExpression *pExp)
    :AstStatement(line,script),m_pExpression(pExp)
{
}

AstReturnStatement::~AstReturnStatement()
{
    delete m_pExpression;
}

AstStatement::eStopType AstReturnStatement::execute(SteelInterpreter *pInterpreter)
{
    if( m_pExpression )
        pInterpreter->setReturn( m_pExpression->evaluate(pInterpreter) );
    else pInterpreter->setReturn ( SteelType() );
    return RETURN;
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
    out << "#include \"" << m_ns << "\";\n";
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
    :AstStatement(line,script),m_pStart(pStart),m_pCondition(pCondition),m_pIteration(pIteration),m_pStatement(pStmt)
{
}

AstLoopStatement::~AstLoopStatement()
{
    delete m_pStart;
    delete m_pCondition;
    delete m_pIteration;
    delete m_pStatement;
}

AstStatement::eStopType AstLoopStatement::execute(SteelInterpreter *pInterpreter)
{
    // A for loop has its own scope like in C++
    // (Beyond the scope that the body may have anyway)
    pInterpreter->pushScope();
    for( m_pStart->evaluate( pInterpreter) ;
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

ostream & AstLoopStatement::print(std::ostream &out)
{
    out << "for (" << *m_pStart << ';' << *m_pCondition << ';' << *m_pIteration << ')' << *m_pStatement;
    
    return out;
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
    delete m_pLValue;
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

        if(NULL == pVar) throw SteelException(SteelException::INVALID_LVALUE,
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
    catch(UnknownIdentifier)
    {
        throw SteelException(SteelException::UNKNOWN_IDENTIFIER,
                             GetLine(),
                             GetScript(),
                             "Unknown identifier before increment.");
    }

    return SteelType();
}

SteelType * AstIncrement::lvalue(SteelInterpreter *pInterpreter)
{
    return NULL;
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

        if(NULL == pVar) throw SteelException(SteelException::INVALID_LVALUE,
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
    catch(UnknownIdentifier)
    {
        throw SteelException(SteelException::UNKNOWN_IDENTIFIER,
                             GetLine(),
                             GetScript(),
                             "Unknown identifier before decrement.");
    }

    return SteelType();
}

SteelType * AstDecrement::lvalue(SteelInterpreter *pInterpreter)
{
    return NULL;
}




AstBinOp::AstBinOp(unsigned int line,
                   const std::string &script,
                   Op op, AstExpression *left, AstExpression *right)
    :AstExpression(line,script),m_op(op),m_right(right),m_left(left)
{
}
AstBinOp::~AstBinOp()
{
    delete m_right;
    delete m_left;
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
    case CAT:
        return " : ";
    default:
        assert(0);
        return "";
    }
}

SteelType *AstBinOp::lvalue(SteelInterpreter *pInterpreter)
{
    return NULL;
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
        case AND:
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
        case CAT:
            return m_left->evaluate(pInterpreter).cat( m_right->evaluate(pInterpreter) );
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

    return SteelType();
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
    delete m_operand;
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
    case CAT:
        return " : " ;
    default:
        assert ( 0 );
        return "";
    }
}

SteelType *AstUnaryOp::lvalue(SteelInterpreter *pInterpreter)
{
    return NULL;
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
        case NOT:
            return ! m_operand->evaluate(pInterpreter);
        case CAT:
        {
            SteelType var;
            var.set ( SteelArray() );
            var.add( m_operand->evaluate(pInterpreter) );
            return var;
        }
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


AstPop::AstPop(unsigned int line,
               const std::string &script,
               AstExpression *pLValue)
    :AstExpression(line,script),m_pLValue(pLValue)
{
}

AstPop::~AstPop()
{
    delete m_pLValue;
}

SteelType AstPop::evaluate(SteelInterpreter *pInterpreter)
{
    
    SteelType *pL = m_pLValue->lvalue(pInterpreter);
    
    if(NULL == pL) 
    {
        throw SteelException(SteelException::INVALID_LVALUE,
                             GetLine(),
                             GetScript(),
                             "Invalid lvalue after pop.");
    }

    return pL->pop();
    
}



AstCallExpression::AstCallExpression(unsigned int line,
                                     const std::string &script, AstFuncIdentifier *pId, AstParamList *pList)
    :AstExpression(line,script),m_pId(pId),m_pParams(pList),m_pFunctor(NULL)
{
    assert ( m_pId );
}
AstCallExpression::~AstCallExpression()
{
    delete m_pId;
    delete m_pParams;
}

SteelType AstCallExpression::evaluate(SteelInterpreter *pInterpreter)
{
    SteelType ret;
    try{
        if(!m_pFunctor)
        {
            m_pFunctor = pInterpreter->lookup_functor(m_pId->getValue(),m_pId->GetNamespace());
        }

        if(m_pParams)
            ret = m_pFunctor->Call(pInterpreter,m_pParams->getParamList(pInterpreter));
        else 
            ret = m_pFunctor->Call(pInterpreter,std::vector<SteelType>());
        
#if 0
        if(m_pParams)
            ret = pInterpreter->call( m_pId->getValue(), m_pId->GetNamespace(),m_pParams->getParamList(pInterpreter) );
        else ret = pInterpreter->call( m_pId->getValue(), m_pId->GetNamespace(),std::vector<SteelType>() );
#endif
    }
    catch(ParamMismatch )
    {
        throw SteelException(SteelException::PARAM_MISMATCH,
                             GetLine(),GetScript(),
                             "Function '" + m_pId->getValue() + "' called with incorrect number of parameters.");
    }
    catch(UnknownIdentifier )
    {
        throw SteelException(SteelException::UNKNOWN_IDENTIFIER,
                             GetLine(), GetScript(),
                             "Unknown function: '" + m_pId->getValue() + '\'');
    }
    catch(TypeMismatch)
    {
        throw SteelException(SteelException::TYPE_MISMATCH,
                             GetLine(), GetScript(),
                             "Type mismatch in parameter to function '" + m_pId->getValue() + '\'');
    }


    return ret;
}

ostream & AstCallExpression::print(std::ostream &out)
{
    if(m_pParams)
    {
        out << *m_pId 
            << '(' << *m_pParams << ')';
    }
    else
    {
        out << *m_pId << "()";
    }
    
    return out;
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
    delete m_pLValue;
    delete m_pExp;
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
        if(NULL == pArray)
        {
            throw SteelException(SteelException::INVALID_LVALUE,
                                 GetLine(),
                                 GetScript(),
                                 "Invalid lvalue before subscript.");
        }

        if(!pArray->isArray())
        {
            throw SteelException(SteelException::TYPE_MISMATCH,
                                 GetLine(),
                                 GetScript(),
                                 "Lvalue is not an array.");
        }

        int index = m_pExp->evaluate(pInterpreter);
    
        // It will throw out of bounds, or what not.
        return pArray->getLValue(index);
    }
    catch(UnknownIdentifier)
    {
        // TODO: Make these exceptions carry a god damn string.
        throw SteelException(SteelException::UNKNOWN_IDENTIFIER,
                             GetLine(),
                             GetScript(),
                             "Unknown identifier in array lvalue.");
                 
    }
    catch(OutOfBounds)
    {
        throw SteelException(SteelException::OUT_OF_BOUNDS,
                             GetLine(),
                             GetScript(),
                             "lvalue subscript was out of bounds.");
    }

    return NULL;
}

SteelType AstArrayElement::evaluate(SteelInterpreter *pInterpreter)
{
    try
    {
        SteelType * pL = m_pLValue->lvalue(pInterpreter);
        if(NULL == pL)
        {
            // Okay. It wasn't an lvalue, but thats okay, 
            // We're not in an lvalue context (Although, 
            // If it IS an lvalue, we speed up the interpreter
            // By knowing that. 
            // We can just evaluate it and hope that it's
            // An array type that it evaluates to. 
            // If not, then we're in trouble. 

            SteelType val = m_pLValue->evaluate(pInterpreter);
        
            try
            {
                return val.getElement(m_pExp->evaluate(pInterpreter)); 
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
    
        return pInterpreter->lookup(pL, m_pExp->evaluate(pInterpreter));
    }
    catch(UnknownIdentifier)
    {
        throw SteelException(SteelException::UNKNOWN_IDENTIFIER,
                             GetLine(),
                             GetScript(),
                             "Unknown identifier.");
    }
    catch(OutOfBounds)
    {
        throw SteelException(SteelException::OUT_OF_BOUNDS,
                             GetLine(),
                             GetScript(),
                             "Array index out of bounds.");
    }

}

ostream & AstArrayElement::print(std::ostream &out)
{
    out << *m_pLValue << '[' << *m_pExp << ']' ;
    return out;
}

SteelType * AstArrayIdentifier::lvalue(SteelInterpreter *pInterpreter)
{
    try
    {
        return pInterpreter->lookup_lvalue(getValue());
    }
    catch(UnknownIdentifier)
    {
        throw SteelException(SteelException::UNKNOWN_IDENTIFIER,
                             GetLine(),
                             GetScript(),
                             "Unknown array identifier:'" + getValue() + '\'');

    }

    return NULL;
}


SteelType AstArrayIdentifier::evaluate(SteelInterpreter *pInterpreter)
{
    
    // Find our reference variable in the file. 
    SteelType var;

    try {
        var = pInterpreter->lookup(getValue()); 
    }
    catch(UnknownIdentifier)
    {
        throw SteelException(SteelException::UNKNOWN_IDENTIFIER,
                             GetLine(),
                             GetScript(),
                             "Unknown array identifier:'" + getValue() + '\'');
    }

    return var;
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
    delete m_pLValue;
    delete m_pExpression;
}

SteelType *AstVarAssignmentExpression::lvalue(SteelInterpreter *pInterpreter)
{
    // Not an lvalue. I found out.
    return NULL;
}

SteelType AstVarAssignmentExpression::evaluate(SteelInterpreter *pInterpreter)
{
    SteelType exp = m_pExpression->evaluate(pInterpreter);
    try
    {
        SteelType *pL = m_pLValue->lvalue(pInterpreter);
        if(pL == NULL)
            throw SteelException(SteelException::INVALID_LVALUE,
                                 GetLine(),
                                 GetScript(),
                                 "Left of '=' is not a valid lvalue.");

        pInterpreter->assign ( pL, exp );
    }
    catch(UnknownIdentifier)
    {
        throw SteelException(SteelException::UNKNOWN_IDENTIFIER,
                             GetLine(),
                             GetScript(),
                             "Unknown identifier");
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
    for(std::list<AstExpression*>::iterator i = m_params.begin();
        i != m_params.end(); i++) delete *i;
}

std::vector<SteelType> AstParamList::getParamList(SteelInterpreter *pInterpreter) const 
{
    std::vector<SteelType> params;

    for(std::list<AstExpression*>::const_iterator i = m_params.begin();
        i != m_params.end(); i++)
    {
    
        SteelType var = (*i)->evaluate(pInterpreter);
        params.push_back ( var );
    }
    

    return params;
}

void AstParamList::add(AstExpression *pExp)
{
    m_params.push_back(pExp);
}
ostream & AstParamList::print(std::ostream &out)
{
    for(std::list<AstExpression*>::const_iterator i = m_params.begin();
        i != m_params.end(); i++)
    {
        std::list<AstExpression*>::const_iterator next = i ;
        next++;

        if(next == m_params.end())
            out << *(*i);
        else
            out  << *(*i) << ',';
    }
    

    return out;
}

SteelType * AstVarIdentifier::lvalue(SteelInterpreter *pInterpreter)
{
    try
    {
        return pInterpreter->lookup_lvalue ( getValue() );
    }
    catch(ConstViolation)
    {
        throw SteelException(SteelException::VALUE_IS_CONSTANT,
                             GetLine(),
                             GetScript(),
                             "Cannot modify constant value '" + getValue() + '\'');
    }
    catch(UnknownIdentifier)
    {
        throw SteelException(SteelException::UNKNOWN_IDENTIFIER,
                             GetLine(),
                             GetScript(),
                             "Unknown identifier:'" + getValue() + '\'');
    }

    return NULL;
};

SteelType AstVarIdentifier::evaluate(SteelInterpreter *pInterpreter)
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

void AstDeclaration::setValue(const SteelType &value)
{
    m_bHasValue = true;
    m_value = value;
}


AstVarDeclaration::AstVarDeclaration(unsigned int line,
                                     const std::string &script,
                                     AstVarIdentifier *pId,
                                     AstExpression *pExp)
    :AstDeclaration(line,script),m_pId(pId),m_bConst(false),m_pExp(pExp)
{
}

AstVarDeclaration::AstVarDeclaration(unsigned int line,
                                     const std::string &script,
                                     AstVarIdentifier *pId,
                                     bool bConst,
                                     AstExpression *pExp)
    :AstDeclaration(line,script),m_pId(pId),m_bConst(bConst),m_pExp(pExp)
{
}


AstVarDeclaration::~AstVarDeclaration()
{
    delete m_pId;
    delete m_pExp;
}

AstStatement::eStopType AstVarDeclaration::execute(SteelInterpreter *pInterpreter)
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
            assert ( NULL != pVar);
            
            if(m_pExp) 
                pInterpreter->assign( pVar, m_pExp->evaluate(pInterpreter) );
            
            // If there is a value, it overrides whatever the expression was.
            // This is for parameters
            if(m_bHasValue) pInterpreter->assign( pVar, m_value);
        }
    }
    catch(TypeMismatch)
    {
        throw SteelException(SteelException::TYPE_MISMATCH,
                             GetLine(),
                             GetScript(),
                             "Type mismatch in assignment part of declaration of '" + m_pId->getValue() + '\'');
    }
    catch(UnknownIdentifier)
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

ostream & AstVarDeclaration::print(std::ostream &out)
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
                                         AstArrayIdentifier *pId,
                                         AstExpression *pInt)
    :AstDeclaration(line,script),m_pId(pId),m_pIndex(pInt),m_pExp(NULL)
{
}

AstArrayDeclaration::~AstArrayDeclaration()
{
    delete m_pId;
    delete m_pIndex;
    delete m_pExp;
}

void AstArrayDeclaration::assign(AstExpression *pExp)
{
    // Todo: Actually evaluate this here and toss it
    m_pExp = pExp;
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
        assert ( NULL != pVar);
        if(m_pExp)
            pInterpreter->assign( pVar, m_pExp->evaluate(pInterpreter) );

        if(m_bHasValue) pInterpreter->assign( pVar, m_value);
    }
    catch(TypeMismatch)
    {
        throw SteelException(SteelException::TYPE_MISMATCH,
                             GetLine(),
                             GetScript(),
                             "Attempt to assign scalar to array in declaration of :'" + m_pId->getValue() + '\'');
    }
    catch(UnknownIdentifier)
    {
        throw SteelException(SteelException::UNKNOWN_IDENTIFIER,
                             GetLine(),
                             GetScript(),
                             "Unknown identifier in assignment.");
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



AstParamDefinitionList::AstParamDefinitionList(unsigned int line,
                                               const std::string &script)
    :AstBase(line,script),mnDefaults(0)
{
}

AstParamDefinitionList::~AstParamDefinitionList()
{
    for(std::list<AstDeclaration*>::iterator i = m_params.begin();
        i != m_params.end(); i++) 
        delete *i;
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

    m_params.push_back( pDef );
}

void AstParamDefinitionList::executeDeclarations(SteelInterpreter *pInterpreter)
{
    for(std::list<AstDeclaration*>::const_iterator i = m_params.begin();
        i != m_params.end(); i++)
        (*i)->execute(pInterpreter);
}

int AstParamDefinitionList::size() const
{
    return m_params.size();
}

int AstParamDefinitionList::defaultCount() const
{
    return mnDefaults;
}


void AstParamDefinitionList::executeDeclarations(SteelInterpreter *pInterpreter, 
                                                 const std::vector<SteelType> &params)
{
    assert ( params.size() == m_params.size() );


    int parameter = 0;
    for(std::list<AstDeclaration*>::const_iterator i = m_params.begin();
        i != m_params.end(); i++)
    {
        if(parameter < params.size())
            (*i)->setValue(params[parameter++]); 

        (*i)->execute(pInterpreter);
    }
}


ostream & AstParamDefinitionList::print (std::ostream &out)
{
    for(std::list<AstDeclaration*>::const_iterator i = m_params.begin();
        i != m_params.end(); i++)
    {
        std::list<AstDeclaration*>::const_iterator next = i ;
        next++;

        if(next == m_params.end())
            out << *(*i);
        else
            out  << *(*i) << ',';
    }
    return out;
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
                                             AstFuncIdentifier *pId,
                                             AstParamDefinitionList *pParams,
                                             AstStatementList * pStmts,
                                             bool final)
    :AstStatement(line,script),m_pId(pId),m_pParams(pParams),m_pStatements(pStmts),mbFinal(final)
{

}
AstFunctionDefinition::~AstFunctionDefinition()
{
    delete m_pId;
    delete m_pParams;
    delete m_pStatements;
}

AstStatement::eStopType AstFunctionDefinition::execute(SteelInterpreter *pInterpreter)
{
    try{
      // For user functions, if they don't specify a namespace, its global (Not unspecified, thats for calling..)
      if(m_pId->GetNamespace() == SteelInterpreter::kszUnspecifiedNamespace){
	pInterpreter->registerFunction( m_pId->getValue(), SteelInterpreter::kszGlobalNamespace, m_pParams, m_pStatements, mbFinal );
      }else   pInterpreter->registerFunction( m_pId->getValue(), m_pId->GetNamespace(), m_pParams , m_pStatements, mbFinal );
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




