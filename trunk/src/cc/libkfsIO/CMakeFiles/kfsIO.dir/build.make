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
include src/cc/libkfsIO/CMakeFiles/kfsIO.dir/depend.make

# Include the progress variables for this target.
include src/cc/libkfsIO/CMakeFiles/kfsIO.dir/progress.make

# Include the compile flags for this target's objects.
include src/cc/libkfsIO/CMakeFiles/kfsIO.dir/flags.make

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Checksum.o: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/flags.make
src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Checksum.o: src/cc/libkfsIO/Checksum.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /home/fify/Project/KFS/trunk/CMakeFiles $(CMAKE_PROGRESS_1)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Checksum.o"
	cd /home/fify/Project/KFS/trunk/src/cc/libkfsIO && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/kfsIO.dir/Checksum.o -c /home/fify/Project/KFS/trunk/src/cc/libkfsIO/Checksum.cc

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Checksum.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/kfsIO.dir/Checksum.i"
	cd /home/fify/Project/KFS/trunk/src/cc/libkfsIO && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/fify/Project/KFS/trunk/src/cc/libkfsIO/Checksum.cc > CMakeFiles/kfsIO.dir/Checksum.i

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Checksum.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/kfsIO.dir/Checksum.s"
	cd /home/fify/Project/KFS/trunk/src/cc/libkfsIO && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/fify/Project/KFS/trunk/src/cc/libkfsIO/Checksum.cc -o CMakeFiles/kfsIO.dir/Checksum.s

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Checksum.o.requires:
.PHONY : src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Checksum.o.requires

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Checksum.o.provides: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Checksum.o.requires
	$(MAKE) -f src/cc/libkfsIO/CMakeFiles/kfsIO.dir/build.make src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Checksum.o.provides.build
.PHONY : src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Checksum.o.provides

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Checksum.o.provides.build: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Checksum.o
.PHONY : src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Checksum.o.provides.build

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/TcpSocket.o: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/flags.make
src/cc/libkfsIO/CMakeFiles/kfsIO.dir/TcpSocket.o: src/cc/libkfsIO/TcpSocket.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /home/fify/Project/KFS/trunk/CMakeFiles $(CMAKE_PROGRESS_2)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/cc/libkfsIO/CMakeFiles/kfsIO.dir/TcpSocket.o"
	cd /home/fify/Project/KFS/trunk/src/cc/libkfsIO && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/kfsIO.dir/TcpSocket.o -c /home/fify/Project/KFS/trunk/src/cc/libkfsIO/TcpSocket.cc

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/TcpSocket.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/kfsIO.dir/TcpSocket.i"
	cd /home/fify/Project/KFS/trunk/src/cc/libkfsIO && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/fify/Project/KFS/trunk/src/cc/libkfsIO/TcpSocket.cc > CMakeFiles/kfsIO.dir/TcpSocket.i

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/TcpSocket.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/kfsIO.dir/TcpSocket.s"
	cd /home/fify/Project/KFS/trunk/src/cc/libkfsIO && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/fify/Project/KFS/trunk/src/cc/libkfsIO/TcpSocket.cc -o CMakeFiles/kfsIO.dir/TcpSocket.s

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/TcpSocket.o.requires:
.PHONY : src/cc/libkfsIO/CMakeFiles/kfsIO.dir/TcpSocket.o.requires

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/TcpSocket.o.provides: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/TcpSocket.o.requires
	$(MAKE) -f src/cc/libkfsIO/CMakeFiles/kfsIO.dir/build.make src/cc/libkfsIO/CMakeFiles/kfsIO.dir/TcpSocket.o.provides.build
.PHONY : src/cc/libkfsIO/CMakeFiles/kfsIO.dir/TcpSocket.o.provides

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/TcpSocket.o.provides.build: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/TcpSocket.o
.PHONY : src/cc/libkfsIO/CMakeFiles/kfsIO.dir/TcpSocket.o.provides.build

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Globals.o: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/flags.make
src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Globals.o: src/cc/libkfsIO/Globals.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /home/fify/Project/KFS/trunk/CMakeFiles $(CMAKE_PROGRESS_3)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Globals.o"
	cd /home/fify/Project/KFS/trunk/src/cc/libkfsIO && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/kfsIO.dir/Globals.o -c /home/fify/Project/KFS/trunk/src/cc/libkfsIO/Globals.cc

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Globals.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/kfsIO.dir/Globals.i"
	cd /home/fify/Project/KFS/trunk/src/cc/libkfsIO && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/fify/Project/KFS/trunk/src/cc/libkfsIO/Globals.cc > CMakeFiles/kfsIO.dir/Globals.i

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Globals.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/kfsIO.dir/Globals.s"
	cd /home/fify/Project/KFS/trunk/src/cc/libkfsIO && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/fify/Project/KFS/trunk/src/cc/libkfsIO/Globals.cc -o CMakeFiles/kfsIO.dir/Globals.s

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Globals.o.requires:
.PHONY : src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Globals.o.requires

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Globals.o.provides: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Globals.o.requires
	$(MAKE) -f src/cc/libkfsIO/CMakeFiles/kfsIO.dir/build.make src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Globals.o.provides.build
.PHONY : src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Globals.o.provides

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Globals.o.provides.build: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Globals.o
.PHONY : src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Globals.o.provides.build

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/BufferedSocket.o: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/flags.make
src/cc/libkfsIO/CMakeFiles/kfsIO.dir/BufferedSocket.o: src/cc/libkfsIO/BufferedSocket.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /home/fify/Project/KFS/trunk/CMakeFiles $(CMAKE_PROGRESS_4)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/cc/libkfsIO/CMakeFiles/kfsIO.dir/BufferedSocket.o"
	cd /home/fify/Project/KFS/trunk/src/cc/libkfsIO && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/kfsIO.dir/BufferedSocket.o -c /home/fify/Project/KFS/trunk/src/cc/libkfsIO/BufferedSocket.cc

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/BufferedSocket.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/kfsIO.dir/BufferedSocket.i"
	cd /home/fify/Project/KFS/trunk/src/cc/libkfsIO && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/fify/Project/KFS/trunk/src/cc/libkfsIO/BufferedSocket.cc > CMakeFiles/kfsIO.dir/BufferedSocket.i

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/BufferedSocket.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/kfsIO.dir/BufferedSocket.s"
	cd /home/fify/Project/KFS/trunk/src/cc/libkfsIO && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/fify/Project/KFS/trunk/src/cc/libkfsIO/BufferedSocket.cc -o CMakeFiles/kfsIO.dir/BufferedSocket.s

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/BufferedSocket.o.requires:
.PHONY : src/cc/libkfsIO/CMakeFiles/kfsIO.dir/BufferedSocket.o.requires

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/BufferedSocket.o.provides: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/BufferedSocket.o.requires
	$(MAKE) -f src/cc/libkfsIO/CMakeFiles/kfsIO.dir/build.make src/cc/libkfsIO/CMakeFiles/kfsIO.dir/BufferedSocket.o.provides.build
.PHONY : src/cc/libkfsIO/CMakeFiles/kfsIO.dir/BufferedSocket.o.provides

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/BufferedSocket.o.provides.build: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/BufferedSocket.o
.PHONY : src/cc/libkfsIO/CMakeFiles/kfsIO.dir/BufferedSocket.o.provides.build

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetManager.o: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/flags.make
src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetManager.o: src/cc/libkfsIO/NetManager.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /home/fify/Project/KFS/trunk/CMakeFiles $(CMAKE_PROGRESS_5)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetManager.o"
	cd /home/fify/Project/KFS/trunk/src/cc/libkfsIO && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/kfsIO.dir/NetManager.o -c /home/fify/Project/KFS/trunk/src/cc/libkfsIO/NetManager.cc

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetManager.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/kfsIO.dir/NetManager.i"
	cd /home/fify/Project/KFS/trunk/src/cc/libkfsIO && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/fify/Project/KFS/trunk/src/cc/libkfsIO/NetManager.cc > CMakeFiles/kfsIO.dir/NetManager.i

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetManager.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/kfsIO.dir/NetManager.s"
	cd /home/fify/Project/KFS/trunk/src/cc/libkfsIO && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/fify/Project/KFS/trunk/src/cc/libkfsIO/NetManager.cc -o CMakeFiles/kfsIO.dir/NetManager.s

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetManager.o.requires:
.PHONY : src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetManager.o.requires

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetManager.o.provides: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetManager.o.requires
	$(MAKE) -f src/cc/libkfsIO/CMakeFiles/kfsIO.dir/build.make src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetManager.o.provides.build
.PHONY : src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetManager.o.provides

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetManager.o.provides.build: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetManager.o
.PHONY : src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetManager.o.provides.build

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/DiskManager.o: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/flags.make
src/cc/libkfsIO/CMakeFiles/kfsIO.dir/DiskManager.o: src/cc/libkfsIO/DiskManager.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /home/fify/Project/KFS/trunk/CMakeFiles $(CMAKE_PROGRESS_6)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/cc/libkfsIO/CMakeFiles/kfsIO.dir/DiskManager.o"
	cd /home/fify/Project/KFS/trunk/src/cc/libkfsIO && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/kfsIO.dir/DiskManager.o -c /home/fify/Project/KFS/trunk/src/cc/libkfsIO/DiskManager.cc

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/DiskManager.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/kfsIO.dir/DiskManager.i"
	cd /home/fify/Project/KFS/trunk/src/cc/libkfsIO && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/fify/Project/KFS/trunk/src/cc/libkfsIO/DiskManager.cc > CMakeFiles/kfsIO.dir/DiskManager.i

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/DiskManager.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/kfsIO.dir/DiskManager.s"
	cd /home/fify/Project/KFS/trunk/src/cc/libkfsIO && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/fify/Project/KFS/trunk/src/cc/libkfsIO/DiskManager.cc -o CMakeFiles/kfsIO.dir/DiskManager.s

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/DiskManager.o.requires:
.PHONY : src/cc/libkfsIO/CMakeFiles/kfsIO.dir/DiskManager.o.requires

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/DiskManager.o.provides: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/DiskManager.o.requires
	$(MAKE) -f src/cc/libkfsIO/CMakeFiles/kfsIO.dir/build.make src/cc/libkfsIO/CMakeFiles/kfsIO.dir/DiskManager.o.provides.build
.PHONY : src/cc/libkfsIO/CMakeFiles/kfsIO.dir/DiskManager.o.provides

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/DiskManager.o.provides.build: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/DiskManager.o
.PHONY : src/cc/libkfsIO/CMakeFiles/kfsIO.dir/DiskManager.o.provides.build

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Acceptor.o: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/flags.make
src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Acceptor.o: src/cc/libkfsIO/Acceptor.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /home/fify/Project/KFS/trunk/CMakeFiles $(CMAKE_PROGRESS_7)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Acceptor.o"
	cd /home/fify/Project/KFS/trunk/src/cc/libkfsIO && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/kfsIO.dir/Acceptor.o -c /home/fify/Project/KFS/trunk/src/cc/libkfsIO/Acceptor.cc

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Acceptor.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/kfsIO.dir/Acceptor.i"
	cd /home/fify/Project/KFS/trunk/src/cc/libkfsIO && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/fify/Project/KFS/trunk/src/cc/libkfsIO/Acceptor.cc > CMakeFiles/kfsIO.dir/Acceptor.i

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Acceptor.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/kfsIO.dir/Acceptor.s"
	cd /home/fify/Project/KFS/trunk/src/cc/libkfsIO && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/fify/Project/KFS/trunk/src/cc/libkfsIO/Acceptor.cc -o CMakeFiles/kfsIO.dir/Acceptor.s

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Acceptor.o.requires:
.PHONY : src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Acceptor.o.requires

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Acceptor.o.provides: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Acceptor.o.requires
	$(MAKE) -f src/cc/libkfsIO/CMakeFiles/kfsIO.dir/build.make src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Acceptor.o.provides.build
.PHONY : src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Acceptor.o.provides

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Acceptor.o.provides.build: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Acceptor.o
.PHONY : src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Acceptor.o.provides.build

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetKicker.o: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/flags.make
src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetKicker.o: src/cc/libkfsIO/NetKicker.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /home/fify/Project/KFS/trunk/CMakeFiles $(CMAKE_PROGRESS_8)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetKicker.o"
	cd /home/fify/Project/KFS/trunk/src/cc/libkfsIO && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/kfsIO.dir/NetKicker.o -c /home/fify/Project/KFS/trunk/src/cc/libkfsIO/NetKicker.cc

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetKicker.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/kfsIO.dir/NetKicker.i"
	cd /home/fify/Project/KFS/trunk/src/cc/libkfsIO && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/fify/Project/KFS/trunk/src/cc/libkfsIO/NetKicker.cc > CMakeFiles/kfsIO.dir/NetKicker.i

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetKicker.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/kfsIO.dir/NetKicker.s"
	cd /home/fify/Project/KFS/trunk/src/cc/libkfsIO && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/fify/Project/KFS/trunk/src/cc/libkfsIO/NetKicker.cc -o CMakeFiles/kfsIO.dir/NetKicker.s

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetKicker.o.requires:
.PHONY : src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetKicker.o.requires

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetKicker.o.provides: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetKicker.o.requires
	$(MAKE) -f src/cc/libkfsIO/CMakeFiles/kfsIO.dir/build.make src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetKicker.o.provides.build
.PHONY : src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetKicker.o.provides

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetKicker.o.provides.build: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetKicker.o
.PHONY : src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetKicker.o.provides.build

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/EventManager.o: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/flags.make
src/cc/libkfsIO/CMakeFiles/kfsIO.dir/EventManager.o: src/cc/libkfsIO/EventManager.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /home/fify/Project/KFS/trunk/CMakeFiles $(CMAKE_PROGRESS_9)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/cc/libkfsIO/CMakeFiles/kfsIO.dir/EventManager.o"
	cd /home/fify/Project/KFS/trunk/src/cc/libkfsIO && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/kfsIO.dir/EventManager.o -c /home/fify/Project/KFS/trunk/src/cc/libkfsIO/EventManager.cc

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/EventManager.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/kfsIO.dir/EventManager.i"
	cd /home/fify/Project/KFS/trunk/src/cc/libkfsIO && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/fify/Project/KFS/trunk/src/cc/libkfsIO/EventManager.cc > CMakeFiles/kfsIO.dir/EventManager.i

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/EventManager.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/kfsIO.dir/EventManager.s"
	cd /home/fify/Project/KFS/trunk/src/cc/libkfsIO && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/fify/Project/KFS/trunk/src/cc/libkfsIO/EventManager.cc -o CMakeFiles/kfsIO.dir/EventManager.s

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/EventManager.o.requires:
.PHONY : src/cc/libkfsIO/CMakeFiles/kfsIO.dir/EventManager.o.requires

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/EventManager.o.provides: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/EventManager.o.requires
	$(MAKE) -f src/cc/libkfsIO/CMakeFiles/kfsIO.dir/build.make src/cc/libkfsIO/CMakeFiles/kfsIO.dir/EventManager.o.provides.build
.PHONY : src/cc/libkfsIO/CMakeFiles/kfsIO.dir/EventManager.o.provides

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/EventManager.o.provides.build: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/EventManager.o
.PHONY : src/cc/libkfsIO/CMakeFiles/kfsIO.dir/EventManager.o.provides.build

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Counter.o: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/flags.make
src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Counter.o: src/cc/libkfsIO/Counter.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /home/fify/Project/KFS/trunk/CMakeFiles $(CMAKE_PROGRESS_10)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Counter.o"
	cd /home/fify/Project/KFS/trunk/src/cc/libkfsIO && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/kfsIO.dir/Counter.o -c /home/fify/Project/KFS/trunk/src/cc/libkfsIO/Counter.cc

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Counter.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/kfsIO.dir/Counter.i"
	cd /home/fify/Project/KFS/trunk/src/cc/libkfsIO && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/fify/Project/KFS/trunk/src/cc/libkfsIO/Counter.cc > CMakeFiles/kfsIO.dir/Counter.i

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Counter.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/kfsIO.dir/Counter.s"
	cd /home/fify/Project/KFS/trunk/src/cc/libkfsIO && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/fify/Project/KFS/trunk/src/cc/libkfsIO/Counter.cc -o CMakeFiles/kfsIO.dir/Counter.s

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Counter.o.requires:
.PHONY : src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Counter.o.requires

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Counter.o.provides: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Counter.o.requires
	$(MAKE) -f src/cc/libkfsIO/CMakeFiles/kfsIO.dir/build.make src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Counter.o.provides.build
.PHONY : src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Counter.o.provides

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Counter.o.provides.build: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Counter.o
.PHONY : src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Counter.o.provides.build

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/DiskConnection.o: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/flags.make
src/cc/libkfsIO/CMakeFiles/kfsIO.dir/DiskConnection.o: src/cc/libkfsIO/DiskConnection.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /home/fify/Project/KFS/trunk/CMakeFiles $(CMAKE_PROGRESS_11)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/cc/libkfsIO/CMakeFiles/kfsIO.dir/DiskConnection.o"
	cd /home/fify/Project/KFS/trunk/src/cc/libkfsIO && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/kfsIO.dir/DiskConnection.o -c /home/fify/Project/KFS/trunk/src/cc/libkfsIO/DiskConnection.cc

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/DiskConnection.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/kfsIO.dir/DiskConnection.i"
	cd /home/fify/Project/KFS/trunk/src/cc/libkfsIO && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/fify/Project/KFS/trunk/src/cc/libkfsIO/DiskConnection.cc > CMakeFiles/kfsIO.dir/DiskConnection.i

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/DiskConnection.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/kfsIO.dir/DiskConnection.s"
	cd /home/fify/Project/KFS/trunk/src/cc/libkfsIO && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/fify/Project/KFS/trunk/src/cc/libkfsIO/DiskConnection.cc -o CMakeFiles/kfsIO.dir/DiskConnection.s

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/DiskConnection.o.requires:
.PHONY : src/cc/libkfsIO/CMakeFiles/kfsIO.dir/DiskConnection.o.requires

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/DiskConnection.o.provides: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/DiskConnection.o.requires
	$(MAKE) -f src/cc/libkfsIO/CMakeFiles/kfsIO.dir/build.make src/cc/libkfsIO/CMakeFiles/kfsIO.dir/DiskConnection.o.provides.build
.PHONY : src/cc/libkfsIO/CMakeFiles/kfsIO.dir/DiskConnection.o.provides

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/DiskConnection.o.provides.build: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/DiskConnection.o
.PHONY : src/cc/libkfsIO/CMakeFiles/kfsIO.dir/DiskConnection.o.provides.build

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetConnection.o: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/flags.make
src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetConnection.o: src/cc/libkfsIO/NetConnection.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /home/fify/Project/KFS/trunk/CMakeFiles $(CMAKE_PROGRESS_12)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetConnection.o"
	cd /home/fify/Project/KFS/trunk/src/cc/libkfsIO && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/kfsIO.dir/NetConnection.o -c /home/fify/Project/KFS/trunk/src/cc/libkfsIO/NetConnection.cc

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetConnection.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/kfsIO.dir/NetConnection.i"
	cd /home/fify/Project/KFS/trunk/src/cc/libkfsIO && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/fify/Project/KFS/trunk/src/cc/libkfsIO/NetConnection.cc > CMakeFiles/kfsIO.dir/NetConnection.i

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetConnection.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/kfsIO.dir/NetConnection.s"
	cd /home/fify/Project/KFS/trunk/src/cc/libkfsIO && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/fify/Project/KFS/trunk/src/cc/libkfsIO/NetConnection.cc -o CMakeFiles/kfsIO.dir/NetConnection.s

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetConnection.o.requires:
.PHONY : src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetConnection.o.requires

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetConnection.o.provides: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetConnection.o.requires
	$(MAKE) -f src/cc/libkfsIO/CMakeFiles/kfsIO.dir/build.make src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetConnection.o.provides.build
.PHONY : src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetConnection.o.provides

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetConnection.o.provides.build: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetConnection.o
.PHONY : src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetConnection.o.provides.build

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/IOBuffer.o: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/flags.make
src/cc/libkfsIO/CMakeFiles/kfsIO.dir/IOBuffer.o: src/cc/libkfsIO/IOBuffer.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /home/fify/Project/KFS/trunk/CMakeFiles $(CMAKE_PROGRESS_13)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/cc/libkfsIO/CMakeFiles/kfsIO.dir/IOBuffer.o"
	cd /home/fify/Project/KFS/trunk/src/cc/libkfsIO && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/kfsIO.dir/IOBuffer.o -c /home/fify/Project/KFS/trunk/src/cc/libkfsIO/IOBuffer.cc

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/IOBuffer.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/kfsIO.dir/IOBuffer.i"
	cd /home/fify/Project/KFS/trunk/src/cc/libkfsIO && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/fify/Project/KFS/trunk/src/cc/libkfsIO/IOBuffer.cc > CMakeFiles/kfsIO.dir/IOBuffer.i

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/IOBuffer.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/kfsIO.dir/IOBuffer.s"
	cd /home/fify/Project/KFS/trunk/src/cc/libkfsIO && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/fify/Project/KFS/trunk/src/cc/libkfsIO/IOBuffer.cc -o CMakeFiles/kfsIO.dir/IOBuffer.s

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/IOBuffer.o.requires:
.PHONY : src/cc/libkfsIO/CMakeFiles/kfsIO.dir/IOBuffer.o.requires

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/IOBuffer.o.provides: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/IOBuffer.o.requires
	$(MAKE) -f src/cc/libkfsIO/CMakeFiles/kfsIO.dir/build.make src/cc/libkfsIO/CMakeFiles/kfsIO.dir/IOBuffer.o.provides.build
.PHONY : src/cc/libkfsIO/CMakeFiles/kfsIO.dir/IOBuffer.o.provides

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/IOBuffer.o.provides.build: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/IOBuffer.o
.PHONY : src/cc/libkfsIO/CMakeFiles/kfsIO.dir/IOBuffer.o.provides.build

# Object files for target kfsIO
kfsIO_OBJECTS = \
"CMakeFiles/kfsIO.dir/Checksum.o" \
"CMakeFiles/kfsIO.dir/TcpSocket.o" \
"CMakeFiles/kfsIO.dir/Globals.o" \
"CMakeFiles/kfsIO.dir/BufferedSocket.o" \
"CMakeFiles/kfsIO.dir/NetManager.o" \
"CMakeFiles/kfsIO.dir/DiskManager.o" \
"CMakeFiles/kfsIO.dir/Acceptor.o" \
"CMakeFiles/kfsIO.dir/NetKicker.o" \
"CMakeFiles/kfsIO.dir/EventManager.o" \
"CMakeFiles/kfsIO.dir/Counter.o" \
"CMakeFiles/kfsIO.dir/DiskConnection.o" \
"CMakeFiles/kfsIO.dir/NetConnection.o" \
"CMakeFiles/kfsIO.dir/IOBuffer.o"

# External object files for target kfsIO
kfsIO_EXTERNAL_OBJECTS =

src/cc/libkfsIO/libkfsIO.a: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Checksum.o
src/cc/libkfsIO/libkfsIO.a: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/TcpSocket.o
src/cc/libkfsIO/libkfsIO.a: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Globals.o
src/cc/libkfsIO/libkfsIO.a: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/BufferedSocket.o
src/cc/libkfsIO/libkfsIO.a: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetManager.o
src/cc/libkfsIO/libkfsIO.a: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/DiskManager.o
src/cc/libkfsIO/libkfsIO.a: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Acceptor.o
src/cc/libkfsIO/libkfsIO.a: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetKicker.o
src/cc/libkfsIO/libkfsIO.a: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/EventManager.o
src/cc/libkfsIO/libkfsIO.a: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Counter.o
src/cc/libkfsIO/libkfsIO.a: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/DiskConnection.o
src/cc/libkfsIO/libkfsIO.a: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetConnection.o
src/cc/libkfsIO/libkfsIO.a: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/IOBuffer.o
src/cc/libkfsIO/libkfsIO.a: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/build.make
src/cc/libkfsIO/libkfsIO.a: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --red --bold "Linking CXX static library libkfsIO.a"
	cd /home/fify/Project/KFS/trunk/src/cc/libkfsIO && $(CMAKE_COMMAND) -P CMakeFiles/kfsIO.dir/cmake_clean_target.cmake
	cd /home/fify/Project/KFS/trunk/src/cc/libkfsIO && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/kfsIO.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
src/cc/libkfsIO/CMakeFiles/kfsIO.dir/build: src/cc/libkfsIO/libkfsIO.a
.PHONY : src/cc/libkfsIO/CMakeFiles/kfsIO.dir/build

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/requires: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Checksum.o.requires
src/cc/libkfsIO/CMakeFiles/kfsIO.dir/requires: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/TcpSocket.o.requires
src/cc/libkfsIO/CMakeFiles/kfsIO.dir/requires: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Globals.o.requires
src/cc/libkfsIO/CMakeFiles/kfsIO.dir/requires: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/BufferedSocket.o.requires
src/cc/libkfsIO/CMakeFiles/kfsIO.dir/requires: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetManager.o.requires
src/cc/libkfsIO/CMakeFiles/kfsIO.dir/requires: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/DiskManager.o.requires
src/cc/libkfsIO/CMakeFiles/kfsIO.dir/requires: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Acceptor.o.requires
src/cc/libkfsIO/CMakeFiles/kfsIO.dir/requires: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetKicker.o.requires
src/cc/libkfsIO/CMakeFiles/kfsIO.dir/requires: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/EventManager.o.requires
src/cc/libkfsIO/CMakeFiles/kfsIO.dir/requires: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/Counter.o.requires
src/cc/libkfsIO/CMakeFiles/kfsIO.dir/requires: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/DiskConnection.o.requires
src/cc/libkfsIO/CMakeFiles/kfsIO.dir/requires: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/NetConnection.o.requires
src/cc/libkfsIO/CMakeFiles/kfsIO.dir/requires: src/cc/libkfsIO/CMakeFiles/kfsIO.dir/IOBuffer.o.requires
.PHONY : src/cc/libkfsIO/CMakeFiles/kfsIO.dir/requires

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/clean:
	cd /home/fify/Project/KFS/trunk/src/cc/libkfsIO && $(CMAKE_COMMAND) -P CMakeFiles/kfsIO.dir/cmake_clean.cmake
.PHONY : src/cc/libkfsIO/CMakeFiles/kfsIO.dir/clean

src/cc/libkfsIO/CMakeFiles/kfsIO.dir/depend:
	cd /home/fify/Project/KFS/trunk && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/fify/Project/KFS/trunk /home/fify/Project/KFS/trunk/src/cc/libkfsIO /home/fify/Project/KFS/trunk /home/fify/Project/KFS/trunk/src/cc/libkfsIO /home/fify/Project/KFS/trunk/src/cc/libkfsIO/CMakeFiles/kfsIO.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : src/cc/libkfsIO/CMakeFiles/kfsIO.dir/depend
