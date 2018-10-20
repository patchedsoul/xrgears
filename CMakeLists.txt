cmake_minimum_required(VERSION 3.8)
project(xrgears)

# Yeah it's gross but it's to find the assets. You're welcome to make it better.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/")

function(add_cxxflag FLAG)
    string(APPEND CMAKE_CXX_FLAGS " ${FLAG}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" PARENT_SCOPE)
endfunction()

add_definitions(-DVK_USE_PLATFORM_WAYLAND_KHR)
add_definitions(-DVK_USE_PLATFORM_XCB_KHR)

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

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
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
pkg_search_module(GLM REQUIRED glm)

pkg_search_module(X11 REQUIRED x11)

file(GLOB VITAMINK_SRC vitamin-k/*.hpp)

pkg_get_variable(WAYLAND_PROTOCOLS_DATADIR wayland-protocols pkgdatadir)
pkg_get_variable(WAYLAND_SCANNER wayland-scanner wayland_scanner)
message("WAYLAND_SCANNER found: ${WAYLAND_SCANNER}")
message("WAYLAND_PROTOCOLS_DATADIR: ${WAYLAND_PROTOCOLS_DATADIR}")

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/vitamin-k
    ${CMAKE_CURRENT_SOURCE_DIR}/wayland-protocols
    ${CMAKE_CURRENT_SOURCE_DIR}/fonts
    ${OPENHMD_INCLUDE_DIRS}
    ${DRM_INCLUDE_DIRS})

include(CheckIncludeFileCXX)
find_file(VULKAN_INTEL_INCLUDE_FILE
    NAMES
    vulkan/vulkan_intel.h
    HINTS
    # Vulkan include dir?
    )
CHECK_INCLUDE_FILE_CXX(vulkan/vulkan_intel.h HAVE_VULKAN_INTEL)

find_path(GLI_INCLUDE_DIR
    NAMES
    gli/gli.hpp)
if(NOT GLI_INCLUDE_DIR)
    message(FATAL_ERROR "GLI not found. Get a version matching GLM from https://github.com/g-truc/gli")
endif()
include_directories(${GLI_INCLUDE_DIR})

if(VULKAN_INTEL_INCLUDE_FILE)
   add_definitions(-DHAVE_VULKAN_INTEL)
endif()

set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)

file(GLOB_RECURSE SHADER_GLOB
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    ${SHADER_DIR}/*.vert
    ${SHADER_DIR}/*.frag
    ${SHADER_DIR}/*.geom)

# build shaders

find_program(GLSLANG glslangValidator)

if(NOT GLSLANG)
  message(FATAL_ERROR "glslang was not found.")
endif()

set(CUSTOM_OUTPUTS)
foreach(SHADER ${SHADER_GLOB})
    set(OUTFILE "${CMAKE_CURRENT_SOURCE_DIR}/${SHADER}.spv")
    add_custom_command(OUTPUT "${OUTFILE}"
        COMMAND
        "${GLSLANG}" -V -o "${OUTFILE}" "${CMAKE_CURRENT_SOURCE_DIR}/${SHADER}"
        VERBATIM
        WORKING_DIRECTORY "${SHADER_DIR}"
        COMMENT "Compiling ${SHADER} with glslang")
    list(APPEND CUSTOM_OUTPUTS "${OUTFILE}")
endforeach()

# generate wayland protocols
set(WAYLAND_PROTOCOLS_DIR "${CMAKE_CURRENT_BINARY_DIR}/wayland-protocols/")
file(MAKE_DIRECTORY "${WAYLAND_PROTOCOLS_DIR}")

set(PROTOCOL xdg-shell-unstable-v6)
set(PROTOCOL_XML
    "${WAYLAND_PROTOCOLS_DATADIR}/unstable/xdg-shell/${PROTOCOL}.xml")

set(WAYLAND_PROTOCOLS_SRC)
set(OUTFILE "${WAYLAND_PROTOCOLS_DIR}/${PROTOCOL}.c")
list(APPEND WAYLAND_PROTOCOLS_SRC "${OUTFILE}")
add_custom_command(OUTPUT "${OUTFILE}"
    COMMAND
    ${WAYLAND_SCANNER}
    code
    "${PROTOCOL_XML}"
    "${OUTFILE}"
    VERBATIM
    COMMENT "Generating code file with wayland-scanner")


set(OUTFILE "${WAYLAND_PROTOCOLS_DIR}/${PROTOCOL}.h")
list(APPEND WAYLAND_PROTOCOLS_SRC "${OUTFILE}")
add_custom_command(OUTPUT "${OUTFILE}"
    COMMAND
    ${WAYLAND_SCANNER}
    client-header
    "${PROTOCOL_XML}"
    "${OUTFILE}"
    VERBATIM
    COMMENT "Generating client header file with wayland-scanner")

add_library(wayland-protocols ${WAYLAND_PROTOCOLS_SRC})
target_include_directories(wayland-protocols PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")

file(GLOB_RECURSE VITAMINK_SRC vitamin-k/*.hpp)
add_library(vitamin-k ${VITAMINK_SRC})
set_target_properties(vitamin-k  PROPERTIES LINKER_LANGUAGE CXX)
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
    add_executable(${EXAMPLE_NAME} ${EXAMPLE_SRC} ${CUSTOM_OUTPUTS})
    set(LINT_SOURCES ${LINT_SOURCES} ${EXAMPLE_SRC} PARENT_SCOPE)
    target_link_libraries(${EXAMPLE_NAME} ${EXAMPLE_LIBS})
    target_compile_features(${EXAMPLE_NAME} PRIVATE cxx_std_14)
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
