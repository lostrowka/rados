/*
 * This file is a part of RadOs project
 * Copyright (c) 2013, Radoslaw Biernaki <radoslaw.biernacki@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2) Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3) No personal names or organizations' names associated with the 'RadOs' project
 *    may be used to endorse or promote products derived from this software without
 *    specific prior written permission.
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

/* This file  contains all definitions used by various kernel modules which
   cannot be exposed for user view (in other words they are shared between kernel
   modules). Those definitions can be used only by the internal kernel code
   (similarity to private class clause in C++)*/

#ifndef __OS_PRIVATE_
#define __OS_PRIVATE_

#include "os.h" /* to include all public definitions */

/* --- OS macro definitions --- */

#ifdef OS_CONFIG_APICHECK
#define OS_ASSERT(_cond) \
   do \
   { \
      if( OS_UNLIKELY(!(_cond)) ) { \
         arch_halt(); \
      } \
   }while(0)
#else
#define OS_ASSERT
#endif

#ifdef OS_CONFIG_SELFCHECKING
#define OS_SELFCHECK_ASSERT(_cond) \
   do \
   { \
      if( OS_UNLIKELY(!(_cond)) ) { \
         arch_halt(); \
      } \
   }while(0)
#else
#define OS_SELFCHECK_ASSERT
#endif

/**
* Common macro that allows to calculate pointer to parent,
* from pointer to member, name of the member inside parent and parent object type
* Using of temporary pointer _mptr is necessary to prevent macro side effects for
* operands like pointer++
*
* @param _prt Pointer to member
* @param _type Parent object type
* @param _member Name of the member inside parent object
*
* @return Pointer to parent
*/
#define os_container_of(_ptr, _type, _member) ({ \
   const typeof( ((_type *)0)->_member ) *_mptr = (_ptr); \
   (_type *)( (char *)_mptr - offsetof(_type,_member) );})

/*
* Common min macro with strict type-checking,
* returns the smaller value from two operands
*
* Strict type checking in important aspect of secure code,
* (sign type mixed checking is common source of exploitable bugs).
* Using of temporary values is necessary to prevent macro side effects for
* operands like variable++
*
* @param _x First value
* @param _y Second value
*
* @return Smaller of two passed values
*/
#define os_min(_x, _y) ({ \
   typeof(_x) _min1 = (_x); \
   typeof(_y) _min2 = (_y); \
   (void) (&_min1 == &_min2); \
   _min1 < _min2 ? _min1 : _min2; })

/*
* Common max macro with strict type-checking,
* returns the greater value from two operands
*
* Strict type checking in important aspect of secure code,
* (sign type mixed checking is common source of exploitable bugs).
* Using of temporary values is necessary to prevent macro side effects for
* operands like variable++
*
* @param _x First value
* @param _y Second value
*
* @return Greater of two passed values
*/

#define os_max(_x, _y) ({ \
   typeof(_x) _max1 = (_x); \
   typeof(_y) _max2 = (_y); \
   (void) (&_max1 == &_max2); \
   _max1 > _max2 ? _max1 : _max2; })

/**
* Macro returns number of table elements
*
* @param _table Table pinter
* @return Number of table elements, the return type is size_t
*/
#define os_element_cnt(_table) (sizeof((_table)) / sizeof((_table)[0]))

/* --- Scheduler section --- */

os_taskqueue_t ready_queue; /* visiable only for os files, not for user */
volatile os_atomic_t sched_lock; /* visiable only for os files, not for user */

void OS_HOT os_task_enqueue(os_taskqueue_t* task_queue, os_task_t* task);
void OS_HOT os_task_unlink(os_task_t* OS_RESTRICT task);
void OS_HOT os_task_reprio( os_task_t* task, uint_fast8_t new_prio);
os_task_t* OS_HOT os_task_dequeue(os_taskqueue_t* task_queue);
os_task_t* OS_HOT os_task_dequeue_prio(os_taskqueue_t* task_queue, uint_fast8_t prio);
os_task_t* OS_HOT os_task_peekqueue(os_taskqueue_t* OS_RESTRICT task_queue);
void os_taskqueue_init(os_taskqueue_t *task_queue);
void OS_HOT os_schedule(uint_fast8_t higher_prio);
void OS_HOT os_block_andswitch(
   os_taskqueue_t* OS_RESTRICT task_queue,
   os_taskblock_t block_type);
void os_task_exit(int retv);

/* functions provided by arch port */

/** Architecture and platform dependent low level initialization
  *
  * This function is called from the inside of os_start() after idle task
  * creation, but before it is started. This function should initialize remain
  * resources which are needed for OS to run the idle task. In contrast to all
  * platform dependent actions done in main (before calling os_start()), this
  * function may interact with kernel internals (but not external API), since
  * basic structures are initialized. Keep in mind that things like timer for
  * ticks setup should be leaved up to app (user) so it will have more freedom
  * while assigning HW resources for his app. Also this function cannot create
  * tasks, mutexet ect, since this should be done in app_init()
  * In general this function will be emply for most platforms, since all
  * necessary preparations are usualy done in main, before calling os_start()
  */
void arch_os_start(void);

/* \note we dont mark arch_context_switch as OS_NAKED since not all arch use that */
void /* OS_NAKED */ OS_HOT arch_context_switch(os_task_t *new_task);
void OS_NORETURN OS_COLD arch_halt(void);
void arch_task_init(os_task_t *tcb, void* stack, size_t stack_size, os_taskproc_t proc, void* param);
void arch_idle(void);

/* this function blocks the preemptive scheduling
   Take into account that interrupts are still enabled, while only the task switch will not be performed
   Make sure that implementation changes the isr_nesting in atomic maner (same apply to arch_contextstore_i)

   /note disabling the scheduler betwen two interrupts is save (atomic), because os_schedule is called in critical section, so if we are in interupt then we didnt break the execution of code protected by critical section */
static inline void os_scheduler_lock(void) {
   os_atomic_inc(sched_lock);
}

static inline void os_scheduler_unlock(void) {
   os_atomic_dec(sched_lock);
   /* \TODO we should call os_schedule() external API here since we may be not
    * in attomic section while it may be that we need to schedule task right
    * away. The reason for that is that usualy os_scheduler_lock() was called
    * for temporary locking in some critical section of user code, and that
    * after unlock() we usualy should schedule(). This situation is for instance
    * in os_start() where after creating idle task and unlocking the scheduler,
    * any user task should be scheduled right away() (those created in
    * app_init() */
}

static inline void os_task_makeready(os_task_t *task)
{
   task->state = TASKSTATE_READY; /* set the task state */
   if(NULL != task->wait_queue) {
       /* in case task has assosiated wait_queue, add task to task_queue of
        * wait_queue assigned to this task */
      os_task_enqueue(&(task->wait_queue->task_queue), task);
   } else {
      /* otherwise put it into ready_queue */
      os_task_enqueue(&ready_queue, task);
   }
}

static inline void os_task_makewait(
   os_taskqueue_t *task_queue,
   os_taskblock_t block_type)
{
   //task_current->block_code = OS_INVALID;
   task_current->state = TASKSTATE_WAIT;
   task_current->block_type = block_type;
   os_task_enqueue(task_queue, task_current);
}

static inline void os_blocktimer_create(
   os_timer_t *timer,
   timer_proc_t clbck,
   uint_fast16_t timeout_ticks)
{
   OS_SELFCHECK_ASSERT(NULL == task_current->timer);

   os_timer_create(timer, clbck, task_current, timeout_ticks, 0);
   task_current->timer = timer;
}

static inline void os_blocktimer_destroy(os_task_t *task)
{
   /* check if there is a timeout assosiated with task */
   if( NULL != task->timer ) {
     os_timer_destroy(task->timer);
     task->timer = NULL;
   }
}

/* --- Timers protected types --- */

/* protected function from timer module */
void os_timers_init(void);
void OS_HOT os_timer_tick(void);

#endif

