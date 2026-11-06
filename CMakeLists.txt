cmake_minimum_required(VERSION 3.21)

project(Stm32f334NucleoBaremetal LANGUAGES C ASM)

if(NOT CMAKE_CROSSCOMPILING OR NOT CMAKE_C_COMPILER_ID STREQUAL "GNU")
    message(FATAL_ERROR "Use the GNU Arm toolchain: cmake --preset debug")
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build configuration" FORCE)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Shared MCU options apply to the driver, application and startup assembly.
add_library(stm32f334_options INTERFACE)
target_compile_options(stm32f334_options INTERFACE
    -mcpu=cortex-m4
    -mthumb
    -mfloat-abi=soft
    "$<$<COMPILE_LANGUAGE:C>:-Wall;-Wextra;-ffreestanding;-ffunction-sections;-fdata-sections>"
    "$<$<CONFIG:Debug>:-Og;-g3>"
)
target_link_options(stm32f334_options INTERFACE
    -mcpu=cortex-m4
    -mthumb
    -mfloat-abi=soft
    -nostdlib
)

add_library(stm32f334_gpio STATIC
    src/drivers/STM32F334.c
    src/drivers/STM32F334.h
    src/drivers/STM32F334_GPIO.h
)
target_include_directories(stm32f334_gpio PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/drivers"
)
target_link_libraries(stm32f334_gpio PUBLIC stm32f334_options)

add_executable(firmware
    src/test/main.c
    src/systerm/system_stm32f334.c
    src/startup/startup_stm32f334x8.s
)
set_target_properties(firmware PROPERTIES SUFFIX ".elf")
target_link_libraries(firmware PRIVATE stm32f334_gpio gcc)

set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/STM32F334R8_FLASH.ld")
set(FIRMWARE_MAP "${CMAKE_CURRENT_BINARY_DIR}/firmware.map")
set(FIRMWARE_HEX "${CMAKE_CURRENT_BINARY_DIR}/firmware.hex")
set(FIRMWARE_BIN "${CMAKE_CURRENT_BINARY_DIR}/firmware.bin")

target_link_options(firmware PRIVATE
    "-T${LINKER_SCRIPT}"
    -Wl,--gc-sections
    "-Wl,-Map=${FIRMWARE_MAP}"
)
# Relink when the memory layout changes, even if no source file changed.
set_property(TARGET firmware APPEND PROPERTY LINK_DEPENDS "${LINKER_SCRIPT}")

add_custom_command(TARGET firmware POST_BUILD
    COMMAND "${CMAKE_SIZE}" "$<TARGET_FILE:firmware>"
    BYPRODUCTS "${FIRMWARE_MAP}"
    COMMENT "Firmware size"
    VERBATIM
)

# Explicit outputs let the build regenerate HEX/BIN if either file is removed.
add_custom_command(
    OUTPUT "${FIRMWARE_HEX}" "${FIRMWARE_BIN}"
    COMMAND "${CMAKE_OBJCOPY}" -O ihex "$<TARGET_FILE:firmware>" "${FIRMWARE_HEX}"
    COMMAND "${CMAKE_OBJCOPY}" -O binary "$<TARGET_FILE:firmware>" "${FIRMWARE_BIN}"
    DEPENDS firmware
    COMMENT "Generating firmware.hex and firmware.bin"
    VERBATIM
)
add_custom_target(firmware_images ALL DEPENDS "${FIRMWARE_HEX}" "${FIRMWARE_BIN}")

add_custom_target(size
    COMMAND "${CMAKE_SIZE}" "$<TARGET_FILE:firmware>"
    DEPENDS firmware
    COMMENT "Firmware size"
    VERBATIM
)
