cmake_minimum_required(VERSION 3.15)
cmake_policy(SET CMP0091 NEW)
cmake_policy(SET CMP0042 NEW)

project(grid-solver-native CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W3)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -fPIC)
endif()

include_directories(${CMAKE_JS_INC})
include_directories(${CMAKE_SOURCE_DIR}/include)

file(GLOB SOURCE_FILES
    "src/*.cpp"
    "src/core/*.cpp"
    "src/solver/*.cpp"
    "src/binding/*.cpp"
)

file(GLOB HEADER_FILES
    "include/*.h"
    "include/core/*.h"
    "include/solver/*.h"
    "include/binding/*.h"
)

add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES} ${HEADER_FILES} ${CMAKE_JS_SRC})

set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" SUFFIX ".node")
target_link_libraries(${PROJECT_NAME} ${CMAKE_JS_LIB})

if(MSVC AND CMAKE_JS_NODELIB_DEF AND CMAKE_JS_NODELIB_TARGET)
    execute_process(COMMAND ${CMAKE_NODELIB_PATH} CMAKE_JS_NODELIB_DEF=${CMAKE_JS_NODELIB_DEF} CMAKE_JS_NODELIB_TARGET=${CMAKE_JS_NODELIB_TARGET} CMAKE_GENERATOR=${CMAKE_GENERATOR})
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/node_modules/node-addon-api
)
