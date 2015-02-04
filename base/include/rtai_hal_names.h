/*
 * Copyright 2015 Paolo Mantegazza <mantegazza@aero.polimi.it>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#ifndef _RTAI_HAL_NAMES_H
#define _RTAI_HAL_NAMES_H

#include <linux/version.h>

//#define IPIPE_ROOT_NPTDKEYS  4

#define TSKEXT0  0 // (HAL_ROOT_NPTDKEYS - 4)
#define TSKEXT1  1 // (HAL_ROOT_NPTDKEYS - 3)
#define TSKEXT2  2 // (HAL_ROOT_NPTDKEYS - 2)
#define TSKEXT3  3 // (HAL_ROOT_NPTDKEYS - 1)

#define HAL_VERSION_STRING   IPIPE_VERSION_STRING

#define HAL_NR_CPUS          IPIPE_NR_CPUS
#define HAL_NR_FAULTS        IPIPE_NR_FAULTS
#define HAL_NR_EVENTS        IPIPE_NR_EVENTS
#define HAL_ROOT_NPTDKEYS    IPIPE_ROOT_NPTDKEYS

#define HAL_APIC_HIGH_VECTOR  IPIPE_HRTIMER_VECTOR //IPIPE_SERVICE_VECTOR3
#define HAL_APIC_LOW_VECTOR   IPIPE_RESCHEDULE_VECTOR //IPIPE_SERVICE_VECTOR2

#define hal_pipeline        __ipipe_pipeline
#define hal_domain_struct   ipipe_domain 
#define hal_root_domain     ipipe_root_domain 

#define hal_current_domain(cpuid)  per_cpu(ipipe_percpu_domain, cpuid) 

#define hal_propagate_irq   ipipe_propagate_irq
#define hal_schedule_irq    ipipe_schedule_irq

#define hal_critical_enter  ipipe_critical_enter
#define hal_critical_exit   ipipe_critical_exit

#define hal_lock_irq    __ipipe_lock_irq
#define hal_unlock_irq  __ipipe_unlock_irq

#define hal_std_irq_dtype        __ipipe_std_irq_dtype
#define hal_ipipe_std_irq_dtype  __adeos_std_irq_dtype

#define hal_tick_regs  __ipipe_tick_regs
#define hal_tick_irq   __ipipe_tick_irq

#define hal_sync_stage  __ipipe_sync_stage

#define hal_set_irq_affinity  ipipe_set_irq_affinity

#define hal_propagate_event  ipipe_propagate_event

#define hal_get_sysinfo  ipipe_get_sysinfo

#define HAL_TYPE  "IPIPE-NOTHREADS"

#define INTERCEPT_WAKE_UP_TASK(data)  ((struct task_struct *)data)

#define hal_alloc_irq       ipipe_alloc_virq
#define hal_free_irq        ipipe_free_virq

#define hal_sysinfo_struct         ipipe_sysinfo

#define hal_set_printk_sync   ipipe_set_printk_sync
#define hal_set_printk_async  ipipe_set_printk_async

#define hal_schedule_back_root(prev)  __ipipe_reenter_root() 

#define hal_processor_id  ipipe_processor_id

#define hal_hw_cli                local_irq_disable_hw
#define hal_hw_sti                local_irq_enable_hw
#define hal_hw_local_irq_save     local_irq_save_hw
#define hal_hw_local_irq_restore  local_irq_restore_hw
#define hal_hw_local_irq_flags    local_save_flags_hw

#define hal_set_timer(ns)  ipipe_tune_timer(ns, 0)
#define hal_reset_timer()  ipipe_tune_timer(0, IPIPE_RESET_TIMER)

#define hal_unstall_pipeline_from  ipipe_unstall_pipeline_from

#define hal_ack_system_irq  __ipipe_ack_apic

#define hal_irq_handler     rtai_irq_handler

#define hal_tskext  ptd

#endif
