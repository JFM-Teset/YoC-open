/*
 * Copyright (C) 2017-2019 Alibaba Group Holding Limited
 */

/******************************************************************************
 * @file     lib.c
 * @brief    source file for the lib
 * @version  V1.0
 * @date     02. June 2017
 ******************************************************************************/
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include "soc.h"
#include "csi_core.h"     //for test
#include "drv/usart.h"
#include "drv/timer.h"

extern uint32_t csi_coret_get_load(void);
extern uint32_t csi_coret_get_value(void);

extern int32_t csi_usart_putchar(usart_handle_t handle, uint8_t ch);
extern int32_t csi_usart_getchar(usart_handle_t handle, uint8_t *ch);

static void _mdelay(void)
{
    uint32_t load  = csi_coret_get_load();
    uint32_t start = csi_coret_get_value();
    uint32_t cnt   = (drv_get_sys_freq() / 1000);

    while (1) {
        uint32_t cur = csi_coret_get_value();

        if (start > cur) {
            if (start - cur >= cnt) {
                return;
            }
        } else {
            if (load - cur + start > cnt) {
                return;
            }
        }
    }
}

void mdelay(uint32_t ms)
{
    if (ms == 0) {
        return;
    }

    while (ms--) {
        _mdelay();
    }
}

static void _10udelay(void)
{
    uint32_t load  = csi_coret_get_load();
    uint32_t start = csi_coret_get_value();
    uint32_t cnt   = (drv_get_sys_freq() / 1000 / 100);

    while (1) {
        uint32_t cur = csi_coret_get_value();

        if (start > cur) {
            if (start - cur >= cnt) {
                return;
            }
        } else {
            if (load - cur + start > cnt) {
                return;
            }
        }
    }
}

void udelay(uint32_t us)
{
    us = (us / 10);

    if (us == 0) {
        return;
    }

    while (us--) {
        _10udelay();
    }
}
