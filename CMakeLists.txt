cmake_minimum_required(VERSION 3.10) # I had problems if I only specified "VERSION 3".
project(steel CXX)

# This defines rules for adding reflex and trison sources to produce C++ scanners and parsers.
# It requires having installed the barf package; the find_package function looks for the file
# barf-config.cmake in various locations to determine barf_DIR such that
# ${barf_DIR}/barf-config.cmake is a valid path, and then includes that cmake script.
find_package(barf REQUIRED)

# This sets the DOXYGEN_EXECUTABLE and DOXYGEN_DOT_EXECUTABLE vars.
find_package(Doxygen REQUIRED dot)

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

reflex_add_source(${PROJECT_SOURCE_DIR}/src/SteelScanner.reflex SteelScanner)
trison_add_source(${PROJECT_SOURCE_DIR}/src/SteelParser.trison SteelParser)

set(libsteel_HEADERS
  src/Ast.h
  src/File.h
  src/Functor.h
  src/Mutex.h
  src/SteelException.h
  src/SteelFunctor.h
  src/SteelInterpreter.h
  src/SteelParser.hpp
  src/SteelScanner.hpp
  src/SteelType.h
  )

set(libsteel_SOURCES
 src/Ast.cpp
 src/File.cpp
 src/Mutex.cpp
 src/SteelException.cpp
 src/SteelFunctor.cpp
 src/SteelInterpreter.cpp
 src/SteelParser.cpp
 src/SteelParser.trison
 src/SteelScanner.cpp
 src/SteelScanner.reflex
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


add_custom_target(test-all-scripts COMMAND find ${PROJECT_SOURCE_DIR}/test_scripts -name "*.steel" -exec ${PROJECT_SOURCE_DIR}/test.sh {} "\;")
add_custom_target(test-all-scripts-with-valgrind COMMAND find ${PROJECT_SOURCE_DIR}/test_scripts -name "*.steel" -exec ${PROJECT_SOURCE_DIR}/test-with-valgrind.sh {} "\;")
