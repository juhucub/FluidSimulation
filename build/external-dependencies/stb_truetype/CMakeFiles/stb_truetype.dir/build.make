# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.29

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /opt/homebrew/Cellar/cmake/3.29.5/bin/cmake

# The command to remove a file.
RM = /opt/homebrew/Cellar/cmake/3.29.5/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /Users/jacobgardner/Documents/FluidSimulation

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /Users/jacobgardner/Documents/FluidSimulation/build

# Include any dependencies generated for this target.
include external-dependencies/stb_truetype/CMakeFiles/stb_truetype.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include external-dependencies/stb_truetype/CMakeFiles/stb_truetype.dir/compiler_depend.make

# Include the progress variables for this target.
include external-dependencies/stb_truetype/CMakeFiles/stb_truetype.dir/progress.make

# Include the compile flags for this target's objects.
include external-dependencies/stb_truetype/CMakeFiles/stb_truetype.dir/flags.make

external-dependencies/stb_truetype/CMakeFiles/stb_truetype.dir/src/stb_truetype.cpp.o: external-dependencies/stb_truetype/CMakeFiles/stb_truetype.dir/flags.make
external-dependencies/stb_truetype/CMakeFiles/stb_truetype.dir/src/stb_truetype.cpp.o: /Users/jacobgardner/Documents/FluidSimulation/external-dependencies/stb_truetype/src/stb_truetype.cpp
external-dependencies/stb_truetype/CMakeFiles/stb_truetype.dir/src/stb_truetype.cpp.o: external-dependencies/stb_truetype/CMakeFiles/stb_truetype.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/Users/jacobgardner/Documents/FluidSimulation/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object external-dependencies/stb_truetype/CMakeFiles/stb_truetype.dir/src/stb_truetype.cpp.o"
	cd /Users/jacobgardner/Documents/FluidSimulation/build/external-dependencies/stb_truetype && /usr/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT external-dependencies/stb_truetype/CMakeFiles/stb_truetype.dir/src/stb_truetype.cpp.o -MF CMakeFiles/stb_truetype.dir/src/stb_truetype.cpp.o.d -o CMakeFiles/stb_truetype.dir/src/stb_truetype.cpp.o -c /Users/jacobgardner/Documents/FluidSimulation/external-dependencies/stb_truetype/src/stb_truetype.cpp

external-dependencies/stb_truetype/CMakeFiles/stb_truetype.dir/src/stb_truetype.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/stb_truetype.dir/src/stb_truetype.cpp.i"
	cd /Users/jacobgardner/Documents/FluidSimulation/build/external-dependencies/stb_truetype && /usr/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /Users/jacobgardner/Documents/FluidSimulation/external-dependencies/stb_truetype/src/stb_truetype.cpp > CMakeFiles/stb_truetype.dir/src/stb_truetype.cpp.i

external-dependencies/stb_truetype/CMakeFiles/stb_truetype.dir/src/stb_truetype.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/stb_truetype.dir/src/stb_truetype.cpp.s"
	cd /Users/jacobgardner/Documents/FluidSimulation/build/external-dependencies/stb_truetype && /usr/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /Users/jacobgardner/Documents/FluidSimulation/external-dependencies/stb_truetype/src/stb_truetype.cpp -o CMakeFiles/stb_truetype.dir/src/stb_truetype.cpp.s

# Object files for target stb_truetype
stb_truetype_OBJECTS = \
"CMakeFiles/stb_truetype.dir/src/stb_truetype.cpp.o"

# External object files for target stb_truetype
stb_truetype_EXTERNAL_OBJECTS =

external-dependencies/stb_truetype/libstb_truetype.a: external-dependencies/stb_truetype/CMakeFiles/stb_truetype.dir/src/stb_truetype.cpp.o
external-dependencies/stb_truetype/libstb_truetype.a: external-dependencies/stb_truetype/CMakeFiles/stb_truetype.dir/build.make
external-dependencies/stb_truetype/libstb_truetype.a: external-dependencies/stb_truetype/CMakeFiles/stb_truetype.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --bold --progress-dir=/Users/jacobgardner/Documents/FluidSimulation/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX static library libstb_truetype.a"
	cd /Users/jacobgardner/Documents/FluidSimulation/build/external-dependencies/stb_truetype && $(CMAKE_COMMAND) -P CMakeFiles/stb_truetype.dir/cmake_clean_target.cmake
	cd /Users/jacobgardner/Documents/FluidSimulation/build/external-dependencies/stb_truetype && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/stb_truetype.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
external-dependencies/stb_truetype/CMakeFiles/stb_truetype.dir/build: external-dependencies/stb_truetype/libstb_truetype.a
.PHONY : external-dependencies/stb_truetype/CMakeFiles/stb_truetype.dir/build

external-dependencies/stb_truetype/CMakeFiles/stb_truetype.dir/clean:
	cd /Users/jacobgardner/Documents/FluidSimulation/build/external-dependencies/stb_truetype && $(CMAKE_COMMAND) -P CMakeFiles/stb_truetype.dir/cmake_clean.cmake
.PHONY : external-dependencies/stb_truetype/CMakeFiles/stb_truetype.dir/clean

external-dependencies/stb_truetype/CMakeFiles/stb_truetype.dir/depend:
	cd /Users/jacobgardner/Documents/FluidSimulation/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /Users/jacobgardner/Documents/FluidSimulation /Users/jacobgardner/Documents/FluidSimulation/external-dependencies/stb_truetype /Users/jacobgardner/Documents/FluidSimulation/build /Users/jacobgardner/Documents/FluidSimulation/build/external-dependencies/stb_truetype /Users/jacobgardner/Documents/FluidSimulation/build/external-dependencies/stb_truetype/CMakeFiles/stb_truetype.dir/DependInfo.cmake "--color=$(COLOR)"
.PHONY : external-dependencies/stb_truetype/CMakeFiles/stb_truetype.dir/depend

