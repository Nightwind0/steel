#ifndef SR_STEEL_EXCEPTION_H
#define SR_STEEL_EXCEPTION_H

#include <string>
namespace Steel { 

class SteelException
{
public:
    enum eType
    {
        SCANNING,
        PARSING,
        RUNTIME,
        OUT_OF_BOUNDS,
        UNKNOWN_IDENTIFIER,
        INVALID_LVALUE,
        TYPE_MISMATCH,
        PARAM_MISMATCH,
        VARIABLE_DEFINED,
        FUNCTION_DEFINED,
        DEFAULTS_MISMATCH,
        ASSIGNMENT_REQUIRED,
        VALUE_IS_CONSTANT,
	FILE_NOT_FOUND
    };


    SteelException(eType type, 
                   int line,
                   std::string script,
                   std::string message);
    virtual ~SteelException();

    eType getType() const;
    int getLine() const;
    std::string getScript() const;
    std::string getMessage() const;
    
private:
    eType m_eType;
    int m_line;
    std::string m_script;
    std::string m_message;
};


class FileNotFound
{
};

class OperationMismatch
{
public:
    OperationMismatch(){}
    virtual ~OperationMismatch(){}
private:
};


class ParamMismatch
{
};

class TypeMismatch
{
};

struct UnknownIdentifier
{
  UnknownIdentifier(const std::string id)
  {
    identifier = id;
  }
  std::string identifier;
};

class OutOfBounds
{
};

class AlreadyDefined
{
public:
    AlreadyDefined(const std::string& name):m_name(name){
    }
    std::string GetName() const { return m_name; }
private:
    std::string m_name;
};

class ConstViolation
{
};

class DivideByZero
{
};

}
#endif


