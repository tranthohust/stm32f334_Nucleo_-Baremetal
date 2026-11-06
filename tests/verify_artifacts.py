"""Verify linked IRQ vectors, static memory, vendor bytes and local docs links.

Run after building debug, release, rtos-debug and rtos-release:
    python tests/verify_artifacts.py
This checks artifacts; it does not execute ARM code or emulate a board.
"""
from pathlib import Path
import hashlib
import json
import re
import shutil
import struct
import subprocess
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]
PRESETS = ("debug", "release", "rtos-debug", "rtos-release")
VECTORS = {
    "SysTick_Handler": 15,
    "DMA1_Channel6_IRQHandler": 32,
    "DMA1_Channel7_IRQHandler": 33,
    "CAN_TX_IRQHandler": 35,
    "CAN_RX0_IRQHandler": 36,
    "CAN_RX1_IRQHandler": 37,
    "CAN_SCE_IRQHandler": 38,
    "I2C1_EV_IRQHandler": 47,
    "I2C1_ER_IRQHandler": 48,
    "SPI1_IRQHandler": 51,
    "USART2_IRQHandler": 54,
    "EXTI0_IRQHandler": 22,
    "ADC1_2_IRQHandler": 34,
    "TIM3_IRQHandler": 45,
}


def run(tool, *args):
    executable = shutil.which(tool)
    if executable is None:
        raise RuntimeError(f"Missing tool on PATH: {tool}")
    return subprocess.check_output([executable, *map(str, args)], text=True)


def verify_firmware(preset):
    elf = ROOT / "build" / preset / "firmware.elf"
    if not elf.is_file():
        raise RuntimeError(f"Build this preset first: {preset}")
    symbols = {}
    for line in run("arm-none-eabi-nm", "--defined-only", elf).splitlines():
        match = re.fullmatch(r"([0-9a-fA-F]+)\s+([A-Za-z])\s+(\S+)", line)
        if match:
            address, kind, name = match.groups()
            symbols[name] = (int(address, 16), kind)
    unresolved = run("arm-none-eabi-nm", "--undefined-only", elf).strip()
    if unresolved:
        raise RuntimeError(f"{preset}: unresolved symbols: {unresolved}")

    output = elf.with_name("review-vectors.bin")
    run("arm-none-eabi-objcopy", "-O", "binary", "--only-section=.isr_vector", elf, output)
    vectors = output.read_bytes()
    required = dict(VECTORS)
    rtos = preset.startswith("rtos-")
    if rtos:
        required.update(SVC_Handler=11, PendSV_Handler=14)
    for name, slot in required.items():
        address, kind = symbols.get(name, (0, "?"))
        if kind != "T":
            raise RuntimeError(f"{preset}: {name} is not a strong code symbol")
        actual = struct.unpack_from("<I", vectors, slot * 4)[0]
        if actual != address | 1:
            raise RuntimeError(f"{preset}: wrong vector entry for {name}")
        if actual == symbols["Default_Handler"][0] | 1:
            raise RuntimeError(f"{preset}: {name} routes to Default_Handler")

    if rtos:
        for name in ("xTaskCreateStatic", "vTaskStartScheduler", "xPortSysTickHandler"):
            if symbols.get(name, (0, "?"))[1] != "T":
                raise RuntimeError(f"{preset}: missing actual kernel function {name}")
        for name in ("malloc", "calloc", "free", "pvPortMalloc", "vPortFree", "ucHeap"):
            if name in symbols:
                raise RuntimeError(f"{preset}: unexpected dynamic allocation symbol {name}")
    elif "vTaskStartScheduler" in symbols:
        raise RuntimeError(f"{preset}: kernel linked into bare-metal variant")

    if symbols["_end"][0] > symbols["_estack"][0] - 1024:
        raise RuntimeError(f"{preset}: less than 1 KB left for MSP")
    size_lines = run("arm-none-eabi-size", elf).splitlines()
    text, data, bss = map(int, size_lines[-1].split()[:3])
    print(f"{preset}: {len(required)} vectors OK, Flash={text + data} B, "
          f"data+bss={data + bss} B, no unresolved symbols")


def verify_vendor():
    vendor = ROOT / "third_party" / "FreeRTOS-Kernel"
    manifest = json.loads((vendor / "SHA256SUMS.json").read_text(encoding="utf-8"))
    for relative, expected in manifest.items():
        path = (vendor / relative).resolve()
        if vendor.resolve() not in path.parents:
            raise RuntimeError(f"Vendor manifest path leaves vendor directory: {relative}")
        actual = hashlib.sha256(path.read_bytes()).hexdigest()
        if actual != expected:
            raise RuntimeError(f"Upstream file changed: {relative}")
    print(f"FreeRTOS: {len(manifest)} vendored upstream file hashes OK")


def verify_docs():
    docs = [ROOT / "README.md", *sorted((ROOT / "docs").glob("*.md"))]
    checked = 0
    for document in docs:
        body = document.read_text(encoding="utf-8")
        if "\ufffd" in body:
            raise RuntimeError(f"Unicode replacement character in {document}")
        fenced = False
        for line in body.splitlines():
            if line.lstrip().startswith("```"):
                fenced = not fenced
                continue
            if fenced:
                continue
            for match in re.finditer(r"(?<!!)\[[^\]\n]+\]\(([^)\n]+)\)", line):
                raw = match.group(1).strip().strip("<>")
                link = urlsplit(raw)
                if link.scheme or link.netloc:
                    continue
                target = (document.parent / unquote(link.path)).resolve() if link.path else document
                if not target.exists():
                    raise RuntimeError(f"Broken link in {document.name}: {raw}")
                if link.fragment and target.suffix == ".md":
                    # The architecture guide deliberately uses stable explicit IDs.
                    target_body = target.read_text(encoding="utf-8")
                    anchors = set(re.findall(r'<a\s+id="([^"]+)"', target_body))
                    if anchors and unquote(link.fragment) not in anchors:
                        raise RuntimeError(f"Broken explicit anchor in {document.name}: {raw}")
                checked += 1
        if fenced:
            raise RuntimeError(f"Unclosed fenced block in {document}")
    print(f"Docs: UTF-8/fences and {checked} local links/anchors OK")


if __name__ == "__main__":
    for preset in PRESETS:
        verify_firmware(preset)
    verify_vendor()
    verify_docs()
