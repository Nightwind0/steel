#ifndef SR_STEEL_EXCEPTION_H
#define SR_STEEL_EXCEPTION_H

#include <string>

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
        FUNCTION_DEFINED
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

class UnknownIdentifier
{
};

class OutOfBounds
{
};

class AlreadyDefined
{
};

#endif


