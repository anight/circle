#
# toolchain.cmake - the GNU toolchain for a Circle build with CMake (see
# CMakeLists.txt): pass -DCMAKE_TOOLCHAIN_FILE=<circle>/cmake/toolchain.cmake
# and -DCIRCLE_PREFIX=<the toolchain's command prefix>, as configure's -p,
# e.g. .../bin/arm-none-eabi- (32 bit) or .../bin/aarch64-none-elf- (64 bit).
#
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

if(NOT CIRCLE_PREFIX)
	message(FATAL_ERROR "CIRCLE_PREFIX: the toolchain's command prefix (as configure's -p)")
endif()
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES CIRCLE_PREFIX)

set(CMAKE_C_COMPILER ${CIRCLE_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${CIRCLE_PREFIX}g++)
set(CMAKE_ASM_COMPILER ${CIRCLE_PREFIX}gcc)
set(CMAKE_AR ${CIRCLE_PREFIX}ar CACHE FILEPATH "")
set(CMAKE_RANLIB ${CIRCLE_PREFIX}ranlib CACHE FILEPATH "")
set(CMAKE_LINKER ${CIRCLE_PREFIX}ld CACHE FILEPATH "")
set(CMAKE_OBJCOPY ${CIRCLE_PREFIX}objcopy CACHE FILEPATH "")
set(CMAKE_OBJDUMP ${CIRCLE_PREFIX}objdump CACHE FILEPATH "")
set(CIRCLE_CPPFILT ${CIRCLE_PREFIX}c++filt CACHE FILEPATH "")

# the compiler checks only compile (there's no C library to link against)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# no flags of CMake's own: Circle's are all (CMakeLists.txt: circle_flags)
set(CMAKE_C_FLAGS_INIT "")
set(CMAKE_CXX_FLAGS_INIT "")
set(CMAKE_ASM_FLAGS_INIT "")
