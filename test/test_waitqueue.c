/*
 * This file is a part of RadOs project
 * Copyright (c) 2013, Radoslaw Biernacki <radoslaw.biernacki@gmail.com>
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

/**
 * /file Test os semaphore rutines
 * /ingroup tests
 *
 * /{
 */

#include "os.h"
#include "os_test.h"

#define TEST_TASKS ((unsigned)10)

#if 1
#define test_verbose_debug test_debug
//#define test_verbose_debug(format, ...)

#else
static uint_fast8_t test_debug_idx = 0;
static const char test_debug_star[] = { '-', '\\', '|', '/' };

#include <stdio.h>
#define test_verbose_debug(format, ...) \
{ \
  test_debug_idx = (test_debug_idx + 1u > sizeof(test_debug_star) ? 0 : test_debug_idx + 1u); \
  printf("\b%c", test_debug_star[test_debug_idx]); \
  fflush(stdout); \
}
#endif

typedef struct {
   os_task_t task;
   OS_TASKSTACK task1_stack[OS_STACK_MINSIZE];
   unsigned idx;
   os_waitqueue_t wait_queue;
   volatile unsigned spin_intcond;
   volatile unsigned spin_extcond;
   volatile unsigned spin_spincnt;
   volatile unsigned spin_condmeetcnt;
   volatile unsigned spin_intcond_reload;
   uint_fast16_t timeout;
   os_retcode_t retcode;
} task_data_t;

typedef int (*test_case_t)(void);

static os_task_t task_main;
static OS_TASKSTACK task_main_stack[OS_STACK_MINSIZE];
static task_data_t worker_tasks[TEST_TASKS];
static os_waitqueue_t global_wait_queue;
static os_sem_t global_sem;
static volatile unsigned global_tick_cnt = 0;

void idle(void)
{
   /* nothing to do */
}

/**
 * Simple test procedure for typicall waitloop usage.
 */
int slavetask_proc(void* param)
{
   task_data_t *data = (task_data_t*)param;

  /* we have two loops
   * internal one simulates waiting for external event and on that loop we focus
   * external one is used only for making multiple test until we decide that
   * task should be joined, therefore os_waitqueue_prepare() is inside the
   * outern loop */

   test_verbose_debug("Task %u started", data->idx);

   while(0 == data->spin_extcond)
   {
      if(data->spin_intcond_reload > 0)
      {
         --(data->spin_intcond_reload);
         data->spin_intcond = 1;
      } else {
         data->spin_intcond = 0;
      }

      while(1)
      {
         os_waitqueue_prepare(&global_wait_queue);
         test_verbose_debug("Task %u spins ...", data->idx);
         ++(data->spin_spincnt); /* signalize that we performed condition test */
         if(0 != data->spin_intcond)
         {
            os_waitqueue_break();
            break;
         }
         /* condition not meet, go to sleep */
         data->retcode = os_waitqueue_wait(
            (0 == data->timeout) ? OS_TIMEOUT_INFINITE : data->timeout);
         /* only timeout and success is allowed as return code */
         if(OS_TIMEOUT == data->retcode)
         {
            test_verbose_debug("Task %u timeouted on wait_queue", data->idx);
         }
         else if(OS_DESTROYED == data->retcode)
         {
            test_verbose_debug("Task %u returned from wait_queue with code OS_DESTROYED", data->idx);
            return 0;
         }
      }
      test_verbose_debug("Task %u found condition meet", data->idx);
      (data->spin_condmeetcnt)++;
   }
   test_verbose_debug("Task %u exiting by condition meet", data->idx);

   return 0;
}

/**
 * Testing of all tasks wakeup
 * - testing OS_WAITQUEUE_ALL
 * - testing param nbr = TEST_TASKS (should be same effect as OS_WAITQUEUE_ALL)
 */
int testcase_1(void)
{
   os_retcode_t retcode;
   unsigned i;

   /* wait until all tasks will suspend on wait_queue */
   for(i = 0; i < TEST_TASKS; i++)
   {
      test_verbose_debug("Spin count for task %u is %u",
                         i, worker_tasks[i].spin_spincnt);
      if(0 == worker_tasks[i].spin_spincnt)
      {
         i = -1; /* start check loop from begining */
         /* give time for test tasks to run, we use this sem only for timeout,
          * nobody will rise this sem */
         retcode = os_sem_down(&global_sem, 10); /* 10 ticks of sleep */
         test_assert(OS_TIMEOUT == retcode);
      }
   }

   /* all tasks had been started, they should now be suspended on wait_queue
    * wake up all tasks and check if they made a spin around condition check */
   test_verbose_debug("Main task - wake up all slaves - OS_WAITQUEUE_ALL");
   os_waitqueue_wakeup(&global_wait_queue, OS_WAITQUEUE_ALL);

   /* need to sleep to make slaves run (main is most prioritized) */
   retcode = os_sem_down(&global_sem, 10); /* 10 ticks of sleep */
   test_assert(OS_TIMEOUT == retcode);

   /* verify that all tasks have made a spin */
   for(i = 0; i < TEST_TASKS; i++) {
      test_verbose_debug("Spin count for task %u is %u",
                         i, worker_tasks[i].spin_spincnt);
      test_assert(2 == worker_tasks[i].spin_spincnt);
   }

   /* last time we woken up all task with OS_WAITQUEUE_ALL
    * try to woke up all tasks by specifying the exact amount */
   test_verbose_debug("Main task - wake up all slaves - nbr = TEST_TASKS");
   os_waitqueue_wakeup(&global_wait_queue, TEST_TASKS);

   /* need to sleep to make slaves run (main is most prioritized) */
   retcode = os_sem_down(&global_sem, 10); /* 10 ticks of sleep */
   test_assert(OS_TIMEOUT == retcode);

   /* verify that all tasks have made a spin */
   for(i = 0; i < TEST_TASKS; i++) {
      test_verbose_debug("Spin count for task %u is %u",
                         i, worker_tasks[i].spin_spincnt);
      test_assert(3 == worker_tasks[i].spin_spincnt);
   }

   return 0;
}

/**
 * Testing param nbr
 * - checking if most prio task could be woken up without waking up others
 */
int testcase_2(void)
{
   os_retcode_t retcode;
   unsigned i;

   /* task 0 has highest priority from slaves, if we wake only one slave it
    * should be the only task which will woke up */
   test_verbose_debug("Main task signalizes single slave");
   os_waitqueue_wakeup(&global_wait_queue, 1);

   /* need to sleep to make slaves run (main is most prioritized) */
   retcode = os_sem_down(&global_sem, 10); /* 10 ticks of sleep */
   test_assert(OS_TIMEOUT == retcode);

   /* verify that only task 10 made a spin and if other does not */
   for(i = 0; i < TEST_TASKS; i++) {
      test_verbose_debug("Spin count for task %u is %u",
                         i, worker_tasks[i].spin_spincnt);
      test_assert((9 == i ? 4 : 3) == worker_tasks[i].spin_spincnt);
   }

   return 0;
}

/**
 * Testing the time guard of os_waitqueue_wait(), with multiple time delays
 */
int testcase_3(void)
{
   os_retcode_t retcode;
   unsigned i;

   /* modify the timeout of each task */
   for(i = 0; i < TEST_TASKS; i++)
   {
      worker_tasks[i].timeout = 1 + i * 2; /* in number of ticks */
   }

   /* wake up tasks to allow them to use timeout in next prepare call */
   test_verbose_debug("Main signalizing slaves for timeout reload");
   os_waitqueue_wakeup(&global_wait_queue, OS_WAITQUEUE_ALL);
   /* need to sleep to make slaves run (main is most prioritized) */
   retcode = os_sem_down(&global_sem, 1);
   test_assert(OS_TIMEOUT == retcode);

   /* reset sampling data for test */
   for(i = 0; i < TEST_TASKS; i++) {
      worker_tasks[i].spin_spincnt = 0;
   }

   /* now slave tasks waits on timeout condition, we don't want for next loop to
    * also use timeout, so we clear up the communication variable */
   for(i = 0; i < TEST_TASKS; i++) {
      worker_tasks[i].timeout = 0;
   }

   global_tick_cnt = 1;
   test_verbose_debug("Main wait until slaves will timeout");
   /* verify that only desired task will timeout at each tick count */
   while(global_tick_cnt < (TEST_TASKS * 2) + 1)
   {
       retcode = os_sem_down(&global_sem, 1);
       test_assert(OS_TIMEOUT == retcode);
       test_verbose_debug("Main woken up global_tick_cnt = %u", global_tick_cnt);
       for(i = 0; i < TEST_TASKS; i++)
       {
         test_verbose_debug("Spin count for task %u is %u",
                            i, worker_tasks[i].spin_spincnt);
         if(i < global_tick_cnt / 2)
         {
            /* this task should already timeout and spin the loop */
            test_assert(1 == worker_tasks[i].spin_spincnt);
            test_assert(OS_TIMEOUT == worker_tasks[i].retcode);
         } else {
            /* this task should not timeout */
            test_assert(0 == worker_tasks[i].spin_spincnt);
         }
      }
   }

   return 0;
}

/**
 * Initially I forgot about implementation of os_waitqueue_break()
 * Testing of os_waitqueue_break()
 */
int testcase_4regresion(void)
{
   os_retcode_t retcode;
   unsigned i;

   /* WARNING task 10 is excluded from this test since it has bigger priority than other tasks
      if it will run, it will not allow other tasks to schedule */

   /* prepare data for test */
   for(i = 0; i < TEST_TASKS - 1; i++) {
      /* all task should found condition meet without calling os_waitqueue_wait */
      worker_tasks[i].spin_intcond = 1;
      worker_tasks[i].spin_spincnt = 0;
      /* after exiting from intcond reload the condition and set it to 1
       * reload=10 will cause 10 spins with cond meet in row */
      worker_tasks[i].spin_intcond_reload = 10;
   }

   /* wake up all tasks and sleep 1 tick
    * this will cause multiple spins of external loop sine worker_tasks[i].spin_extcond is
    * still 0 while worker_tasks[i].spin_intcond is 1 */
   test_verbose_debug("Main allowing slaves to spin multiple times on external loop");
   os_waitqueue_wakeup(&global_wait_queue, OS_WAITQUEUE_ALL);

   /* need to sleep to make slaves run (main is most prioritized) */
   retcode = os_sem_down(&global_sem, 10);
   test_assert(OS_TIMEOUT == retcode);

   /* check that all task have made a spin multiple times */
   for(i = 0; i < TEST_TASKS - 1; i++) {
      test_assert(worker_tasks[i].spin_spincnt > 1);
   }

   /* clean up after test, stop the spinning of tasks */
   for(i = 0; i < TEST_TASKS - 1; i++) {
      worker_tasks[i].spin_intcond = 1;
      worker_tasks[i].spin_intcond_reload = 0;
   }

   return 0;
}

void stack_overwrite(void)
{
   uint8_t tmp[64];
   size_t i;

   for (i = 0; i < 64; i++)
   {
      tmp = i;
   }
   test_assert(tmp[0] == 0);
   test_assert(tmp[63] == 63);
}

/**
 * Testing os_waitqueue_wait() and os_waitqueue_break() implementation with
 * timeout guard.
 */
int slave_proc(void* param)
{
   slave_param_t *slave_param = (slave_param_t*)param;
   os_retcode_t ret;

   os_waitqueue_prepare(waitqueue);

   /* lest spin and check if preemption will not kick in */
   while(1)
   {
      if (global_tick_cnt > slave_param->spin_until)
      {
         test_verbose_debug("slave %zu finishes condition",
                            slave_param->slave_idx);
         break;
      }
   }

   if (wait)
   {
      slave_param->retcode = os_waitqueue_wait(slave_param->timeout);
   } else {
      os_waitqueue_break();
   }
   slave_param->exit = true;

   /* verify the cleanup (following two will assert if waitqueue_current will no
    * be properly cleared */
   stack_overwrite();
   test_assert(NULL == task_current->timer);

   return 0;
}

typedef struct {
   size_t slave_idx;
   os_waitqueue_t *waitqueue;
   os_ticks_t spin_until;
   os_ticks_t timeout,
   os_retcode_t retcode;
   bool exit;
} slave_param_t;

static volatile unsigned irq_trigger_tick = 0;
static os_waitqueue_t *irq_trigger_waitqueue = NULL;
static uint_fast8_t irq_trigger_nbr = 0;

static os_task_t slave_task[5];
static OS_TASKSTACK slave_stack[5][OS_STACK_MINSIZE];
static slave_param_t slave_param[5];

void start_helper_task(
   size_t slave_idx,
   os_waitqueue_t *waitqueue,
   os_ticks_t spin_until,
   os_ticks_t timeout)
{
   slave_param_t *param = &slave_param[slave_idx];

   param->slave_idx = slave_idx;
   param->waitqueue = waitqueue;
   param->spin_until = spin_until;
   param->timeout = timeout;
   param->spin = 0;
   test_verbose_debug("creating slave task %zu", slave_idx);
   os_task_create(
      &slave_task, slave_idx == 0 ? 1 : 2, /* slave[0] with prio 1, rest 2 */
      slave_stack[slave_idx], sizeof(slave_stack[slave_idx]),
      slave_proc, param);
}

int join_helper_task(size_t slave_idx)
{
   test_verbose_debug("joining helper task %zu", slave_idx);
   return os_task_join(&slave_task[slave_idx]);
}

int testcase_6(void)
{
   os_waitqueue_t waitqueue;

   os_waitqueue_create(&waitqueue);

   /*  */
   test_verbose_debug("main: ISR signal before timeout");
   global_tick_cnt = 0; /* reset tickcnt's */
   testcase_6_impl(true, &waitqueue, false, true);
   test_verbose_debug("testing timeout impl with os_waitqueue_break()");
   global_tick_cnt = 0; /* reset tickcnt's */
   testcase_6_impl(true, &waitqueue, true, true);

   /* next we will test wakeup from ISR, in this case in os_waitqueue_wakeup()
    * there is a special code which detects that we interrupted the task
    * spinning wait_queue related condition which ISR is going to wakeup. There
    * is also the second section for other tasks. We would like to test both of
    * code sections so we create additional task for that. Both will share the
    * same priority so we will always run both sections (either main_task will
    * be the task_current or it will be the other task, depending on tick epoh)
    */
   irq_trigger_waitqueue = &waitqueue;
   irq_trigger_tick = 2;
   start_sleeper_task(&waitqueue);

   /* test timeout behaviour when wakeup happen from ISR before timeout (but
    * before suspend) */
   test_verbose_debug("testing timeout impl with os_waitqueue_wait() with wakeup"
                      "from ISR before timeout");
   global_tick_cnt = 0; /* reset tickcnt's */
   start_helper_task(&waitqueue, false, false);
   testcase_6_impl(true, &waitqueue, false, false);
   join_helper_task();
   test_assert(false == sleeper_wokenup);

   test_verbose_debug("testing timeout impl with os_waitqueue_break() with wakeup"
                      "from ISR before timeout");
   global_tick_cnt = 0; /* reset tickcnt's */
   start_helper_task(&waitqueue, false, false);
   testcase_6_impl(true, &waitqueue, true, false);
   join_helper_task();
   test_assert(false == sleeper_wokenup);

   /* test timeout behaviour when wakeup happen from ISR after timeout (but
    * before suspend) */
   irq_trigger_waitqueue = &waitqueue;
   irq_trigger_tick = 4;
   test_verbose_debug("testing timeout impl with os_waitqueue_wait() with wakeup"
                      "from ISR after timeout");
   global_tick_cnt = 0; /* reset tickcnt's */
   start_helper_task(&waitqueue, false, false);
   testcase_6_impl(true, &waitqueue, false, true);
   join_helper_task();
   test_assert(false == sleeper_wokenup);

   test_verbose_debug("testing timeout impl with os_waitqueue_break() with wakeup"
                      "from ISR after timeout");
   global_tick_cnt = 0; /* reset tickcnt's */
   start_helper_task(&waitqueue, false, false);
   testcase_6_impl(true, &waitqueue, true, true);
   join_helper_task();
   test_assert(false == sleeper_wokenup);

   irq_trigger_waitqueue = NULL;
   irq_trigger_tick = 0;

   /* join sleeper task */
   os_waitqueue_wakeup(&waitqueue, 1);
   (void)join_helper_task();
   test_assert(true == sleeper_wokenup);

   os_waitqueue_destroy(&waitqueue);

   return 0;
}

/**
 * Testing os_waitqueue_destroy() with tasks which suspend with time guards
 */
int testcase_7(void)
{
   int ret;
   unsigned i;
   os_retcode_t retcode;

   /* modify the timeout of each task so we will be also able to verify if
    * we properly remove timer in os_waitqueue_destroy() */
   for(i = 0; i < TEST_TASKS; i++) {
      worker_tasks[i].spin_intcond = 0;
      worker_tasks[i].spin_extcond = 0;
      worker_tasks[i].spin_spincnt = 0;
      worker_tasks[i].spin_condmeetcnt = 0;
      worker_tasks[i].spin_intcond_reload = 0;
      worker_tasks[i].timeout = 5; /* some close future */
   }

   /* wake up tasks to allow them to use timeout in next waitqueue_prepare() call */
   test_verbose_debug("Main signalizing slaves for timeout reload");
   os_waitqueue_wakeup(&global_wait_queue, OS_WAITQUEUE_ALL);

   /* need to sleep to make slaves run (main is most prioritized)
    * sleeping for 2 ticks should be enough to synchronize with slaves */
   retcode = os_sem_down(&global_sem, 2);
   test_assert(OS_TIMEOUT == retcode);

   /* destroy the wait queue */
   test_verbose_debug("Main destroys the wait queue");
   os_waitqueue_destroy(&global_wait_queue);

   /* join the slave tasks */
   test_verbose_debug("Main joining slaves");
   ret = 0;
   for(i = 0; i < TEST_TASKS; i++) {
      ret = os_task_join(&(worker_tasks[i].task));
      test_assert(0 == ret);
   }

   /* verify that slave task did not take next spin after waitqueue was
    * destroyed and that retcode was OS_DEStROYED (not OS_TIMEOUTED) */
   for(i = 0; i < TEST_TASKS; i++) {
      test_assert(OS_DESTROYED == worker_tasks[i].retcode);
      test_assert(1 == worker_tasks[i].spin_spincnt);
   }

   return ret;
}

/* \TODO write bit banging on two threads and waitqueue as test5
 * this will be the stress proff of concept */

#if 0
#warning There are some regresion test to be made for following cases (probably already fixed in the code)
#endif
/* following bugs was probably fixed in some of last commit, but we need to
 * revoke fixes, make the regresion test and then apply fixes again to verify
 * that this work
 *
 * \TODO \FIXME second bug!!!
 * again calling waitqueue_wakeup when task_current is spinning, so the same
 * special condition will triger and totaly piss off nbr parameter, so if we
 * would like to wake up many task from ISR, but in the same  time task_current
 * was about to block on waitqueue_wait then we will unblock just this one ano
 * no more of others
 *
 * KISS keep it simple stupid!!!
 *
 * \TODO \FIXME another bug
 * waitqueue timer has missing check for os_task_makeready if task state is
 * already RUNNING. From fisr sight this may cause some problems since we
 * touching ready_queue in that case while this is forbiden for such task (it is
 * already scheduled)!!!
 */

/**
 * The main task for tests manage
 */
int mastertask_proc(void* OS_UNUSED(param))
{
   int ret, retv;
   unsigned i;

   /* clear out memory */
   memset(worker_tasks, 0, sizeof(worker_tasks));

   /* initialize variables */
   os_waitqueue_create(&global_wait_queue);
   os_sem_create(&global_sem, 0);
   for(i = 0; i < TEST_TASKS; i++) {
      worker_tasks[i].idx = i + 1;
   }

   /* create tasks and perform tests */
   for(i = 0; i < TEST_TASKS; i++) {
      worker_tasks[i].idx = i + 1;
      os_task_create(
         &(worker_tasks[i].task), 9 == i ? 2 : 1, /* task 10 will have priority 2
                                                     all other will have priority 1 */
         worker_tasks[i].task1_stack, sizeof(worker_tasks[i].task1_stack),
         slavetask_proc, &(worker_tasks[i]));
   }

   /* we threat this (main) task as IRQ and HW
    * so we will perform all actions here in busy loop to simulate the
    * uncontrolled environment. Since this task has highest priority even
    * preemption is not able to force this task to sleep */

   test_case_t test_cases[] = {
      testcase_1,
      testcase_2,
      testcase_3,
      testcase_4regresion,
      testcase_6,
      testcase_7,
   };

   retv = 0;
   for (i = 0; i < (sizeof(test_cases) / sizeof(test_cases[0])); i++)
   {
      ret = test_cases[i]();
      test_debug_printf("test case %u: %s\n", i + 1, ret ? "FAILED" : "PASSED");
      retv |= ret;
   }

   test_result(retv);
   return 0;
}

void test_tick(void)
{
   test_verbose_debug("Tick %u", global_tick_cnt);

   global_tick_cnt++;

   if (irq_trigger_waitqueue && (irq_trigger_tick == global_tick_cnt))
   {
      /* passing 2 as nbr will wake up main and helper task, but not the sleeper */
      os_waitqueue_wakeup(irq_trigger_waitqueue, 2);
   }
}

void init(void)
{
   test_setuptick(test_tick, 50000000);

   os_task_create(
      &task_main, OS_CONFIG_PRIOCNT - 1,
      task_main_stack, sizeof(task_main_stack),
      mastertask_proc, NULL);
}

int main(void)
{
   test_setupmain("Test_Waitqueue");
   os_start(init, idle);
   return 0;
}

/** /} */

