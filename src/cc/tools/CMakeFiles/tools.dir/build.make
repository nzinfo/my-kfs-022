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
include src/cc/tools/CMakeFiles/tools.dir/depend.make

# Include the progress variables for this target.
include src/cc/tools/CMakeFiles/tools.dir/progress.make

# Include the compile flags for this target's objects.
include src/cc/tools/CMakeFiles/tools.dir/flags.make

src/cc/tools/CMakeFiles/tools.dir/MonUtils.o: src/cc/tools/CMakeFiles/tools.dir/flags.make
src/cc/tools/CMakeFiles/tools.dir/MonUtils.o: src/cc/tools/MonUtils.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /home/fify/Project/KFS/trunk/CMakeFiles $(CMAKE_PROGRESS_1)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/cc/tools/CMakeFiles/tools.dir/MonUtils.o"
	cd /home/fify/Project/KFS/trunk/src/cc/tools && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/tools.dir/MonUtils.o -c /home/fify/Project/KFS/trunk/src/cc/tools/MonUtils.cc

src/cc/tools/CMakeFiles/tools.dir/MonUtils.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/tools.dir/MonUtils.i"
	cd /home/fify/Project/KFS/trunk/src/cc/tools && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/fify/Project/KFS/trunk/src/cc/tools/MonUtils.cc > CMakeFiles/tools.dir/MonUtils.i

src/cc/tools/CMakeFiles/tools.dir/MonUtils.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/tools.dir/MonUtils.s"
	cd /home/fify/Project/KFS/trunk/src/cc/tools && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/fify/Project/KFS/trunk/src/cc/tools/MonUtils.cc -o CMakeFiles/tools.dir/MonUtils.s

src/cc/tools/CMakeFiles/tools.dir/MonUtils.o.requires:
.PHONY : src/cc/tools/CMakeFiles/tools.dir/MonUtils.o.requires

src/cc/tools/CMakeFiles/tools.dir/MonUtils.o.provides: src/cc/tools/CMakeFiles/tools.dir/MonUtils.o.requires
	$(MAKE) -f src/cc/tools/CMakeFiles/tools.dir/build.make src/cc/tools/CMakeFiles/tools.dir/MonUtils.o.provides.build
.PHONY : src/cc/tools/CMakeFiles/tools.dir/MonUtils.o.provides

src/cc/tools/CMakeFiles/tools.dir/MonUtils.o.provides.build: src/cc/tools/CMakeFiles/tools.dir/MonUtils.o
.PHONY : src/cc/tools/CMakeFiles/tools.dir/MonUtils.o.provides.build

src/cc/tools/CMakeFiles/tools.dir/KfsCd.o: src/cc/tools/CMakeFiles/tools.dir/flags.make
src/cc/tools/CMakeFiles/tools.dir/KfsCd.o: src/cc/tools/KfsCd.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /home/fify/Project/KFS/trunk/CMakeFiles $(CMAKE_PROGRESS_2)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/cc/tools/CMakeFiles/tools.dir/KfsCd.o"
	cd /home/fify/Project/KFS/trunk/src/cc/tools && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/tools.dir/KfsCd.o -c /home/fify/Project/KFS/trunk/src/cc/tools/KfsCd.cc

src/cc/tools/CMakeFiles/tools.dir/KfsCd.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/tools.dir/KfsCd.i"
	cd /home/fify/Project/KFS/trunk/src/cc/tools && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/fify/Project/KFS/trunk/src/cc/tools/KfsCd.cc > CMakeFiles/tools.dir/KfsCd.i

src/cc/tools/CMakeFiles/tools.dir/KfsCd.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/tools.dir/KfsCd.s"
	cd /home/fify/Project/KFS/trunk/src/cc/tools && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/fify/Project/KFS/trunk/src/cc/tools/KfsCd.cc -o CMakeFiles/tools.dir/KfsCd.s

src/cc/tools/CMakeFiles/tools.dir/KfsCd.o.requires:
.PHONY : src/cc/tools/CMakeFiles/tools.dir/KfsCd.o.requires

src/cc/tools/CMakeFiles/tools.dir/KfsCd.o.provides: src/cc/tools/CMakeFiles/tools.dir/KfsCd.o.requires
	$(MAKE) -f src/cc/tools/CMakeFiles/tools.dir/build.make src/cc/tools/CMakeFiles/tools.dir/KfsCd.o.provides.build
.PHONY : src/cc/tools/CMakeFiles/tools.dir/KfsCd.o.provides

src/cc/tools/CMakeFiles/tools.dir/KfsCd.o.provides.build: src/cc/tools/CMakeFiles/tools.dir/KfsCd.o
.PHONY : src/cc/tools/CMakeFiles/tools.dir/KfsCd.o.provides.build

src/cc/tools/CMakeFiles/tools.dir/KfsChangeReplication.o: src/cc/tools/CMakeFiles/tools.dir/flags.make
src/cc/tools/CMakeFiles/tools.dir/KfsChangeReplication.o: src/cc/tools/KfsChangeReplication.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /home/fify/Project/KFS/trunk/CMakeFiles $(CMAKE_PROGRESS_3)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/cc/tools/CMakeFiles/tools.dir/KfsChangeReplication.o"
	cd /home/fify/Project/KFS/trunk/src/cc/tools && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/tools.dir/KfsChangeReplication.o -c /home/fify/Project/KFS/trunk/src/cc/tools/KfsChangeReplication.cc

src/cc/tools/CMakeFiles/tools.dir/KfsChangeReplication.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/tools.dir/KfsChangeReplication.i"
	cd /home/fify/Project/KFS/trunk/src/cc/tools && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/fify/Project/KFS/trunk/src/cc/tools/KfsChangeReplication.cc > CMakeFiles/tools.dir/KfsChangeReplication.i

src/cc/tools/CMakeFiles/tools.dir/KfsChangeReplication.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/tools.dir/KfsChangeReplication.s"
	cd /home/fify/Project/KFS/trunk/src/cc/tools && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/fify/Project/KFS/trunk/src/cc/tools/KfsChangeReplication.cc -o CMakeFiles/tools.dir/KfsChangeReplication.s

src/cc/tools/CMakeFiles/tools.dir/KfsChangeReplication.o.requires:
.PHONY : src/cc/tools/CMakeFiles/tools.dir/KfsChangeReplication.o.requires

src/cc/tools/CMakeFiles/tools.dir/KfsChangeReplication.o.provides: src/cc/tools/CMakeFiles/tools.dir/KfsChangeReplication.o.requires
	$(MAKE) -f src/cc/tools/CMakeFiles/tools.dir/build.make src/cc/tools/CMakeFiles/tools.dir/KfsChangeReplication.o.provides.build
.PHONY : src/cc/tools/CMakeFiles/tools.dir/KfsChangeReplication.o.provides

src/cc/tools/CMakeFiles/tools.dir/KfsChangeReplication.o.provides.build: src/cc/tools/CMakeFiles/tools.dir/KfsChangeReplication.o
.PHONY : src/cc/tools/CMakeFiles/tools.dir/KfsChangeReplication.o.provides.build

src/cc/tools/CMakeFiles/tools.dir/KfsCp.o: src/cc/tools/CMakeFiles/tools.dir/flags.make
src/cc/tools/CMakeFiles/tools.dir/KfsCp.o: src/cc/tools/KfsCp.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /home/fify/Project/KFS/trunk/CMakeFiles $(CMAKE_PROGRESS_4)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/cc/tools/CMakeFiles/tools.dir/KfsCp.o"
	cd /home/fify/Project/KFS/trunk/src/cc/tools && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/tools.dir/KfsCp.o -c /home/fify/Project/KFS/trunk/src/cc/tools/KfsCp.cc

src/cc/tools/CMakeFiles/tools.dir/KfsCp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/tools.dir/KfsCp.i"
	cd /home/fify/Project/KFS/trunk/src/cc/tools && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/fify/Project/KFS/trunk/src/cc/tools/KfsCp.cc > CMakeFiles/tools.dir/KfsCp.i

src/cc/tools/CMakeFiles/tools.dir/KfsCp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/tools.dir/KfsCp.s"
	cd /home/fify/Project/KFS/trunk/src/cc/tools && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/fify/Project/KFS/trunk/src/cc/tools/KfsCp.cc -o CMakeFiles/tools.dir/KfsCp.s

src/cc/tools/CMakeFiles/tools.dir/KfsCp.o.requires:
.PHONY : src/cc/tools/CMakeFiles/tools.dir/KfsCp.o.requires

src/cc/tools/CMakeFiles/tools.dir/KfsCp.o.provides: src/cc/tools/CMakeFiles/tools.dir/KfsCp.o.requires
	$(MAKE) -f src/cc/tools/CMakeFiles/tools.dir/build.make src/cc/tools/CMakeFiles/tools.dir/KfsCp.o.provides.build
.PHONY : src/cc/tools/CMakeFiles/tools.dir/KfsCp.o.provides

src/cc/tools/CMakeFiles/tools.dir/KfsCp.o.provides.build: src/cc/tools/CMakeFiles/tools.dir/KfsCp.o
.PHONY : src/cc/tools/CMakeFiles/tools.dir/KfsCp.o.provides.build

src/cc/tools/CMakeFiles/tools.dir/KfsLs.o: src/cc/tools/CMakeFiles/tools.dir/flags.make
src/cc/tools/CMakeFiles/tools.dir/KfsLs.o: src/cc/tools/KfsLs.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /home/fify/Project/KFS/trunk/CMakeFiles $(CMAKE_PROGRESS_5)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/cc/tools/CMakeFiles/tools.dir/KfsLs.o"
	cd /home/fify/Project/KFS/trunk/src/cc/tools && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/tools.dir/KfsLs.o -c /home/fify/Project/KFS/trunk/src/cc/tools/KfsLs.cc

src/cc/tools/CMakeFiles/tools.dir/KfsLs.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/tools.dir/KfsLs.i"
	cd /home/fify/Project/KFS/trunk/src/cc/tools && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/fify/Project/KFS/trunk/src/cc/tools/KfsLs.cc > CMakeFiles/tools.dir/KfsLs.i

src/cc/tools/CMakeFiles/tools.dir/KfsLs.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/tools.dir/KfsLs.s"
	cd /home/fify/Project/KFS/trunk/src/cc/tools && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/fify/Project/KFS/trunk/src/cc/tools/KfsLs.cc -o CMakeFiles/tools.dir/KfsLs.s

src/cc/tools/CMakeFiles/tools.dir/KfsLs.o.requires:
.PHONY : src/cc/tools/CMakeFiles/tools.dir/KfsLs.o.requires

src/cc/tools/CMakeFiles/tools.dir/KfsLs.o.provides: src/cc/tools/CMakeFiles/tools.dir/KfsLs.o.requires
	$(MAKE) -f src/cc/tools/CMakeFiles/tools.dir/build.make src/cc/tools/CMakeFiles/tools.dir/KfsLs.o.provides.build
.PHONY : src/cc/tools/CMakeFiles/tools.dir/KfsLs.o.provides

src/cc/tools/CMakeFiles/tools.dir/KfsLs.o.provides.build: src/cc/tools/CMakeFiles/tools.dir/KfsLs.o
.PHONY : src/cc/tools/CMakeFiles/tools.dir/KfsLs.o.provides.build

src/cc/tools/CMakeFiles/tools.dir/KfsMkdirs.o: src/cc/tools/CMakeFiles/tools.dir/flags.make
src/cc/tools/CMakeFiles/tools.dir/KfsMkdirs.o: src/cc/tools/KfsMkdirs.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /home/fify/Project/KFS/trunk/CMakeFiles $(CMAKE_PROGRESS_6)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/cc/tools/CMakeFiles/tools.dir/KfsMkdirs.o"
	cd /home/fify/Project/KFS/trunk/src/cc/tools && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/tools.dir/KfsMkdirs.o -c /home/fify/Project/KFS/trunk/src/cc/tools/KfsMkdirs.cc

src/cc/tools/CMakeFiles/tools.dir/KfsMkdirs.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/tools.dir/KfsMkdirs.i"
	cd /home/fify/Project/KFS/trunk/src/cc/tools && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/fify/Project/KFS/trunk/src/cc/tools/KfsMkdirs.cc > CMakeFiles/tools.dir/KfsMkdirs.i

src/cc/tools/CMakeFiles/tools.dir/KfsMkdirs.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/tools.dir/KfsMkdirs.s"
	cd /home/fify/Project/KFS/trunk/src/cc/tools && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/fify/Project/KFS/trunk/src/cc/tools/KfsMkdirs.cc -o CMakeFiles/tools.dir/KfsMkdirs.s

src/cc/tools/CMakeFiles/tools.dir/KfsMkdirs.o.requires:
.PHONY : src/cc/tools/CMakeFiles/tools.dir/KfsMkdirs.o.requires

src/cc/tools/CMakeFiles/tools.dir/KfsMkdirs.o.provides: src/cc/tools/CMakeFiles/tools.dir/KfsMkdirs.o.requires
	$(MAKE) -f src/cc/tools/CMakeFiles/tools.dir/build.make src/cc/tools/CMakeFiles/tools.dir/KfsMkdirs.o.provides.build
.PHONY : src/cc/tools/CMakeFiles/tools.dir/KfsMkdirs.o.provides

src/cc/tools/CMakeFiles/tools.dir/KfsMkdirs.o.provides.build: src/cc/tools/CMakeFiles/tools.dir/KfsMkdirs.o
.PHONY : src/cc/tools/CMakeFiles/tools.dir/KfsMkdirs.o.provides.build

src/cc/tools/CMakeFiles/tools.dir/KfsMv.o: src/cc/tools/CMakeFiles/tools.dir/flags.make
src/cc/tools/CMakeFiles/tools.dir/KfsMv.o: src/cc/tools/KfsMv.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /home/fify/Project/KFS/trunk/CMakeFiles $(CMAKE_PROGRESS_7)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/cc/tools/CMakeFiles/tools.dir/KfsMv.o"
	cd /home/fify/Project/KFS/trunk/src/cc/tools && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/tools.dir/KfsMv.o -c /home/fify/Project/KFS/trunk/src/cc/tools/KfsMv.cc

src/cc/tools/CMakeFiles/tools.dir/KfsMv.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/tools.dir/KfsMv.i"
	cd /home/fify/Project/KFS/trunk/src/cc/tools && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/fify/Project/KFS/trunk/src/cc/tools/KfsMv.cc > CMakeFiles/tools.dir/KfsMv.i

src/cc/tools/CMakeFiles/tools.dir/KfsMv.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/tools.dir/KfsMv.s"
	cd /home/fify/Project/KFS/trunk/src/cc/tools && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/fify/Project/KFS/trunk/src/cc/tools/KfsMv.cc -o CMakeFiles/tools.dir/KfsMv.s

src/cc/tools/CMakeFiles/tools.dir/KfsMv.o.requires:
.PHONY : src/cc/tools/CMakeFiles/tools.dir/KfsMv.o.requires

src/cc/tools/CMakeFiles/tools.dir/KfsMv.o.provides: src/cc/tools/CMakeFiles/tools.dir/KfsMv.o.requires
	$(MAKE) -f src/cc/tools/CMakeFiles/tools.dir/build.make src/cc/tools/CMakeFiles/tools.dir/KfsMv.o.provides.build
.PHONY : src/cc/tools/CMakeFiles/tools.dir/KfsMv.o.provides

src/cc/tools/CMakeFiles/tools.dir/KfsMv.o.provides.build: src/cc/tools/CMakeFiles/tools.dir/KfsMv.o
.PHONY : src/cc/tools/CMakeFiles/tools.dir/KfsMv.o.provides.build

src/cc/tools/CMakeFiles/tools.dir/KfsRm.o: src/cc/tools/CMakeFiles/tools.dir/flags.make
src/cc/tools/CMakeFiles/tools.dir/KfsRm.o: src/cc/tools/KfsRm.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /home/fify/Project/KFS/trunk/CMakeFiles $(CMAKE_PROGRESS_8)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/cc/tools/CMakeFiles/tools.dir/KfsRm.o"
	cd /home/fify/Project/KFS/trunk/src/cc/tools && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/tools.dir/KfsRm.o -c /home/fify/Project/KFS/trunk/src/cc/tools/KfsRm.cc

src/cc/tools/CMakeFiles/tools.dir/KfsRm.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/tools.dir/KfsRm.i"
	cd /home/fify/Project/KFS/trunk/src/cc/tools && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/fify/Project/KFS/trunk/src/cc/tools/KfsRm.cc > CMakeFiles/tools.dir/KfsRm.i

src/cc/tools/CMakeFiles/tools.dir/KfsRm.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/tools.dir/KfsRm.s"
	cd /home/fify/Project/KFS/trunk/src/cc/tools && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/fify/Project/KFS/trunk/src/cc/tools/KfsRm.cc -o CMakeFiles/tools.dir/KfsRm.s

src/cc/tools/CMakeFiles/tools.dir/KfsRm.o.requires:
.PHONY : src/cc/tools/CMakeFiles/tools.dir/KfsRm.o.requires

src/cc/tools/CMakeFiles/tools.dir/KfsRm.o.provides: src/cc/tools/CMakeFiles/tools.dir/KfsRm.o.requires
	$(MAKE) -f src/cc/tools/CMakeFiles/tools.dir/build.make src/cc/tools/CMakeFiles/tools.dir/KfsRm.o.provides.build
.PHONY : src/cc/tools/CMakeFiles/tools.dir/KfsRm.o.provides

src/cc/tools/CMakeFiles/tools.dir/KfsRm.o.provides.build: src/cc/tools/CMakeFiles/tools.dir/KfsRm.o
.PHONY : src/cc/tools/CMakeFiles/tools.dir/KfsRm.o.provides.build

src/cc/tools/CMakeFiles/tools.dir/KfsRmdir.o: src/cc/tools/CMakeFiles/tools.dir/flags.make
src/cc/tools/CMakeFiles/tools.dir/KfsRmdir.o: src/cc/tools/KfsRmdir.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /home/fify/Project/KFS/trunk/CMakeFiles $(CMAKE_PROGRESS_9)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/cc/tools/CMakeFiles/tools.dir/KfsRmdir.o"
	cd /home/fify/Project/KFS/trunk/src/cc/tools && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/tools.dir/KfsRmdir.o -c /home/fify/Project/KFS/trunk/src/cc/tools/KfsRmdir.cc

src/cc/tools/CMakeFiles/tools.dir/KfsRmdir.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/tools.dir/KfsRmdir.i"
	cd /home/fify/Project/KFS/trunk/src/cc/tools && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/fify/Project/KFS/trunk/src/cc/tools/KfsRmdir.cc > CMakeFiles/tools.dir/KfsRmdir.i

src/cc/tools/CMakeFiles/tools.dir/KfsRmdir.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/tools.dir/KfsRmdir.s"
	cd /home/fify/Project/KFS/trunk/src/cc/tools && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/fify/Project/KFS/trunk/src/cc/tools/KfsRmdir.cc -o CMakeFiles/tools.dir/KfsRmdir.s

src/cc/tools/CMakeFiles/tools.dir/KfsRmdir.o.requires:
.PHONY : src/cc/tools/CMakeFiles/tools.dir/KfsRmdir.o.requires

src/cc/tools/CMakeFiles/tools.dir/KfsRmdir.o.provides: src/cc/tools/CMakeFiles/tools.dir/KfsRmdir.o.requires
	$(MAKE) -f src/cc/tools/CMakeFiles/tools.dir/build.make src/cc/tools/CMakeFiles/tools.dir/KfsRmdir.o.provides.build
.PHONY : src/cc/tools/CMakeFiles/tools.dir/KfsRmdir.o.provides

src/cc/tools/CMakeFiles/tools.dir/KfsRmdir.o.provides.build: src/cc/tools/CMakeFiles/tools.dir/KfsRmdir.o
.PHONY : src/cc/tools/CMakeFiles/tools.dir/KfsRmdir.o.provides.build

src/cc/tools/CMakeFiles/tools.dir/KfsPwd.o: src/cc/tools/CMakeFiles/tools.dir/flags.make
src/cc/tools/CMakeFiles/tools.dir/KfsPwd.o: src/cc/tools/KfsPwd.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /home/fify/Project/KFS/trunk/CMakeFiles $(CMAKE_PROGRESS_10)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/cc/tools/CMakeFiles/tools.dir/KfsPwd.o"
	cd /home/fify/Project/KFS/trunk/src/cc/tools && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/tools.dir/KfsPwd.o -c /home/fify/Project/KFS/trunk/src/cc/tools/KfsPwd.cc

src/cc/tools/CMakeFiles/tools.dir/KfsPwd.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/tools.dir/KfsPwd.i"
	cd /home/fify/Project/KFS/trunk/src/cc/tools && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/fify/Project/KFS/trunk/src/cc/tools/KfsPwd.cc > CMakeFiles/tools.dir/KfsPwd.i

src/cc/tools/CMakeFiles/tools.dir/KfsPwd.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/tools.dir/KfsPwd.s"
	cd /home/fify/Project/KFS/trunk/src/cc/tools && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/fify/Project/KFS/trunk/src/cc/tools/KfsPwd.cc -o CMakeFiles/tools.dir/KfsPwd.s

src/cc/tools/CMakeFiles/tools.dir/KfsPwd.o.requires:
.PHONY : src/cc/tools/CMakeFiles/tools.dir/KfsPwd.o.requires

src/cc/tools/CMakeFiles/tools.dir/KfsPwd.o.provides: src/cc/tools/CMakeFiles/tools.dir/KfsPwd.o.requires
	$(MAKE) -f src/cc/tools/CMakeFiles/tools.dir/build.make src/cc/tools/CMakeFiles/tools.dir/KfsPwd.o.provides.build
.PHONY : src/cc/tools/CMakeFiles/tools.dir/KfsPwd.o.provides

src/cc/tools/CMakeFiles/tools.dir/KfsPwd.o.provides.build: src/cc/tools/CMakeFiles/tools.dir/KfsPwd.o
.PHONY : src/cc/tools/CMakeFiles/tools.dir/KfsPwd.o.provides.build

src/cc/tools/CMakeFiles/tools.dir/utils.o: src/cc/tools/CMakeFiles/tools.dir/flags.make
src/cc/tools/CMakeFiles/tools.dir/utils.o: src/cc/tools/utils.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /home/fify/Project/KFS/trunk/CMakeFiles $(CMAKE_PROGRESS_11)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/cc/tools/CMakeFiles/tools.dir/utils.o"
	cd /home/fify/Project/KFS/trunk/src/cc/tools && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/tools.dir/utils.o -c /home/fify/Project/KFS/trunk/src/cc/tools/utils.cc

src/cc/tools/CMakeFiles/tools.dir/utils.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/tools.dir/utils.i"
	cd /home/fify/Project/KFS/trunk/src/cc/tools && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/fify/Project/KFS/trunk/src/cc/tools/utils.cc > CMakeFiles/tools.dir/utils.i

src/cc/tools/CMakeFiles/tools.dir/utils.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/tools.dir/utils.s"
	cd /home/fify/Project/KFS/trunk/src/cc/tools && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/fify/Project/KFS/trunk/src/cc/tools/utils.cc -o CMakeFiles/tools.dir/utils.s

src/cc/tools/CMakeFiles/tools.dir/utils.o.requires:
.PHONY : src/cc/tools/CMakeFiles/tools.dir/utils.o.requires

src/cc/tools/CMakeFiles/tools.dir/utils.o.provides: src/cc/tools/CMakeFiles/tools.dir/utils.o.requires
	$(MAKE) -f src/cc/tools/CMakeFiles/tools.dir/build.make src/cc/tools/CMakeFiles/tools.dir/utils.o.provides.build
.PHONY : src/cc/tools/CMakeFiles/tools.dir/utils.o.provides

src/cc/tools/CMakeFiles/tools.dir/utils.o.provides.build: src/cc/tools/CMakeFiles/tools.dir/utils.o
.PHONY : src/cc/tools/CMakeFiles/tools.dir/utils.o.provides.build

# Object files for target tools
tools_OBJECTS = \
"CMakeFiles/tools.dir/MonUtils.o" \
"CMakeFiles/tools.dir/KfsCd.o" \
"CMakeFiles/tools.dir/KfsChangeReplication.o" \
"CMakeFiles/tools.dir/KfsCp.o" \
"CMakeFiles/tools.dir/KfsLs.o" \
"CMakeFiles/tools.dir/KfsMkdirs.o" \
"CMakeFiles/tools.dir/KfsMv.o" \
"CMakeFiles/tools.dir/KfsRm.o" \
"CMakeFiles/tools.dir/KfsRmdir.o" \
"CMakeFiles/tools.dir/KfsPwd.o" \
"CMakeFiles/tools.dir/utils.o"

# External object files for target tools
tools_EXTERNAL_OBJECTS =

src/cc/tools/libtools.a: src/cc/tools/CMakeFiles/tools.dir/MonUtils.o
src/cc/tools/libtools.a: src/cc/tools/CMakeFiles/tools.dir/KfsCd.o
src/cc/tools/libtools.a: src/cc/tools/CMakeFiles/tools.dir/KfsChangeReplication.o
src/cc/tools/libtools.a: src/cc/tools/CMakeFiles/tools.dir/KfsCp.o
src/cc/tools/libtools.a: src/cc/tools/CMakeFiles/tools.dir/KfsLs.o
src/cc/tools/libtools.a: src/cc/tools/CMakeFiles/tools.dir/KfsMkdirs.o
src/cc/tools/libtools.a: src/cc/tools/CMakeFiles/tools.dir/KfsMv.o
src/cc/tools/libtools.a: src/cc/tools/CMakeFiles/tools.dir/KfsRm.o
src/cc/tools/libtools.a: src/cc/tools/CMakeFiles/tools.dir/KfsRmdir.o
src/cc/tools/libtools.a: src/cc/tools/CMakeFiles/tools.dir/KfsPwd.o
src/cc/tools/libtools.a: src/cc/tools/CMakeFiles/tools.dir/utils.o
src/cc/tools/libtools.a: src/cc/tools/CMakeFiles/tools.dir/build.make
src/cc/tools/libtools.a: src/cc/tools/CMakeFiles/tools.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --red --bold "Linking CXX static library libtools.a"
	cd /home/fify/Project/KFS/trunk/src/cc/tools && $(CMAKE_COMMAND) -P CMakeFiles/tools.dir/cmake_clean_target.cmake
	cd /home/fify/Project/KFS/trunk/src/cc/tools && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/tools.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
src/cc/tools/CMakeFiles/tools.dir/build: src/cc/tools/libtools.a
.PHONY : src/cc/tools/CMakeFiles/tools.dir/build

src/cc/tools/CMakeFiles/tools.dir/requires: src/cc/tools/CMakeFiles/tools.dir/MonUtils.o.requires
src/cc/tools/CMakeFiles/tools.dir/requires: src/cc/tools/CMakeFiles/tools.dir/KfsCd.o.requires
src/cc/tools/CMakeFiles/tools.dir/requires: src/cc/tools/CMakeFiles/tools.dir/KfsChangeReplication.o.requires
src/cc/tools/CMakeFiles/tools.dir/requires: src/cc/tools/CMakeFiles/tools.dir/KfsCp.o.requires
src/cc/tools/CMakeFiles/tools.dir/requires: src/cc/tools/CMakeFiles/tools.dir/KfsLs.o.requires
src/cc/tools/CMakeFiles/tools.dir/requires: src/cc/tools/CMakeFiles/tools.dir/KfsMkdirs.o.requires
src/cc/tools/CMakeFiles/tools.dir/requires: src/cc/tools/CMakeFiles/tools.dir/KfsMv.o.requires
src/cc/tools/CMakeFiles/tools.dir/requires: src/cc/tools/CMakeFiles/tools.dir/KfsRm.o.requires
src/cc/tools/CMakeFiles/tools.dir/requires: src/cc/tools/CMakeFiles/tools.dir/KfsRmdir.o.requires
src/cc/tools/CMakeFiles/tools.dir/requires: src/cc/tools/CMakeFiles/tools.dir/KfsPwd.o.requires
src/cc/tools/CMakeFiles/tools.dir/requires: src/cc/tools/CMakeFiles/tools.dir/utils.o.requires
.PHONY : src/cc/tools/CMakeFiles/tools.dir/requires

src/cc/tools/CMakeFiles/tools.dir/clean:
	cd /home/fify/Project/KFS/trunk/src/cc/tools && $(CMAKE_COMMAND) -P CMakeFiles/tools.dir/cmake_clean.cmake
.PHONY : src/cc/tools/CMakeFiles/tools.dir/clean

src/cc/tools/CMakeFiles/tools.dir/depend:
	cd /home/fify/Project/KFS/trunk && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/fify/Project/KFS/trunk /home/fify/Project/KFS/trunk/src/cc/tools /home/fify/Project/KFS/trunk /home/fify/Project/KFS/trunk/src/cc/tools /home/fify/Project/KFS/trunk/src/cc/tools/CMakeFiles/tools.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : src/cc/tools/CMakeFiles/tools.dir/depend
