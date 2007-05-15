#include <iostream>
//#include <ifstream>
#include "SteelParser.h"

int main()
{
    SteelParser parser;
    std::string script;

    while(!std::cin.eof())
    {
    script += std::cin.get();
    }

    parser.SetDebugSpewLevel(2);

//    parser.setBuffer( script.c_str() , "Magic Script");
    parser.setBuffer("playSound(\"Hello\");","Magic Script");
    if(parser.Parse() != SteelParser::PRC_SUCCESS)
    {
    std::cout << "Parse error unknown." << std::endl;
    return 1;
    }

    AstScript *pScript = static_cast<AstScript*>( parser.GetAcceptedToken() );

    std::cout << *pScript;

    return 0;
}


