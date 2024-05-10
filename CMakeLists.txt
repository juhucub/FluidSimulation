#Ensures build system features up to date
cmake_minimum_required(VERSION 3.20.2)
project(FluidSimulation)

#Ensures C++ version is standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


#Locate OpenGL, GLEW, and GLFW libraries on system
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)


add_executable(FluidSimulation main.cpp)
target_link_libraries(FluidSimulation ${OPENGL_LIBRARIES} ${GLEW_LIBRARIES} glfw)
target_include_directories(FluidSimulation PRIVATE ${OPENGL_INCLUDE_DIR} ${GLEW_INCLUDE_DIRS})