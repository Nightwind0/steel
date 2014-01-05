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
#include <cstring>
#include <string>
#include <cstdlib>
#include <algorithm>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#pragma warning(disable: 4355)

#if !defined(max)
#define max std::max
#endif

using namespace Steel;

const std::string SteelInterpreter::kszGlobalNamespace = "_global";
const std::string SteelInterpreter::kszUnspecifiedNamespace = "?";
namespace Steel { 
class FileHandle : public SteelType::IHandle
{
public:
    FileHandle(){}
    virtual ~FileHandle(){}
    
    virtual std::ios* GetIos() const = 0;
private:
    
};


class FileInHandle : public FileHandle
{
public:
    FileInHandle(){
    }
    virtual ~FileInHandle(){
    }
    virtual std::ios* GetIos() const { return m_fstream; }
    std::istream* GetStream() const { return m_fstream; }
    void SetStream(std::istream* stream) { m_fstream = stream; }
private:
    std::istream* m_fstream;
};

class FileOutHandle : public FileHandle
{
public:
    FileOutHandle(){
    }
    virtual ~FileOutHandle(){
    }
    virtual std::ios* GetIos() const { return m_fstream; }
    std::ostream* GetStream() const { return m_fstream; }
    void SetStream(std::ostream* stream) { m_fstream = stream; }
private:
    std::ostream* m_fstream;
};

}
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
    m_nContextCount(0),m_file_provider(NULL)
#if USE_DYNAMIC_MUTEXES
    ,m_function_mutex(NULL),m_symbol_mutex(NULL),
    m_stack_mutex(NULL),m_scope_mutex(NULL),m_import_mutex(NULL)
#endif
{
#if USE_DYNAMIC_MUTEXES
    m_function_mutex = new Mutex();
    m_symbol_mutex = new Mutex();
    m_stack_mutex = new Mutex();
    m_scope_mutex = new Mutex();
    m_import_mutex = new Mutex();
#endif
    m_file_provider = &m_default_file_provider;
    pushScope();
    registerBifs();
    srand(time(0));
}

SteelInterpreter::~SteelInterpreter()
{
    popScope();
}

const std::string SteelInterpreter::getVersion(){
  return VERSION;
}

void SteelInterpreter::addFunction(const std::string &name,
                                   std::shared_ptr<SteelFunctor> pFunc)
{
    // global namespace
    std::string ns = kszGlobalNamespace;
    addFunction(name,ns,pFunc);    
}

void SteelInterpreter::addFunction(const std::string &name,
                                   SteelFunctor* pFunc)
{
    // global namespace
    std::shared_ptr<SteelFunctor> functor(pFunc);
    std::string ns = kszGlobalNamespace;
    addFunction(name,ns,functor);    
}

void SteelInterpreter::addFunction(const std::string &name, const std::string &ns, 
                                   SteelFunctor* pFunc)
{
    std::shared_ptr<SteelFunctor> functor(pFunc);
    addFunction(name,ns,functor);
}




void SteelInterpreter::addFunction(const std::string &name, const std::string &ns, 
                                   std::shared_ptr<SteelFunctor> pFunc)
{
    AutoLock mutex(m_function_mutex);
  
    pFunc->setIdentifier(name); // TODO: Include namespace?
    declare_function(ns,name,pFunc);  
}



void SteelInterpreter::disableThreadSafety(){
#if USE_DYNAMIC_MUTEXES
    m_symbol_mutex->disable();
    m_function_mutex->disable();
    m_scope_mutex->disable();
    m_stack_mutex->disable();
    m_import_mutex->disable();
#else
    m_symbol_mutex.disable();
    m_function_mutex.disable();
    m_scope_mutex.disable();
    m_stack_mutex.disable();
    m_import_mutex.disable();
#endif
}

void SteelInterpreter::enableThreadSafety(){
#if USE_DYNAMIC_MUTEXES
    m_symbol_mutex->enable();
    m_function_mutex->enable();
    m_scope_mutex->enable();
    m_stack_mutex->enable();
    m_import_mutex->enable();
#else
    m_symbol_mutex.enable();
    m_function_mutex.enable();
    m_scope_mutex.enable();
    m_stack_mutex.enable();
    m_import_mutex.enable();
#endif
}

AstScript * SteelInterpreter::prebuildAst(const std::string &script_name,
                                          const std::string &script,
                                          bool debugparser,
                                          bool debugscanner)
{
    SteelParser parser;
    parser.setBuffer(script.c_str(),script_name);
    parser.setFileProvider(m_file_provider);

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
    AstStatement::eStopType stopType =  pScript->execute(this);
    popScope();
    SteelType returnval;
    if(stopType == AstStatement::RETURN){
        returnval = popReturnStack();
    }
    return returnval;
}

bool SteelInterpreter::paramStackEmpty() const{
    return m_param_stack.empty(); // TODO: On all stack operations look up stack for thread, to properly do threading
}
SteelType SteelInterpreter::popParamStack(){
    AutoLock lock(m_stack_mutex);
    SteelType ret = m_param_stack.front(); // TODO: On all stack operations look up stack for thread, to properly do threading
    m_param_stack.pop_front();
    return ret;
}
void SteelInterpreter::pushParamStack(const SteelType& value){
    AutoLock lock(m_stack_mutex);
    m_param_stack.push_back(value); // TODO: On all stack operations look up stack for thread, to properly do threading
}
bool SteelInterpreter::returnStackEmpty() const{
    return m_return_stack.empty();
}
SteelType SteelInterpreter::popReturnStack(){
    AutoLock lock(m_stack_mutex);
    // TODO: On all stack operations look up stack for thread, to properly do threading
    SteelType ret = m_return_stack.top();
    m_return_stack.pop();
    return ret;
}
void SteelInterpreter::pushReturnStack(const SteelType& value){
    // TODO: On all stack operations look up stack for thread, to properly do threading
    AutoLock lock(m_stack_mutex);
    m_return_stack.push(value);
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

    AstStatement::eStopType stopType = pScript->execute(this);
    popScope();
    SteelType returnval;
    if(stopType == AstStatement::RETURN){
        returnval = popReturnStack();
    }
    return returnval;
}

SteelType SteelInterpreter::run(const std::string &name,const std::string &script)
{
    SteelParser parser;
    push_context();
    AutoCall<SteelInterpreter> popper(this,&SteelInterpreter::pop_context);
//    parser.SetDebugSpewLevel(2);
    parser.setFileProvider(m_file_provider);
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

    AstStatement::eStopType stop = pScript->execute(this);
    
    delete pScript;

    SteelType returnval;
    if(stop == AstStatement::RETURN){
        returnval = popReturnStack();
    }
    return returnval;
}

void SteelInterpreter::clear_imports()
{
    AutoLock mutex(m_import_mutex);
    m_namespace_scope.clear();
    import(kszGlobalNamespace);
}

void SteelInterpreter::import(const std::string &ns)
{
    AutoLock mutex(m_import_mutex);
    for(std::vector<std::string>::iterator it = m_namespace_scope.begin();
        it != m_namespace_scope.end(); it++)
        if(*it == ns) return; // Already have it imported

    m_namespace_scope.push_back(ns);
}

SteelType SteelInterpreter::call(const std::string &name, const SteelType::Container &pList)
{
    AutoLock mutex(m_function_mutex);
    SteelType *pFunctor = lookup_internal(name);
    return pFunctor->getFunctor()->Call(this,pList);
}
#if 0 
shared_ptr<SteelFunctor> SteelInterpreter::lookup_functor(const std::string &name)
{
    AutoLock mutex(m_function_mutex);
    for(std::list<std::string>::const_reverse_iterator it = m_namespace_scope.rbegin(); it != m_namespace_scope.rend();
        it++)
    {
        std::map<std::string,FunctionSet>::iterator ns_it = m_functions.find(*it);
        if(ns_it != m_functions.end()){
            FunctionSet &set = ns_it->second;

            std::map<std::string,shared_ptr<SteelFunctor> >::iterator iter = set.find( name );

            if( iter != set.end() )
            {
                shared_ptr<SteelFunctor> pFunctor = iter->second;
                assert ( pFunctor != NULL );

                return pFunctor;
            }
        }
    }

    throw UnknownIdentifier(name);

    return shared_ptr<SteelFunctor>();
}
#endif
SteelType SteelInterpreter::call(const std::string &name, const std::string &ns, const SteelType::Container &pList)
{
    AutoLock mutex(m_function_mutex);
    SteelType *pFunctor = lookup_internal(make_id(ns,name));
    return pFunctor->getFunctor()->Call(this,pList); // calling getFunctor can throw ValueNotFunction
}
#if 0 
void SteelInterpreter::removeFunctions(const std::string &ns)
{
    AutoLock mutex(m_function_mutex);
    std::map<std::string,FunctionSet>::iterator it = m_functions.find(ns);
    if(it != m_functions.end()){
        FunctionSet &set = it->second;

        m_functions.erase(ns);
    }
}
#endif
std::string SteelInterpreter::get_namespace(const std::string& id){
    int scope = id.find("::");
    assert(scope != std::string::npos);
    return id.substr(0,scope);
}
std::string SteelInterpreter::make_id(const std::string& ns, const std::string& name){
    return ns + "::" + name;
}

void SteelInterpreter::push_context()
{
    AutoLock mutex(m_stack_mutex);
    if(m_nContextCount++ == 0)
    {
        import(kszGlobalNamespace);
    }
    //m_return_stack.push(SteelType());
}

void SteelInterpreter::pop_context()
{
    AutoLock mutex(m_stack_mutex);
    if(--m_nContextCount == 0)
    {
        clear_imports();
        free_file_handles();
        m_requires.clear();
    }
    //m_return_stack.pop_front();
}

void SteelInterpreter::free_file_handles()
{
    for(std::set<FileHandle*>::iterator iter=m_file_handles.begin();iter!=m_file_handles.end();iter++)
    {
        FileInHandle* inhandle = dynamic_cast<FileInHandle*>(*iter);
        FileOutHandle* outhandle = dynamic_cast<FileOutHandle*>(*iter);
	
        if(inhandle){ 
            std::ifstream * stream = dynamic_cast<std::ifstream*>(inhandle->GetStream());
            if(stream){ 
                stream->close();
                delete stream;
            }
            delete inhandle;
        }
        if(outhandle){
            std::ofstream * stream = dynamic_cast<std::ofstream*>(outhandle->GetStream());
            if(stream){
                stream->flush();
                stream->close();
                delete stream;
            }
            delete outhandle;
        }
    }
    m_file_handles.clear();
}





void SteelInterpreter::declare(const std::string &name)
{
    AutoLock mutex(m_symbol_mutex);
    assert(!m_symbols.empty());
    VariableFile &file = m_symbols.front();

    VariableFile::iterator it = file.find(name);

    if(it != file.end() )
    {
        throw AlreadyDefined(name);
    }

    file[name] = SteelType();
}
void SteelInterpreter::declare_function(const std::string& ns, const std::string &name, std::shared_ptr<SteelFunctor> &datum){
    SteelType func;
    func.set(datum);
    declare_const(make_id(ns,name),func);
}

void SteelInterpreter::declare_const(const std::string &name, const SteelType &datum)
{
    declare(name);

    SteelType *pDatum = lookup_internal(name);
    assert(pDatum);
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
    AutoLock mutex(m_symbol_mutex);
    assert(!m_symbols.empty());
    VariableFile &file = m_symbols.front();

    VariableFile::iterator it = file.find(array_name);

    if(it != file.end() )
    {
        throw AlreadyDefined(array_name);
    }

    SteelType var;
    var.set ( SteelArray( max(size,0)  ) );
    
    file[array_name]  = var;
}

SteelType *SteelInterpreter::enclose_value(const std::string &name)
{
    SteelType *p = lookup_internal(name);

    if(p == NULL) throw UnknownIdentifier(name);
    return p;
}

SteelType *SteelInterpreter::lookup_lvalue(const std::string &name)
{
    SteelType *p = lookup_internal(name);

    if(p == NULL) throw UnknownIdentifier(name);

    if(p->isConst()) 
        throw ConstViolation();

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

SteelType SteelInterpreter::lookup_function(const std::string &ns, const std::string &name){
    return lookup(make_id(ns,name));
}
SteelType SteelInterpreter::lookup(SteelType *pVar, const std::string& key)
{
    // if strict, throw Unknown Identifier
    // TODO: Unknown ID, or was it not an lvalue
    if(pVar == NULL) throw UnknownIdentifier("???");
    if(!pVar->isHashMap()) throw TypeMismatch();
  
    return pVar->getElement(key);
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
                                        std::shared_ptr<AstParamDefinitionList> pParams, 
                                        std::shared_ptr<AstStatementList> pStatements)
{
    std::shared_ptr<SteelFunctor> functor(new SteelUserFunction(pParams,pStatements));
    addFunction(name,ns,functor);
}

void SteelInterpreter::registerAuxVariables( AuxVariables* pVariables ) {
    m_aux_variables.push_front(pVariables);
}


void SteelInterpreter::removeAuxVariables( AuxVariables* pVariables ) {
    std::list<AuxVariables*>::iterator it = std::find(m_aux_variables.begin(),m_aux_variables.end(),pVariables);
    if(it != m_aux_variables.end())
        m_aux_variables.erase(it);
}

void SteelInterpreter::linkAuxVariables( AuxVariables* pVariables ) {
    m_linked_aux_variables.push_front(pVariables);
}


void SteelInterpreter::unlinkAuxVariables( AuxVariables* pVariables ) {
    std::list<AuxVariables*>::iterator it = std::find(m_linked_aux_variables.begin(),m_linked_aux_variables.end(),pVariables);
    if(it != m_linked_aux_variables.end())
        m_linked_aux_variables.erase(it);
}

SteelType * SteelInterpreter::_lookup_internal(const std::string &i_name){
    for(std::deque<VariableFile>::iterator i = m_symbols.begin();
        i != m_symbols.end(); i++)
    {
        VariableFile::iterator it = (*i).find(i_name);
        if( it != (*i).end()  )
        {
            // Found a match.
            return &(it->second);
        }
    }
    
    // Still not found. Try the aux variables
    for(std::list<AuxVariables*>::iterator it = m_aux_variables.begin();
        it != m_aux_variables.end(); it++){
        SteelType* pVar = (*it)->lookupLValue(i_name);
        if(pVar != NULL) 
            return pVar;
    }
    return NULL;
}

SteelType * SteelInterpreter::lookup_internal(const std::string &i_name)
{
    AutoLock mutex(m_symbol_mutex);
    SteelType * val = _lookup_internal(i_name); // Try with no namespace
    if(val) return val;

    for(auto ns_it = m_namespace_scope.begin(); ns_it != m_namespace_scope.end(); ns_it++){
        const std::string name = make_id(*ns_it,i_name);
        val = _lookup_internal(name);
        if(val) return val;
    }

    return NULL;
}


void SteelInterpreter::pushScope()
{
    AutoLock mutex(m_scope_mutex);
    m_symbols.push_front ( VariableFile() );
}

void SteelInterpreter::popScope()
{
    AutoLock mutex(m_scope_mutex);
    assert(!m_symbols.empty());
    for(std::list<AuxVariables*>::iterator aux_it = m_linked_aux_variables.begin();
        aux_it != m_linked_aux_variables.end(); aux_it++){
        (*aux_it)->transferOwnership(m_symbols.front());
    }
    m_symbols.pop_front();
}

void SteelInterpreter::setFileProvider( IFileProvider* provider )
{
    m_file_provider = provider;
}


void SteelInterpreter::registerBifs()
{

    srand(time(0));
    addFunction("require",std::shared_ptr<SteelFunctor>(new SteelFunctor1Arg<SteelInterpreter,const std::string&>
                                                   (this,&SteelInterpreter::require)));
    addFunction("print",std::shared_ptr<SteelFunctor>(new SteelFunctor1Arg<SteelInterpreter,const std::string&>
                                                  (this,&SteelInterpreter::print)));
    addFunction("println",std::shared_ptr<SteelFunctor>(new SteelFunctor1Arg<SteelInterpreter,const std::string&>
                                                   (this,&SteelInterpreter::println)));
    addFunction("len",std::shared_ptr<SteelFunctor>(new SteelFunctor1Arg<SteelInterpreter,const SteelType&>(this,&SteelInterpreter::len)));
    addFunction("real",std::shared_ptr<SteelFunctor>(new SteelFunctor1Arg<SteelInterpreter,const SteelType&>(this,&SteelInterpreter::real)));
    addFunction("integer",std::shared_ptr<SteelFunctor>(new SteelFunctor1Arg<SteelInterpreter,const SteelType&>(this,&SteelInterpreter::integer)));
    addFunction("boolean",std::shared_ptr<SteelFunctor>(new SteelFunctor1Arg<SteelInterpreter,const SteelType&>(this,&SteelInterpreter::boolean)));
    addFunction("substr",std::shared_ptr<SteelFunctor>(new SteelFunctor3Arg<SteelInterpreter,const std::string&,int,int>(this,&SteelInterpreter::substr)));
    addFunction("strlen",new SteelFunctor1Arg<SteelInterpreter,const std::string&>(this,&SteelInterpreter::strlen));
    addFunction("is_array",new SteelFunctor1Arg<SteelInterpreter,const SteelType&>(this,&SteelInterpreter::is_array));
    addFunction("is_handle",new SteelFunctor1Arg<SteelInterpreter,const SteelType&>(this,&SteelInterpreter::is_handle));
    addFunction("is_function",new SteelFunctor1Arg<SteelInterpreter,const SteelType&>(this,&SteelInterpreter::is_function));    
    addFunction("is_valid",new SteelFunctor1Arg<SteelInterpreter,const SteelType&>(this,&SteelInterpreter::is_valid));
    addFunction("array",new SteelFunctorArray<SteelInterpreter>(this,&SteelInterpreter::array));
    addFunction("hash",new SteelFunctorNoArgs<SteelInterpreter>(this,&SteelInterpreter::hash));



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
    addFunction("rad2deg","math", new SteelFunctor1Arg<SteelInterpreter,double>(this,&SteelInterpreter::rad2deg));
    addFunction("deg2rad","math", new SteelFunctor1Arg<SteelInterpreter,double>(this,&SteelInterpreter::deg2rad));
    
    
    addFunction("read","io", new SteelFunctor1Arg<SteelInterpreter,SteelType::Handle>(this,&SteelInterpreter::read));
    addFunction("close","io", new SteelFunctor1Arg<SteelInterpreter,SteelType::Handle>(this,&SteelInterpreter::close));
    addFunction("write","io", new SteelFunctor2Arg<SteelInterpreter,SteelType::Handle,const std::string&>(this,&SteelInterpreter::write));
    addFunction("open","io", new SteelFunctor2Arg<SteelInterpreter,const std::string&,const std::string&>(this,&SteelInterpreter::open));
    
    SteelType pi;
    pi.set( 4.0 * std::atan(1.0) );
    declare_const("$_PI",pi);
    
    SteelType io_in;
    FileInHandle* io_in_handle = new FileInHandle();
    io_in_handle->SetStream(&std::cin);
    io_in.set(io_in_handle);
    declare_const("$_IN",io_in);
    SteelType io_out;
    FileOutHandle* io_out_handle = new FileOutHandle();
    io_out_handle->SetStream(&std::cout);
    io_out.set(io_out_handle);
    declare_const("$_OUT",io_out);
    
    m_file_handles.insert(io_in_handle);
    m_file_handles.insert(io_out_handle);
}

SteelType SteelInterpreter::require(const std::string &filename)
{
    if(m_requires.find(filename) == m_requires.end())
    {
        IFile * file = m_file_provider->create();
        if(!file->open(filename.c_str()))
        {
            throw FileNotFound();
        }
	
        std::ostringstream strstream;
        while(!file->eof())
        {
            char buffer[1024];
            memset(buffer,0,1024);
            int r = file->read(buffer,1024);
            strstream.write(buffer,r);
        }
        file->close();
        delete file;
	
        m_requires.insert(filename);
		
        SteelParser parser;
        parser.setFileProvider(m_file_provider);
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

        delete pScript; // Right? We ran the code, so we're okay there... and any lingering functions we want will survive this.
    }
    return SteelType();
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

SteelType SteelInterpreter::len(const SteelType &array)
{
    SteelType val;

    val.set(array.getArraySize());

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

SteelType SteelInterpreter::hash()
{
    SteelType hash;
    hash.set(SteelType::Map());

    return hash;
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

SteelType SteelInterpreter::is_function(const SteelType &handle)
{
    SteelType var;
    var.set ( handle.isFunctor() );

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

SteelType SteelInterpreter::deg2rad(double deg)
{
    SteelType var;
    var.set(deg * (M_PI/180.0));
    return var;
}

SteelType SteelInterpreter::rad2deg(double rad)
{
    SteelType var;
    var.set(rad * (180.0 / M_PI));
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




SteelType SteelInterpreter::is_good(SteelType::Handle file)
{
    FileHandle* hstream = dynamic_cast<FileHandle*>(file);
    if(hstream == NULL) throw TypeMismatch();
    std::ios* stream = hstream->GetIos();
    
    SteelType var;
    var.set ( stream->good() );
    return var;
}

SteelType SteelInterpreter::is_eof(SteelType::Handle file)
{
    FileHandle* hstream = dynamic_cast<FileHandle*>(file);
    if(hstream == NULL) throw TypeMismatch();
    std::ios* stream = hstream->GetIos();
    SteelType var;
    var.set ( stream->eof() );
    return var;
}

SteelType SteelInterpreter::read(SteelType::Handle file)
{
    FileInHandle* hstream = dynamic_cast<FileInHandle*>(file);
    if(hstream == NULL) throw TypeMismatch();
    std::istream * stream = hstream->GetStream();
    std::string str;
    SteelType var;
    std::getline(*stream,str);
    var.set(str);
    return var;
}

SteelType SteelInterpreter::write(SteelType::Handle file, const std::string &string)
{

    FileOutHandle* hstream = dynamic_cast<FileOutHandle*>(file);
    if(hstream == NULL) throw TypeMismatch();
    std::ostream * stream = hstream->GetStream();
    std::string str;
    SteelType var;
    *stream << string;
    var.set(string);
    return var;
}


SteelType SteelInterpreter::open(const std::string& filename, const std::string& mode)
{
    SteelType var;
    FileHandle * handle;
    bool write = false;
    bool append = false;
    for(int i=0;i<mode.length();i++){
        if(mode[i] == 'w') write = true;
        else if(mode[i] == '+') append = true;
    }
    
    if(write){
        FileOutHandle * _handle = new FileOutHandle();

        std::ios_base::openmode mode = std::ostream::out;
	
        if(append)
            mode |= std::ostream::app;
	
        std::ostream * stream = new std::ofstream(filename.c_str(),mode);
        _handle->SetStream(stream);
        handle = _handle;
    }else{
        FileInHandle * _handle = new FileInHandle();

        std::ios::openmode mode = std::istream::in;
	
        std::istream * stream = new std::ifstream(filename.c_str(),mode);
        _handle->SetStream(stream);
        handle = _handle;
    }
    m_file_handles.insert(handle);
    var.set(handle);
    return var;    
}

SteelType SteelInterpreter::close(SteelType::Handle file)
{
    FileInHandle * inhandle = dynamic_cast<FileInHandle*>(file);
    FileOutHandle * outhandle = dynamic_cast<FileOutHandle*>(file);
    
    if(inhandle)
    {
        std::ifstream * stream = dynamic_cast<std::ifstream*>(inhandle->GetStream());
	
        if(stream){
            stream->close();
        }
        // otherwise its probably some other kind of istream, and doesnt need closing
    }
    else if(outhandle)
    {
        std::ofstream * stream = dynamic_cast<std::ofstream*>(outhandle->GetStream());
	
        if(stream){
            stream->close();
        }
    }
    else
    {
        throw TypeMismatch();
    }
    
    return SteelType();
}


SteelInterpreter& SteelInterpreter::operator= ( const SteelInterpreter& other )
{
    if(&other == this) return *this;

    m_symbols = other.m_symbols;
    m_namespace_scope = other.m_namespace_scope;
    m_requires = other.m_requires;
    
    /* Intentionally not copying return stack or context count
     * 
     */
    return *this;
}






