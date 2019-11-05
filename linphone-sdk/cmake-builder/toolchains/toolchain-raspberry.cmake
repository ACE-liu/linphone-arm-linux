############################################################################
# toolchain-raspberry.cmake
# Copyright (C) 2014  Belledonne Communications, Grenoble France
#
############################################################################
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
############################################################################

	set(RASPBERRY_VERSION 1)

	set(SYSROOT_PATH "/opt/hisi-linux/x86-arm/arm-himix200-linux/host_bin/../target")
#set(TOOLCHAIN_HOST "arm-linux-gnueabihf")
set(TOOLCHAIN_HOST "arm-himix200-linux")

message(STATUS "Using sysroot path: ${SYSROOT_PATH}")

set(TOOLCHAIN_CC "${TOOLCHAIN_HOST}-gcc")
set(TOOLCHAIN_CXX "${TOOLCHAIN_HOST}-g++")
set(TOOLCHAIN_LD "${TOOLCHAIN_HOST}-ld")
set(TOOLCHAIN_AR "${TOOLCHAIN_HOST}-ar")
set(TOOLCHAIN_RANLIB "${TOOLCHAIN_HOST}-ranlib")
set(TOOLCHAIN_STRIP "${TOOLCHAIN_HOST}-strip")
set(TOOLCHAIN_NM "${TOOLCHAIN_HOST}-nm")

set(CMAKE_CROSSCOMPILING TRUE)

# Define name of the target system
set(CMAKE_SYSTEM_NAME "Linux")
#if(RASPBERRY_VERSION VERSION_GREATER 1)
	set(CMAKE_SYSTEM_PROCESSOR "armv7")
#else()
#	set(CMAKE_SYSTEM_PROCESSOR "arm")
#endif()

# Define the compiler
set(CMAKE_C_COMPILER ${TOOLCHAIN_CC})
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_CXX})
set(CMAKE_SYSROOT "${SYSROOT_PATH}")

#set(CMAKE_C_FLAGS "-mcpu=cortex-a9 -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=softfp -mthumb" CACHE STRING "Flags for Raspberry PI 1 B+")
set(CMAKE_C_FLAGS "-mcpu=cortex-a7  -mfpu=neon-vfpv4 -mfloat-abi=softfp" CACHE STRING "Flags for Raspberry PI 1 B+")
#set(CMAKE_C_FLAGS "-mcpu=cortex-a7 -march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=softfp -mthumb" CACHE STRING "Flags for Raspberry PI 1 B+")
#set(CMAKE_C_FLAGS "-march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=softfp -mthumb -fPIC" CACHE STRING "Flags for Raspberry PI 1 B+")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}" CACHE STRING "Flags for Raspberry PI 1 B+")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -std=c++11")


set(CMAKE_FIND_ROOT_PATH "${CMAKE_INSTALL_PREFIX}" "${CMAKE_SYSROOT}")
#include_directories(/opt/hisi-linux/x86-arm/arm-himix200-linux/arm-linux-gnueabi/include/c++/6.3.0)
# search for programs in the build host directories
# for libraries and headers in the target directories
#set(CMAKE_LIBRARY_PATH /home/app_comm/linphone/linphone-desktop/OUTPUT/desktop-raspberry/lib  ${CMAKE_LIBRARY_PATH})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

