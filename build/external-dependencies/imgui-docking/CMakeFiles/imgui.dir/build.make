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
include external-dependencies/imgui-docking/CMakeFiles/imgui.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include external-dependencies/imgui-docking/CMakeFiles/imgui.dir/compiler_depend.make

# Include the progress variables for this target.
include external-dependencies/imgui-docking/CMakeFiles/imgui.dir/progress.make

# Include the compile flags for this target's objects.
include external-dependencies/imgui-docking/CMakeFiles/imgui.dir/flags.make

external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/imgui.cpp.o: external-dependencies/imgui-docking/CMakeFiles/imgui.dir/flags.make
external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/imgui.cpp.o: /Users/jacobgardner/Documents/FluidSimulation/external-dependencies/imgui-docking/imgui/imgui.cpp
external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/imgui.cpp.o: external-dependencies/imgui-docking/CMakeFiles/imgui.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/Users/jacobgardner/Documents/FluidSimulation/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/imgui.cpp.o"
	cd /Users/jacobgardner/Documents/FluidSimulation/build/external-dependencies/imgui-docking && /usr/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/imgui.cpp.o -MF CMakeFiles/imgui.dir/imgui/imgui.cpp.o.d -o CMakeFiles/imgui.dir/imgui/imgui.cpp.o -c /Users/jacobgardner/Documents/FluidSimulation/external-dependencies/imgui-docking/imgui/imgui.cpp

external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/imgui.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/imgui.dir/imgui/imgui.cpp.i"
	cd /Users/jacobgardner/Documents/FluidSimulation/build/external-dependencies/imgui-docking && /usr/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /Users/jacobgardner/Documents/FluidSimulation/external-dependencies/imgui-docking/imgui/imgui.cpp > CMakeFiles/imgui.dir/imgui/imgui.cpp.i

external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/imgui.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/imgui.dir/imgui/imgui.cpp.s"
	cd /Users/jacobgardner/Documents/FluidSimulation/build/external-dependencies/imgui-docking && /usr/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /Users/jacobgardner/Documents/FluidSimulation/external-dependencies/imgui-docking/imgui/imgui.cpp -o CMakeFiles/imgui.dir/imgui/imgui.cpp.s

external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/imgui_demo.cpp.o: external-dependencies/imgui-docking/CMakeFiles/imgui.dir/flags.make
external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/imgui_demo.cpp.o: /Users/jacobgardner/Documents/FluidSimulation/external-dependencies/imgui-docking/imgui/imgui_demo.cpp
external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/imgui_demo.cpp.o: external-dependencies/imgui-docking/CMakeFiles/imgui.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/Users/jacobgardner/Documents/FluidSimulation/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building CXX object external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/imgui_demo.cpp.o"
	cd /Users/jacobgardner/Documents/FluidSimulation/build/external-dependencies/imgui-docking && /usr/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/imgui_demo.cpp.o -MF CMakeFiles/imgui.dir/imgui/imgui_demo.cpp.o.d -o CMakeFiles/imgui.dir/imgui/imgui_demo.cpp.o -c /Users/jacobgardner/Documents/FluidSimulation/external-dependencies/imgui-docking/imgui/imgui_demo.cpp

external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/imgui_demo.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/imgui.dir/imgui/imgui_demo.cpp.i"
	cd /Users/jacobgardner/Documents/FluidSimulation/build/external-dependencies/imgui-docking && /usr/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /Users/jacobgardner/Documents/FluidSimulation/external-dependencies/imgui-docking/imgui/imgui_demo.cpp > CMakeFiles/imgui.dir/imgui/imgui_demo.cpp.i

external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/imgui_demo.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/imgui.dir/imgui/imgui_demo.cpp.s"
	cd /Users/jacobgardner/Documents/FluidSimulation/build/external-dependencies/imgui-docking && /usr/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /Users/jacobgardner/Documents/FluidSimulation/external-dependencies/imgui-docking/imgui/imgui_demo.cpp -o CMakeFiles/imgui.dir/imgui/imgui_demo.cpp.s

external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/imgui_draw.cpp.o: external-dependencies/imgui-docking/CMakeFiles/imgui.dir/flags.make
external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/imgui_draw.cpp.o: /Users/jacobgardner/Documents/FluidSimulation/external-dependencies/imgui-docking/imgui/imgui_draw.cpp
external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/imgui_draw.cpp.o: external-dependencies/imgui-docking/CMakeFiles/imgui.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/Users/jacobgardner/Documents/FluidSimulation/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Building CXX object external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/imgui_draw.cpp.o"
	cd /Users/jacobgardner/Documents/FluidSimulation/build/external-dependencies/imgui-docking && /usr/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/imgui_draw.cpp.o -MF CMakeFiles/imgui.dir/imgui/imgui_draw.cpp.o.d -o CMakeFiles/imgui.dir/imgui/imgui_draw.cpp.o -c /Users/jacobgardner/Documents/FluidSimulation/external-dependencies/imgui-docking/imgui/imgui_draw.cpp

external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/imgui_draw.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/imgui.dir/imgui/imgui_draw.cpp.i"
	cd /Users/jacobgardner/Documents/FluidSimulation/build/external-dependencies/imgui-docking && /usr/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /Users/jacobgardner/Documents/FluidSimulation/external-dependencies/imgui-docking/imgui/imgui_draw.cpp > CMakeFiles/imgui.dir/imgui/imgui_draw.cpp.i

external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/imgui_draw.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/imgui.dir/imgui/imgui_draw.cpp.s"
	cd /Users/jacobgardner/Documents/FluidSimulation/build/external-dependencies/imgui-docking && /usr/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /Users/jacobgardner/Documents/FluidSimulation/external-dependencies/imgui-docking/imgui/imgui_draw.cpp -o CMakeFiles/imgui.dir/imgui/imgui_draw.cpp.s

external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/imgui_tables.cpp.o: external-dependencies/imgui-docking/CMakeFiles/imgui.dir/flags.make
external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/imgui_tables.cpp.o: /Users/jacobgardner/Documents/FluidSimulation/external-dependencies/imgui-docking/imgui/imgui_tables.cpp
external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/imgui_tables.cpp.o: external-dependencies/imgui-docking/CMakeFiles/imgui.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/Users/jacobgardner/Documents/FluidSimulation/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_4) "Building CXX object external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/imgui_tables.cpp.o"
	cd /Users/jacobgardner/Documents/FluidSimulation/build/external-dependencies/imgui-docking && /usr/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/imgui_tables.cpp.o -MF CMakeFiles/imgui.dir/imgui/imgui_tables.cpp.o.d -o CMakeFiles/imgui.dir/imgui/imgui_tables.cpp.o -c /Users/jacobgardner/Documents/FluidSimulation/external-dependencies/imgui-docking/imgui/imgui_tables.cpp

external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/imgui_tables.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/imgui.dir/imgui/imgui_tables.cpp.i"
	cd /Users/jacobgardner/Documents/FluidSimulation/build/external-dependencies/imgui-docking && /usr/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /Users/jacobgardner/Documents/FluidSimulation/external-dependencies/imgui-docking/imgui/imgui_tables.cpp > CMakeFiles/imgui.dir/imgui/imgui_tables.cpp.i

external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/imgui_tables.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/imgui.dir/imgui/imgui_tables.cpp.s"
	cd /Users/jacobgardner/Documents/FluidSimulation/build/external-dependencies/imgui-docking && /usr/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /Users/jacobgardner/Documents/FluidSimulation/external-dependencies/imgui-docking/imgui/imgui_tables.cpp -o CMakeFiles/imgui.dir/imgui/imgui_tables.cpp.s

external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/imgui_widgets.cpp.o: external-dependencies/imgui-docking/CMakeFiles/imgui.dir/flags.make
external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/imgui_widgets.cpp.o: /Users/jacobgardner/Documents/FluidSimulation/external-dependencies/imgui-docking/imgui/imgui_widgets.cpp
external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/imgui_widgets.cpp.o: external-dependencies/imgui-docking/CMakeFiles/imgui.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/Users/jacobgardner/Documents/FluidSimulation/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_5) "Building CXX object external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/imgui_widgets.cpp.o"
	cd /Users/jacobgardner/Documents/FluidSimulation/build/external-dependencies/imgui-docking && /usr/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/imgui_widgets.cpp.o -MF CMakeFiles/imgui.dir/imgui/imgui_widgets.cpp.o.d -o CMakeFiles/imgui.dir/imgui/imgui_widgets.cpp.o -c /Users/jacobgardner/Documents/FluidSimulation/external-dependencies/imgui-docking/imgui/imgui_widgets.cpp

external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/imgui_widgets.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/imgui.dir/imgui/imgui_widgets.cpp.i"
	cd /Users/jacobgardner/Documents/FluidSimulation/build/external-dependencies/imgui-docking && /usr/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /Users/jacobgardner/Documents/FluidSimulation/external-dependencies/imgui-docking/imgui/imgui_widgets.cpp > CMakeFiles/imgui.dir/imgui/imgui_widgets.cpp.i

external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/imgui_widgets.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/imgui.dir/imgui/imgui_widgets.cpp.s"
	cd /Users/jacobgardner/Documents/FluidSimulation/build/external-dependencies/imgui-docking && /usr/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /Users/jacobgardner/Documents/FluidSimulation/external-dependencies/imgui-docking/imgui/imgui_widgets.cpp -o CMakeFiles/imgui.dir/imgui/imgui_widgets.cpp.s

external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/backends/imgui_impl_glfw.cpp.o: external-dependencies/imgui-docking/CMakeFiles/imgui.dir/flags.make
external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/backends/imgui_impl_glfw.cpp.o: /Users/jacobgardner/Documents/FluidSimulation/external-dependencies/imgui-docking/imgui/backends/imgui_impl_glfw.cpp
external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/backends/imgui_impl_glfw.cpp.o: external-dependencies/imgui-docking/CMakeFiles/imgui.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/Users/jacobgardner/Documents/FluidSimulation/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_6) "Building CXX object external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/backends/imgui_impl_glfw.cpp.o"
	cd /Users/jacobgardner/Documents/FluidSimulation/build/external-dependencies/imgui-docking && /usr/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/backends/imgui_impl_glfw.cpp.o -MF CMakeFiles/imgui.dir/imgui/backends/imgui_impl_glfw.cpp.o.d -o CMakeFiles/imgui.dir/imgui/backends/imgui_impl_glfw.cpp.o -c /Users/jacobgardner/Documents/FluidSimulation/external-dependencies/imgui-docking/imgui/backends/imgui_impl_glfw.cpp

external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/backends/imgui_impl_glfw.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/imgui.dir/imgui/backends/imgui_impl_glfw.cpp.i"
	cd /Users/jacobgardner/Documents/FluidSimulation/build/external-dependencies/imgui-docking && /usr/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /Users/jacobgardner/Documents/FluidSimulation/external-dependencies/imgui-docking/imgui/backends/imgui_impl_glfw.cpp > CMakeFiles/imgui.dir/imgui/backends/imgui_impl_glfw.cpp.i

external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/backends/imgui_impl_glfw.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/imgui.dir/imgui/backends/imgui_impl_glfw.cpp.s"
	cd /Users/jacobgardner/Documents/FluidSimulation/build/external-dependencies/imgui-docking && /usr/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /Users/jacobgardner/Documents/FluidSimulation/external-dependencies/imgui-docking/imgui/backends/imgui_impl_glfw.cpp -o CMakeFiles/imgui.dir/imgui/backends/imgui_impl_glfw.cpp.s

external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/backends/imgui_impl_opengl3.cpp.o: external-dependencies/imgui-docking/CMakeFiles/imgui.dir/flags.make
external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/backends/imgui_impl_opengl3.cpp.o: /Users/jacobgardner/Documents/FluidSimulation/external-dependencies/imgui-docking/imgui/backends/imgui_impl_opengl3.cpp
external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/backends/imgui_impl_opengl3.cpp.o: external-dependencies/imgui-docking/CMakeFiles/imgui.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/Users/jacobgardner/Documents/FluidSimulation/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_7) "Building CXX object external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/backends/imgui_impl_opengl3.cpp.o"
	cd /Users/jacobgardner/Documents/FluidSimulation/build/external-dependencies/imgui-docking && /usr/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/backends/imgui_impl_opengl3.cpp.o -MF CMakeFiles/imgui.dir/imgui/backends/imgui_impl_opengl3.cpp.o.d -o CMakeFiles/imgui.dir/imgui/backends/imgui_impl_opengl3.cpp.o -c /Users/jacobgardner/Documents/FluidSimulation/external-dependencies/imgui-docking/imgui/backends/imgui_impl_opengl3.cpp

external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/backends/imgui_impl_opengl3.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/imgui.dir/imgui/backends/imgui_impl_opengl3.cpp.i"
	cd /Users/jacobgardner/Documents/FluidSimulation/build/external-dependencies/imgui-docking && /usr/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /Users/jacobgardner/Documents/FluidSimulation/external-dependencies/imgui-docking/imgui/backends/imgui_impl_opengl3.cpp > CMakeFiles/imgui.dir/imgui/backends/imgui_impl_opengl3.cpp.i

external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/backends/imgui_impl_opengl3.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/imgui.dir/imgui/backends/imgui_impl_opengl3.cpp.s"
	cd /Users/jacobgardner/Documents/FluidSimulation/build/external-dependencies/imgui-docking && /usr/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /Users/jacobgardner/Documents/FluidSimulation/external-dependencies/imgui-docking/imgui/backends/imgui_impl_opengl3.cpp -o CMakeFiles/imgui.dir/imgui/backends/imgui_impl_opengl3.cpp.s

# Object files for target imgui
imgui_OBJECTS = \
"CMakeFiles/imgui.dir/imgui/imgui.cpp.o" \
"CMakeFiles/imgui.dir/imgui/imgui_demo.cpp.o" \
"CMakeFiles/imgui.dir/imgui/imgui_draw.cpp.o" \
"CMakeFiles/imgui.dir/imgui/imgui_tables.cpp.o" \
"CMakeFiles/imgui.dir/imgui/imgui_widgets.cpp.o" \
"CMakeFiles/imgui.dir/imgui/backends/imgui_impl_glfw.cpp.o" \
"CMakeFiles/imgui.dir/imgui/backends/imgui_impl_opengl3.cpp.o"

# External object files for target imgui
imgui_EXTERNAL_OBJECTS =

external-dependencies/imgui-docking/libimgui.a: external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/imgui.cpp.o
external-dependencies/imgui-docking/libimgui.a: external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/imgui_demo.cpp.o
external-dependencies/imgui-docking/libimgui.a: external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/imgui_draw.cpp.o
external-dependencies/imgui-docking/libimgui.a: external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/imgui_tables.cpp.o
external-dependencies/imgui-docking/libimgui.a: external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/imgui_widgets.cpp.o
external-dependencies/imgui-docking/libimgui.a: external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/backends/imgui_impl_glfw.cpp.o
external-dependencies/imgui-docking/libimgui.a: external-dependencies/imgui-docking/CMakeFiles/imgui.dir/imgui/backends/imgui_impl_opengl3.cpp.o
external-dependencies/imgui-docking/libimgui.a: external-dependencies/imgui-docking/CMakeFiles/imgui.dir/build.make
external-dependencies/imgui-docking/libimgui.a: external-dependencies/imgui-docking/CMakeFiles/imgui.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --bold --progress-dir=/Users/jacobgardner/Documents/FluidSimulation/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_8) "Linking CXX static library libimgui.a"
	cd /Users/jacobgardner/Documents/FluidSimulation/build/external-dependencies/imgui-docking && $(CMAKE_COMMAND) -P CMakeFiles/imgui.dir/cmake_clean_target.cmake
	cd /Users/jacobgardner/Documents/FluidSimulation/build/external-dependencies/imgui-docking && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/imgui.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
external-dependencies/imgui-docking/CMakeFiles/imgui.dir/build: external-dependencies/imgui-docking/libimgui.a
.PHONY : external-dependencies/imgui-docking/CMakeFiles/imgui.dir/build

external-dependencies/imgui-docking/CMakeFiles/imgui.dir/clean:
	cd /Users/jacobgardner/Documents/FluidSimulation/build/external-dependencies/imgui-docking && $(CMAKE_COMMAND) -P CMakeFiles/imgui.dir/cmake_clean.cmake
.PHONY : external-dependencies/imgui-docking/CMakeFiles/imgui.dir/clean

external-dependencies/imgui-docking/CMakeFiles/imgui.dir/depend:
	cd /Users/jacobgardner/Documents/FluidSimulation/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /Users/jacobgardner/Documents/FluidSimulation /Users/jacobgardner/Documents/FluidSimulation/external-dependencies/imgui-docking /Users/jacobgardner/Documents/FluidSimulation/build /Users/jacobgardner/Documents/FluidSimulation/build/external-dependencies/imgui-docking /Users/jacobgardner/Documents/FluidSimulation/build/external-dependencies/imgui-docking/CMakeFiles/imgui.dir/DependInfo.cmake "--color=$(COLOR)"
.PHONY : external-dependencies/imgui-docking/CMakeFiles/imgui.dir/depend

