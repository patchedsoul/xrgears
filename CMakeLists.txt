
cmake_minimum_required(VERSION 3.8)
project(xrgears)

# use XCB

# find_package(XCB REQUIRED)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_USE_PLATFORM_XCB_KHR")

set(XRGEARS_SRC
    ${CMAKE_SOURCE_DIR}/src/gears.cpp
    ${CMAKE_SOURCE_DIR}/src/vulkangear.cpp

    ${CMAKE_SOURCE_DIR}/src/vulkantools.cpp
    ${CMAKE_SOURCE_DIR}/src/vulkanexamplebase.cpp
    ${CMAKE_SOURCE_DIR}/src/vulkandebug.cpp
)

FILE(GLOB_RECURSE LibFiles "${CMAKE_SOURCE_DIR}/src/*.hpp")
add_custom_target(headers SOURCES ${LibFiles})

FILE(GLOB_RECURSE XRGEARS_SRC "${CMAKE_SOURCE_DIR}/src/*.cpp")
#include_directories("src")
add_executable(xrgears ${XRGEARS_SRC})

target_link_libraries(xrgears vulkan xcb assimp)
