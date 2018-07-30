cmake_minimum_required(VERSION 3.8)
project(xrgears)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/")

function(add_cxxflag FLAG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FLAG}" PARENT_SCOPE)
endfunction(add_cxxflag)

add_definitions(-DVK_USE_PLATFORM_WAYLAND_KHR)
add_definitions(-DVK_USE_PLATFORM_XCB_KHR)

add_cxxflag("-std=c++14")

# warnings
# add_cxxflag("-Werror")

add_cxxflag("-Wall")
add_cxxflag("-Wcast-qual")
add_cxxflag("-Wformat=2")
add_cxxflag("-Winline")
add_cxxflag("-Wlogical-op")
add_cxxflag("-Wmissing-declarations")
add_cxxflag("-Wno-cast-qual")
add_cxxflag("-Wno-delete-non-virtual-dtor")
add_cxxflag("-Wno-sign-compare")
add_cxxflag("-Wno-switch")
add_cxxflag("-Wno-unused-parameter")
add_cxxflag("-Woverflow")
add_cxxflag("-Woverloaded-virtual")
add_cxxflag("-Wpointer-arith")
add_cxxflag("-Wredundant-decls")
add_cxxflag("-Wsuggest-attribute=const")
add_cxxflag("-Wundef")
add_cxxflag("-Wuninitialized")
add_cxxflag("-Wunreachable-code")

# not fixed
#add_cxxflag("-Wconversion")
#add_cxxflag("-Weffc++")
#add_cxxflag("-Wextra")
#add_cxxflag("-Wold-style-cast")
#add_cxxflag("-Wsign-conversion")

# gcc coverage stats
#add_cxxflag("--coverage")

find_package(PkgConfig REQUIRED)

pkg_search_module(VULKAN REQUIRED vulkan)
pkg_search_module(OPENHMD REQUIRED openhmd)
pkg_search_module(ASSIMP REQUIRED assimp)
pkg_search_module(XCB REQUIRED xcb)
pkg_search_module(XCB_KEY REQUIRED xcb-keysyms)
pkg_search_module(XCB_RANDR REQUIRED xcb-randr)
pkg_search_module(WAYLAND REQUIRED wayland-client)
pkg_search_module(GBM REQUIRED gbm)
pkg_search_module(DRM REQUIRED libdrm)
pkg_search_module(WAYLAND_SCANNER REQUIRED wayland-scanner)
pkg_search_module(WAYLAND_PROTOCOLS REQUIRED wayland-protocols)

pkg_search_module(X11 REQUIRED x11)

file(GLOB VITAMINK_SRC vitamin-k/*.hpp)

pkg_get_variable(WAYLAND_PROTOCOLS_DATADIR wayland-protocols pkgdatadir)
pkg_get_variable(WAYLAND_SCANNER wayland-scanner wayland_scanner)
message("WAYLAND_SCANNER found: ${WAYLAND_SCANNER}")
message("WAYLAND_PROTOCOLS_DATADIR: ${WAYLAND_PROTOCOLS_DATADIR}")

include_directories(
    ${CMAKE_SOURCE_DIR}/vitamin-k
    ${CMAKE_SOURCE_DIR}/wayland-protocols
    ${CMAKE_SOURCE_DIR}/fonts
    ${OPENHMD_INCLUDE_DIRS}
    ${DRM_INCLUDE_DIRS})

set(SHADER_DIR ${CMAKE_SOURCE_DIR}/shaders)

file(GLOB_RECURSE SHADER_GLOB
    ${SHADER_DIR}/*.vert
    ${SHADER_DIR}/*.frag
    ${SHADER_DIR}/*.geom)

# build shaders
foreach(SHADER ${SHADER_GLOB})
    message("Compiling SPV shader for ${SHADER}")
    execute_process(
	WORKING_DIRECTORY ${SHADER_DIR}
	COMMAND glslc -o ${SHADER}.spv ${SHADER})
endforeach(SHADER)

# generate wayland protocols
set(WAYLAND_PROTOCOLS_DIR ${CMAKE_SOURCE_DIR}/wayland-protocols/)
file(MAKE_DIRECTORY ${WAYLAND_PROTOCOLS_DIR})

set(PROTOCOL xdg-shell-unstable-v6)
set(PROTOCOL_XML
    ${WAYLAND_PROTOCOLS_DATADIR}/unstable/xdg-shell/${PROTOCOL}.xml)

execute_process(COMMAND
    ${WAYLAND_SCANNER}
    code
    ${PROTOCOL_XML}
    ${WAYLAND_PROTOCOLS_DIR}/${PROTOCOL}.c)
execute_process(COMMAND
    ${WAYLAND_SCANNER}
    client-header
    ${PROTOCOL_XML}
    ${WAYLAND_PROTOCOLS_DIR}/${PROTOCOL}.h)

file(GLOB WAYLAND_PROTOCOLS_SRC ${WAYLAND_PROTOCOLS_DIR}/*.c)
add_library(wayland-protocols ${WAYLAND_PROTOCOLS_SRC})

file(GLOB_RECURSE VITAMINK_SRC vitamin-k/*.hpp)
add_library(vitamin-k ${VITAMINK_SRC})
SET_TARGET_PROPERTIES(vitamin-k  PROPERTIES LINKER_LANGUAGE CXX)
set(LINT_SOURCES ${VITAMINK_SRC})

set(EXAMPLE_LIBS
    ${VULKAN_LIBRARIES}
    ${OPENHMD_LIBRARIES}
    ${ASSIMP_LIBRARIES}
    ${XCB_LIBRARIES}
    ${XCB_KEY_LIBRARIES}
    ${WAYLAND_LIBRARIES}
    wayland-protocols
    ${GBM_LIBRARIES}
    ${XCB_RANDR_LIBRARIES}
    ${X11_LIBRARIES}
    X11
    ${DRM_LIBRARIES})

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
    buildExample(${EXAMPLE})
endforeach(EXAMPLE)

set(IGNORE
    "-build/c++11,"
    "-build/header_guard,"
    "-readability/casting,"
    "-whitespace/line_length,"
    "-whitespace/parens")

add_custom_target(style COMMAND
    python2 ${CMAKE_SOURCE_DIR}/scripts/cpplint.py --headers=hpp --filter="${IGNORE}" ${LINT_SOURCES})
