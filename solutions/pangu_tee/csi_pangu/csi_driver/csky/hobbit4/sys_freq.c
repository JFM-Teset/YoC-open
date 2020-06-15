/*
 * Copyright (C) 2017 C-SKY Microsystems Co., Ltd. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/******************************************************************************
 * @file     sys_freq.c
 * @brief    source file for setting system frequency.
 * @version  V1.0
 * @date     18. July 2018
 ******************************************************************************/

#include <stdint.h>
#include <soc.h>
#include <sys_freq.h>
#include "yun_pmu.h"

int g_system_clock = IHS_VALUE;

int32_t drv_get_sys_freq(void)
{
    return g_system_clock;
}

int32_t drv_get_timer_freq(int32_t idx)
{
    if (idx < 2) {
        return (int32_t)yun_get_apb0_freq();
    } else if (idx < 4) {
        return (32768);
    } else {
        // return (int32_t)yun_get_apb1_freq();
    }
    return 0;
}

int32_t drv_get_usart_freq(int32_t idx)
{
    if (idx < 2) {
        return (int32_t)yun_get_apb0_freq();
    } else {
        // return (int32_t)yun_get_apb1_freq();
    }
    return 0;
}