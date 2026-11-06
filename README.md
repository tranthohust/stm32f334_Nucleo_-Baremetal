# STM32F334 Nucleo bare-metal

Target: NUCLEO-F334R8 / STM32F334R8.

Learning example: USART2 IRQ or circular DMA RX, buffered DMA TX, a line-command
parser, SPI1 master transaction queue using IRQs, and CAN1 silent loopback with
frame queues. Read the Vietnamese walkthrough in
[docs/communication-learning.md](docs/communication-learning.md) for source reading
order, wiring, terminal commands, buffer ownership, and host tests.

Drivers follow the project's GPIO form: peripheral `Config_t` + `Handle_t`,
clock control, init/deinit, and IRQ handling with a handle argument. Each handle
owns its buffers and runtime state. `src/app/board_peripherals.c` configures pins
through the existing GPIO API; `src/app/interrupts.c` forwards IRQs to drivers.
Supported instances are USART2, SPI1 master (8-bit), CAN1, I2C1 master (7-bit),
TIM3 periodic IRQ, TIM2 channel 1 PWM, and ADC1 single-conversion IRQ.

Start with the offline Vietnamese
[protocol architecture guide (HTML with rendered diagrams)](docs/protocol-architecture-guide.html)
or its [Markdown source](docs/protocol-architecture-guide.md): protocol
structure, each layer's purpose, and actual application -> driver -> hardware
call paths. It links directly to the source and the focused learning examples.

The guide also covers [GPIO input/EXTI](docs/protocol-architecture-guide.md#gpio-input),
[timer](docs/protocol-architecture-guide.md#timer),
[PWM](docs/protocol-architecture-guide.md#pwm), and
[ADC](docs/protocol-architecture-guide.md#adc), with wiring, register roles,
callback paths, and exercises. `IO HELP` lists the added terminal commands.
PB0 is an external button to GND (internal pull-up, 30 ms debounce); TIM3
counts 100 ms updates; PA0 AF1 outputs TIM2_CH1 PWM at 1 kHz; PA1 analog is
ADC1_IN2. Keep analog input within 0..VDDA and share board GND.

```text
GPIO READ
GPIO WATCH ON
TIMER STATS
PWM 25
PWM START
ADC READ
ADC PWM ON
ADC READ
ADC PWM OFF
PWM STOP
```

PWM starts stopped. Each `ADC READ` takes one sample; with `ADC PWM ON`, its
callback maps that sample to PWM duty. ADC mV assumes a 3300 mV reference;
it is an estimate. After a conversion timeout, `ADC INIT` recalibrates before
retrying. The ADC lesson owns the ADC12 clock/common reset and excludes ADC2.
IO/TIM/ADC processing belongs to main or the existing Comm task; SysTick
continues to provide system time and the FreeRTOS tick.

The I2C/FreeRTOS lesson is in
[docs/i2c-freertos-learning.md](docs/i2c-freertos-learning.md). I2C1 uses PB8/PB9
AF4 open-drain with external pull-ups. `I2C READ` reads one byte at the example
address/register configured in `src/app/demo_config.h`.

Build the FreeRTOS variant in a separate directory:

```powershell
cmake --preset rtos-debug
cmake --build --preset rtos-debug --parallel 4
```

Flash `build/rtos-debug/firmware.hex`. `RTOS START`, `RTOS STOP` and `RTOS STATS`
control a 2-second request producer and show queue/stack counters. The ordinary
`debug`/`release` presets retain the bare-metal loop; `rtos-debug`/`rtos-release`
set `FIRMWARE_USE_FREERTOS=ON`.

FreeRTOS-Kernel V11.3.3 is vendored under `third_party/FreeRTOS-Kernel` with its
MIT license and upstream commit/hash record. Tasks, queues and mutexes use static
memory; no FreeRTOS heap implementation or build-time downloads are required.

Requirements: CMake 3.21 or newer, GNU Make (`mingw32-make` on Windows), and
the GNU Arm Embedded toolchain (`arm-none-eabi-gcc`, `arm-none-eabi-objcopy`,
`arm-none-eabi-size`) on PATH.

Build in PowerShell:

```powershell
cmake --preset debug
cmake --build --preset debug --parallel 4
```

Run these commands from the project root. Configure once, then repeat only
the build command after editing sources. CMake tracks header dependencies
and updates the build rules when CMakeLists.txt changes.

The presets use the MinGW Makefiles generator. CMake creates Makefiles in
`build/debug/` or `build/release/` and invokes GNU Make for the build.
After configuring, you can also invoke GNU Make directly:

```powershell
mingw32-make -C build/debug -j4
mingw32-make -C build/debug size
```

Edit CMakeLists.txt to change the build rules; generated Makefiles are
recreated by CMake. When switching an existing directory from another
generator, remove that build directory and configure it again. The clean
target retains CMakeCache.txt, so it does not reset the generator.

Additional commands:

```powershell
cmake --build --preset debug --target size
cmake --build --preset debug --target clean
cmake --preset release
cmake --build --preset release --parallel 4
```

Debug uses `-Og -g3`; Release uses CMake's GNU Release options (`-O3 -DNDEBUG`).
The clean target removes generated firmware and object files while retaining
the CMake configuration. No handwritten Makefile or PowerShell build script
is needed.

Debug outputs are `build/debug/firmware.elf`, `firmware.hex`, `firmware.bin`,
and `firmware.map` in the same directory. Release outputs use `build/release/`.
`build/debug/compile_commands.json` is also generated for editor tooling.
Old Makefile outputs directly under `build/` are not used by CMake.
In STM32CubeProgrammer, connect through ST-LINK, open `build/debug/firmware.hex`,
program and verify, then reset/run. For BIN use address 0x08000000.

If the GNU Arm toolchain is not on PATH, configure with its bin directory:

```powershell
cmake --preset debug -DARM_TOOLCHAIN_PATH="C:/path/to/arm-toolchain/bin"
```

Build configuration files:

- `CMakeLists.txt`: shared MCU flags, the `stm32f334_drivers` driver library,
  firmware sources, linker script, HEX/BIN generation, and the size target.
- `cmake/arm-none-eabi-toolchain.cmake`: cross-compiler and binary utilities.
- `CMakePresets.json`: GNU Make-based Debug and Release configurations.

Add new driver source files to the driver target, and application sources
to the firmware target in CMakeLists.txt. Keep machine-specific overrides
in an untracked CMakeUserPresets.json if needed.

`src/test/main.c` starts a 1ms SysTick and the communication demo. The PA5 LED
blinks without blocking the main loop. USART2 uses PA2/PA3 at 115200 8N1; send
`HELP` followed by LF or CRLF to list commands. Configuration is in
`src/app/demo_config.h` (DMA/IRQ RX and CAN loopback). SPI1 uses PB3/PB4/PB5,
with software CS on PB6. CAN defaults to silent loopback; physical CAN requires
additional wiring described in the walkthrough.
I2C uses the independent HSI8 kernel clock and nominal 100kHz timing. No I2C
device is assumed present: NACK or timeout is reported while other services run.

The reset HSI 8MHz clock is retained. This freestanding C build uses software
floating point and libgcc, without HAL or libc; malloc/printf are not provided.
Small memcpy/memset helpers support compiler-generated struct operations.

Boot files:

- src/startup/startup_stm32f334x8.s: official ST vector table and Reset_Handler.
  Calls SystemInit, copies .data, clears .bss, initializes constructors,
  and calls main. Override weak IRQ handlers using exact vector-table names.
- STM32F334R8_FLASH.ld: 64 KB Flash at 0x08000000, 12 KB SRAM at 0x20000000.
  CCM RAM is not used. Leaves at least 1 KB for stack at link time;
  this does not detect runtime stack overflow.
- src/systerm/system_stm32f334.c: sets VTOR and supplies minimal constructor startup.

ST startup source (license included in LICENSE.md):
https://github.com/STMicroelectronics/cmsis-device-f3/tree/master/Source/Templates/gcc

NVIC and interrupt critical sections live in `STM32F334_CORE.c`. The GPIO IRQ
compatibility API uses that shared implementation; EXTI input/IMR and pending
clears are configured. Protocol parsing and user callbacks run in main.
Builds and host register-mock tests do not replace validation on a physical board.
The RTOS scheduler has been compiled and linked for the target; execution and
stack high-water marks require validation on the board.
