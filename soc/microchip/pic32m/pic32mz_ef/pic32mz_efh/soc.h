/*
 * Copyright (c) 2026 Microchip Technology Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SOC_MICROCHIP_PIC32MZ_EFH_SOC_H_
#define SOC_MICROCHIP_PIC32MZ_EFH_SOC_H_

#ifndef _ASMLANGUAGE

#include <zephyr/types.h>

#if defined(CONFIG_SOC_PIC32MZ2048EFH144)
#include <pic32mz2048efh144.h>
#elif defined(CONFIG_SOC_PIC32MZ2048EFH100)
#include <pic32mz2048efh100.h>
#elif defined(CONFIG_SOC_PIC32MZ2048EFH064)
#include <pic32mz2048efh064.h>
#else
#error "Library does not support the specified device."
#endif

#endif /* _ASMLANGUAGE */

#endif /* SOC_MICROCHIP_PIC32MZ_EFH_SOC_H_ */
