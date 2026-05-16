cmake_minimum_required(VERSION 3.28)
set(CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS clang-scan-deps-18)

project(app 
    LANGUAGES CXX  
)

set(CMAKE_CXX_COMPILER clang++)

add_executable(app
    main.cpp
    scanner/lexer.cpp
    scanner/parser.cpp
    errors/error.cpp
    analysis/resolve.cpp
)

target_include_directories(app 
    PRIVATE include
    PRIVATE errors
    PRIVATE syntax

)

find_package(LLVM REQUIRED CONFIG)

target_link_libraries(app ${llvm_libs})

target_compile_features(app PRIVATE
    cxx_std_23
)

target_compile_options(app PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -g

)

#target_sources(app PRIVATE
#    FILE_SET CXX_MODULES FILES
#        main.cpp
#)