set confirm off
file kernel/kernel8.elf
target remote 127.0.0.1:1234
symbol-file kernel/kernel8.elf

source procs.gdb
