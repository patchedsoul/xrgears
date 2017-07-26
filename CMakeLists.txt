
cmake_minimum_required(VERSION 3.8)
project(xrgears CXX)

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

file(GLOB VKS_SRC vks/*.cpp vks/*.h vks/*.hpp)
file(GLOB VITAMINK_SRC vitamin-k/*.hpp)

add_library(vks ${VKS_SRC})

add_library(vitamin-k ${VITAMINK_SRC})

SET_TARGET_PROPERTIES(vitamin-k  PROPERTIES LINKER_LANGUAGE CXX)

include_directories(vks)
include_directories(vitamin-k)

target_link_libraries(vks vulkan xcb assimp wayland-client)

INCLUDE_DIRECTORIES(/usr/include/openhmd/)

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

set(LINT_SOURCES ${VITAMINK_SRC} ${VKS_SRC})

# Function for building single example
function(buildExample EXAMPLE_NAME)
	file(GLOB EXAMPLE_SRC examples/${EXAMPLE_NAME}/*.cpp)
        add_executable(${EXAMPLE_NAME} ${EXAMPLE_SRC})

	set(LINT_SOURCES ${LINT_SOURCES} ${EXAMPLE_SRC})

        target_link_libraries(${EXAMPLE_NAME}
          ${Vulkan_LIBRARY} ${ASSIMP_LIBRARIES} ${WAYLAND_CLIENT_LIBRARIES}
	  ${CMAKE_THREAD_LIBS_INIT} ${XCB_LIBRARIES} dl glfw openhmd
	  vks)
endfunction(buildExample)

# build examples
FILE(GLOB EXAMPLE_DIRS RELATIVE
  ${CMAKE_SOURCE_DIR}/examples
  ${CMAKE_SOURCE_DIR}/examples/*)

# Build all examples
foreach(EXAMPLE ${EXAMPLE_DIRS})
	buildExample(${EXAMPLE})
endforeach(EXAMPLE)

#add_custom_target(shaders
#        #clean and compile with 1 thread per core
#	COMMAND glslc -o gears.vert.spv gears.vert
#)


#set(IGNORE "-build/header_guard,-readability/streams,-readability/casting,-whitespace/line_length")
set(IGNORE "+legal/copyright,-whitespace,-build/header_guard,-build/c++11,-readability/casting")
add_custom_target(style
    COMMAND python2 ${CMAKE_SOURCE_DIR}/scripts/cpplint.py --headers=hpp --filter=${IGNORE} ${LINT_SOURCES}
)
