# FreeRTOS-Kernel subset

- Upstream: https://github.com/FreeRTOS/FreeRTOS-Kernel
- Tag: V11.3.3
- Commit: `89592801dd8b73329a7583719b7e3567b50910e5`
- License: MIT; see LICENSE.md and retained upstream source headers.
- Download: https://codeload.github.com/FreeRTOS/FreeRTOS-Kernel/zip/refs/tags/V11.3.3
- Archive SHA256: `604b689104c923df74a29215e537f95b4fab90aaeac6482b48e5527da2dc776d`

This project vendors unchanged tasks.c, queue.c, list.c, include/*.h,
portable/GCC/ARM_CM3/port.c and portmacro.h, license and history. Per-file hashes
are in SHA256SUMS.json. Other kernel features/ports are not included or built.
Builds use these local files and do not download dependencies.

The ARM_CM3 port is selected for the Cortex-M4 application built entirely with
software floating point. FPU register context is not saved by this port.
