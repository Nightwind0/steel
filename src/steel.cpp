#include <fstream>
#include <iostream>
#include <sstream>
#include "SteelInterpreter.h"
#include "SteelException.h"
#include "SteelType.h"

using Steel::SteelInterpreter;
using Steel::AstScript;
using Steel::SteelException;

void usage(){
  std::cout << "steel [options] <filename>" << std::endl << 
    "Options:" << std::endl <<
    "-p,--print: ask the script to print itself out after being built" << std::endl <<
    "-n,--no-exec: don't execute the script, just build it" << std::endl <<
    "-d,--debug: show scanner and parser debug spew" << std::endl << 
    "--no-scan-debug: use with -d, show only parser debug spew" << std::endl <<
    "-t: enable thread safety (for testing purposes)" << std::endl <<
    "-v,--version: show version of steel interpreter" << std::endl;
}


bool read_script(std::istream& stream,std::string& into)
{
  into.assign( (std::istreambuf_iterator<char>(stream) ),
                       (std::istreambuf_iterator<char>()    ) );
}

int main(int argc, char * argv[])
{
    std::string script;
    SteelInterpreter interpreter;


    bool bPrint=false;
    bool bRun=true;
    bool bDebug = false;
    bool bScanDebug = false;
    bool bCin = false;
    bool bEnableThreadSafety=false;
    bool bShowVersion=false;
    std::string filename;

    for(int i=1;i<argc;i++)
    {
        std::string arg = argv[i];

        if(arg == "--print" ||
           arg == "-p")
        {
            bPrint = true;
        }
	else if(arg == "--no-scan-debug")
	{
	  bScanDebug = false;
	}
        else if(arg == "--version" || arg == "-v"){
          bShowVersion = true;
        }else if(arg == "--no-exec" ||
                arg == "-n")
        {
            bRun = false;
        }else if(arg == "--debug" ||
                 arg == "-d")
        {
            bDebug = true;
	    bScanDebug = true;
        }else if(arg == "-" && i == (argc-1)){
	  bCin = true;
	}else if(arg == "-t"){
	  bEnableThreadSafety = true;
	}else {
	  if(!filename.size()){
	    filename = arg;
	  }else{
	    usage();
	    return -1;
	  }
	}
    }

    if(!bCin && !filename.size()) {
      usage();
      return -1;
    }

    if(bShowVersion){
      std::cout << "steel version " << SteelInterpreter::getVersion() << std::endl;
      return 0;
    }


    if(bCin){
      filename =  "<STDIN>";
      read_script(std::cin,script);
    }else{
      std::ifstream file(filename.c_str());
      if(file){
	read_script(file,script);
	file.close();
      }else{
	std::cerr<< "Couldn't open file: " << filename << std::endl;
	return -1;
      }
    }


    if(bEnableThreadSafety){
      interpreter.enableThreadSafety();
    }else{
      interpreter.disableThreadSafety();
    }



    try{
      AstScript * pScript = interpreter.prebuildAst(filename,script,bDebug,bScanDebug);
        if(bPrint)
            pScript->print(std::cout);

        if(bRun)
            interpreter.runAst(pScript);
    }
    catch(SteelException ex)
    {
        if(ex.getType() != SteelException::PARSING)
            std::cerr << ex.getLine() << ':';
        
        std::cerr << ex.getMessage() << std::endl;
    }

}


