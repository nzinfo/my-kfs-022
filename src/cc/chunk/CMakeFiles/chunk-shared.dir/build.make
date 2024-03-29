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
include src/cc/chunk/CMakeFiles/chunk-shared.dir/depend.make

# Include the progress variables for this target.
include src/cc/chunk/CMakeFiles/chunk-shared.dir/progress.make

# Include the compile flags for this target's objects.
include src/cc/chunk/CMakeFiles/chunk-shared.dir/flags.make

src/cc/chunk/CMakeFiles/chunk-shared.dir/ChunkManager.o: src/cc/chunk/CMakeFiles/chunk-shared.dir/flags.make
src/cc/chunk/CMakeFiles/chunk-shared.dir/ChunkManager.o: src/cc/chunk/ChunkManager.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /home/fify/Project/KFS/trunk/CMakeFiles $(CMAKE_PROGRESS_1)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/cc/chunk/CMakeFiles/chunk-shared.dir/ChunkManager.o"
	cd /home/fify/Project/KFS/trunk/src/cc/chunk && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/chunk-shared.dir/ChunkManager.o -c /home/fify/Project/KFS/trunk/src/cc/chunk/ChunkManager.cc

src/cc/chunk/CMakeFiles/chunk-shared.dir/ChunkManager.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/chunk-shared.dir/ChunkManager.i"
	cd /home/fify/Project/KFS/trunk/src/cc/chunk && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/fify/Project/KFS/trunk/src/cc/chunk/ChunkManager.cc > CMakeFiles/chunk-shared.dir/ChunkManager.i

src/cc/chunk/CMakeFiles/chunk-shared.dir/ChunkManager.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/chunk-shared.dir/ChunkManager.s"
	cd /home/fify/Project/KFS/trunk/src/cc/chunk && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/fify/Project/KFS/trunk/src/cc/chunk/ChunkManager.cc -o CMakeFiles/chunk-shared.dir/ChunkManager.s

src/cc/chunk/CMakeFiles/chunk-shared.dir/ChunkManager.o.requires:
.PHONY : src/cc/chunk/CMakeFiles/chunk-shared.dir/ChunkManager.o.requires

src/cc/chunk/CMakeFiles/chunk-shared.dir/ChunkManager.o.provides: src/cc/chunk/CMakeFiles/chunk-shared.dir/ChunkManager.o.requires
	$(MAKE) -f src/cc/chunk/CMakeFiles/chunk-shared.dir/build.make src/cc/chunk/CMakeFiles/chunk-shared.dir/ChunkManager.o.provides.build
.PHONY : src/cc/chunk/CMakeFiles/chunk-shared.dir/ChunkManager.o.provides

src/cc/chunk/CMakeFiles/chunk-shared.dir/ChunkManager.o.provides.build: src/cc/chunk/CMakeFiles/chunk-shared.dir/ChunkManager.o
.PHONY : src/cc/chunk/CMakeFiles/chunk-shared.dir/ChunkManager.o.provides.build

src/cc/chunk/CMakeFiles/chunk-shared.dir/ChunkServer.o: src/cc/chunk/CMakeFiles/chunk-shared.dir/flags.make
src/cc/chunk/CMakeFiles/chunk-shared.dir/ChunkServer.o: src/cc/chunk/ChunkServer.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /home/fify/Project/KFS/trunk/CMakeFiles $(CMAKE_PROGRESS_2)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/cc/chunk/CMakeFiles/chunk-shared.dir/ChunkServer.o"
	cd /home/fify/Project/KFS/trunk/src/cc/chunk && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/chunk-shared.dir/ChunkServer.o -c /home/fify/Project/KFS/trunk/src/cc/chunk/ChunkServer.cc

src/cc/chunk/CMakeFiles/chunk-shared.dir/ChunkServer.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/chunk-shared.dir/ChunkServer.i"
	cd /home/fify/Project/KFS/trunk/src/cc/chunk && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/fify/Project/KFS/trunk/src/cc/chunk/ChunkServer.cc > CMakeFiles/chunk-shared.dir/ChunkServer.i

src/cc/chunk/CMakeFiles/chunk-shared.dir/ChunkServer.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/chunk-shared.dir/ChunkServer.s"
	cd /home/fify/Project/KFS/trunk/src/cc/chunk && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/fify/Project/KFS/trunk/src/cc/chunk/ChunkServer.cc -o CMakeFiles/chunk-shared.dir/ChunkServer.s

src/cc/chunk/CMakeFiles/chunk-shared.dir/ChunkServer.o.requires:
.PHONY : src/cc/chunk/CMakeFiles/chunk-shared.dir/ChunkServer.o.requires

src/cc/chunk/CMakeFiles/chunk-shared.dir/ChunkServer.o.provides: src/cc/chunk/CMakeFiles/chunk-shared.dir/ChunkServer.o.requires
	$(MAKE) -f src/cc/chunk/CMakeFiles/chunk-shared.dir/build.make src/cc/chunk/CMakeFiles/chunk-shared.dir/ChunkServer.o.provides.build
.PHONY : src/cc/chunk/CMakeFiles/chunk-shared.dir/ChunkServer.o.provides

src/cc/chunk/CMakeFiles/chunk-shared.dir/ChunkServer.o.provides.build: src/cc/chunk/CMakeFiles/chunk-shared.dir/ChunkServer.o
.PHONY : src/cc/chunk/CMakeFiles/chunk-shared.dir/ChunkServer.o.provides.build

src/cc/chunk/CMakeFiles/chunk-shared.dir/ClientManager.o: src/cc/chunk/CMakeFiles/chunk-shared.dir/flags.make
src/cc/chunk/CMakeFiles/chunk-shared.dir/ClientManager.o: src/cc/chunk/ClientManager.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /home/fify/Project/KFS/trunk/CMakeFiles $(CMAKE_PROGRESS_3)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/cc/chunk/CMakeFiles/chunk-shared.dir/ClientManager.o"
	cd /home/fify/Project/KFS/trunk/src/cc/chunk && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/chunk-shared.dir/ClientManager.o -c /home/fify/Project/KFS/trunk/src/cc/chunk/ClientManager.cc

src/cc/chunk/CMakeFiles/chunk-shared.dir/ClientManager.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/chunk-shared.dir/ClientManager.i"
	cd /home/fify/Project/KFS/trunk/src/cc/chunk && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/fify/Project/KFS/trunk/src/cc/chunk/ClientManager.cc > CMakeFiles/chunk-shared.dir/ClientManager.i

src/cc/chunk/CMakeFiles/chunk-shared.dir/ClientManager.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/chunk-shared.dir/ClientManager.s"
	cd /home/fify/Project/KFS/trunk/src/cc/chunk && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/fify/Project/KFS/trunk/src/cc/chunk/ClientManager.cc -o CMakeFiles/chunk-shared.dir/ClientManager.s

src/cc/chunk/CMakeFiles/chunk-shared.dir/ClientManager.o.requires:
.PHONY : src/cc/chunk/CMakeFiles/chunk-shared.dir/ClientManager.o.requires

src/cc/chunk/CMakeFiles/chunk-shared.dir/ClientManager.o.provides: src/cc/chunk/CMakeFiles/chunk-shared.dir/ClientManager.o.requires
	$(MAKE) -f src/cc/chunk/CMakeFiles/chunk-shared.dir/build.make src/cc/chunk/CMakeFiles/chunk-shared.dir/ClientManager.o.provides.build
.PHONY : src/cc/chunk/CMakeFiles/chunk-shared.dir/ClientManager.o.provides

src/cc/chunk/CMakeFiles/chunk-shared.dir/ClientManager.o.provides.build: src/cc/chunk/CMakeFiles/chunk-shared.dir/ClientManager.o
.PHONY : src/cc/chunk/CMakeFiles/chunk-shared.dir/ClientManager.o.provides.build

src/cc/chunk/CMakeFiles/chunk-shared.dir/ClientSM.o: src/cc/chunk/CMakeFiles/chunk-shared.dir/flags.make
src/cc/chunk/CMakeFiles/chunk-shared.dir/ClientSM.o: src/cc/chunk/ClientSM.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /home/fify/Project/KFS/trunk/CMakeFiles $(CMAKE_PROGRESS_4)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/cc/chunk/CMakeFiles/chunk-shared.dir/ClientSM.o"
	cd /home/fify/Project/KFS/trunk/src/cc/chunk && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/chunk-shared.dir/ClientSM.o -c /home/fify/Project/KFS/trunk/src/cc/chunk/ClientSM.cc

src/cc/chunk/CMakeFiles/chunk-shared.dir/ClientSM.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/chunk-shared.dir/ClientSM.i"
	cd /home/fify/Project/KFS/trunk/src/cc/chunk && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/fify/Project/KFS/trunk/src/cc/chunk/ClientSM.cc > CMakeFiles/chunk-shared.dir/ClientSM.i

src/cc/chunk/CMakeFiles/chunk-shared.dir/ClientSM.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/chunk-shared.dir/ClientSM.s"
	cd /home/fify/Project/KFS/trunk/src/cc/chunk && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/fify/Project/KFS/trunk/src/cc/chunk/ClientSM.cc -o CMakeFiles/chunk-shared.dir/ClientSM.s

src/cc/chunk/CMakeFiles/chunk-shared.dir/ClientSM.o.requires:
.PHONY : src/cc/chunk/CMakeFiles/chunk-shared.dir/ClientSM.o.requires

src/cc/chunk/CMakeFiles/chunk-shared.dir/ClientSM.o.provides: src/cc/chunk/CMakeFiles/chunk-shared.dir/ClientSM.o.requires
	$(MAKE) -f src/cc/chunk/CMakeFiles/chunk-shared.dir/build.make src/cc/chunk/CMakeFiles/chunk-shared.dir/ClientSM.o.provides.build
.PHONY : src/cc/chunk/CMakeFiles/chunk-shared.dir/ClientSM.o.provides

src/cc/chunk/CMakeFiles/chunk-shared.dir/ClientSM.o.provides.build: src/cc/chunk/CMakeFiles/chunk-shared.dir/ClientSM.o
.PHONY : src/cc/chunk/CMakeFiles/chunk-shared.dir/ClientSM.o.provides.build

src/cc/chunk/CMakeFiles/chunk-shared.dir/KfsOps.o: src/cc/chunk/CMakeFiles/chunk-shared.dir/flags.make
src/cc/chunk/CMakeFiles/chunk-shared.dir/KfsOps.o: src/cc/chunk/KfsOps.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /home/fify/Project/KFS/trunk/CMakeFiles $(CMAKE_PROGRESS_5)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/cc/chunk/CMakeFiles/chunk-shared.dir/KfsOps.o"
	cd /home/fify/Project/KFS/trunk/src/cc/chunk && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/chunk-shared.dir/KfsOps.o -c /home/fify/Project/KFS/trunk/src/cc/chunk/KfsOps.cc

src/cc/chunk/CMakeFiles/chunk-shared.dir/KfsOps.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/chunk-shared.dir/KfsOps.i"
	cd /home/fify/Project/KFS/trunk/src/cc/chunk && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/fify/Project/KFS/trunk/src/cc/chunk/KfsOps.cc > CMakeFiles/chunk-shared.dir/KfsOps.i

src/cc/chunk/CMakeFiles/chunk-shared.dir/KfsOps.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/chunk-shared.dir/KfsOps.s"
	cd /home/fify/Project/KFS/trunk/src/cc/chunk && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/fify/Project/KFS/trunk/src/cc/chunk/KfsOps.cc -o CMakeFiles/chunk-shared.dir/KfsOps.s

src/cc/chunk/CMakeFiles/chunk-shared.dir/KfsOps.o.requires:
.PHONY : src/cc/chunk/CMakeFiles/chunk-shared.dir/KfsOps.o.requires

src/cc/chunk/CMakeFiles/chunk-shared.dir/KfsOps.o.provides: src/cc/chunk/CMakeFiles/chunk-shared.dir/KfsOps.o.requires
	$(MAKE) -f src/cc/chunk/CMakeFiles/chunk-shared.dir/build.make src/cc/chunk/CMakeFiles/chunk-shared.dir/KfsOps.o.provides.build
.PHONY : src/cc/chunk/CMakeFiles/chunk-shared.dir/KfsOps.o.provides

src/cc/chunk/CMakeFiles/chunk-shared.dir/KfsOps.o.provides.build: src/cc/chunk/CMakeFiles/chunk-shared.dir/KfsOps.o
.PHONY : src/cc/chunk/CMakeFiles/chunk-shared.dir/KfsOps.o.provides.build

src/cc/chunk/CMakeFiles/chunk-shared.dir/LeaseClerk.o: src/cc/chunk/CMakeFiles/chunk-shared.dir/flags.make
src/cc/chunk/CMakeFiles/chunk-shared.dir/LeaseClerk.o: src/cc/chunk/LeaseClerk.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /home/fify/Project/KFS/trunk/CMakeFiles $(CMAKE_PROGRESS_6)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/cc/chunk/CMakeFiles/chunk-shared.dir/LeaseClerk.o"
	cd /home/fify/Project/KFS/trunk/src/cc/chunk && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/chunk-shared.dir/LeaseClerk.o -c /home/fify/Project/KFS/trunk/src/cc/chunk/LeaseClerk.cc

src/cc/chunk/CMakeFiles/chunk-shared.dir/LeaseClerk.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/chunk-shared.dir/LeaseClerk.i"
	cd /home/fify/Project/KFS/trunk/src/cc/chunk && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/fify/Project/KFS/trunk/src/cc/chunk/LeaseClerk.cc > CMakeFiles/chunk-shared.dir/LeaseClerk.i

src/cc/chunk/CMakeFiles/chunk-shared.dir/LeaseClerk.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/chunk-shared.dir/LeaseClerk.s"
	cd /home/fify/Project/KFS/trunk/src/cc/chunk && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/fify/Project/KFS/trunk/src/cc/chunk/LeaseClerk.cc -o CMakeFiles/chunk-shared.dir/LeaseClerk.s

src/cc/chunk/CMakeFiles/chunk-shared.dir/LeaseClerk.o.requires:
.PHONY : src/cc/chunk/CMakeFiles/chunk-shared.dir/LeaseClerk.o.requires

src/cc/chunk/CMakeFiles/chunk-shared.dir/LeaseClerk.o.provides: src/cc/chunk/CMakeFiles/chunk-shared.dir/LeaseClerk.o.requires
	$(MAKE) -f src/cc/chunk/CMakeFiles/chunk-shared.dir/build.make src/cc/chunk/CMakeFiles/chunk-shared.dir/LeaseClerk.o.provides.build
.PHONY : src/cc/chunk/CMakeFiles/chunk-shared.dir/LeaseClerk.o.provides

src/cc/chunk/CMakeFiles/chunk-shared.dir/LeaseClerk.o.provides.build: src/cc/chunk/CMakeFiles/chunk-shared.dir/LeaseClerk.o
.PHONY : src/cc/chunk/CMakeFiles/chunk-shared.dir/LeaseClerk.o.provides.build

src/cc/chunk/CMakeFiles/chunk-shared.dir/Logger.o: src/cc/chunk/CMakeFiles/chunk-shared.dir/flags.make
src/cc/chunk/CMakeFiles/chunk-shared.dir/Logger.o: src/cc/chunk/Logger.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /home/fify/Project/KFS/trunk/CMakeFiles $(CMAKE_PROGRESS_7)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/cc/chunk/CMakeFiles/chunk-shared.dir/Logger.o"
	cd /home/fify/Project/KFS/trunk/src/cc/chunk && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/chunk-shared.dir/Logger.o -c /home/fify/Project/KFS/trunk/src/cc/chunk/Logger.cc

src/cc/chunk/CMakeFiles/chunk-shared.dir/Logger.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/chunk-shared.dir/Logger.i"
	cd /home/fify/Project/KFS/trunk/src/cc/chunk && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/fify/Project/KFS/trunk/src/cc/chunk/Logger.cc > CMakeFiles/chunk-shared.dir/Logger.i

src/cc/chunk/CMakeFiles/chunk-shared.dir/Logger.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/chunk-shared.dir/Logger.s"
	cd /home/fify/Project/KFS/trunk/src/cc/chunk && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/fify/Project/KFS/trunk/src/cc/chunk/Logger.cc -o CMakeFiles/chunk-shared.dir/Logger.s

src/cc/chunk/CMakeFiles/chunk-shared.dir/Logger.o.requires:
.PHONY : src/cc/chunk/CMakeFiles/chunk-shared.dir/Logger.o.requires

src/cc/chunk/CMakeFiles/chunk-shared.dir/Logger.o.provides: src/cc/chunk/CMakeFiles/chunk-shared.dir/Logger.o.requires
	$(MAKE) -f src/cc/chunk/CMakeFiles/chunk-shared.dir/build.make src/cc/chunk/CMakeFiles/chunk-shared.dir/Logger.o.provides.build
.PHONY : src/cc/chunk/CMakeFiles/chunk-shared.dir/Logger.o.provides

src/cc/chunk/CMakeFiles/chunk-shared.dir/Logger.o.provides.build: src/cc/chunk/CMakeFiles/chunk-shared.dir/Logger.o
.PHONY : src/cc/chunk/CMakeFiles/chunk-shared.dir/Logger.o.provides.build

src/cc/chunk/CMakeFiles/chunk-shared.dir/MetaServerSM.o: src/cc/chunk/CMakeFiles/chunk-shared.dir/flags.make
src/cc/chunk/CMakeFiles/chunk-shared.dir/MetaServerSM.o: src/cc/chunk/MetaServerSM.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /home/fify/Project/KFS/trunk/CMakeFiles $(CMAKE_PROGRESS_8)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/cc/chunk/CMakeFiles/chunk-shared.dir/MetaServerSM.o"
	cd /home/fify/Project/KFS/trunk/src/cc/chunk && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/chunk-shared.dir/MetaServerSM.o -c /home/fify/Project/KFS/trunk/src/cc/chunk/MetaServerSM.cc

src/cc/chunk/CMakeFiles/chunk-shared.dir/MetaServerSM.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/chunk-shared.dir/MetaServerSM.i"
	cd /home/fify/Project/KFS/trunk/src/cc/chunk && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/fify/Project/KFS/trunk/src/cc/chunk/MetaServerSM.cc > CMakeFiles/chunk-shared.dir/MetaServerSM.i

src/cc/chunk/CMakeFiles/chunk-shared.dir/MetaServerSM.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/chunk-shared.dir/MetaServerSM.s"
	cd /home/fify/Project/KFS/trunk/src/cc/chunk && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/fify/Project/KFS/trunk/src/cc/chunk/MetaServerSM.cc -o CMakeFiles/chunk-shared.dir/MetaServerSM.s

src/cc/chunk/CMakeFiles/chunk-shared.dir/MetaServerSM.o.requires:
.PHONY : src/cc/chunk/CMakeFiles/chunk-shared.dir/MetaServerSM.o.requires

src/cc/chunk/CMakeFiles/chunk-shared.dir/MetaServerSM.o.provides: src/cc/chunk/CMakeFiles/chunk-shared.dir/MetaServerSM.o.requires
	$(MAKE) -f src/cc/chunk/CMakeFiles/chunk-shared.dir/build.make src/cc/chunk/CMakeFiles/chunk-shared.dir/MetaServerSM.o.provides.build
.PHONY : src/cc/chunk/CMakeFiles/chunk-shared.dir/MetaServerSM.o.provides

src/cc/chunk/CMakeFiles/chunk-shared.dir/MetaServerSM.o.provides.build: src/cc/chunk/CMakeFiles/chunk-shared.dir/MetaServerSM.o
.PHONY : src/cc/chunk/CMakeFiles/chunk-shared.dir/MetaServerSM.o.provides.build

src/cc/chunk/CMakeFiles/chunk-shared.dir/RemoteSyncSM.o: src/cc/chunk/CMakeFiles/chunk-shared.dir/flags.make
src/cc/chunk/CMakeFiles/chunk-shared.dir/RemoteSyncSM.o: src/cc/chunk/RemoteSyncSM.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /home/fify/Project/KFS/trunk/CMakeFiles $(CMAKE_PROGRESS_9)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/cc/chunk/CMakeFiles/chunk-shared.dir/RemoteSyncSM.o"
	cd /home/fify/Project/KFS/trunk/src/cc/chunk && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/chunk-shared.dir/RemoteSyncSM.o -c /home/fify/Project/KFS/trunk/src/cc/chunk/RemoteSyncSM.cc

src/cc/chunk/CMakeFiles/chunk-shared.dir/RemoteSyncSM.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/chunk-shared.dir/RemoteSyncSM.i"
	cd /home/fify/Project/KFS/trunk/src/cc/chunk && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/fify/Project/KFS/trunk/src/cc/chunk/RemoteSyncSM.cc > CMakeFiles/chunk-shared.dir/RemoteSyncSM.i

src/cc/chunk/CMakeFiles/chunk-shared.dir/RemoteSyncSM.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/chunk-shared.dir/RemoteSyncSM.s"
	cd /home/fify/Project/KFS/trunk/src/cc/chunk && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/fify/Project/KFS/trunk/src/cc/chunk/RemoteSyncSM.cc -o CMakeFiles/chunk-shared.dir/RemoteSyncSM.s

src/cc/chunk/CMakeFiles/chunk-shared.dir/RemoteSyncSM.o.requires:
.PHONY : src/cc/chunk/CMakeFiles/chunk-shared.dir/RemoteSyncSM.o.requires

src/cc/chunk/CMakeFiles/chunk-shared.dir/RemoteSyncSM.o.provides: src/cc/chunk/CMakeFiles/chunk-shared.dir/RemoteSyncSM.o.requires
	$(MAKE) -f src/cc/chunk/CMakeFiles/chunk-shared.dir/build.make src/cc/chunk/CMakeFiles/chunk-shared.dir/RemoteSyncSM.o.provides.build
.PHONY : src/cc/chunk/CMakeFiles/chunk-shared.dir/RemoteSyncSM.o.provides

src/cc/chunk/CMakeFiles/chunk-shared.dir/RemoteSyncSM.o.provides.build: src/cc/chunk/CMakeFiles/chunk-shared.dir/RemoteSyncSM.o
.PHONY : src/cc/chunk/CMakeFiles/chunk-shared.dir/RemoteSyncSM.o.provides.build

src/cc/chunk/CMakeFiles/chunk-shared.dir/Replicator.o: src/cc/chunk/CMakeFiles/chunk-shared.dir/flags.make
src/cc/chunk/CMakeFiles/chunk-shared.dir/Replicator.o: src/cc/chunk/Replicator.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /home/fify/Project/KFS/trunk/CMakeFiles $(CMAKE_PROGRESS_10)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/cc/chunk/CMakeFiles/chunk-shared.dir/Replicator.o"
	cd /home/fify/Project/KFS/trunk/src/cc/chunk && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/chunk-shared.dir/Replicator.o -c /home/fify/Project/KFS/trunk/src/cc/chunk/Replicator.cc

src/cc/chunk/CMakeFiles/chunk-shared.dir/Replicator.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/chunk-shared.dir/Replicator.i"
	cd /home/fify/Project/KFS/trunk/src/cc/chunk && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/fify/Project/KFS/trunk/src/cc/chunk/Replicator.cc > CMakeFiles/chunk-shared.dir/Replicator.i

src/cc/chunk/CMakeFiles/chunk-shared.dir/Replicator.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/chunk-shared.dir/Replicator.s"
	cd /home/fify/Project/KFS/trunk/src/cc/chunk && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/fify/Project/KFS/trunk/src/cc/chunk/Replicator.cc -o CMakeFiles/chunk-shared.dir/Replicator.s

src/cc/chunk/CMakeFiles/chunk-shared.dir/Replicator.o.requires:
.PHONY : src/cc/chunk/CMakeFiles/chunk-shared.dir/Replicator.o.requires

src/cc/chunk/CMakeFiles/chunk-shared.dir/Replicator.o.provides: src/cc/chunk/CMakeFiles/chunk-shared.dir/Replicator.o.requires
	$(MAKE) -f src/cc/chunk/CMakeFiles/chunk-shared.dir/build.make src/cc/chunk/CMakeFiles/chunk-shared.dir/Replicator.o.provides.build
.PHONY : src/cc/chunk/CMakeFiles/chunk-shared.dir/Replicator.o.provides

src/cc/chunk/CMakeFiles/chunk-shared.dir/Replicator.o.provides.build: src/cc/chunk/CMakeFiles/chunk-shared.dir/Replicator.o
.PHONY : src/cc/chunk/CMakeFiles/chunk-shared.dir/Replicator.o.provides.build

src/cc/chunk/CMakeFiles/chunk-shared.dir/Utils.o: src/cc/chunk/CMakeFiles/chunk-shared.dir/flags.make
src/cc/chunk/CMakeFiles/chunk-shared.dir/Utils.o: src/cc/chunk/Utils.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /home/fify/Project/KFS/trunk/CMakeFiles $(CMAKE_PROGRESS_11)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/cc/chunk/CMakeFiles/chunk-shared.dir/Utils.o"
	cd /home/fify/Project/KFS/trunk/src/cc/chunk && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/chunk-shared.dir/Utils.o -c /home/fify/Project/KFS/trunk/src/cc/chunk/Utils.cc

src/cc/chunk/CMakeFiles/chunk-shared.dir/Utils.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/chunk-shared.dir/Utils.i"
	cd /home/fify/Project/KFS/trunk/src/cc/chunk && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/fify/Project/KFS/trunk/src/cc/chunk/Utils.cc > CMakeFiles/chunk-shared.dir/Utils.i

src/cc/chunk/CMakeFiles/chunk-shared.dir/Utils.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/chunk-shared.dir/Utils.s"
	cd /home/fify/Project/KFS/trunk/src/cc/chunk && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/fify/Project/KFS/trunk/src/cc/chunk/Utils.cc -o CMakeFiles/chunk-shared.dir/Utils.s

src/cc/chunk/CMakeFiles/chunk-shared.dir/Utils.o.requires:
.PHONY : src/cc/chunk/CMakeFiles/chunk-shared.dir/Utils.o.requires

src/cc/chunk/CMakeFiles/chunk-shared.dir/Utils.o.provides: src/cc/chunk/CMakeFiles/chunk-shared.dir/Utils.o.requires
	$(MAKE) -f src/cc/chunk/CMakeFiles/chunk-shared.dir/build.make src/cc/chunk/CMakeFiles/chunk-shared.dir/Utils.o.provides.build
.PHONY : src/cc/chunk/CMakeFiles/chunk-shared.dir/Utils.o.provides

src/cc/chunk/CMakeFiles/chunk-shared.dir/Utils.o.provides.build: src/cc/chunk/CMakeFiles/chunk-shared.dir/Utils.o
.PHONY : src/cc/chunk/CMakeFiles/chunk-shared.dir/Utils.o.provides.build

# Object files for target chunk-shared
chunk__shared_OBJECTS = \
"CMakeFiles/chunk-shared.dir/ChunkManager.o" \
"CMakeFiles/chunk-shared.dir/ChunkServer.o" \
"CMakeFiles/chunk-shared.dir/ClientManager.o" \
"CMakeFiles/chunk-shared.dir/ClientSM.o" \
"CMakeFiles/chunk-shared.dir/KfsOps.o" \
"CMakeFiles/chunk-shared.dir/LeaseClerk.o" \
"CMakeFiles/chunk-shared.dir/Logger.o" \
"CMakeFiles/chunk-shared.dir/MetaServerSM.o" \
"CMakeFiles/chunk-shared.dir/RemoteSyncSM.o" \
"CMakeFiles/chunk-shared.dir/Replicator.o" \
"CMakeFiles/chunk-shared.dir/Utils.o"

# External object files for target chunk-shared
chunk__shared_EXTERNAL_OBJECTS =

src/cc/chunk/libchunk.so: src/cc/chunk/CMakeFiles/chunk-shared.dir/ChunkManager.o
src/cc/chunk/libchunk.so: src/cc/chunk/CMakeFiles/chunk-shared.dir/ChunkServer.o
src/cc/chunk/libchunk.so: src/cc/chunk/CMakeFiles/chunk-shared.dir/ClientManager.o
src/cc/chunk/libchunk.so: src/cc/chunk/CMakeFiles/chunk-shared.dir/ClientSM.o
src/cc/chunk/libchunk.so: src/cc/chunk/CMakeFiles/chunk-shared.dir/KfsOps.o
src/cc/chunk/libchunk.so: src/cc/chunk/CMakeFiles/chunk-shared.dir/LeaseClerk.o
src/cc/chunk/libchunk.so: src/cc/chunk/CMakeFiles/chunk-shared.dir/Logger.o
src/cc/chunk/libchunk.so: src/cc/chunk/CMakeFiles/chunk-shared.dir/MetaServerSM.o
src/cc/chunk/libchunk.so: src/cc/chunk/CMakeFiles/chunk-shared.dir/RemoteSyncSM.o
src/cc/chunk/libchunk.so: src/cc/chunk/CMakeFiles/chunk-shared.dir/Replicator.o
src/cc/chunk/libchunk.so: src/cc/chunk/CMakeFiles/chunk-shared.dir/Utils.o
src/cc/chunk/libchunk.so: src/cc/chunk/CMakeFiles/chunk-shared.dir/build.make
src/cc/chunk/libchunk.so: src/cc/chunk/CMakeFiles/chunk-shared.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --red --bold "Linking CXX shared library libchunk.so"
	cd /home/fify/Project/KFS/trunk/src/cc/chunk && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/chunk-shared.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
src/cc/chunk/CMakeFiles/chunk-shared.dir/build: src/cc/chunk/libchunk.so
.PHONY : src/cc/chunk/CMakeFiles/chunk-shared.dir/build

src/cc/chunk/CMakeFiles/chunk-shared.dir/requires: src/cc/chunk/CMakeFiles/chunk-shared.dir/ChunkManager.o.requires
src/cc/chunk/CMakeFiles/chunk-shared.dir/requires: src/cc/chunk/CMakeFiles/chunk-shared.dir/ChunkServer.o.requires
src/cc/chunk/CMakeFiles/chunk-shared.dir/requires: src/cc/chunk/CMakeFiles/chunk-shared.dir/ClientManager.o.requires
src/cc/chunk/CMakeFiles/chunk-shared.dir/requires: src/cc/chunk/CMakeFiles/chunk-shared.dir/ClientSM.o.requires
src/cc/chunk/CMakeFiles/chunk-shared.dir/requires: src/cc/chunk/CMakeFiles/chunk-shared.dir/KfsOps.o.requires
src/cc/chunk/CMakeFiles/chunk-shared.dir/requires: src/cc/chunk/CMakeFiles/chunk-shared.dir/LeaseClerk.o.requires
src/cc/chunk/CMakeFiles/chunk-shared.dir/requires: src/cc/chunk/CMakeFiles/chunk-shared.dir/Logger.o.requires
src/cc/chunk/CMakeFiles/chunk-shared.dir/requires: src/cc/chunk/CMakeFiles/chunk-shared.dir/MetaServerSM.o.requires
src/cc/chunk/CMakeFiles/chunk-shared.dir/requires: src/cc/chunk/CMakeFiles/chunk-shared.dir/RemoteSyncSM.o.requires
src/cc/chunk/CMakeFiles/chunk-shared.dir/requires: src/cc/chunk/CMakeFiles/chunk-shared.dir/Replicator.o.requires
src/cc/chunk/CMakeFiles/chunk-shared.dir/requires: src/cc/chunk/CMakeFiles/chunk-shared.dir/Utils.o.requires
.PHONY : src/cc/chunk/CMakeFiles/chunk-shared.dir/requires

src/cc/chunk/CMakeFiles/chunk-shared.dir/clean:
	cd /home/fify/Project/KFS/trunk/src/cc/chunk && $(CMAKE_COMMAND) -P CMakeFiles/chunk-shared.dir/cmake_clean.cmake
.PHONY : src/cc/chunk/CMakeFiles/chunk-shared.dir/clean

src/cc/chunk/CMakeFiles/chunk-shared.dir/depend:
	cd /home/fify/Project/KFS/trunk && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/fify/Project/KFS/trunk /home/fify/Project/KFS/trunk/src/cc/chunk /home/fify/Project/KFS/trunk /home/fify/Project/KFS/trunk/src/cc/chunk /home/fify/Project/KFS/trunk/src/cc/chunk/CMakeFiles/chunk-shared.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : src/cc/chunk/CMakeFiles/chunk-shared.dir/depend

