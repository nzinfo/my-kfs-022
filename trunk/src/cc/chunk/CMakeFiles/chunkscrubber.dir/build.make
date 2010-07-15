# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 2.6

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canoncical targets will work.
.SUFFIXES:

# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list

# Produce verbose output by default.
VERBOSE = 1

# Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/local/bin/cmake

# The command to remove a file.
RM = /usr/local/bin/cmake -E remove -f

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/fify/Project/KFS/trunk

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/fify/Project/KFS/trunk

# Include any dependencies generated for this target.
include src/cc/chunk/CMakeFiles/chunkscrubber.dir/depend.make

# Include the progress variables for this target.
include src/cc/chunk/CMakeFiles/chunkscrubber.dir/progress.make

# Include the compile flags for this target's objects.
include src/cc/chunk/CMakeFiles/chunkscrubber.dir/flags.make

src/cc/chunk/CMakeFiles/chunkscrubber.dir/chunkscrubber_main.o: src/cc/chunk/CMakeFiles/chunkscrubber.dir/flags.make
src/cc/chunk/CMakeFiles/chunkscrubber.dir/chunkscrubber_main.o: src/cc/chunk/chunkscrubber_main.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /home/fify/Project/KFS/trunk/CMakeFiles $(CMAKE_PROGRESS_1)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/cc/chunk/CMakeFiles/chunkscrubber.dir/chunkscrubber_main.o"
	cd /home/fify/Project/KFS/trunk/src/cc/chunk && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/chunkscrubber.dir/chunkscrubber_main.o -c /home/fify/Project/KFS/trunk/src/cc/chunk/chunkscrubber_main.cc

src/cc/chunk/CMakeFiles/chunkscrubber.dir/chunkscrubber_main.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/chunkscrubber.dir/chunkscrubber_main.i"
	cd /home/fify/Project/KFS/trunk/src/cc/chunk && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/fify/Project/KFS/trunk/src/cc/chunk/chunkscrubber_main.cc > CMakeFiles/chunkscrubber.dir/chunkscrubber_main.i

src/cc/chunk/CMakeFiles/chunkscrubber.dir/chunkscrubber_main.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/chunkscrubber.dir/chunkscrubber_main.s"
	cd /home/fify/Project/KFS/trunk/src/cc/chunk && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/fify/Project/KFS/trunk/src/cc/chunk/chunkscrubber_main.cc -o CMakeFiles/chunkscrubber.dir/chunkscrubber_main.s

src/cc/chunk/CMakeFiles/chunkscrubber.dir/chunkscrubber_main.o.requires:
.PHONY : src/cc/chunk/CMakeFiles/chunkscrubber.dir/chunkscrubber_main.o.requires

src/cc/chunk/CMakeFiles/chunkscrubber.dir/chunkscrubber_main.o.provides: src/cc/chunk/CMakeFiles/chunkscrubber.dir/chunkscrubber_main.o.requires
	$(MAKE) -f src/cc/chunk/CMakeFiles/chunkscrubber.dir/build.make src/cc/chunk/CMakeFiles/chunkscrubber.dir/chunkscrubber_main.o.provides.build
.PHONY : src/cc/chunk/CMakeFiles/chunkscrubber.dir/chunkscrubber_main.o.provides

src/cc/chunk/CMakeFiles/chunkscrubber.dir/chunkscrubber_main.o.provides.build: src/cc/chunk/CMakeFiles/chunkscrubber.dir/chunkscrubber_main.o
.PHONY : src/cc/chunk/CMakeFiles/chunkscrubber.dir/chunkscrubber_main.o.provides.build

# Object files for target chunkscrubber
chunkscrubber_OBJECTS = \
"CMakeFiles/chunkscrubber.dir/chunkscrubber_main.o"

# External object files for target chunkscrubber
chunkscrubber_EXTERNAL_OBJECTS =

src/cc/chunk/chunkscrubber: src/cc/chunk/CMakeFiles/chunkscrubber.dir/chunkscrubber_main.o
src/cc/chunk/chunkscrubber: src/cc/chunk/libchunk.a
src/cc/chunk/chunkscrubber: src/cc/libkfsIO/libkfsIO.a
src/cc/chunk/chunkscrubber: src/cc/common/libkfsCommon.a
src/cc/chunk/chunkscrubber: /usr/local/lib/liblog4cpp.so
src/cc/chunk/chunkscrubber: src/cc/chunk/CMakeFiles/chunkscrubber.dir/build.make
src/cc/chunk/chunkscrubber: src/cc/chunk/CMakeFiles/chunkscrubber.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --red --bold "Linking CXX executable chunkscrubber"
	cd /home/fify/Project/KFS/trunk/src/cc/chunk && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/chunkscrubber.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
src/cc/chunk/CMakeFiles/chunkscrubber.dir/build: src/cc/chunk/chunkscrubber
.PHONY : src/cc/chunk/CMakeFiles/chunkscrubber.dir/build

src/cc/chunk/CMakeFiles/chunkscrubber.dir/requires: src/cc/chunk/CMakeFiles/chunkscrubber.dir/chunkscrubber_main.o.requires
.PHONY : src/cc/chunk/CMakeFiles/chunkscrubber.dir/requires

src/cc/chunk/CMakeFiles/chunkscrubber.dir/clean:
	cd /home/fify/Project/KFS/trunk/src/cc/chunk && $(CMAKE_COMMAND) -P CMakeFiles/chunkscrubber.dir/cmake_clean.cmake
.PHONY : src/cc/chunk/CMakeFiles/chunkscrubber.dir/clean

src/cc/chunk/CMakeFiles/chunkscrubber.dir/depend:
	cd /home/fify/Project/KFS/trunk && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/fify/Project/KFS/trunk /home/fify/Project/KFS/trunk/src/cc/chunk /home/fify/Project/KFS/trunk /home/fify/Project/KFS/trunk/src/cc/chunk /home/fify/Project/KFS/trunk/src/cc/chunk/CMakeFiles/chunkscrubber.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : src/cc/chunk/CMakeFiles/chunkscrubber.dir/depend
