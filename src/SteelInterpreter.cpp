#include "SteelInterpreter.h"
#include "Ast.h"
#include "SteelParser.h"
#include "SteelFunctor.h"
#include "SteelException.h"
#include <math.h>
#include <cmath>
#include <ctime>
#include <sstream>
#include <iostream>
#include <string>

ParameterListItem::ParameterListItem(const std::string &name, double d)
{
    m_name = name;
    m_value.set(d);
}

ParameterListItem::ParameterListItem(const std::string &name, int i)
{
    m_name = name;
    m_value.set(i);
}

ParameterListItem::ParameterListItem(const std::string &name, bool b)
{
    m_name = name;
    m_value.set(b);
}

ParameterListItem::ParameterListItem(const std::string &name, std::string &s)
{
    m_name = name;
    m_value.set(s);
}


SteelInterpreter::SteelInterpreter()
{
    registerBifs();
    srand(time(0));
}

SteelInterpreter::~SteelInterpreter()
{
    // Delete functors
    // Since the user defined functions are in the main statement list,
    // they are already deleted when you delete the tree... therefore,
    // we don't want to delete them here.
    for(std::map<std::string,SteelFunctor*>::iterator i = m_functions.begin();
        i != m_functions.end(); i++)
    {
        delete i->second;
    }
        
}


void SteelInterpreter::addFunction(const std::string &name,
                                   SteelFunctor *pFunc)
{
    std::map<std::string,SteelFunctor*>::iterator it = m_functions.find ( name );
    
    if(it != m_functions.end() && it->second->isFinal()) throw AlreadyDefined();

    m_functions[name] = pFunc;
}

AstScript * SteelInterpreter::prebuildAst(const std::string &script_name,
                                          const std::string &script)
{
    SteelParser parser;
    parser.setBuffer(script.c_str(),script_name);
    if(parser.Parse() != SteelParser::PRC_SUCCESS)
    {
        if(parser.hadError())
        {
            throw SteelException(SteelException::PARSING,0,script_name, parser.getErrors());
        } 
        else
        {
            throw SteelException(SteelException::PARSING,0,script_name, "Unknown parsing error.");
        }                
    }
    else if (parser.hadError())
    {
        throw SteelException(SteelException::PARSING,0,script_name, parser.getErrors());
    }

    AstScript *pScript = static_cast<AstScript*>( parser.GetAcceptedToken() );

    return pScript;
}

SteelType SteelInterpreter::runAst(AstScript *pScript)
{
    assert ( NULL != pScript );
    pScript->executeScript(this);

    return getReturn();
}

// This method allows you to set up some variables at a global scope
// for the script to use.
SteelType SteelInterpreter::runAst(AstScript *pScript, const ParameterList &params)
{
    assert ( NULL != pScript );

    pushScope();
    for(ParameterList::const_iterator it = params.begin(); it != params.end();
        it++)
    {
        declare( it->getName() );
        SteelType * pVar  = lookup_internal(it->getName());
        assign(pVar,it->getValue());
    }

    pScript->executeScript(this);
    popScope();

    return getReturn();
}

SteelType SteelInterpreter::run(const std::string &name,const std::string &script)
{
    SteelParser parser;
//    parser.SetDebugSpewLevel(2);
    parser.setBuffer(script.c_str(),name);
    if(parser.Parse() != SteelParser::PRC_SUCCESS)
    {
        if(parser.hadError())
        {
            throw SteelException(SteelException::PARSING,0,name, parser.getErrors());
        } 
        else
        {
            throw SteelException(SteelException::PARSING,0,name, "Unknown parsing error.");
        }                
    }
    else if (parser.hadError())
    {
        throw SteelException(SteelException::PARSING,0,name, parser.getErrors());
    }

    AstScript *pScript = static_cast<AstScript*>( parser.GetAcceptedToken() );

    pScript->executeScript(this);
    
    delete pScript;

    parser.ClearAcceptedToken();

    return getReturn();
}

SteelType SteelInterpreter::call(const std::string &name, const std::vector<SteelType> &pList)
{
    // First, check the builtins. They can be considered like keywords.

    std::map<std::string,SteelFunctor*>::iterator it = m_functions.find( name );

    if( it != m_functions.end() )
    {
        // Its found, and its a bif.
        SteelFunctor * pFunctor = it->second;
        assert ( pFunctor != NULL );

        return pFunctor->Call(this,pList);
    }
    else
    {

        throw UnknownIdentifier();
    }

    return SteelType();
}

void SteelInterpreter::setReturn(const SteelType &var)
{
    m_return = var;
}

SteelType SteelInterpreter::getReturn() const
{
    return m_return;
}


void SteelInterpreter::declare(const std::string &name)
{
    VariableFile &file = m_symbols.front();

    VariableFile::iterator it = file.find(name);

    if(it != file.end() )
    {
        throw AlreadyDefined();
    }

    file[name] = SteelType();
}

void SteelInterpreter::declare_const(const std::string &name, const SteelType &datum)
{
    declare(name);

    SteelType *pDatum = lookup_internal(name);
    pDatum->operator=(datum);

    pDatum->makeConst();
}

template<class T>
void SteelInterpreter::declare_const(const std::string &name, const T &value)
{
    SteelType datum;
    datum.set(value);

    declare_const(name,datum);
}

// Note: Step 1 is to create a SteelArray in the ArrayFile
// is to create a SteelType in the variable file, and set it's 
// array reference to the array
void SteelInterpreter::declare_array(const std::string &array_name, int size)
{
    VariableFile &file = m_symbols.front();

    VariableFile::iterator it = file.find(array_name);

    if(it != file.end() )
    {
        throw AlreadyDefined();
    }

    SteelType var;
    var.set ( SteelArray( std::max(size,0)  ) );
    
    file[array_name]  = var;
}


SteelType *SteelInterpreter::lookup_lvalue(const std::string &name)
{
    SteelType *p = lookup_internal(name);

    if(p->isConst()) throw ConstViolation();

    if(p == NULL) throw UnknownIdentifier();
    return p;
}



SteelType SteelInterpreter::lookup(const std::string &name)
{
    // if strict, throw Unknown Identifier
    SteelType * pVar = lookup_internal(name);
    if(pVar == NULL) throw UnknownIdentifier();
    return *pVar;
}

SteelType SteelInterpreter::lookup(SteelType *pVar, int index)
{
    // if strict, throw Unknown Identifier
    // TODO: Unknown ID, or was it not an lvalue
    if(pVar == NULL) throw UnknownIdentifier();
    if(!pVar->isArray()) throw TypeMismatch();
  
    return pVar->getElement(index);
}

void SteelInterpreter::assign(SteelType *pVar, const SteelType &value)
{
    // if strict, throw unknown id
    if(pVar == NULL) throw UnknownIdentifier();

    if(pVar->isArray() && (! value.isArray() ))
        throw TypeMismatch();


    *pVar = value;
}


void SteelInterpreter::registerFunction(const std::string &name, 
                                        AstParamDefinitionList *pParams, 
                                        AstStatementList *pStatements, 
                                        bool final)
{
    addFunction(name,new SteelUserFunction( pParams, pStatements, final ));
}

SteelType * SteelInterpreter::lookup_internal(const std::string &name)
{
    for(std::list<VariableFile>::iterator i = m_symbols.begin();
        i != m_symbols.end(); i++)
    {
        VariableFile::iterator it = (*i).find(name);
        if( it != (*i).end()  )
        {
            // Found a match.
            return &(it->second);
        }
    }

    return NULL;
}


void SteelInterpreter::pushScope()
{
    m_symbols.push_front ( VariableFile() );
}

void SteelInterpreter::popScope()
{
    m_symbols.pop_front();
}



void SteelInterpreter::registerBifs()
{
    addFunction( "print", new SteelFunctor1Arg<SteelInterpreter,const std::string &>(this, &SteelInterpreter::print ) );
    addFunction( "println", new SteelFunctor1Arg<SteelInterpreter,const std::string &>(this,&SteelInterpreter::println ) );
    addFunction( "len", new SteelFunctor1Arg<SteelInterpreter,const SteelArray&>(this, &SteelInterpreter::len ) );
    addFunction( "real", new SteelFunctor1Arg<SteelInterpreter,const SteelType&>(this,&SteelInterpreter::real ) );
    addFunction( "integer", new SteelFunctor1Arg<SteelInterpreter,const SteelType&>(this,&SteelInterpreter::integer ) );
    addFunction( "boolean", new SteelFunctor1Arg<SteelInterpreter,const SteelType&>(this,&SteelInterpreter::boolean ) );
    addFunction( "substr", new SteelFunctor3Arg<SteelInterpreter,const std::string&,int, int>(this,&SteelInterpreter::substr ) );
    addFunction( "strlen", new SteelFunctor1Arg<SteelInterpreter,const std::string&>(this,&SteelInterpreter::strlen));
    addFunction( "is_array", new SteelFunctor1Arg<SteelInterpreter,const SteelType&>(this,&SteelInterpreter::is_array));

    // Math functions
    addFunction("ceil", new SteelFunctor1Arg<SteelInterpreter,double>(this,&SteelInterpreter::ceil));
    addFunction("abs", new SteelFunctor1Arg<SteelInterpreter,double>(this,&SteelInterpreter::abs));
    addFunction("floor", new SteelFunctor1Arg<SteelInterpreter,double>(this,&SteelInterpreter::floor));
    addFunction("exp", new SteelFunctor1Arg<SteelInterpreter,double>(this,&SteelInterpreter::exp));
    addFunction("log", new SteelFunctor1Arg<SteelInterpreter,double>(this,&SteelInterpreter::log));
    addFunction("log10", new SteelFunctor1Arg<SteelInterpreter,double>(this,&SteelInterpreter::log10));
    addFunction("sqrt", new SteelFunctor1Arg<SteelInterpreter,double>(this,&SteelInterpreter::sqrt));
    addFunction("acos", new SteelFunctor1Arg<SteelInterpreter,double>(this,&SteelInterpreter::acos));
    addFunction("asin", new SteelFunctor1Arg<SteelInterpreter,double>(this,&SteelInterpreter::asin));
    addFunction("atan", new SteelFunctor1Arg<SteelInterpreter,double>(this,&SteelInterpreter::atan));
    addFunction("atan2", new SteelFunctor2Arg<SteelInterpreter,double,double>(this,&SteelInterpreter::atan2));
    addFunction("cos", new SteelFunctor1Arg<SteelInterpreter,double>(this,&SteelInterpreter::cos));
    addFunction("sin", new SteelFunctor1Arg<SteelInterpreter,double>(this,&SteelInterpreter::sin));
    addFunction("tan", new SteelFunctor1Arg<SteelInterpreter,double>(this,&SteelInterpreter::tan));
    addFunction("cosh", new SteelFunctor1Arg<SteelInterpreter,double>(this,&SteelInterpreter::cosh));
    addFunction("sinh", new SteelFunctor1Arg<SteelInterpreter,double>(this,&SteelInterpreter::sinh));
    addFunction("tanh", new SteelFunctor1Arg<SteelInterpreter,double>(this,&SteelInterpreter::tanh));
    addFunction("round", new SteelFunctor1Arg<SteelInterpreter,double>(this,&SteelInterpreter::round));
}


SteelType SteelInterpreter::print(const std::string &str)
{
    std::cout << str;
    SteelType var;
    var.set(str);

    return var;
}

SteelType SteelInterpreter::println(const std::string &str)
{
    std::cout << str << std::endl;
    SteelType var;
    var.set(str);

    return var;
}

SteelType SteelInterpreter::len(const SteelArray &array)
{
    SteelType val;

    val.set((int)array.size());

    return val;
}

SteelType SteelInterpreter::real(const SteelType &value)
{
    SteelType var;
    var.set ( (double)value );

    return var;
}

SteelType SteelInterpreter::integer(const SteelType &value)
{
    SteelType var;
    var.set ( (int)value );

    return var;
}

SteelType SteelInterpreter::boolean(const SteelType &value)
{
    SteelType var;
    var.set ( (bool)value );

    return var;
}

SteelType SteelInterpreter::substr(const std::string &str, int start, int len)
{
    SteelType var;
    std::string value;

    value = str.substr (start, len);
    var.set ( value );

    return var;
}

SteelType SteelInterpreter::strlen(const std::string &str)
{
    SteelType var;

    var.set ( (int)str.size() );

    return var;
}

SteelType SteelInterpreter::is_array(const SteelType &array)
{
    SteelType var;
    var.set ( array.isArray() );

    return var;
}

///////////////////////////////////////////////////////////////
//
//  Mathematical built-ins
//
///////////////////////////////////////////////////////////////

SteelType SteelInterpreter::ceil (double f)
{
    SteelType var;
    var.set(std::ceil(f));

    return var;
}

SteelType SteelInterpreter::abs  (double f)
{
    SteelType var;
    var.set(std::abs(f));

    return var;
}

SteelType SteelInterpreter::floor(double f)
{
    SteelType var;
    var.set(std::floor(f));

    return var;
}

SteelType SteelInterpreter::exp  (double f)
{
    SteelType var;
    var.set(std::exp(f));

    return var;
}

SteelType SteelInterpreter::log  (double f) // Natural lo
{
    SteelType var;
    var.set(std::log(f));

    return var;
}

SteelType SteelInterpreter::log10(double f)
{
    SteelType var;
    var.set(std::log10(f));

    return var;    
}

SteelType SteelInterpreter::sqrt (double f)
{
    SteelType var;
    var.set(std::sqrt(f));

    return var;
}

SteelType SteelInterpreter::acos (double f)
{
    SteelType var;
    var.set(std::acos(f));

    return var;   
}

SteelType SteelInterpreter::asin (double f)
{
    SteelType var;
    var.set(std::asin(f));

    return var;    
}

SteelType SteelInterpreter::atan (double f)
{
    SteelType var;
    var.set(std::atan(f));

    return var;    
}

SteelType SteelInterpreter::atan2(double f, double g)
{
    SteelType var;
    var.set(std::atan2(f,g));

    return var;    
}

SteelType SteelInterpreter::cos  (double f)
{
    SteelType var;
    var.set(std::cos(f));

    return var;    
}

SteelType SteelInterpreter::sin  (double f)
{
    SteelType var;
    var.set(std::sin(f));

    return var;    
}

SteelType SteelInterpreter::tan  (double f)
{
    SteelType var;
    var.set(std::tan(f));

    return var;
}

SteelType SteelInterpreter::cosh (double f)
{
    SteelType var;
    var.set(std::cosh(f));

    return var;    
}

SteelType SteelInterpreter::sinh (double f)
{
    SteelType var;
    var.set(std::sinh(f));

    return var;    
}

SteelType SteelInterpreter::tanh (double f)
{
    SteelType var;
    var.set(std::tanh(f));

    return var;    
}

SteelType SteelInterpreter::round (double f)
{
    SteelType var;
    var.set(static_cast<double>(floor(f+0.5)));

    return var;    
}
