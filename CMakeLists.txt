#https://github.com/meemknight/cmakeSetup
#REFERENCE ^

#Ensures build system features up to date
cmake_minimum_required(VERSION 3.20.2)
project(FluidSimulation)

#PRovent GFLW from building unecessary components
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

option(PRODUCTION_BUILD " MAKE BUILD" OFF)

#link time optimization
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE) 

add_subdirectory(external-dependencies/glfw-3.3.2)			#window oppener
add_subdirectory(external-dependencies/glad)				#opengl loader
add_subdirectory(external-dependencies/stb_image)			#loading immaged
add_subdirectory(external-dependencies/stb_truetype)		#loading ttf files
add_subdirectory(external-dependencies/glm)				#math
add_subdirectory(external-dependencies/imgui-docking)		#ui


#MY_SOURCES list of all source files .cpp for Simulation
#MACRO
file(GLOB_RECURSE MY_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I/opt/homebrew/opt/llvm/include")
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L/opt/homebrew/opt/llvm/lib -Wl,-rpath,/opt/homebrew/opt/llvm/lib")
set(GLEW_INCLUDE_DIRS "/opt/homebrew/include")
set(GLEW_LIBRARIES "/opt/homebrew/lib/libGLEW.dylib")
# Find packages
#Locate OpenGL, GLEW, and GLFW libraries on system
find_package(OpenGL REQUIRED)
#find_package(GLEW REQUIRED)
find_package(PkgConfig REQUIRED)

#set(GLFW_INCLUDE_DIRS "/usr/local/include")
#set(GLFW_LIBRARY_DIRS "/usr/local/lib")
#set(GLFW_LIBRARIES "glfw")




#####add_executable(FluidSimulation main.cpp)
add_executable("${CMAKE_PROJECT_NAME}" ${MY_SOURCES})

#Ensures C++ version is standard
set_property(TARGET "${CMAKE_PROJECT_NAME}" PROPERTY CXX_STANDARD 17)

if(PRODUCTION_BUILD)
    #ASSETS_PATH macro to be in the root folder of exe
    target_compile_definitions("{CMAKE_PROJECT_NAME}" PUBLIC RESOURCE_PATH="./resources/")
    #remove option to debug asserts
    target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC PRODUCTION_BUILD=1) 
else()
    #get an ASSETS_PATH in IDE during development
    target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC RESOURCES_PATH="${CMAKE_CURRENT_SOURCE_DIR}/resources/")
	target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC PRODUCTION_BUILD=0) 

endif()

#target_sources("${CMAKE_PROJECT_NAME}" PRIVATE ${MY_SOURCES} )


target_include_directories("${CMAKE_PROJECT_NAME}" PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include/"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/gameLayer/"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/platform/"
    ${OPENGL_INCLUDE_DIR} 
    ${GLEW_INCLUDE_DIRS} 
    ${GLFW_INCLUDE_DIRS}
)

target_link_libraries("${CMAKE_PROJECT_NAME}" PRIVATE
    ${OPENGL_LIBRARIES} 
    ${GLEW_LIBRARIES} 
   #${GLFW_LIBRARIES}
    glfw
    glm
    glad
    stb_image
    stb_truetype
    gl2d
    raudio
    imgui
    safeSave
    profilerLib
    glui
)
