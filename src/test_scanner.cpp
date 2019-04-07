#include <iostream>
#include "SteelScanner.hpp"

int main()
{
    SteelScanner scanner;

    int *thing = NULL;
    
    while(scanner.Scan((void**)&thing) != SteelParser::Token::END_) 
    std::cout << std::endl;
}


