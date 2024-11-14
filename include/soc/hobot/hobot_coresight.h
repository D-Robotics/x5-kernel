/*
 * Horizon Robotics
 *  Author:     Neil Zhang <zhangwm@marvell.com>
 *  Copyright (C) 2020 Horizon Robotics Inc.
 *  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __HOBOT_CORSIGHT_H__
#define __HOBOT_CORSIGHT_H__


#ifdef CONFIG_HOBOT_CORESIGHT
void coresight_dump_pcsr(u32 cpu);
void coresight_trigger_panic(int cpu);


/* The following functions must be implemented by each architeture */
extern void arch_enable_access(u32 cpu);
extern void arch_dump_pcsr(u32 cpu);
extern int arch_halt_cpu(u32 cpu);
extern void arch_insert_inst(u32 cpu);
extern void arch_restart_cpu(u32 cpu);

extern void arch_save_coreinfo(void);
extern void arch_restore_coreinfo(void);
extern void arch_save_mpinfo(void);
extern void arch_restore_mpinfo(void);

extern void __init arch_coresight_init(void);
extern void __init arch_enable_trace(u32 enable_mask);

#else
#define coresight_dump_pcsr(cpu)     do {} while (0)
#define coresight_trigger_panic(cpu)        do {} while (0)
#endif

#endif /* __HOBOT_CORSIGHT_H__ */
