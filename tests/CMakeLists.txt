cmake_minimum_required(VERSION 3.21)
project(CommunicationHostTests LANGUAGES C)
enable_testing()
add_executable(test_gpio test_gpio.c)
target_compile_features(test_gpio PRIVATE c_std_11)
target_compile_options(test_gpio PRIVATE -Wall -Wextra -Werror -UNDEBUG)
add_test(NAME gpio_register_mock COMMAND test_gpio)

# Cau hinh tests/ thanh mot project RIENG bang compiler cua PC, khong dung
# cross-toolchain hay CMake presets cua firmware.
add_executable(test_line_parser test_line_parser.c ../src/services/line_parser.c)
target_include_directories(test_line_parser PRIVATE ../src/services)
target_compile_features(test_line_parser PRIVATE c_std_11)
target_compile_options(test_line_parser PRIVATE -Wall -Wextra -Werror -UNDEBUG)
add_test(NAME line_parser COMMAND test_line_parser)

add_executable(test_spi test_spi.c)
target_compile_features(test_spi PRIVATE c_std_11)
target_compile_options(test_spi PRIVATE -Wall -Wextra -Werror -UNDEBUG)
add_test(NAME spi_register_mock COMMAND test_spi)

add_executable(test_uart test_uart.c)
target_compile_features(test_uart PRIVATE c_std_11)
target_compile_options(test_uart PRIVATE -Wall -Wextra -Werror -UNDEBUG)
add_test(NAME uart_register_mock COMMAND test_uart)

add_executable(test_can test_can.c)
target_compile_features(test_can PRIVATE c_std_11)
target_compile_options(test_can PRIVATE -Wall -Wextra -Werror -UNDEBUG)
add_test(NAME can_register_mock COMMAND test_can)

add_executable(test_i2c test_i2c.c)
target_compile_features(test_i2c PRIVATE c_std_11)
target_compile_options(test_i2c PRIVATE -Wall -Wextra -Werror -UNDEBUG)
add_test(NAME i2c_register_mock COMMAND test_i2c)

add_executable(test_tim test_tim.c)
target_compile_features(test_tim PRIVATE c_std_11)
target_compile_options(test_tim PRIVATE -Wall -Wextra -Werror -UNDEBUG)
add_test(NAME timer_pwm_register_mock COMMAND test_tim)

add_executable(test_adc test_adc.c)
target_compile_features(test_adc PRIVATE c_std_11)
target_compile_options(test_adc PRIVATE -Wall -Wextra -Werror -UNDEBUG)
add_test(NAME adc_register_mock COMMAND test_adc)
