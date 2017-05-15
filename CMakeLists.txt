
cmake_minimum_required(VERSION 3.8)
project(xrgears)

# use XCB

function(add_cxxflag FLAG)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FLAG}" PARENT_SCOPE)
endfunction(add_cxxflag)

#add_cxxflag("-Wconversion")
#add_cxxflag("-Wsign-conversion")
#add_cxxflag("-Winline")
#add_cxxflag("-Weffc++")
#add_cxxflag("-Wsuggest-attribute=const")
#support ISO C++ standard (remove GNU extensions)
#add_cxxflag("-ansi")
#warnings for strict ISO C and ISO C++
#add_cxxflag("-pedantic")

function(add_verbose_warnings)
    add_cxxflag("-Werror")
    add_cxxflag("-Wall")
    add_cxxflag("-Wextra")
    add_cxxflag("-Wuninitialized")
    #add_cxxflag("-Wold-style-cast")
    add_cxxflag("-Woverloaded-virtual")
    add_cxxflag("-Wformat=2")
    add_cxxflag("-Wundef")
    add_cxxflag("-Wpointer-arith")
    add_cxxflag("-Wcast-qual")
    add_cxxflag("-Wlogical-op")
    add_cxxflag("-Woverflow")
    add_cxxflag("-Wmissing-declarations")
    add_cxxflag("-Wredundant-decls")
    add_cxxflag("-Wno-sign-compare")
    add_cxxflag("-Wno-unused-parameter")
    add_cxxflag("-Wno-delete-non-virtual-dtor")
    add_cxxflag("-Wno-cast-qual")
    add_cxxflag("-Wno-switch")
    add_cxxflag("-Wunreachable-code")
endfunction(add_verbose_warnings)

#    add_cxxflag("--coverage")


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
