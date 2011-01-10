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
#include <stdlib.h>

#pragma warning(disable: 4355)


const char * SteelInterpreter::kszGlobalNamespace = "_global";
const char * SteelInterpreter::kszUnspecifiedNamespace = "?";

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

ParameterListItem::ParameterListItem(const std::string &name, const SteelType &var)
{
    m_name = name;
    m_value = var;
}

ParameterListItem::ParameterListItem(const std::string &name, SteelType::Handle p)
{
    m_name = name;
    m_value.set(p);
}


SteelInterpreter::SteelInterpreter()
:
  m_add_f(this,&SteelInterpreter::add),
    m_print_f(this, &SteelInterpreter::print ),
    m_println_f(this,&SteelInterpreter::println ),
    m_len_f(this, &SteelInterpreter::len ),
    m_real_f(this,&SteelInterpreter::real ),
    m_integer_f(this,&SteelInterpreter::integer ),
    m_boolean_f(this,&SteelInterpreter::boolean ),
    m_substr_f(this,&SteelInterpreter::substr ),
    m_strlen_f(this,&SteelInterpreter::strlen),
    m_is_array_f(this,&SteelInterpreter::is_array),
    m_is_handle_f(this,&SteelInterpreter::is_handle),
    m_is_valid_f(this,&SteelInterpreter::is_valid),
    m_array_f(this,&SteelInterpreter::array)
{
    pushScope();
    registerBifs();
    srand(time(0));

}

SteelInterpreter::~SteelInterpreter()
{
    popScope();
}


void SteelInterpreter::addFunction(const std::string &name,
                                   SteelFunctor *pFunc)
{
    // global namespace
    addFunction(name,kszGlobalNamespace,pFunc);    
}

void SteelInterpreter::addFunction(const std::string &name, const std::string &ns, 
                                  SteelFunctor * pFunc)
{
    std::map<std::string,FunctionSet>::iterator sset = m_functions.find ( ns );
    if(sset == m_functions.end())
    {
        m_functions[ns][name] = pFunc;
    }
    else
    {
        std::map<std::string,SteelFunctor*>::iterator it = sset->second.find ( name );
    
        if(it != sset->second.end() && it->second->isFinal()) throw AlreadyDefined();

        sset->second[name] = pFunc;
    }
}

SteelFunctor *SteelInterpreter::removeFunction(const std::string &name, const std::string &ns)
{
    std::map<std::string,FunctionSet>::iterator set = m_functions.find ( ns );
    std::map<std::string,SteelFunctor*>::iterator it = set->second.find ( name );

    set->second.erase(it);

    if(set->second.empty()) m_functions.erase(set);

    return it->second;
}

AstScript * SteelInterpreter::prebuildAst(const std::string &script_name,
                                          const std::string &script,
                                          bool debugparser,
					  bool debugscanner)
{
    SteelParser parser;
    parser.setBuffer(script.c_str(),script_name);

    AstBase *pBase;

    parser.DebugSpew(debugparser);
    parser.SetScannerDebugSpew(debugscanner);
    

    if(parser.Parse(&pBase) != SteelParser::PRC_SUCCESS)
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

    //AstScript *pScript = static_cast<AstScript*>( parser.GetAcceptedToken() );

    AstScript * pScript = dynamic_cast<AstScript*> ( pBase );

    return pScript;
}

SteelType SteelInterpreter::runAst(AstScript *pScript)
{
    import(kszGlobalNamespace);
    assert ( NULL != pScript );
    pScript->execute(this);

    remove_user_functions();
    clear_imports();
    return getReturn();
}

// This method allows you to set up some variables at a global scope
// for the script to use.
SteelType SteelInterpreter::runAst(AstScript *pScript, const ParameterList &params)
{
    assert ( NULL != pScript );
    import(kszGlobalNamespace);
    pushScope();
    for(ParameterList::const_iterator it = params.begin(); it != params.end();
        it++)
    {
        declare( it->getName() );
        SteelType * pVar  = lookup_internal(it->getName());
        assign(pVar,it->getValue());
    }

    pScript->execute(this);
    popScope();
    remove_user_functions();
    clear_imports();
    return getReturn();
}

SteelType SteelInterpreter::run(const std::string &name,const std::string &script)
{
    SteelParser parser;
    import(kszGlobalNamespace);
//    parser.SetDebugSpewLevel(2);
    parser.setBuffer(script.c_str(),name);
    AstBase * pBase;
    if(parser.Parse(&pBase) != SteelParser::PRC_SUCCESS)
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

    AstScript *pScript = static_cast<AstScript*>( pBase );

    pScript->execute(this);
    
    delete pScript;

    remove_user_functions();
    clear_imports();
    return getReturn();
}

void SteelInterpreter::clear_imports()
{
    m_namespace_scope.clear();
    import(kszGlobalNamespace);
}

void SteelInterpreter::import(const std::string &ns)
{
    for(std::deque<std::string>::iterator it = m_namespace_scope.begin();
        it != m_namespace_scope.end(); it++)
        if(*it == ns) return; // Already have it imported

    m_namespace_scope.push_back(ns);
}

SteelType SteelInterpreter::call(const std::string &name, const std::vector<SteelType> &pList)
{
    return lookup_functor(name)->Call(this,pList);
}

SteelFunctor* SteelInterpreter::lookup_functor(const std::string &name)
{
    for(std::deque<std::string>::reverse_iterator it = m_namespace_scope.rbegin(); it != m_namespace_scope.rend();
        it++)
    {
        FunctionSet &set = m_functions[*it];

        std::map<std::string,SteelFunctor*>::iterator iter = set.find( name );

        if( iter != set.end() )
        {
            SteelFunctor * pFunctor = iter->second;
            assert ( pFunctor != NULL );

            return pFunctor;
        }
    }

    throw UnknownIdentifier();

    return NULL;
}

SteelType SteelInterpreter::call(const std::string &name, const std::string &ns, const std::vector<SteelType> &pList)
{
    SteelFunctor * pFunctor = lookup_functor(name,ns);

    return pFunctor->Call(this,pList);
}

SteelFunctor* SteelInterpreter::lookup_functor(const std::string &name, const std::string &ns)
{
    static std::string unspecifiedNS(kszUnspecifiedNamespace);
    // If this call has no namespace, we have to search for it using this
    // version...
    if(ns == unspecifiedNS)
    {
        return lookup_functor(name);
    }

    std::map<std::string,FunctionSet>::iterator setiter = m_functions.find(ns);

    if(setiter == m_functions.end())
    {
        throw UnknownIdentifier();
    }

    FunctionSet &set = setiter->second;

    std::map<std::string,SteelFunctor*>::iterator iter = set.find( name );

    if( iter != set.end() )
    {
        SteelFunctor * pFunctor = iter->second;
        assert ( pFunctor != NULL );

        return pFunctor;
    }
    throw UnknownIdentifier();

    return NULL;
}

void SteelInterpreter::setReturn(const SteelType &var)
{
    m_return = var;
}

SteelType SteelInterpreter::getReturn() const
{
    return m_return;
}

void SteelInterpreter::removeFunctions(const std::string &ns, bool del)
{
    FunctionSet &set = m_functions[ns];

    if(del)
    {
        for(FunctionSet::iterator it=set.begin();it!=set.end(); it++)
        {
            SteelFunctor *pFunc = it->second;
            delete pFunc;
        }
    }

    m_functions.erase(ns);
}

void SteelInterpreter::remove_user_functions()
{
      for(std::map<std::string,FunctionSet>::iterator iter = m_functions.begin();
        iter != m_functions.end(); iter++)
        {
            FunctionSet& sset = iter->second;
            for(FunctionSet::iterator fiter = sset.begin(); fiter != sset.end(); fiter++)
            {
                SteelFunctor * functor = fiter->second;
                if(functor->isUserFunction()){
                    // No need to delete the functor itself, right?                                                                                                     
                    sset.erase(fiter);
                }
            }
        }

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

    if(p == NULL) throw UnknownIdentifier();

    if(p->isConst()) throw ConstViolation();

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
                                        const std::string &ns,
                                        AstParamDefinitionList *pParams, 
                                        AstStatementList *pStatements, 
                                        bool final)
{
    addFunction(name,ns,new SteelUserFunction( pParams, pStatements, final ));
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

    srand(time(0));
    addFunction("add",&m_add_f);
    addFunction("print", &m_print_f);
    addFunction("println",&m_println_f);
    addFunction("len",&m_len_f);
    addFunction("real",&m_real_f);
    addFunction("integer",&m_integer_f);
    addFunction("boolean",&m_boolean_f);
    addFunction("substr",&m_substr_f);
    addFunction("strlen",&m_strlen_f);
    addFunction("is_array",&m_is_array_f);
    addFunction("is_handle",&m_is_handle_f);
    addFunction("is_valid",&m_is_valid_f);
    addFunction("array",&m_array_f);


    // Math functions
  
    addFunction("ceil", "math", new SteelFunctor1Arg<SteelInterpreter,double>(this,&SteelInterpreter::ceil));
    addFunction("abs", "math", new SteelFunctor1Arg<SteelInterpreter,double>(this,&SteelInterpreter::abs));
    addFunction("floor", "math", new SteelFunctor1Arg<SteelInterpreter,double>(this,&SteelInterpreter::floor));
    addFunction("exp", "math",new SteelFunctor1Arg<SteelInterpreter,double>(this,&SteelInterpreter::exp));
    addFunction("log", "math",new SteelFunctor1Arg<SteelInterpreter,double>(this,&SteelInterpreter::log));
    addFunction("log10", "math", new SteelFunctor1Arg<SteelInterpreter,double>(this,&SteelInterpreter::log10));
    addFunction("sqrt", "math",new SteelFunctor1Arg<SteelInterpreter,double>(this,&SteelInterpreter::sqrt));
    addFunction("acos", "math",new SteelFunctor1Arg<SteelInterpreter,double>(this,&SteelInterpreter::acos));
    addFunction("asin", "math",new SteelFunctor1Arg<SteelInterpreter,double>(this,&SteelInterpreter::asin));
    addFunction("atan", "math",new SteelFunctor1Arg<SteelInterpreter,double>(this,&SteelInterpreter::atan));
    addFunction("atan2", "math",new SteelFunctor2Arg<SteelInterpreter,double,double>(this,&SteelInterpreter::atan2));
    addFunction("cos", "math",new SteelFunctor1Arg<SteelInterpreter,double>(this,&SteelInterpreter::cos));
    addFunction("sin", "math",new SteelFunctor1Arg<SteelInterpreter,double>(this,&SteelInterpreter::sin));
    addFunction("tan", "math",new SteelFunctor1Arg<SteelInterpreter,double>(this,&SteelInterpreter::tan));
    addFunction("cosh", "math",new SteelFunctor1Arg<SteelInterpreter,double>(this,&SteelInterpreter::cosh));
    addFunction("sinh", "math",new SteelFunctor1Arg<SteelInterpreter,double>(this,&SteelInterpreter::sinh));
    addFunction("tanh", "math",new SteelFunctor1Arg<SteelInterpreter,double>(this,&SteelInterpreter::tanh));
    addFunction("round", "math",new SteelFunctor1Arg<SteelInterpreter,double>(this,&SteelInterpreter::round));
    addFunction("pow", "math", new SteelFunctor2Arg<SteelInterpreter,double,double>(this,&SteelInterpreter::pow));
    addFunction("rand","math", new SteelFunctorNoArgs<SteelInterpreter>(this, &SteelInterpreter::rand));
    addFunction("srand","math", new SteelFunctor1Arg<SteelInterpreter,int>(this, &SteelInterpreter::srand));
    addFunction("randf","math", new SteelFunctorNoArgs<SteelInterpreter>(this, &SteelInterpreter::randf));
    
    SteelType pi;
    pi.set( 4.0 * std::atan(1.0) );
    declare_const("$_PI",pi);
}

SteelType SteelInterpreter::add(const SteelArray &array, const SteelType &type)
{
  SteelType array2;
  array2.set(array);
  array2.add(type);

  return array2;
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

SteelType SteelInterpreter::array(const SteelArray& value)
{
  SteelType array;
  array.set(value);

  return array;
}

SteelType SteelInterpreter::is_array(const SteelType &array)
{
    SteelType var;
    var.set ( array.isArray() );

    return var;
}


SteelType SteelInterpreter::is_handle(const SteelType &handle)
{
    SteelType var;
    var.set ( handle.isHandle() );

    return var;
}


SteelType SteelInterpreter::is_valid(const SteelType &handle)
{
    SteelType var;
    var.set ( handle.isValidHandle() );

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

SteelType SteelInterpreter::pow (double f, double g)
{
    SteelType var;
    var.set(static_cast<double>(::pow(f,g)));

    return var;
}

SteelType SteelInterpreter::srand ( int s )
{
  ::srand(s);

  return SteelType();
}

SteelType SteelInterpreter::rand ( )
{
  SteelType var;
  var.set(::rand());
  return var;
}

SteelType SteelInterpreter::randf ( )
{
  SteelType var;
  var.set( (double)::rand() / ((double)RAND_MAX + 1) );
  return var;
}
