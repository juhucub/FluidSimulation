#https://github.com/meemknight/cmakeSetup
#REFERENCE ^

#Ensures build system features up to date
cmake_minimum_required(VERSION 3.5)
project(FluidSimulation LANGUAGES C CXX)

if(APPLE)
    enable_language(OBJC)
    enable_language(OBJCXX)
endif()

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

#Ensures C++ version is standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#PRovent GFLW from building unecessary components
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

option(PRODUCTION_BUILD "MAKE BUILD" OFF)

#link time optimization
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE) 

add_subdirectory(external-dependencies/glfw-3.4)			#window oppener
add_subdirectory(external-dependencies/glad)				#opengl loader
add_subdirectory(external-dependencies/stb_image)			#loading immaged
add_subdirectory(external-dependencies/stb_truetype)		#loading ttf files
add_subdirectory(external-dependencies/glm)				#math
add_subdirectory(external-dependencies/imgui-docking)		#ui

#MY_SOURCES list of all source files .cpp for Simulation
#MACRO
file(GLOB_RECURSE MY_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
if(APPLE)
    set(MY_OBJCXX_SOURCES)
    file(GLOB_RECURSE MY_OBJCXX_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.mm")
endif()

#Locate OpenGL and GLFW libraries on system
find_package(OpenGL REQUIRED)

#####add_executable(FluidSimulation main.cpp)
add_executable("${CMAKE_PROJECT_NAME}" ${MY_SOURCES} ${MY_OBJCXX_SOURCES})

if(PRODUCTION_BUILD)
    #ASSETS_PATH macro to be in the root folder of exe
    target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC RESOURCES_PATH="./resources/")
    #remove option to debug asserts
    target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC PRODUCTION_BUILD=1) 
else()
    #get an ASSETS_PATH in IDE during development
    target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC RESOURCES_PATH="${CMAKE_CURRENT_SOURCE_DIR}/resources/")
	target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC PRODUCTION_BUILD=0) 
endif()

target_include_directories("${CMAKE_PROJECT_NAME}" PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include/" 
    "${CMAKE_CURRENT_SOURCE_DIR}/external-dependencies/imgui-docking/imgui"
    "${CMAKE_CURRENT_SOURCE_DIR}/external-dependencies/imgui-docking/imgui/backends"
    ${OPENGL_INCLUDE_DIRS}
)

#FIXME: THE ERROR MAKES IT SO THAT IT DOESNT RECOGNIZE IMGUI FILE, DROP IMGUI FILE INTO FILE IMGUI-DOCKING

target_link_libraries("${CMAKE_PROJECT_NAME}" PRIVATE
    OpenGL::GL
    glfw
    glad
    glm
    stb_image
    stb_truetype
    imgui
)

if(APPLE)
    find_library(METAL_FRAMEWORK Metal REQUIRED)
    find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
    target_link_libraries("${CMAKE_PROJECT_NAME}" PRIVATE
        ${METAL_FRAMEWORK}
        ${FOUNDATION_FRAMEWORK}
    )
    set_source_files_properties(
        "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/MetalSimulationBackend.mm"
        PROPERTIES COMPILE_OPTIONS "-fobjc-arc"
    )
endif()
