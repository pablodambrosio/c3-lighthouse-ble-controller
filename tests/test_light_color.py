"""Run production colour conversion C code with ESP RISC-V GCC and QEMU.

Usage: python tests/test_light_color.py [--tools C:/Espressif/tools]
Only esp_err.h is stubbed; no board or firmware flash is required.
"""
import argparse
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def run(tools, suite):
    gcc = sorted(tools.glob("riscv32-esp-elf/*/riscv32-esp-elf/bin/riscv32-esp-elf-gcc.exe"))[-1]
    qemu = sorted(tools.glob("qemu-riscv32/*/qemu/bin/qemu-system-riscv32.exe"))[-1]
    output = ROOT / "build" / "qemu_color_tests"
    output.mkdir(parents=True, exist_ok=True)
    (output / "esp_err.h").write_text(
        "#pragma once\ntypedef int esp_err_t;\n#define ESP_OK 0\n#define ESP_FAIL -1\n"
        "#define ESP_ERR_NO_MEM 0x101\n#define ESP_ERR_INVALID_ARG 0x102\n"
        "#define ESP_ERR_INVALID_STATE 0x103\n#define ESP_ERR_NOT_SUPPORTED 0x106\n"
        "#define ESP_ERR_TIMEOUT 0x107\n"
    )
    # ELF loader supplies data/BSS; semihosting reports main's return value.
    (output / "start.S").write_text("""
.section .text.start
.global _start
_start:
    li sp, 0x81000000
    li t0, 0x2000
    csrs mstatus, t0
    csrw fcsr, zero
    .option push
    .option norelax
    la gp, __global_pointer$
    .option pop
    call main
    addi sp, sp, -16
    sw a0, 4(sp)
    li t0, 0x20026
    sw t0, 0(sp)
    mv a1, sp
    li a0, 0x20
    .balign 16
    .option push
    .option norvc
    slli zero, zero, 31
    ebreak
    srai zero, zero, 7
    .option pop
1:  j 1b
""")
    (output / "link.ld").write_text("""
ENTRY(_start)
SECTIONS {
    . = 0x80000000;
    .text : { *(.text.start) *(.text*) }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    . = ALIGN(16);
    __global_pointer$ = . + 0x800;
    .sdata : { *(.sdata*) }
    .bss : { *(.sbss*) *(.bss*) *(COMMON) }
    _end = .;
}
""")
    executable = output / f"{suite}_tests.elf"
    subprocess.run([
        str(gcc), "-march=rv32imc_zicsr", "-mabi=ilp32", "-O2", "-Wall", "-Wextra", "-Werror",
        "-nostartfiles", "--specs=nosys.specs", "-I", str(output),
        "-I", str(ROOT / "tests" / "stubs"),
        "-I", str(ROOT / "main"), str(output / "start.S"),
        str(ROOT / "main" / "light_color.c"),
        str(ROOT / "main" / "candle.c"),
        str(ROOT / "main" / "sparkles.c"),
        str(ROOT / "main" / "color_pattern.c"),
        *([str(ROOT / "main" / "ble_light_protocol.c")] if suite == "ble" else []),
        str(ROOT / "tests" / {"color": "test_light_color.c", "lighthouse": "test_lighthouse.c",
                               "candle": "test_candle.c", "sparkles": "test_sparkles.c",
                               "pattern": "test_color_pattern.c", "ble": "test_ble_light.c"}[suite]),
        "-T", str(output / "link.ld"), "-lm", "-o", str(executable),
    ], check=True)
    env = os.environ.copy()
    # Espressif QEMU uses libiconv supplied by Git for Windows.
    env["PATH"] = "C:/Program Files/Git/mingw64/bin;" + env.get("PATH", "")
    result = subprocess.run([
        str(qemu), "-M", "virt", "-nographic", "-bios", "none",
        "-kernel", str(executable), "-semihosting-config", "enable=on,target=native",
    ], env=env, capture_output=True, text=True, timeout=30)
    if result.returncode:
        raise SystemExit(f"{suite} test failed (C line/exit {result.returncode}): {result.stderr}")
    if suite == "color":
        print("PASS: reference colours, sRGB gamma, 14097 PWM round trips, dimming, "
              "brightness limiting, invalid inputs and output preservation (RISC-V/QEMU).")
    elif suite == "ble":
        print("PASS: BLE fields, endian encoding, validation, rejected-write preservation, queue pressure, independent Group B controls and Lighthouse rejection (RISC-V/QEMU).")
    elif suite == "pattern":
        print("PASS: color modes, shifts, xy interpolation, periods, random independence and validation (RISC-V/QEMU).")
    elif suite == "sparkles":
        print("PASS: independent sparkles, dark gaps, hue, bounds, seeds and frame skipping (RISC-V/QEMU).")
    elif suite == "candle":
        print("PASS: candle hue preservation, brightness bounds, moving light, "
              "temporal continuity, seed variation and deterministic frame skipping (RISC-V/QEMU).")
    else:
        print("PASS: lighthouse full revolution, off/restart, solid switch, delayed steps, "
              "tick wrap, transfer failure, candle transitions and settings validation (RISC-V/QEMU).")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tools", type=Path, default=Path("C:/Espressif/tools"))
    parser.add_argument("--suite", choices=["color", "lighthouse", "candle", "sparkles", "pattern", "ble"], default="color")
    args = parser.parse_args()
    run(args.tools, args.suite)
