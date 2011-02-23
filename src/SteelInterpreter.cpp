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

template <class T>
class AutoCall
{
public:
  AutoCall(T* pointer, void(T::*FuncPointer)(void)):m_func(FuncPointer),m_object(pointer){
    }
    ~AutoCall(){
      (m_object->*m_func)();
    }
private:
  void(T::*m_func)(void);
    T* m_object;
};

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
    m_nContextCount(0)
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
                                   shared_ptr<SteelFunctor> pFunc)
{
    // global namespace
    addFunction(name,kszGlobalNamespace,pFunc);    
}

void SteelInterpreter::addFunction(const std::string &name,
                                   SteelFunctor* pFunc)
{
    // global namespace
    shared_ptr<SteelFunctor> functor(pFunc);
    addFunction(name,kszGlobalNamespace,functor);    
}

void SteelInterpreter::addFunction(const std::string &name, const std::string &ns, 
				   SteelFunctor* pFunc)
{
    shared_ptr<SteelFunctor> functor(pFunc);
    addFunction(name,ns,functor);
}


void SteelInterpreter::addFunction(const std::string &name, const std::string &ns, 
				   shared_ptr<SteelFunctor> pFunc)
{
    pFunc->setIdentifier(name); // TODO: Include namespace?
    std::map<std::string,FunctionSet>::iterator sset = m_functions.find ( ns );
    if(sset == m_functions.end())
    {
        m_functions[ns][name] = pFunc;
    }
    else
    {
	std::map<std::string,shared_ptr<SteelFunctor> >::iterator it = sset->second.find ( name );
    
        if(it != sset->second.end()){
	    throw AlreadyDefined();
        }

        sset->second[name] = pFunc;
    }
}

shared_ptr<SteelFunctor> SteelInterpreter::removeFunction(const std::string &name, const std::string &ns)
{
    std::map<std::string,FunctionSet>::iterator set = m_functions.find ( ns );
    std::map<std::string,shared_ptr<SteelFunctor> >::iterator it = set->second.find ( name );

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
    push_context();
    AutoCall<SteelInterpreter> popper(this,&SteelInterpreter::pop_context);
    assert ( NULL != pScript );
    pushScope();
    pScript->execute(this);
    popScope();
    SteelType returnval = getReturn();
    return returnval;
}

// This method allows you to set up some variables at a global scope
// for the script to use.
SteelType SteelInterpreter::runAst(AstScript *pScript, const ParameterList &params)
{
    assert ( NULL != pScript );
    push_context();
    AutoCall<SteelInterpreter> popper(this,&SteelInterpreter::pop_context);
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
    SteelType returnval = getReturn();
    return returnval;
}

SteelType SteelInterpreter::run(const std::string &name,const std::string &script)
{
    SteelParser parser;
    push_context();
    AutoCall<SteelInterpreter> popper(this,&SteelInterpreter::pop_context);
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

    SteelType returnval = getReturn();
    return returnval;
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

shared_ptr<SteelFunctor> SteelInterpreter::lookup_functor(const std::string &name)
{
    for(std::deque<std::string>::reverse_iterator it = m_namespace_scope.rbegin(); it != m_namespace_scope.rend();
        it++)
    {
        FunctionSet &set = m_functions[*it];

        std::map<std::string,shared_ptr<SteelFunctor> >::iterator iter = set.find( name );

        if( iter != set.end() )
        {
            shared_ptr<SteelFunctor> pFunctor = iter->second;
            assert ( pFunctor != NULL );

            return pFunctor;
        }
    }

    throw UnknownIdentifier(name);

    return shared_ptr<SteelFunctor>((SteelFunctor*)NULL);
}

SteelType SteelInterpreter::call(const std::string &name, const std::string &ns, const std::vector<SteelType> &pList)
{
    shared_ptr<SteelFunctor> pFunctor = lookup_functor(name,ns);

    return pFunctor->Call(this,pList);
}

shared_ptr<SteelFunctor> SteelInterpreter::lookup_functor(const std::string &name, const std::string &ns)
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
        throw UnknownIdentifier(ns + "::" + name);
    }

    FunctionSet &set = setiter->second;

    std::map<std::string,shared_ptr<SteelFunctor> >::iterator iter = set.find( name );

    if( iter != set.end() )
    {
        shared_ptr<SteelFunctor> pFunctor = iter->second;
        assert ( pFunctor != NULL );

        return pFunctor;
    }
    throw UnknownIdentifier(ns + "::" + name);

    return shared_ptr<SteelFunctor>((SteelFunctor*)NULL);
}

void SteelInterpreter::setReturn(const SteelType &var)
{
    m_return_stack.pop_front();
    m_return_stack.push_front(var);
}

SteelType SteelInterpreter::getReturn() const
{
    return m_return_stack.front();
}

void SteelInterpreter::removeFunctions(const std::string &ns)
{
    FunctionSet &set = m_functions[ns];

    m_functions.erase(ns);
}

void SteelInterpreter::push_context()
{
    if(m_nContextCount++ == 0)
    {
	   import(kszGlobalNamespace);
    }
    m_return_stack.push_front(SteelType());
}

void SteelInterpreter::pop_context()
{
    if(--m_nContextCount == 0)
    {
	remove_user_functions();
	clear_imports();
    }
    m_return_stack.pop_front();
}

void SteelInterpreter::remove_user_functions()
{
      for(std::map<std::string,FunctionSet>::iterator iter = m_functions.begin();
        iter != m_functions.end(); iter++)
        {
            FunctionSet& sset = iter->second;
            for(FunctionSet::iterator fiter = sset.begin(); fiter != sset.end(); fiter++)
            {
                shared_ptr<SteelFunctor> functor = fiter->second;
                if(functor->isUserFunction()){
                    // No need to delete the functor itself, right?                                                                                                     
                    sset.erase(fiter);
                }
            }
        }
        
        // I created these scripts when any requires happened, but didn't delete them
        // because I need the code to hang around in case there are functions or whatever
        // but, I have to clean them up sometime. Now makes the most sense. 
        for(std::map<std::string,AstScript*>::iterator iter = m_requires.begin();
	    iter != m_requires.end(); iter++)
	    {
		delete iter->second;
	    }
	    
	    m_requires.clear();

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

    if(p == NULL) throw UnknownIdentifier(name);

    if(p->isConst()) throw ConstViolation();

    return p;
}



SteelType SteelInterpreter::lookup(const std::string &name)
{
    // if strict, throw Unknown Identifier
    SteelType * pVar = lookup_internal(name);
    if(pVar == NULL) throw UnknownIdentifier(name);
    return *pVar;
}

SteelType SteelInterpreter::lookup(SteelType *pVar, int index)
{
    // if strict, throw Unknown Identifier
    // TODO: Unknown ID, or was it not an lvalue
    if(pVar == NULL) throw UnknownIdentifier("???");
    if(!pVar->isArray()) throw TypeMismatch();
  
    return pVar->getElement(index);
}

void SteelInterpreter::assign(SteelType *pVar, const SteelType &value)
{
    // if strict, throw unknown id
    if(pVar == NULL) throw UnknownIdentifier("???");

    if(pVar->isArray() && (! value.isArray() ))
        throw TypeMismatch();


    *pVar = value;
}


void SteelInterpreter::registerFunction(const std::string &name, 
                                        const std::string &ns,
                                        AstParamDefinitionList *pParams, 
                                        AstStatementList *pStatements)
{
    shared_ptr<SteelFunctor> functor(new SteelUserFunction(pParams,pStatements));
    addFunction(name,ns,functor);
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
    addFunction("require",new SteelFunctor1Arg<SteelInterpreter,const std::string&>(this,&SteelInterpreter::require));
    addFunction("add",new SteelFunctor2Arg<SteelInterpreter,const SteelArray&,const SteelType&>(this,&SteelInterpreter::add));
    addFunction("print", new SteelFunctor1Arg<SteelInterpreter,const std::string&>(this,&SteelInterpreter::print));
    addFunction("println",new SteelFunctor1Arg<SteelInterpreter,const std::string&>(this,&SteelInterpreter::println));
    addFunction("len",new SteelFunctor1Arg<SteelInterpreter,const SteelArray&>(this,&SteelInterpreter::len));
    addFunction("real",new SteelFunctor1Arg<SteelInterpreter,const SteelType&>(this,&SteelInterpreter::real));
    addFunction("integer",new SteelFunctor1Arg<SteelInterpreter,const SteelType&>(this,&SteelInterpreter::integer));
    addFunction("boolean",new SteelFunctor1Arg<SteelInterpreter,const SteelType&>(this,&SteelInterpreter::boolean));
    addFunction("substr",new SteelFunctor3Arg<SteelInterpreter,const std::string&,int,int>(this,&SteelInterpreter::substr));
    addFunction("strlen",new SteelFunctor1Arg<SteelInterpreter,const std::string&>(this,&SteelInterpreter::strlen));
    addFunction("is_array",new SteelFunctor1Arg<SteelInterpreter,const SteelType&>(this,&SteelInterpreter::is_array));
    addFunction("is_handle",new SteelFunctor1Arg<SteelInterpreter,const SteelType&>(this,&SteelInterpreter::is_handle));
    addFunction("is_valid",new SteelFunctor1Arg<SteelInterpreter,const SteelType&>(this,&SteelInterpreter::is_valid));
    addFunction("array",new SteelFunctorArray<SteelInterpreter>(this,&SteelInterpreter::array));


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

SteelType SteelInterpreter::require(const std::string &filename)
{
    if(m_requires.find(filename) == m_requires.end())
    {
	std::ifstream instream;
	instream.open(filename.c_str(),std::ios::in);
	if(!instream.good())
	{
	    throw FileNotFound();
	}
	
	std::ostringstream strstream;
	while(!instream.eof())
	{
	    char c = instream.get();
	    strstream.put(c);
	}
	instream.close();
	m_requires[filename] = NULL;
	
	SteelParser parser;
	parser.setBuffer(strstream.str().c_str(),filename);
	AstBase * pBase;
	if(parser.Parse(&pBase) != SteelParser::PRC_SUCCESS)
	{
	    if(parser.hadError())
	    {
		throw SteelException(SteelException::PARSING,0,filename, parser.getErrors());
	    } 
	    else
	    {
		throw SteelException(SteelException::PARSING,0,filename, "Unknown parsing error.");
	    }                
	}
	else if (parser.hadError())
	{
	    throw SteelException(SteelException::PARSING,0,filename, parser.getErrors());
	}

	AstScript *pScript = static_cast<AstScript*>( pBase );

	pScript->execute(this);
    
	m_requires[filename] = pScript;
    }
    return SteelType();
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
