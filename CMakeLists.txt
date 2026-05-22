cmake_minimum_required(VERSION 3.28)
set(CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS clang-scan-deps-18)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(elio 
    LANGUAGES CXX  
)

set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_CXX_STANDARD 23)

add_executable(elio
    main.cpp
)

target_include_directories(elio 
    PRIVATE include
    PRIVATE errors
    PRIVATE syntax

)

find_package(LLVM REQUIRED CONFIG)

target_link_libraries(elio ${llvm_libs})

target_compile_features(elio PRIVATE
    cxx_std_23
)

target_compile_options(elio PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -g

)

target_sources(elio PUBLIC
    FILE_SET CXX_MODULES FILES
        include/tokens.cppm
        include/expr.cppm
        errors/error.cppm
        include/lexer.cppm
        include/parser.cppm
        syntax/resolve.cppm
        syntax/symbol.cppm
        syntax/typecheck.cppm
)