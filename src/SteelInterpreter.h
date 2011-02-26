#ifndef SR_STEEL_INTERPRETER_H
#define SR_STEEL_INTERPRETER_H

#include <map>
#include <string>
#include <list>
#include <set>
#include <vector>
#include <fstream>
#include <memory>
#include <tr1/memory>
#include "SteelParser.h"
#include "SteelFunctor.h"

// fwd
class AstScript;
class AstStatementList; 
class AstParamDefinitionList;
class FileHandle;


class ParameterListItem
{
public:
    ParameterListItem(const std::string &name, double d);
    ParameterListItem(const std::string &name, int i);
    ParameterListItem(const std::string &name, bool b);
    ParameterListItem(const std::string &name, std::string &s);
    ParameterListItem(const std::string &name, SteelType::Handle);
    ParameterListItem(const std::string &name, const SteelType &var);

    std::string getName() const { return m_name; }
    SteelType getValue() const { return m_value; }
private:
    std::string m_name;
    SteelType m_value;
};

typedef std::vector<ParameterListItem> ParameterList;

using std::tr1::shared_ptr;

class SteelInterpreter
{
public:
    SteelInterpreter();
    virtual ~SteelInterpreter();

    // Add function to the global namespace
    void addFunction(const std::string &name, std::tr1::shared_ptr<SteelFunctor> pFunc);
    // Function will be deleted, do not pass stack variables
    void addFunction(const std::string &name, SteelFunctor* pFunc);

    // Add a function with a namespace
    void addFunction(const std::string &name, const std::string &ns, shared_ptr<SteelFunctor> pFunc);
    // Function will be deleted, do not pass stack variables
    void addFunction(const std::string &name, const std::string &ns, SteelFunctor* pFunc);

    // Removes a function and returns the pointer to it
    // (mostly so that you can deallocate it)
    // (but don't forget to check if it's an user function.. you don't
    // want to delete those.)
    shared_ptr<SteelFunctor> removeFunction(const std::string &name, const std::string &ns=kszGlobalNamespace);

    // Removes all functions with corresponding namespace
    void removeFunctions(const std::string &ns);

    // Call a method with parameters.
    // This method builds the Ast then executes it
    SteelType call(const std::string &name, const SteelType::Container &pList);

    // Call with a funciton in a specific namespace
    SteelType call(const std::string &name, const std::string &ns, const SteelType::Container &pList);
    // This allows you to pre-parse a script and keep a pointer to it
    // around, which can be run over and over, and the deletion of it is
    // up to the user of SteelInterpreter
    AstScript * prebuildAst(const std::string &script_name,
                            const std::string &script,
                            bool debugparser=false,
			    bool debugscanner=false);

    // After using prebuildAst, you can later run it using runAst
    SteelType runAst(AstScript *pAst);
    // This allows you to supply some defined global level variables to your 
    // script.
    SteelType runAst(AstScript *pAst, const ParameterList &params);
    SteelType run(const std::string &name,const std::string &script);

    SteelType lookup(const std::string &name);
    // Array element lookup
    SteelType lookup(SteelType *pVar, int index);
    SteelType *lookup_lvalue(const std::string &name);
    // Add namespace
    void import(const std::string &ns);
    void declare(const std::string &name);
    void declare_array(const std::string &array, int size);
    void declare_const(const std::string &name, const SteelType &datum);
    template<class T>
    void declare_const(const std::string &name, const T &datum);
    void assign(SteelType *pVar,const SteelType &value);

    void pushScope();
    void popScope();
    void registerFunction(const std::string &name,
                          const std::string &ns,
                          AstParamDefinitionList *pParams, 
                          AstStatementList *pStatements);
    void setReturn(const SteelType &var);
    SteelType getReturn() const;

    shared_ptr<SteelFunctor> lookup_functor(const std::string &name, const std::string &ns);
    shared_ptr<SteelFunctor> lookup_functor(const std::string &name);

    static const char * kszGlobalNamespace;
    static const char * kszUnspecifiedNamespace;

private:

    void push_context();
    void pop_context();
    
    std::string name_array_ref(const std::string &array_name); 
    typedef std::map<std::string, SteelType> VariableFile;
    typedef std::map<std::string,shared_ptr<SteelFunctor> > FunctionSet;
    std::list<VariableFile> m_symbols;

    void remove_user_functions();
    void clear_imports();
    void free_file_handles();
  
    SteelType * lookup_internal(const std::string &name);
  
    void registerBifs();
    std::deque<std::string> m_namespace_scope;
    std::map<std::string,FunctionSet> m_functions;
    std::set<std::string> m_requires;

    std::list<SteelType> m_return_stack;
    
    std::set<FileHandle*> m_file_handles;
    int m_nContextCount;
private:
    // Bifs
    SteelType require (const std::string &filename);
    SteelType print   (const std::string &str);
    SteelType println (const std::string &str);
    SteelType len     (const SteelArray &ref);
    SteelType real    (const SteelType &str);
    SteelType integer (const SteelType &str);
    SteelType boolean (const SteelType &str);
    SteelType string  (const SteelType& str);
    SteelType substr  (const std::string &str, int start, int len);
    SteelType strlen  (const std::string &str);
    SteelType is_array(const SteelType &);
    SteelType is_handle(const SteelType&);
    SteelType is_valid(const SteelType&);
    SteelType is_function(const SteelType&);
    SteelType array   (const SteelArray&);



    // Math built-ins
    SteelType ceil (double f);
    SteelType abs  (double f);
    SteelType floor(double f);
    SteelType exp  (double f);
    SteelType log  (double f); // Natural log
    SteelType log10(double f);
    SteelType sqrt (double f);
    SteelType acos (double f);
    SteelType asin (double f);
    SteelType atan (double f);
    SteelType atan2(double f, double g);
    SteelType cos  (double f);
    SteelType sin  (double f);
    SteelType tan  (double f);
    SteelType cosh (double f);
    SteelType sinh (double f);
    SteelType tanh (double f);
    SteelType round(double f);
    SteelType pow  (double f, double g);
    SteelType randf (); // Returns double between 0 and 1
    SteelType rand ( );
    SteelType srand ( int );
    
    // IO built-ins
    
    SteelType open(const std::string& filename, const std::string& mode);
    SteelType close(SteelType::Handle file);
    SteelType is_good(SteelType::Handle file);
    SteelType is_eof(SteelType::Handle file);
    SteelType read_integer(SteelType::Handle file);
    SteelType read(SteelType::Handle file);
    SteelType read_real(SteelType::Handle file);
    SteelType read_byte(SteelType::Handle file);
    SteelType read_bytes(SteelType::Handle file, int count);
    SteelType write_byte(SteelType::Handle file, int byte);
    SteelType write_bytes(SteelType::Handle file, const SteelArray& byte_array);
    SteelType write_integer(SteelType::Handle file, int integer);
    SteelType write(SteelType::Handle file, const std::string& string);
    SteelType write_real(SteelType::Handle file, double real);
    
};


#endif


