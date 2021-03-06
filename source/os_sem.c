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

#include "os_private.h"

/* private function forward declarations */
static void os_sem_timerclbck(void *param);

/* --- public functions --- */
/* all public functions are documented in os_sem.h file */

void os_sem_create(
   os_sem_t *sem,
   arch_atomic_t init_value)
{
   OS_ASSERT(init_value < ARCH_ATOMIC_MAX);
   OS_ASSERT(!waitqueue_current); /* cannot call after os_waitqueue_prepare() */

   memset(sem, 0, sizeof(os_sem_t));
   os_taskqueue_init(&(sem->task_queue));
   sem->value = init_value;
}

void os_sem_destroy(os_sem_t *sem)
{
   arch_criticalstate_t cristate;
   os_task_t *task;

   OS_ASSERT(0 == isr_nesting);     /* cannot call from ISR */
   OS_ASSERT(!waitqueue_current);   /* cannot call after os_waitqueue_prepare() */

   arch_critical_enter(cristate);

   /* wake up all task which suspended on semaphore */
   while ((task = os_taskqueue_dequeue(&(sem->task_queue)))) {
      os_blocktimer_destroy(task); /* destroy the tasks timer */
      task->block_code = OS_DESTROYED;
      os_task_makeready(task);
   }
   /* destroy all semaphore data, this can create problems if semaphore is used
    * in interrupt context (feel warned) */
   memset(sem, 0, sizeof(os_sem_t));

   /* schedule to make context switch in case os_sem_destroy() was called by
    * lower priority task than tasks which we just woken up */
   os_schedule(1);

   arch_critical_exit(cristate);
}

os_retcode_t OS_WARN_UNUSEDRET os_sem_down(
   os_sem_t *sem,
   uint_fast16_t timeout_ticks)
{
   os_retcode_t ret;
   os_timer_t timer;
   arch_criticalstate_t cristate;

   OS_ASSERT(0 == isr_nesting); /* cannot call from ISR */
   OS_ASSERT(task_current != &task_idle); /* idle task cannot block */
   OS_ASSERT(!waitqueue_current); /* cannot call after os_waitqueue_prepare() */
   /* calling of blocking function while holding mtx will cause priority inversion */
   OS_ASSERT(list_is_empty(&task_current->mtx_list));

   /* critical section needed because of timers and other ISRs which might call
    * sem_up() while we operate on sem->task_queue and task_queue */
   arch_critical_enter(cristate);
   do {
      if (sem->value > 0) {
         /* in case sem->value is not zero, we do not have to block, just to
          * consume one from sem->value */
         /* /TODO in future try to implement the "condition and decrement" as a
          * CAS operation and move it before critical section, this will
          * increase performance (something like os_atomic_cas() */
         --(sem->value);
         ret = OS_OK;
         break;
      }

      /* sem->value == 0, need to block the calling task */
      if (OS_TIMEOUT_TRY == timeout_ticks) {
         /* task request to bail out in case operation would block */
         ret = OS_WOULDBLOCK;
         break;
      }

      /* does task request timeout guard for operation? */
      if (OS_TIMEOUT_INFINITE != timeout_ticks) {
         /* we will get callback to os_sem_timerclbck() in case of timeout */
         os_blocktimer_create(&timer, os_sem_timerclbck, timeout_ticks);
      }

      /* now block and switch the context */
      os_task_block_switch(&(sem->task_queue), OS_TASKBLOCK_SEM);

      /* we return here once other task call os_sem_up() or timeout burs off
       * cleanup, destroy timeout associated with task if it was created */
      os_blocktimer_destroy(task_current);

      /* check the block_code, it was set in os_sem_destroy(), timer callback or
       * in os_sem_up() */
      ret = task_current->block_code;

   } while (0);
   arch_critical_exit(cristate);

   return ret;
}

/* Copy from semaphore description
 * semaphores allow for single signalization per os_sem_up(). Defining API which
 * would allow for multiple signalizations per os_sem_up() would create
 * controversy behaviour. In case woken up task would have higher priority than
 * task which signal the semaphore it has to be decided if system should allow
 * for preemption and possibly the same task will return to suspend point and
 * consume next signal, or system should wakeup multiple task simultaneously and
 * then allow for preemption the signaling task. */
void os_sem_up_sync(
   os_sem_t *sem,
   bool sync)
{
   arch_criticalstate_t cristate;
   os_task_t *task;

   /* \TODO implement nbr in function param so we can increase the semaphore
    * number more than once. To make it work in this way we have also to wake
    * multiple tasks. Best will be to create loop around existing code and wake
    * up task until cnt in this loop > 0, Example how to do it is in
    * waitqueue_wakeup() */

   /* sync must be == false in case we are called from ISR */
   OS_ASSERT((isr_nesting == 0) || (sync == false));
   /* cannot call after os_waitqueue_prepare() in case of task context */
   OS_ASSERT((isr_nesting > 0) || !waitqueue_current);

   arch_critical_enter(cristate);

   /* check if semaphore value would overflow */
   OS_ASSERT(sem->value < (ARCH_ATOMIC_MAX - 1));

   /* check if there are some suspended tasks on this sem */
   task = os_taskqueue_dequeue(&(sem->task_queue));
   if (!task) {
      /* there was no suspended tasks, in this case just increment the sem value */
      ++(sem->value);
   } else {
      /* there is suspended task, we need to wake it up
       * we need to destroy the guard timer of this task, because otherwise it
       * may fire right after we leave the critical section */
      os_blocktimer_destroy(task);

      task->block_code = OS_OK; /* set the block code to NORMAL WAKEUP */
      os_task_makeready(task);

      /* do not call schedule() if user requested sync mode.
       * User code may call some other OS function right away which will trigger
       * the os_schedule(). Parameter 'sync' is used for such optimization
       * request */
      if (!sync) {
         /* switch to more prioritized READY task, if there is such (1 as param
          * in os_schedule() means just that */
         os_schedule(1);
      }
   }
   arch_critical_exit(cristate);
}

/* --- private functions --- */

/**
 * Function called by timers module. Used for timeout of os_sem_down().
 * Callback to this function are done from contxt of timer_trigger().
 */
static void os_sem_timerclbck(void *param)
{
   /* single timer has param in os_blocktimer_create() as pointer to task
    * structure */
   os_task_t *task = (os_task_t*)param;

   OS_SELFCHECK_ASSERT(TASKSTATE_WAIT == task->state);

   /* remove task from semaphore task queue (in os_sem_up() the
    * os_taskqueue_dequeue() does the same job */
   os_taskqueue_unlink(task);
   task->block_code = OS_TIMEOUT;
   os_task_makeready(task);
   /* we do not call the os_schedule() here, because this will be done at the
    * end of timer_trigger() */

   /* we do not destroy timer here since this timer was created on stack of
    * os_sem_down(), there is proper cleanup code in that function */
   /* timer is not auto reload so we don't have to worry about if it will call
    * this function again */
}

