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
 * THIS SOFTWARE IS PROVIDED BY THE RADOS PROJET AND CONTRIBUTORS "AS IS" AND
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

#ifndef __OS_TEST_
#define __OS_TEST_

#define test_debug(format, ...) \
  test_debug_printf(__FILE__ ":" OS_STR(__LINE__) " " format, ##__VA_ARGS__)

typedef void (*test_tick_clbck_t)(void);


/** Debug function for test verbose output
  *
  * This function is setup and architecture dependend and should pass the
  * verbose debug ouput to console like interface (for instance serial port)
  */
void test_debug_printf(const OS_PROGMEM char* format, ...);

/** Function notifies Human User Interface about test result
  *
  * Since not all architectures use console output, there can be cases where
  * only the test result is important (eor e.g automatic test benches).
  *
  * @param result result of the test, 0 success, != 0 failure
  */
void test_result(int result);

/** Function setups all things needed for paticular architecture to run
  *
  * Details:
  * Function setups the signal disposition tick signal, but do not
  * create the tick timer. This allows tests to handle the manual tick requests
  * (if needed) but also can be used for periodic tick setup (which is more
  * usual case). For periodick tick setup test should call test_setuptick */
void test_setupmain(const OS_PROGMEM char* test_name);

/** Function starts the periodic timer
  *
  * @precondition test_setupmain was called beforehand
  */
void test_setuptick(test_tick_clbck_t clbck, unsigned long nsec);

/** Function forces manual tick
  *
  * This function is used for fine checking the OS rutines which base on tick
  *
  * @precondition test_setupmain was called beforehand
  */
void test_reqtick(void);

#include "arch_test.h"

#endif /* __OS_TEST_ */
