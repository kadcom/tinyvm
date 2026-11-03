fw_jump.bin: OpenSBI v1.7 (BSD-2-Clause, see COPYING.BSD), platform
"generic", fw_jump mode.

Source: https://github.com/riscv-software-src/opensbi/tree/v1.7
(commit a32a91069119e7a5aa31e6bc51d5e00860be3d80)

Built on macOS with Homebrew LLVM 23 (clang + ld.lld), with -Werror
removed from the OpenSBI Makefile (an unused-but-set-global warning in
lib/utils/serial/cadence-uart.c, a driver this machine does not use):

  make LLVM=1 PLATFORM=generic FW_JUMP=y \
       FW_TEXT_START=0x80000000 FW_JUMP_ADDR=0x80200000 \
       FW_JUMP_FDT_ADDR=0x87e00000

sha256 36ce3bbfc6ceae8a27ee975c45252355b5422d3c28447468f34f1251f1415207
