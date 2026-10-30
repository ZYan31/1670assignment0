// Raspberry Pi 3B (BCM2837 SoC) physical memory layout,
// as defined in official documentation:
// https://www.raspberrypi.org/app/uploads/2012/02/BCM2835-ARM-Peripherals.pdf
//
// except for the following differences:
// - RPi 3 uses a peripheral base address of 0x3F00'0000 rather than
//   0x2000'0000

#ifndef _MEMLAYOUT_H
#define _MEMLAYOUT_H

// Location of the initial kernel stack in physical memory.
// Must be 16-byte aligned.
#define INITIAL_KERNEL_STACK 0x02000000

// Peripherals are mapped starting at 0x3F00'0000, giving 1 MB for I/O memory
// Note that the documentation lists addresses starting at 0x7E00'0000. This is
// where the actual ARM hardware has these peripherals, but they are mapped into
// the SoC's address space at 0x3F00'0000
#define PERIPHERALS_BASE 0x3F000000
#define PERIPHERALS_LIMIT 0x3FFFFFFF

// "ARM local peripherals" are those peripherals that sit on the ARM CPU
// itself, rather than on the larger RPi SoC (e.g., the GPIO pins are an SoC
// peripheral, but the per-core timer interrupt routing is an ARM local
// peripheral). They are mapped starting at 0x4000'0000, right above the SoC
// peripherals, and are documented separately from them -- see Appendix B.
#define ARM_LOCAL_PERIPHERALS_BASE 0x40000000

// Project 4 adds one new region: a kernel stack for every process (Quest 1,
// Part B). Pick a place for NPROC of them that collides with nothing else,
// define it here (or in limits.h), and add it to your memory layout diagram:
//
// #define KSTACK_SIZE YOUR_CHOSEN_VALUE   // 16 KiB is plenty
// #define KSTACKS_BASE YOUR_CHOSEN_VALUE  // keep every stack top 16-byte aligned

// Trampoline to user-reachable functions (like yield or printf).
// This is essentially an array of function pointers at a known location, so
// that programs can use these functions as a library.
//
// The pointers get initialized in `kernel_main` (kernel.c) to point to the
// actual machine code. User programs can then call kernel functions by simply
// following those pointers (even though they weren't compiled together with
// the kernel).
#define F_BASE 0x03000000
#define F_VPRINTF (F_BASE - sizeof(void*))
#define F_YIELD (F_BASE - 2*sizeof(void*))
#define F_GETC (F_BASE - 3*sizeof(void*))
#define F_HASC (F_BASE - 4*sizeof(void*))
#define F_UPTIME (F_BASE - 5*sizeof(void*))   // Quest 3D: ms since boot (for `uptime`)
#define F_PS (F_BASE - 6*sizeof(void*))        // Quest 3D: read one process-table slot (for `ps`)
#define F_GET_CANVAS (F_BASE - 7*sizeof(void*))  //Quest 4A
#define F_VSYNC (F_BASE - 8*sizeof(void*))
#define PROC_START 0x00100000
#define FB_INFO_BASE 0x10000000
//note I MADE GETC() THE ADDRESS FOR WAIT_FOR_VSYNC() LMAO
// Project 3 adds several more of these: a way for a process to read a
// character, to find its canvas, and so on. Add them here as you
// go, following the same pattern (successive pointer-sized slots at
// *decreasing* addresses from F_BASE). Anything you name `F_...` is copied
// into user/u_klib.h automatically by user/gen_klib_header.py, so user
// programs can see it without you editing a generated file.
//
// #define F_GETC (F_BASE - 3 * sizeof(void*))
// ...

// Location of the framebuffer info struct (`struct fb_info`, which you will
// define in kernel/drivers/fb.h). Your kernel fills this in at boot and user
// programs read it to find out where the pixels are and how they are laid
// out -- the same fixed-address trick as F_BASE above, but carrying a struct
// rather than a function pointer.
//
// NOTE: this one is *not* named `F_...`, so it is not copied into
// user/u_klib.h for you. User programs need their own matching definition;
// see user/lib/u_fb.h.
// #define FB_INFO_BASE YOUR_CHOSEN_VALUE

#endif  // _MEMLAYOUT_H
