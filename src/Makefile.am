noinst_LTLIBRARIES = libsteel.la
noinst_PROGRAMS = steel

libsteel_la_SOURCES = SteelParser.cpp \
			SteelParser.h \
			SteelScanner.cpp \
			SteelScanner.h \
			SteelFunctor.h \
			SteelFunctor.cpp \
			SteelInterpreter.h \
			SteelInterpreter.cpp \
			SteelException.h \
			SteelException.cpp \
			Ast.cpp \
			Ast.h \
			SteelType.h \
			SteelType.cpp \
			steel.trison \
			steel.reflex

SteelParser.cpp : steel.trison
	trison -S steel.states -o SteelParser -Hh steel.trison

SteelScanner.cpp : steel.reflex SteelParser.cpp
	reflex  steel.reflex

func_test : SteelFunctor.h func_test.cpp SteelFunctor.cpp
	g++ -g func_test.cpp SteelFunctor.cpp -o func_test


steel_SOURCES = steel.cpp
steel_LDADD = libsteel.la

test_parser_SOURCES = test_parser.cpp
test_parser_LDADD = libsteel.la
