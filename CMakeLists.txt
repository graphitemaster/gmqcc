cmake_minimum_required(VERSION 2.8)
project(gmqcc)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")

set(SOURCE_FILES
    algo.h
    ast.cpp ast.h
    code.cpp
    conout.cpp
    fold.cpp fold.h
    ftepp.cpp
    gmqcc.h
    intrin.cpp intrin.h
    ir.cpp
    ir.h
    lexer.cpp lexer.h
    opts.cpp
    parser.cpp parser.h
    stat.cpp
    utf8.cpp
    util.cpp)
add_library(gmqcclib ${SOURCE_FILES})

add_executable(gmqcc main.cpp)
target_link_libraries(gmqcc gmqcclib)

add_executable(testsuite test.cpp)
target_link_libraries(testsuite gmqcclib)

add_executable(qcvm exec.cpp)
target_link_libraries(qcvm gmqcclib)
