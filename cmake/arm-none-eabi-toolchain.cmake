# Bare-metal GNU Arm toolchain. MCU-specific options belong to the targets.
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Optional override when the toolchain executables are not on PATH.
set(ARM_TOOLCHAIN_PATH "" CACHE PATH "Directory containing arm-none-eabi toolchain executables")
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES ARM_TOOLCHAIN_PATH)

find_program(ARM_NONE_EABI_GCC
    NAMES arm-none-eabi-gcc
    HINTS "${ARM_TOOLCHAIN_PATH}"
    REQUIRED
)
set(CMAKE_C_COMPILER "${ARM_NONE_EABI_GCC}")
set(CMAKE_ASM_COMPILER "${ARM_NONE_EABI_GCC}")

get_filename_component(_ARM_TOOLCHAIN_BIN "${ARM_NONE_EABI_GCC}" DIRECTORY)
find_program(CMAKE_OBJCOPY
    NAMES arm-none-eabi-objcopy
    HINTS "${_ARM_TOOLCHAIN_BIN}"
    REQUIRED
)
find_program(CMAKE_SIZE
    NAMES arm-none-eabi-size
    HINTS "${_ARM_TOOLCHAIN_BIN}"
    REQUIRED
)

# Build tools run on the host; target dependencies must not come from the host.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
