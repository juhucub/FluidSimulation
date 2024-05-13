#Ensures build system features up to date
cmake_minimum_required(VERSION 3.20.2)
project(FluidSimulation)

#Ensures C++ version is standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I/opt/homebrew/opt/llvm/include")
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L/opt/homebrew/opt/llvm/lib -Wl,-rpath,/opt/homebrew/opt/llvm/lib")

# Find packages
#Locate OpenGL, GLEW, and GLFW libraries on system
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(PkgConfig REQUIRED)

#set(GLFW_INCLUDE_DIRS "/usr/local/include")
#set(GLFW_LIBRARY_DIRS "/usr/local/lib")
#set(GLFW_LIBRARIES "glfw")

set(GLEW_INCLUDE_DIRS "/opt/homebrew/include")
set(GLEW_LIBRARIES "/opt/homebrew/lib/libGLEW.dylib")

add_executable(FluidSimulation main.cpp)

target_include_directories(FluidSimulation PRIVATE ${OPENGL_INCLUDE_DIR} ${GLEW_INCLUDE_DIRS} ${GLFW_INCLUDE_DIRS})

target_link_libraries(FluidSimulation ${OPENGL_LIBRARIES} ${GLEW_LIBRARIES} ${GLFW_LIBRARIES})
