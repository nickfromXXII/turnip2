cmake_minimum_required(VERSION 3.6)
project(turnip2)

find_package(LLVM REQUIRED CONFIG)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1z")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

set(SOURCE_FILES main.cpp lexer.cpp lexer.h parser.cpp parser.h utilities.h generator.cpp generator.h location.h)
add_executable(turnip2 ${SOURCE_FILES})

set(LIBS
        ${PLATFORM_LIBS}

        #LLVMX86Disassembler
        LLVMX86AsmParser
        #LLVMX86AsmPrinter
        LLVMX86CodeGen

        #LLVMSelectionDAG

        #LLVMAsmPrinter
        #LLVMMCParser
        #LLVMX86Info

        #LLVMJIT
        #LLVMExecutionEngine

        #LLVMCodeGen
        #LLVMScalarOpts
        #LLVMTransformUtils

        #LLVMAnalysis
        #LLVMTarget
        #LLVMMC

        #LLVMCore
        #LLVMSupport
        )

target_link_libraries (turnip2 ${LIBS})
