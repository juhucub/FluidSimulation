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
CMAKE_COMMAND = /Applications/CMake.app/Contents/bin/cmake

# The command to remove a file.
RM = /Applications/CMake.app/Contents/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /Users/jacobgardner/Downloads/glfw-3.4

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /Users/jacobgardner/Downloads/glfw-3.4/build

# Include any dependencies generated for this target.
include tests/CMakeFiles/joysticks.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include tests/CMakeFiles/joysticks.dir/compiler_depend.make

# Include the progress variables for this target.
include tests/CMakeFiles/joysticks.dir/progress.make

# Include the compile flags for this target's objects.
include tests/CMakeFiles/joysticks.dir/flags.make

tests/CMakeFiles/joysticks.dir/joysticks.c.o: tests/CMakeFiles/joysticks.dir/flags.make
tests/CMakeFiles/joysticks.dir/joysticks.c.o: /Users/jacobgardner/Downloads/glfw-3.4/tests/joysticks.c
tests/CMakeFiles/joysticks.dir/joysticks.c.o: tests/CMakeFiles/joysticks.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/Users/jacobgardner/Downloads/glfw-3.4/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object tests/CMakeFiles/joysticks.dir/joysticks.c.o"
	cd /Users/jacobgardner/Downloads/glfw-3.4/build/tests && /Library/Developer/CommandLineTools/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -MD -MT tests/CMakeFiles/joysticks.dir/joysticks.c.o -MF CMakeFiles/joysticks.dir/joysticks.c.o.d -o CMakeFiles/joysticks.dir/joysticks.c.o -c /Users/jacobgardner/Downloads/glfw-3.4/tests/joysticks.c

tests/CMakeFiles/joysticks.dir/joysticks.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing C source to CMakeFiles/joysticks.dir/joysticks.c.i"
	cd /Users/jacobgardner/Downloads/glfw-3.4/build/tests && /Library/Developer/CommandLineTools/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /Users/jacobgardner/Downloads/glfw-3.4/tests/joysticks.c > CMakeFiles/joysticks.dir/joysticks.c.i

tests/CMakeFiles/joysticks.dir/joysticks.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling C source to assembly CMakeFiles/joysticks.dir/joysticks.c.s"
	cd /Users/jacobgardner/Downloads/glfw-3.4/build/tests && /Library/Developer/CommandLineTools/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /Users/jacobgardner/Downloads/glfw-3.4/tests/joysticks.c -o CMakeFiles/joysticks.dir/joysticks.c.s

# Object files for target joysticks
joysticks_OBJECTS = \
"CMakeFiles/joysticks.dir/joysticks.c.o"

# External object files for target joysticks
joysticks_EXTERNAL_OBJECTS =

tests/joysticks.app/Contents/MacOS/joysticks: tests/CMakeFiles/joysticks.dir/joysticks.c.o
tests/joysticks.app/Contents/MacOS/joysticks: tests/CMakeFiles/joysticks.dir/build.make
tests/joysticks.app/Contents/MacOS/joysticks: src/libglfw3.a
tests/joysticks.app/Contents/MacOS/joysticks: tests/CMakeFiles/joysticks.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --bold --progress-dir=/Users/jacobgardner/Downloads/glfw-3.4/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking C executable joysticks.app/Contents/MacOS/joysticks"
	cd /Users/jacobgardner/Downloads/glfw-3.4/build/tests && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/joysticks.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
tests/CMakeFiles/joysticks.dir/build: tests/joysticks.app/Contents/MacOS/joysticks
.PHONY : tests/CMakeFiles/joysticks.dir/build

tests/CMakeFiles/joysticks.dir/clean:
	cd /Users/jacobgardner/Downloads/glfw-3.4/build/tests && $(CMAKE_COMMAND) -P CMakeFiles/joysticks.dir/cmake_clean.cmake
.PHONY : tests/CMakeFiles/joysticks.dir/clean

tests/CMakeFiles/joysticks.dir/depend:
	cd /Users/jacobgardner/Downloads/glfw-3.4/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /Users/jacobgardner/Downloads/glfw-3.4 /Users/jacobgardner/Downloads/glfw-3.4/tests /Users/jacobgardner/Downloads/glfw-3.4/build /Users/jacobgardner/Downloads/glfw-3.4/build/tests /Users/jacobgardner/Downloads/glfw-3.4/build/tests/CMakeFiles/joysticks.dir/DependInfo.cmake "--color=$(COLOR)"
.PHONY : tests/CMakeFiles/joysticks.dir/depend

