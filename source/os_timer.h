/*
 * This file is a part of RadOs project
 * Copyright (c) 2013, Radoslaw Biernacki <radoslaw.biernacki@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2) Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3) No personal names or organizations' names associated with the 'RadOs'
 *    project may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE RADOS PROJECT AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __OS_TIMER_
#define __OS_TIMER_

#define OS_TIMEOUT_INFINITE (OS_TICKS_MAX)
#define OS_TIMEOUT_TRY ((os_ticks_t)0)

typedef void (*timer_proc_t)(void *param);

typedef struct {
   list_t list;
   os_ticks_t ticks_rem;      /**< remaining ticks before burn off */
   os_ticks_t ticks_reload;   /**< reload value in case of auto reload */
   timer_proc_t clbck;
   void *param;
#ifdef OS_CONFIG_APICHECK
   uint_fast16_t magic; /**< use case sanitization checking mark */
#endif
} os_timer_t;

/**
 * System tick function (system timer interrupt)
 *
 * This function need to be called from ISR. It kicks the timer subsystem and
 * also triggers the preemption mechanism. The frequency of os_tick() call is
 * user defined and it defined a jiffy. All timeouts for time guarded OS
 * blocking functions are measured in jiffies.
 *
 * @pre can be called only from ISR
 */
void OS_HOT os_tick(void);

/* \note timer_proc_clbck cannot call the os_sched, this will be done at the end
 * of timer_tigger() */
void os_timer_create(
   os_timer_t *timer,
   timer_proc_t clbck,
   void *param,
   os_ticks_t timeout_ticks,
   os_ticks_t reload_ticks);

/* \note this function is designed in way, that ss long as the memory for timer
 * is valid it allows for multiple destroy operations on the same timer */
void os_timer_destroy(os_timer_t *timer);

os_ticks_t os_ticks_now(void);

os_ticks_t os_ticks_diff(
   os_ticks_t ticks_start,
   os_ticks_t ticks_now);

#endif

