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
include src/cc/fuse/CMakeFiles/kfs_fuse.dir/depend.make

# Include the progress variables for this target.
include src/cc/fuse/CMakeFiles/kfs_fuse.dir/progress.make

# Include the compile flags for this target's objects.
include src/cc/fuse/CMakeFiles/kfs_fuse.dir/flags.make

src/cc/fuse/CMakeFiles/kfs_fuse.dir/kfs_fuse_main.o: src/cc/fuse/CMakeFiles/kfs_fuse.dir/flags.make
src/cc/fuse/CMakeFiles/kfs_fuse.dir/kfs_fuse_main.o: src/cc/fuse/kfs_fuse_main.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /home/fify/Project/KFS/trunk/CMakeFiles $(CMAKE_PROGRESS_1)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/cc/fuse/CMakeFiles/kfs_fuse.dir/kfs_fuse_main.o"
	cd /home/fify/Project/KFS/trunk/src/cc/fuse && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/kfs_fuse.dir/kfs_fuse_main.o -c /home/fify/Project/KFS/trunk/src/cc/fuse/kfs_fuse_main.cc

src/cc/fuse/CMakeFiles/kfs_fuse.dir/kfs_fuse_main.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/kfs_fuse.dir/kfs_fuse_main.i"
	cd /home/fify/Project/KFS/trunk/src/cc/fuse && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/fify/Project/KFS/trunk/src/cc/fuse/kfs_fuse_main.cc > CMakeFiles/kfs_fuse.dir/kfs_fuse_main.i

src/cc/fuse/CMakeFiles/kfs_fuse.dir/kfs_fuse_main.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/kfs_fuse.dir/kfs_fuse_main.s"
	cd /home/fify/Project/KFS/trunk/src/cc/fuse && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/fify/Project/KFS/trunk/src/cc/fuse/kfs_fuse_main.cc -o CMakeFiles/kfs_fuse.dir/kfs_fuse_main.s

src/cc/fuse/CMakeFiles/kfs_fuse.dir/kfs_fuse_main.o.requires:
.PHONY : src/cc/fuse/CMakeFiles/kfs_fuse.dir/kfs_fuse_main.o.requires

src/cc/fuse/CMakeFiles/kfs_fuse.dir/kfs_fuse_main.o.provides: src/cc/fuse/CMakeFiles/kfs_fuse.dir/kfs_fuse_main.o.requires
	$(MAKE) -f src/cc/fuse/CMakeFiles/kfs_fuse.dir/build.make src/cc/fuse/CMakeFiles/kfs_fuse.dir/kfs_fuse_main.o.provides.build
.PHONY : src/cc/fuse/CMakeFiles/kfs_fuse.dir/kfs_fuse_main.o.provides

src/cc/fuse/CMakeFiles/kfs_fuse.dir/kfs_fuse_main.o.provides.build: src/cc/fuse/CMakeFiles/kfs_fuse.dir/kfs_fuse_main.o
.PHONY : src/cc/fuse/CMakeFiles/kfs_fuse.dir/kfs_fuse_main.o.provides.build

# Object files for target kfs_fuse
kfs_fuse_OBJECTS = \
"CMakeFiles/kfs_fuse.dir/kfs_fuse_main.o"

# External object files for target kfs_fuse
kfs_fuse_EXTERNAL_OBJECTS =

src/cc/fuse/kfs_fuse: src/cc/fuse/CMakeFiles/kfs_fuse.dir/kfs_fuse_main.o
src/cc/fuse/kfs_fuse: src/cc/libkfsClient/libkfsClient.a
src/cc/fuse/kfs_fuse: src/cc/libkfsIO/libkfsIO.a
src/cc/fuse/kfs_fuse: src/cc/common/libkfsCommon.a
src/cc/fuse/kfs_fuse: /usr/local/lib/liblog4cpp.so
src/cc/fuse/kfs_fuse: src/cc/fuse/CMakeFiles/kfs_fuse.dir/build.make
src/cc/fuse/kfs_fuse: src/cc/fuse/CMakeFiles/kfs_fuse.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --red --bold "Linking CXX executable kfs_fuse"
	cd /home/fify/Project/KFS/trunk/src/cc/fuse && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/kfs_fuse.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
src/cc/fuse/CMakeFiles/kfs_fuse.dir/build: src/cc/fuse/kfs_fuse
.PHONY : src/cc/fuse/CMakeFiles/kfs_fuse.dir/build

src/cc/fuse/CMakeFiles/kfs_fuse.dir/requires: src/cc/fuse/CMakeFiles/kfs_fuse.dir/kfs_fuse_main.o.requires
.PHONY : src/cc/fuse/CMakeFiles/kfs_fuse.dir/requires

src/cc/fuse/CMakeFiles/kfs_fuse.dir/clean:
	cd /home/fify/Project/KFS/trunk/src/cc/fuse && $(CMAKE_COMMAND) -P CMakeFiles/kfs_fuse.dir/cmake_clean.cmake
.PHONY : src/cc/fuse/CMakeFiles/kfs_fuse.dir/clean

src/cc/fuse/CMakeFiles/kfs_fuse.dir/depend:
	cd /home/fify/Project/KFS/trunk && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/fify/Project/KFS/trunk /home/fify/Project/KFS/trunk/src/cc/fuse /home/fify/Project/KFS/trunk /home/fify/Project/KFS/trunk/src/cc/fuse /home/fify/Project/KFS/trunk/src/cc/fuse/CMakeFiles/kfs_fuse.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : src/cc/fuse/CMakeFiles/kfs_fuse.dir/depend
