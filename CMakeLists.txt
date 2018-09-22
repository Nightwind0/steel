cmake_minimum_required(VERSION 3.0) # I had problems if I only specified "VERSION 3".
project(steel CXX)


add_definitions(-DVERSION="0.5")

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS                "-Wall -fstack-protector")
set(CMAKE_CONFIGURATION_TYPES "Release;Debug" CACHE STRING "" FORCE)


# Options to correctly link the standard C++ lib on Mac.
if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin") # This is the correct way to detect Mac OS X operating system -- see http://www.openguru.com/2009/04/cmake-detecting-platformoperating.html
    set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libc++")
    if(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang") # GCC ("GNU") probably would require a different option
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    endif()
endif()



set(libsteel_HEADERS
  src/Ast.h
  src/ByteCode.h
  src/Compilation.h
  src/CompilationTypes.h
  src/File.h
  src/Functor.h
  src/Mutex.h
  src/SteelException.h
  src/SteelFunctor.h
  src/SteelInterpreter.h
  src/SteelParser.h
  src/SteelScanner.h
  src/SteelType.h
  )

set(libsteel_SOURCES
 src/Ast.cpp
 src/ByteCode.cpp
 src/Compilation.cpp
 src/File.cpp
 src/Mutex.cpp
 src/SteelException.cpp
 src/SteelFunctor.cpp
 src/SteelInterpreter.cpp
 src/SteelParser.cpp
 src/SteelScanner.cpp
 src/SteelType.cpp
 )


add_library(steel ${libsteel_HEADERS} ${libsteel_SOURCES})
install(TARGETS steel DESTINATION lib)
install(FILES ${libsteel_HEADERS} DESTINATION include/steel)
add_executable(steeli src/steel.cpp)

target_link_libraries(steeli steel)

set_property(TARGET steel PROPERTY CXX_STANDARD 14)
set_property(TARGET steeli PROPERTY CXX_STANDARD 14)


target_include_directories(
    steel PUBLIC
    src
)
