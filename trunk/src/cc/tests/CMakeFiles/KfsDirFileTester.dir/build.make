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
include src/cc/tests/CMakeFiles/KfsDirFileTester.dir/depend.make

# Include the progress variables for this target.
include src/cc/tests/CMakeFiles/KfsDirFileTester.dir/progress.make

# Include the compile flags for this target's objects.
include src/cc/tests/CMakeFiles/KfsDirFileTester.dir/flags.make

src/cc/tests/CMakeFiles/KfsDirFileTester.dir/KfsDirFileTester_main.o: src/cc/tests/CMakeFiles/KfsDirFileTester.dir/flags.make
src/cc/tests/CMakeFiles/KfsDirFileTester.dir/KfsDirFileTester_main.o: src/cc/tests/KfsDirFileTester_main.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /home/fify/Project/KFS/trunk/CMakeFiles $(CMAKE_PROGRESS_1)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/cc/tests/CMakeFiles/KfsDirFileTester.dir/KfsDirFileTester_main.o"
	cd /home/fify/Project/KFS/trunk/src/cc/tests && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/KfsDirFileTester.dir/KfsDirFileTester_main.o -c /home/fify/Project/KFS/trunk/src/cc/tests/KfsDirFileTester_main.cc

src/cc/tests/CMakeFiles/KfsDirFileTester.dir/KfsDirFileTester_main.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/KfsDirFileTester.dir/KfsDirFileTester_main.i"
	cd /home/fify/Project/KFS/trunk/src/cc/tests && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/fify/Project/KFS/trunk/src/cc/tests/KfsDirFileTester_main.cc > CMakeFiles/KfsDirFileTester.dir/KfsDirFileTester_main.i

src/cc/tests/CMakeFiles/KfsDirFileTester.dir/KfsDirFileTester_main.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/KfsDirFileTester.dir/KfsDirFileTester_main.s"
	cd /home/fify/Project/KFS/trunk/src/cc/tests && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/fify/Project/KFS/trunk/src/cc/tests/KfsDirFileTester_main.cc -o CMakeFiles/KfsDirFileTester.dir/KfsDirFileTester_main.s

src/cc/tests/CMakeFiles/KfsDirFileTester.dir/KfsDirFileTester_main.o.requires:
.PHONY : src/cc/tests/CMakeFiles/KfsDirFileTester.dir/KfsDirFileTester_main.o.requires

src/cc/tests/CMakeFiles/KfsDirFileTester.dir/KfsDirFileTester_main.o.provides: src/cc/tests/CMakeFiles/KfsDirFileTester.dir/KfsDirFileTester_main.o.requires
	$(MAKE) -f src/cc/tests/CMakeFiles/KfsDirFileTester.dir/build.make src/cc/tests/CMakeFiles/KfsDirFileTester.dir/KfsDirFileTester_main.o.provides.build
.PHONY : src/cc/tests/CMakeFiles/KfsDirFileTester.dir/KfsDirFileTester_main.o.provides

src/cc/tests/CMakeFiles/KfsDirFileTester.dir/KfsDirFileTester_main.o.provides.build: src/cc/tests/CMakeFiles/KfsDirFileTester.dir/KfsDirFileTester_main.o
.PHONY : src/cc/tests/CMakeFiles/KfsDirFileTester.dir/KfsDirFileTester_main.o.provides.build

# Object files for target KfsDirFileTester
KfsDirFileTester_OBJECTS = \
"CMakeFiles/KfsDirFileTester.dir/KfsDirFileTester_main.o"

# External object files for target KfsDirFileTester
KfsDirFileTester_EXTERNAL_OBJECTS =

src/cc/tests/KfsDirFileTester: src/cc/tests/CMakeFiles/KfsDirFileTester.dir/KfsDirFileTester_main.o
src/cc/tests/KfsDirFileTester: src/cc/tools/libtools.a
src/cc/tests/KfsDirFileTester: src/cc/libkfsClient/libkfsClient.a
src/cc/tests/KfsDirFileTester: src/cc/libkfsIO/libkfsIO.a
src/cc/tests/KfsDirFileTester: src/cc/common/libkfsCommon.a
src/cc/tests/KfsDirFileTester: /usr/local/lib/liblog4cpp.so
src/cc/tests/KfsDirFileTester: src/cc/tests/CMakeFiles/KfsDirFileTester.dir/build.make
src/cc/tests/KfsDirFileTester: src/cc/tests/CMakeFiles/KfsDirFileTester.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --red --bold "Linking CXX executable KfsDirFileTester"
	cd /home/fify/Project/KFS/trunk/src/cc/tests && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/KfsDirFileTester.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
src/cc/tests/CMakeFiles/KfsDirFileTester.dir/build: src/cc/tests/KfsDirFileTester
.PHONY : src/cc/tests/CMakeFiles/KfsDirFileTester.dir/build

src/cc/tests/CMakeFiles/KfsDirFileTester.dir/requires: src/cc/tests/CMakeFiles/KfsDirFileTester.dir/KfsDirFileTester_main.o.requires
.PHONY : src/cc/tests/CMakeFiles/KfsDirFileTester.dir/requires

src/cc/tests/CMakeFiles/KfsDirFileTester.dir/clean:
	cd /home/fify/Project/KFS/trunk/src/cc/tests && $(CMAKE_COMMAND) -P CMakeFiles/KfsDirFileTester.dir/cmake_clean.cmake
.PHONY : src/cc/tests/CMakeFiles/KfsDirFileTester.dir/clean

src/cc/tests/CMakeFiles/KfsDirFileTester.dir/depend:
	cd /home/fify/Project/KFS/trunk && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/fify/Project/KFS/trunk /home/fify/Project/KFS/trunk/src/cc/tests /home/fify/Project/KFS/trunk /home/fify/Project/KFS/trunk/src/cc/tests /home/fify/Project/KFS/trunk/src/cc/tests/CMakeFiles/KfsDirFileTester.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : src/cc/tests/CMakeFiles/KfsDirFileTester.dir/depend
