#ifndef SR_STEEL_INTERPRETER_H
#define SR_STEEL_INTERPRETER_H

#include <map>
#include <string>
#include <list>
#include <vector>
#include "SteelParser.h"
#include "SteelFunctor.h"

// fwd
class AstScript;
class AstStatementList; 
class AstParamDefinitionList;


class ParameterListItem
{
public:
    ParameterListItem(const std::string &name, double d);
    ParameterListItem(const std::string &name, int i);
    ParameterListItem(const std::string &name, bool b);
    ParameterListItem(const std::string &name, std::string &s);

    std::string getName() const { return m_name; }
    SteelType getValue() const { return m_value; }
private:
    std::string m_name;
    SteelType m_value;
};

typedef std::vector<ParameterListItem> ParameterList;

class SteelInterpreter
{
public:
    SteelInterpreter();
    virtual ~SteelInterpreter();

    void addFunction(const std::string &name, SteelFunctor *pFunc);
    SteelType call(const std::string &name, const std::vector<SteelType> &pList);

    // This allows you to pre-parse a script and keep a pointer to it
    // around, which can be run over and over, and the deletion of it is
    // up to the user of SteelInterpreter
    AstScript * prebuildAst(const std::string &script_name,
                            const std::string &script);

    // After using prebuildAst, you can later run it using runAst
    SteelType runAst(AstScript *pAst );
    // This allows you to supply some defined global level variables to your 
    // script.
    SteelType runAst(AstScript *pAst, const ParameterList &params);
    SteelType run(const std::string &name,const std::string &script);

    SteelType lookup(const std::string &name);
    // Array element lookup
    SteelType lookup(SteelType *pVar, int index);
    SteelType *lookup_lvalue(const std::string &name);
    void declare(const std::string &name);
    void declare_array(const std::string &array, int size);
    void declare_const(const std::string &name, const SteelType &datum);
    template<class T>
    void declare_const(const std::string &name, const T &datum);
    void assign(SteelType *pVar,const SteelType &value);

    void pushScope();
    void popScope();
    void registerFunction(const std::string &name, 
                          AstParamDefinitionList *pParams, 
                          AstStatementList *pStatements,
                          bool final);
    void setReturn(const SteelType &var);
    SteelType getReturn() const;

private:

    std::string name_array_ref(const std::string &array_name); 
    typedef std::map<std::string, SteelType> VariableFile;
    std::list<VariableFile> m_symbols;
  

    SteelType * lookup_internal(const std::string &name);
  
    void registerBifs();
    std::map<std::string,SteelFunctor*> m_functions;

    SteelType m_return;

private:
    // Bifs
    SteelType print   (const std::string &str);
    SteelType println (const std::string &str);
    SteelType len     (const SteelArray &ref);
    SteelType real    (const SteelType &str);
    SteelType integer (const SteelType &str);
    SteelType boolean (const SteelType &str);
    SteelType substr  (const std::string &str, int start, int len);
    SteelType strlen  (const std::string &str);
    SteelType is_array(const SteelType &);

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
};


#endif


