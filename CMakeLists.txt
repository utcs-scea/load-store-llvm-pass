cmake_minimum_required(VERSION 3.13.4)
project(LLVMGlobalizePass)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(LLVM_DIR /opt/homebrew/Cellar/llvm/18.1.8/lib/cmake/llvm)
find_package(LLVM REQUIRED CONFIG)
add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})

include_directories(include)

add_library(LLVMGlobalizePass MODULE
  src/pass.cpp
)

set_target_properties(LLVMGlobalizePass PROPERTIES
  COMPILE_FLAGS "-fno-rtti"
  PREFIX ""
)

if(APPLE)
  set_target_properties(LLVMGlobalizePass PROPERTIES
    LINK_FLAGS "-undefined dynamic_lookup"
  )
endif(APPLE)

target_link_libraries(LLVMGlobalizePass
  PRIVATE
  ${LLVM_AVAILABLE_LIBS}
)
