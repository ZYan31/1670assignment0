/*
 * Copyright (c) 2016-2019 Raspberry Pi (Trading) Ltd.
 * Copyright (c) 2016 Stephen Warren <swarren@wwwdotorg.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * * Neither the name of the copyright holder nor the names of its contributors
 *   may be used to endorse or promote products derived from this software
 *   without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Weenix2's armstub, adapted from raspberrypi/tools armstubs/armstub8.S.
 * The GPU firmware loads this at 0x0 (config.txt: armstub=armstub.bin) and
 * releases all four cores here, in EL3.
 *
 * Three changes from upstream:
 * - Remove the HIGH_PERI variants and the optional GIC setup; RPi3 has no GIC.
 * - Switch to kernel while still in EL3.
 * - All cores enter the kernel.
 */

#define BIT(x) (1 << (x))

#define LOCAL_CONTROL		0x40000000
#define LOCAL_PRESCALER		0x40000008

#define OSC_FREQ		19200000

#define SCR_RW			BIT(10)
#define SCR_HCE			BIT(8)
#define SCR_SMD			BIT(7)
#define SCR_RES1_5		BIT(5)
#define SCR_RES1_4		BIT(4)
#define SCR_NS			BIT(0)
#define SCR_VAL \
    (SCR_RW | SCR_HCE | SCR_SMD | SCR_RES1_5 | SCR_RES1_4 | SCR_NS)

#define ACTLR_VAL \
	(BIT(0) | BIT(1) | BIT(4) | BIT(5) | BIT(6))

#define CPUECTLR_EL1		S3_1_C15_C2_1
#define CPUECTLR_EL1_SMPEN	BIT(6)

#define L2CTLR_EL1		S3_1_C11_C0_2

/* Where the firmware loads kernel8.img (config.txt: kernel_address=0x80000) */
#define KERNEL_ENTRY		0x80000

.section .text.armstub
.globl _start
_start:
	/*
	 * LOCAL_CONTROL:
	 * Bit 9 clear: Increment by 1 (vs. 2).
	 * Bit 8 clear: Timer source is 19.2MHz crystal (vs. APB).
	 */
	ldr x0, =LOCAL_CONTROL
	str wzr, [x0]
	/* LOCAL_PRESCALER; divide-by (0x80000000 / register_val) == 1 */
	mov w1, #0x80000000
	str w1, [x0, #(LOCAL_PRESCALER - LOCAL_CONTROL)]

	/* Set L2 read/write cache latency to 3 */
	mrs x0, L2CTLR_EL1
	mov x1, #0x22
	orr x0, x0, x1
	msr L2CTLR_EL1, x0

	/* Set up CNTFRQ_EL0 */
	ldr x0, =OSC_FREQ
	msr CNTFRQ_EL0, x0

	/* Set up CNTVOFF_EL2 */
	msr CNTVOFF_EL2, xzr

	/* Enable FP/SIMD */
	/* Bit 10 (TFP) is set to 0 */
	msr CPTR_EL3, xzr

	/* Set up SCR. boot.S rewrites this before dropping to EL2. */
	mov x0, #SCR_VAL
	msr SCR_EL3, x0

	/* Set up ACTLR */
	mov x0, #ACTLR_VAL
	msr ACTLR_EL3, x0

	/* Set SMPEN */
	mov x0, #CPUECTLR_EL1_SMPEN
	msr CPUECTLR_EL1, x0

	/*
	 * Set up SCTLR_EL2
	 * All set bits below are res1. LE, no WXN/I/SA/C/A/M
	 */
	ldr x0, =0x30c50830
	msr SCTLR_EL2, x0

	/*
   * Zero x0-x3, then enter the kernel (still in EL3).
	 */
	mov x0, #0
	mov x1, #0
	mov x2, #0
	mov x3, #0
	ldr x4, =KERNEL_ENTRY
	br x4

.ltorg
