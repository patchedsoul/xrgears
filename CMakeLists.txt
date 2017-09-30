
cmake_minimum_required(VERSION 3.8)
project(xrgears)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/")

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

add_cxxflag("-std=c++14")

add_definitions(-DVK_USE_PLATFORM_WAYLAND_KHR)

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

add_cxxflag("-fPIC")

# find_package(XCB REQUIRED)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_USE_PLATFORM_XCB_KHR")

file(GLOB VITAMINK_SRC vitamin-k/*.hpp)
file(GLOB XDG_SRC xdg-shell/*.c)

add_library(xdg-shell ${XDG_SRC})

include_directories(
    vitamin-k
    xdg-shell
    /usr/include/openhmd
    /usr/include/libdrm/
)

# glslc -o gears.vert.spv gears.vert

file(GLOB_RECURSE SHADER_GLOB
    ${CMAKE_SOURCE_DIR}/data/shaders/*.vert
    ${CMAKE_SOURCE_DIR}/data/shaders/*.frag
    ${CMAKE_SOURCE_DIR}/data/shaders/*.geom)

# build shaders
foreach(SHADER ${SHADER_GLOB})
    message("Compiling SPV shader for ${SHADER}")
    execute_process(
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/data/shaders
	COMMAND glslc -o ${SHADER}.spv ${SHADER}
	)
endforeach(SHADER)

file(GLOB_RECURSE VITAMINK_SRC vitamin-k/*.hpp)
add_library(vitamin-k ${VITAMINK_SRC})
SET_TARGET_PROPERTIES(vitamin-k  PROPERTIES LINKER_LANGUAGE CXX)
set(LINT_SOURCES ${VITAMINK_SRC})

set(EXAMPLE_LIBS
    ${Vulkan_LIBRARY}
    ${ASSIMP_LIBRARIES}
    ${WAYLAND_CLIENT_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${XCB_LIBRARIES}
    dl
    openhmd
    vulkan
    xcb
    xcb-keysyms
    assimp
    wayland-client
    gbm
    drm
    m
    stdc++
    xdg-shell)

# Function for building single example
function(buildExample EXAMPLE_NAME)
    file(GLOB EXAMPLE_SRC examples/${EXAMPLE_NAME}/*.cpp)
    add_executable(${EXAMPLE_NAME} ${EXAMPLE_SRC})
    set(LINT_SOURCES ${LINT_SOURCES} ${EXAMPLE_SRC} PARENT_SCOPE)
    target_link_libraries(${EXAMPLE_NAME} ${EXAMPLE_LIBS})
endfunction(buildExample)

# build examples
FILE(GLOB EXAMPLE_DIRS RELATIVE
    ${CMAKE_SOURCE_DIR}/examples
    ${CMAKE_SOURCE_DIR}/examples/*)

# Build all examples
foreach(EXAMPLE ${EXAMPLE_DIRS})
    if (EXAMPLE STREQUAL "glfw-test")
	message("skipping glfw test for now.")
    else()
	buildExample(${EXAMPLE})
    endif()
endforeach(EXAMPLE)

set(IGNORE "-build/header_guard,-readability/streams,-readability/casting,-whitespace/line_length,-runtime/arrays,-build/c++11,-whitespace/tab,-whitespace/parens")
#set(IGNORE "-build/header_guard,-build/c++11,-readability/casting")
add_custom_target(style
    COMMAND python2 ${CMAKE_SOURCE_DIR}/scripts/cpplint.py --headers=hpp --filter=${IGNORE} ${LINT_SOURCES}
    )
