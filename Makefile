.PHONY: clean format

HOST_OS = $(shell uname -s)
HOST_ARCH = $(shell uname -m)
TOOL_LDFLAGS :=

ifndef IN_CONTAINER
ifneq ("$(wildcard /etc/image-version)","")
    IN_CONTAINER = 1
else
    IN_CONTAINER = 0
endif
endif

# Try to infer the correct TOOLPREFIX if not set
ifndef TOOLPREFIX
ifeq ($(HOST_OS), Darwin)
# MacOS
TOOLPREFIX = aarch64-elf-
TOOL_LDFLAGS += -Wl,-pie,--no-dynamic-linker
else
# Linux/Windows
ifneq ($(HOST_ARCH), aarch64)
TOOLPREFIX = aarch64-linux-gnu-
else
TOOLPREFIX = ""
endif
endif
endif


GCC = $(TOOLPREFIX)gcc
LD = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy
STRIP = $(TOOLPREFIX)strip
PYTHON ?= python3

MAKEDIR = $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
IMGNAME = weenix-ubuntu24
CONTNAME = weenix-container

QEMU ?= qemu-system-aarch64

CFLAGS = -Wall -Wextra -Werror -g -std=gnu2x
# Tell compiler to avoid automatically linking the C standard library and Linux/MacOS startup files
CFLAGS += -nostdlib -nostartfiles -ffreestanding
# Prevent compiler from using floating point and SIMD registers in kernel code
CFLAGS += -mgeneral-regs-only

# Modify the number of debug prints
DEBUG_LVL ?= 0
ifeq ($(DEBUG_LVL),0)
    CFLAGS += -DDEBUG_LEVEL=0
else ifeq ($(DEBUG_LVL),1)
    CFLAGS += -DDEBUG_LEVEL=1
else ifeq ($(DEBUG_LVL),2)
    CFLAGS += -DDEBUG_LEVEL=2
else
    $(error Invalid DEBUG_LVL value: $(DEBUG_LVL). Must be 0, 1 or 2)
endif


ASMFLAGS = -g

# The compiler and flags this build uses, recorded in $(BUILDSTAMP). Every
# compile rule depends on that file, and the rule for it (below) rewrites it
# only when these change, so switching flags -- e.g., `make qemu DEBUG_LVL=1`
# after a plain `make` -- rebuilds everything, and nothing else does.
BUILDSTAMP := .buildflags
BUILDFLAGS := $(GCC) $(CFLAGS) $(ASMFLAGS) $(TOOL_LDFLAGS)


K = kernel
U = user
ULIB = user/lib

# Find all the source files for user code
USRCS  := $(wildcard $(U)/*.c)
UELFS  := $(USRCS:.c=.elf)
UOBJS  := $(patsubst $(U)/%.elf,$(K)/%_kproc.o,$(UELFS))

ULIBSRCS := $(wildcard $(ULIB)/*.c)
ULIBOBJS := $(ULIBSRCS:.c=.o)
UHDRS    := $(wildcard $(U)/*.h) $(wildcard $(ULIB)/*.h)

# Programs that misbehave on purpose (user/diabolical/*.c), which we keep in
# their own directory but embed just like the others.
UDIABSRCS := $(wildcard $(U)/diabolical/*.c)
# Their shared header (diab_delay.h): changing it must rebuild them all.
UDIABHDRS := $(wildcard $(U)/diabolical/*.h)
UOBJS     += $(patsubst $(U)/diabolical/%.c,$(K)/%_kproc.o,$(UDIABSRCS))

# Find all the source files for kernel code
KSRCS  := $(wildcard $(K)/*.c $(K)/drivers/*.c)
KASM   := $(wildcard $(K)/*.S)
KOBJS  := $(KASM:%.S=%.o) $(KSRCS:%.c=%.o)

OBJDIR = obj
DEPSDIR = .deps
DEPS := $(shell find $(DEPSDIR) -name '*.d' 2>/dev/null)

all: kernel8.img armstub.bin procs.gdb

clean:
	rm -rf kernel8.img
	rm -rf $(K)/*.elf
	find $(K) -name *.o -delete
	rm -rf $(DEPSDIR)
	rm -f $(UOBJS)
	rm -f $(U)/u_klib.h
	rm -f $(U)/syscalls.S $(U)/syscalls.o
	rm -f $(ULIBOBJS)
	rm -f armstub.bin
	rm -f bootloader/armstub.o bootloader/armstub.elf
	rm -rf $(OBJDIR)
	rm -f procs.gdb
	rm -f $(BUILDSTAMP)

format:
	find . -name *.[c,h] | xargs clang-format -i

# Always runs, but only touches $(BUILDSTAMP) when the flags differ from the
# ones it recorded; otherwise its timestamp, and everything built after it,
# stays as it was.
$(BUILDSTAMP): FORCE
	@if [ '$(BUILDFLAGS)' != "$$(cat $@ 2>/dev/null)" ]; then \
	    echo "* build flags changed; rebuilding"; \
	    echo '$(BUILDFLAGS)' > $@; \
	fi

.PHONY: FORCE
FORCE:

ifneq ($(DEPS),)
include $(DEPS)
endif

###################
# kernel files
###################

$(K)/%.o: $(K)/%.c $(BUILDSTAMP)
	mkdir -p $(@D) $(DEPSDIR)/$(K)/$(*D)
	$(GCC) $(CFLAGS) -MMD -MF $(DEPSDIR)/$(K)/$*.d -Ikernel -c $< -o $@

$(K)/%.o: $(K)/%.S $(BUILDSTAMP)
	mkdir -p $(DEPSDIR)/$(K)/$(*D)
	$(GCC) $(ASMFLAGS) -MMD -MF $(DEPSDIR)/$(K)/$*.d -c $< -o $@

kernel8.img: $(KOBJS) $(UOBJS)
	$(LD) -T kernel/linker.ld -o $(K)/kernel8.elf $(KOBJS) $(UOBJS)
	$(OBJCOPY) $(K)/kernel8.elf -O binary kernel8.img


###################
# bootloader
###################

# Bootloader for the Raspberry Pi 3 that sets up tick counter and other hardware configuration.
# The firmware loads this at address 0x0, and it eventually hands over control to the code in
# boot.S.
armstub.bin: bootloader/armstub.S $(BUILDSTAMP)
	$(GCC) $(ASMFLAGS) -c $< -o bootloader/armstub.o
	# Force linker to put the .text segment at address 0x0
	$(LD) -Ttext=0x0 -o bootloader/armstub.elf bootloader/armstub.o
	$(OBJCOPY) bootloader/armstub.elf -O binary $@


###################
# user programs
###################

# Generate trampoline header file
$(U)/u_klib.h: $(K)/memlayout.h
	$(PYTHON) $(U)/gen_klib_header.py > $@

# Generate the user-side system call stubs: one assembly function per
# #define SYS_... line in kernel/syscall.h, so that the kernel and user
# programs share one definition of each system call number.
$(U)/syscalls.S: $(K)/syscall.h $(U)/gen_syscall_stubs.py
	$(PYTHON) $(U)/gen_syscall_stubs.py > $@

$(U)/syscalls.o: $(U)/syscalls.S $(BUILDSTAMP)
	$(GCC) $(ASMFLAGS) -c $< -o $@

# Compile user library files into .o files
$(ULIB)/%.o: $(ULIB)/%.c $(UHDRS) $(U)/u_klib.h $(BUILDSTAMP)
	mkdir -p $(DEPSDIR)/$(ULIB)
	$(GCC) $(CFLAGS) -I$(U) -I$(ULIB) -MMD -MF $(DEPSDIR)/$(ULIB)/$*.d -c $< -o $@

# Compile user programs into .elf files (ELF executables)
# -e _start: set entry point to '_start' (in user/lib/u_start.c), which calls
#            main() and then exit()
# -I$(U) -I$(ULIB): include user/ and user/lib directories for headers
# -static: disable dynamic linking
# -static-pie: create a position-independent executable
# -Wl passes options to the linker, specifically:
#  -nmagic: avoid page alignment of segments in ELF executables (ARM64 requires 64kB alignment, which wastes a lot of space)
#  -T $(U)/procs.ld: use custom linker script to define memory layout
$(U)/%.elf: $(U)/%.c $(ULIBOBJS) $(U)/syscalls.o $(U)/u_klib.h $(BUILDSTAMP) | $(OBJDIR)
	$(GCC) $(CFLAGS) -e _start -I$(U) -I$(ULIB) -static-pie $(TOOL_LDFLAGS) -Wl,-nmagic,-T,$(U)/procs.ld -o $@ $(filter-out %.h $(BUILDSTAMP),$^)
	@cp $@ $(OBJDIR)/$(notdir $@) # Save a copy for GDB

# The diabolical programs are compiled from user/diabolical/<name>.c into
# user/<name>.elf, so that they get the same symbol names as every other
# program (_binary_user_<name>_elf_start). This rule only applies when there is
# no user/<name>.c for the program.
$(U)/%.elf: $(U)/diabolical/%.c $(UDIABHDRS) $(ULIBOBJS) $(U)/syscalls.o $(U)/u_klib.h $(BUILDSTAMP) | $(OBJDIR)
	$(GCC) $(CFLAGS) -e _start -I$(U) -I$(ULIB) -static-pie $(TOOL_LDFLAGS) -Wl,-nmagic,-T,$(U)/procs.ld -o $@ $(filter-out %.h $(BUILDSTAMP),$^)
	@cp $@ $(OBJDIR)/$(notdir $@) # Save a copy for GDB

# Turn ELF executable into an object file, so that we can link it into the kernel executable.
$(K)/%_kproc.o: $(U)/%.elf
	$(STRIP) --strip-unneeded $<
	$(OBJCOPY) -I binary $< -O elf64-littleaarch64 -B aarch64 $@ --rename-section .data=.elf_executables,alloc,load,readonly,data,contents

procs.gdb: kernel8.img
	$(PYTHON) $(U)/gen_user_symbols.py -t $(TOOLPREFIX) -o $@ kernel/kernel8.elf $(OBJDIR)

$(OBJDIR):
	mkdir -p $@

###################
# QEMU
###################

include qemu.mk


