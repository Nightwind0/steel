#include "SteelException.h"

namespace Steel { 

SteelException::SteelException(eType type, 
                               int line,
                               std::string script,
                               std::string message)
    :m_eType(type),m_line(line),m_script(script),m_message(message)
{
}

SteelException::~SteelException()
{
}


SteelException::eType 
SteelException::getType() const
{
    return m_eType;
}

int SteelException::getLine() const
{
    return m_line;
}

std::string SteelException::getScript() const
{
    return m_script;
}

std::string SteelException::getMessage() const
{
    return m_message;
}


}
