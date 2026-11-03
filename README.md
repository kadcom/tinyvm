# TinyVM

This is where ClairEmu starts.

Written by Didiet Noor.

![Terminal: Debian 13 riscv64 booting on native temu (TinyVM) to the login prompt, then a root shell running uname -a (Linux 6.12.113+deb13-riscv64) and cat /etc/debian_version (13.7)](https://cdn.kadcom.dev/tinyvm/readme/boot-20261103.png)

[Try it live](https://emu.kadcom.dev): ClairEmu, which grew out of this code.

## What this is

TinyVM is derived from TinyEMU 2019-12-21 by Fabrice Bellard (bellard.org/tinyemu), MIT
licensed. The upstream release is imported unmodified in the first commit, followed by the
small fixes needed to boot a modern Debian riscv64 (Debian 13, kernel 6.12, systemd) to
`login:` on OpenSBI firmware. The emulator (`temu`) runs natively on Linux and macOS and in
the browser as WebAssembly. Each fix is its own commit, describing what changed,
why, and how the problem showed up.

| Commit | Change |
| --- | --- |
| Remove the x86 emulator | RISC-V only: the KVM x86 machine and its devices are gone |
| Build on macOS | header shims in `compat/`, `st_*timespec` mapping, Makefile detects Darwin |
| Device tree: add reg to the HTIF node | OpenSBI finds its HTIF console in the device tree |
| Add the machine ID CSRs | `mvendorid`, `marchid`, `mimpid`, `mconfigptr` read as 0 |
| Device tree: canonical ISA string | `rv64imafdc_zicsr_zifencei`, as Linux 6.x expects |
| Implement the time CSR on the CLINT clock | `rdtime` on the same 10 MHz clock as `mtime`, TM counter-enable bit |
| virtio-blk: complete unknown requests with UNSUPP | `GET_ID` completes, so udev and the initramfs move on |
| Add an ns16550a UART as the main console | `ttyS0` from the first instruction (OpenSBI, earlycon, getty) |
| Device tree: emit blocks in DTSpec order | OpenSBI adds `/reserved-memory`, Linux keeps clear of the firmware |
| virtio: 256-entry queues | the Linux virtio-net TX queue keeps running |
| Full PLIC | per-context priority, enable, threshold and claim/complete |
| virtio-blk: per-drive read-only mode and FLUSH | `mode: "ro" \| "rw" \| "snapshot"` per drive, `VIRTIO_BLK_F_RO`, `VIRTIO_BLK_F_FLUSH` |
| Device tree: RNG seed | `/chosen/rng-seed` from the host, the CRNG is ready at once |
| Add a Goldfish RTC | the guest starts with the correct date and time |
| Bazel build (native, WebAssembly) | `//:temu`, `//:tinyvm_wasm`; `js/lib.js` for current emscripten |
| Add OpenSBI v1.7 fw_jump firmware | SBI firmware for the Debian kernel (`firmware/`) |
| jsemu: resolve the drive file relative to the config file | the browser finds the disk next to its config |

Native `temu` boots Debian 13 to `login:` in about 70 seconds on an arm64 Mac.

## Build

Bazel with bzlmod (version in `.bazelversion`):

```sh
bazel build //:temu //:splitimg //:build_filelist   # native, Linux and macOS
bazel build //:tinyvm_wasm                          # bazel-bin/js/riscvemu64-wasm.{js,wasm}
```

The WebAssembly build uses the hermetic emscripten toolchain of the `emsdk` Bazel module, so
no local emscripten install is needed. It exports the same functions as the upstream JSLinux
build (`vm_start`, `console_queue_char`, ...) and loads files and disks over HTTP. The upstream
`Makefile` is kept for a native build with the optional network filesystem (libcurl) and SDL
display.

## Boot your own Debian image

You need the OpenSBI firmware (`firmware/fw_jump.bin`), a Debian riscv64 kernel and initrd
(for example `/boot/vmlinux-*` and `/boot/initrd.img-*` from a Debian 13 riscv64 system), and
an ext4 root file system image. Put them in one directory with a config file:

```js
/* debian.cfg */
{
    version: 1,
    machine: "riscv64",
    memory_size: 512,
    bios: "fw_jump.bin",
    kernel: "vmlinux",
    initrd: "initrd.img",
    cmdline: "earlycon console=ttyS0 root=/dev/vda rw",
    drive0: { file: "root.img" },
}
```

```sh
bazel-bin/temu debian.cfg
```

The disk opens in snapshot mode by default (writes stay in memory; `-rw` writes to the file,
or set `mode: "rw"` on the drive). Press `C-a x` to quit. On an image whose root account is
not set up yet, systemd's first-boot wizard asks on the console; to answer it from the config,
add for example `systemd.set_credential=passwd.plaintext-password.root:<password>` and
`systemd.set_credential=firstboot.locale:C.UTF-8` to `cmdline`.

The initrd is loaded at RAM + 128 MB, so use at least 256 MB of RAM (512 MB is comfortable for
systemd).

For the browser, `splitimg` turns a disk image into TinyEMU's HTTP block format, which the
WebAssembly build reads one block at a time on demand:

```sh
bazel-bin/splitimg root.img root 256     # root/blk.txt + root/blk000000000.bin ...
```

Use an image whose size is a multiple of the block size (256 KB here), and point the drive at
it with `drive0: { file: "root/blk.txt" }`.

## Firmware

`firmware/fw_jump.bin` is OpenSBI v1.7, platform `generic`, built with:

```sh
make LLVM=1 PLATFORM=generic FW_JUMP=y \
     FW_TEXT_START=0x80000000 FW_JUMP_ADDR=0x80200000 FW_JUMP_FDT_ADDR=0x87e00000
```

TinyVM loads the kernel at the start of RAM plus the firmware size rounded up to 2 MB
(`0x80200000`) and the initrd at RAM + 128 MB (`0x88000000`); the device tree goes just below
the initrd. Source commit and build notes are in `firmware/README.txt`.

## License

This repository as a whole is distributed under the GNU Affero General Public License v3.0
only (`LICENSE`).

- The upstream TinyEMU code keeps its MIT licence, Copyright (c) 2016-2019 Fabrice Bellard
  (`MIT-LICENSE.txt` and the file headers).
- slirp keeps its BSD licence (see the `slirp/` headers); OpenSBI in `firmware/` keeps its
  BSD-2-Clause licence (`firmware/COPYING.BSD`).
- Our changes, Copyright (c) 2026 PT Baris Inovasi Ceria, are AGPL-3.0-only. Files we
  added carry `SPDX-License-Identifier: AGPL-3.0-only`; upstream files we changed carry our
  copyright line and a note above the original MIT notice, which stays in place.

## Credit

TinyVM is derived from TinyEMU 2019-12-21 by Fabrice Bellard
([bellard.org/tinyemu](https://bellard.org/tinyemu/)), MIT licensed. TinyEMU and its design are
his; this tree adds the fixes listed above.
