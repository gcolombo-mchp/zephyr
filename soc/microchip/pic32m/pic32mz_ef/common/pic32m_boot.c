/**********************************************************************************************
 * © 2026 Microchip Technology Inc. and its subsidiares. All rights reserved.
 * This software includes AI generated code created using significant prompting. This
 * software is provided AS IS; you are Responsible for reviewing, testing, and validating for
 * your application.
 **********************************************************************************************/
/*
 * Copyright (c) 2026 Microchip Technology Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * PIC32M kernel bootstrap for XC32 toolchain.
 *
 * XC32's crt0 calls main() after completing hardware initialization
 * (cache, .data copy, BSS clear, TLB, etc.). We intercept main() via
 * the linker --wrap=main trick and redirect to Zephyr's kernel init.
 *
 * Flow:
 *   XC32 crt0 → _main_entry → __wrap_main() [this file]
 *       → z_prep_c() → z_cstart() → kernel scheduler
 *           → main thread → __real_main() [application's main()]
 */

extern void z_prep_c(void);
extern int __real_main(void);



void _on_bootstrap(void)
{
}

void _on_reset(void)
{
}

/*
 * __wrap_main - intercepts ALL calls to main() due to --wrap=main.
 * First call (from crt0): starts Zephyr kernel.
 * Second call (from kernel bg_thread_main): calls the real app main().
 */
int __wrap_main(void)
{
	static int first = 1;

	if (first) {
		first = 0;
		z_prep_c();
		return 0;
	}
	return __real_main();
}
