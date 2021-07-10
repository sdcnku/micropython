/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2018-2020 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "py/runtime.h"
#include "py/stream.h"
#include "py/mphal.h"
#include "extmod/mpbthci.h"
#include "extmod/modbluetooth.h"
#include "modmachine.h"
#include "mpbthciport.h"
#include "pendsv.h"
#include "pico/stdlib.h"

#if MICROPY_PY_BLUETOOTH

#define DEBUG_printf(...) // mp_printf(&mp_plat_print, "mpbthciport.c: " __VA_ARGS__)

uint8_t mp_bluetooth_hci_cmd_buf[4 + 256];

#if MICROPY_PY_BLUETOOTH_USE_SYNC_EVENTS
// Prevent double-enqueuing of the scheduled task.
STATIC volatile bool events_task_is_scheduled;
#endif

static int64_t mp_bluetooth_hci_timer_callback(alarm_id_t id, void *user_data) {
    mp_bluetooth_hci_poll_now();
    return 0;
}

void mp_bluetooth_hci_init(void) {
    #if MICROPY_PY_BLUETOOTH_USE_SYNC_EVENTS
    events_task_is_scheduled = false;
    #endif
}

STATIC void mp_bluetooth_hci_start_polling(void) {
    #if MICROPY_PY_BLUETOOTH_USE_SYNC_EVENTS
    events_task_is_scheduled = false;
    #endif
    mp_bluetooth_hci_poll_now();
}

void mp_bluetooth_hci_poll_in_ms(uint32_t ms) {
    add_alarm_in_ms(ms, mp_bluetooth_hci_timer_callback, NULL, true);
}

#if MICROPY_PY_BLUETOOTH_USE_SYNC_EVENTS

// For synchronous mode, we run all BLE stack code inside a scheduled task.
// This task is scheduled periodically via a soft timer, or
// immediately on HCI UART RXIDLE.

STATIC mp_obj_t run_events_scheduled_task(mp_obj_t none_in) {
    (void)none_in;
    events_task_is_scheduled = false;
    // This will process all buffered HCI UART data, and run any callouts or events.
    mp_bluetooth_hci_poll();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(run_events_scheduled_task_obj, run_events_scheduled_task);

// Called periodically (systick) or directly (e.g. UART RX IRQ) in order to
// request that processing happens ASAP in the scheduler.
void mp_bluetooth_hci_poll_now(void) {
    if (!events_task_is_scheduled) {
        events_task_is_scheduled = mp_sched_schedule(MP_OBJ_FROM_PTR(&run_events_scheduled_task_obj), mp_const_none);
        if (!events_task_is_scheduled) {
            // The schedule queue is full, set callback to try again soon.
            mp_bluetooth_hci_poll_in_ms(5);
        }
    }
}

#else // !MICROPY_PY_BLUETOOTH_USE_SYNC_EVENTS

void mp_bluetooth_hci_poll_now(void) {
    pendsv_schedule_dispatch(PENDSV_DISPATCH_BLUETOOTH_HCI, mp_bluetooth_hci_poll);
}

#endif

mp_obj_t mp_bluetooth_uart;

int mp_bluetooth_hci_uart_init(uint32_t port, uint32_t baudrate) {
    DEBUG_printf("mp_bluetooth_hci_uart_init\n");

    mp_obj_t args[] = {
        MP_OBJ_NEW_SMALL_INT(MICROPY_HW_BLE_UART_ID),
        MP_OBJ_NEW_SMALL_INT(MICROPY_HW_BLE_UART_BAUDRATE),
        MP_OBJ_NEW_QSTR(MP_QSTR_flow), MP_OBJ_NEW_SMALL_INT(true),
    };

    mp_bluetooth_uart = machine_uart_type.make_new((mp_obj_t)&machine_uart_type, 2, 1, args);
    MP_STATE_PORT(mp_bluetooth_uart) = mp_bluetooth_uart;

    // Start the HCI polling to process any initial events/packets.
    mp_bluetooth_hci_start_polling();
    return 0;
}

int mp_bluetooth_hci_uart_deinit(void) {
    DEBUG_printf("mp_bluetooth_hci_uart_deinit\n");
    return 0;
}

int mp_bluetooth_hci_uart_set_baudrate(uint32_t baudrate) {
    DEBUG_printf("mp_bluetooth_hci_uart_set_baudrate(%lu)\n", baudrate);
    return 0;
}

int mp_bluetooth_hci_uart_write(const uint8_t *buf, size_t len) {
    DEBUG_printf("mp_bluetooth_hci_uart_write\n");

    mp_bluetooth_hci_controller_wakeup();
    int errcode;
    const mp_stream_p_t *proto = (mp_stream_p_t *) machine_uart_type.protocol;
    proto->write(&mp_bluetooth_uart, (void *)buf, len, &errcode);
    if (errcode != 0) {
        mp_printf(&mp_plat_print, "\nmp_bluetooth_hci_uart_write: failed to write to UART %d\n", errcode);
    }
    return 0;
}

// This function expects the controller to be in the wake state via a previous call
// to mp_bluetooth_hci_controller_woken.
int mp_bluetooth_hci_uart_readchar(void) {
    DEBUG_printf("mp_bluetooth_hci_uart_readchar\n");

    int errcode = 0;
    const mp_stream_p_t *proto = (mp_stream_p_t *) machine_uart_type.protocol;
    mp_uint_t ret = proto->ioctl(&mp_bluetooth_uart, MP_STREAM_POLL, MP_STREAM_POLL_RD, &errcode);
    if (errcode != 0) {
        mp_printf(&mp_plat_print, "\nmp_bluetooth_hci_uart_readchar: failed to poll UART %d\n", errcode);
    } else if (ret & MP_STREAM_POLL_RD) {
        uint8_t buf;
        errcode = 0;
        ret = proto->read(&mp_bluetooth_uart, (void *) &buf, 1, &errcode);
        if (errcode != 0) {
            mp_printf(&mp_plat_print, "\nmp_bluetooth_hci_uart_readchar: failed to read UART %d\n", errcode);
        } else { 
            return buf;
        }
    }
    return -1;
}

// Default (weak) implementation of the HCI controller interface.
// A driver (e.g. cywbt43.c) can override these for controller-specific
// functionality (i.e. power management).

MP_WEAK int mp_bluetooth_hci_controller_init(void) {
    DEBUG_printf("mp_bluetooth_hci_controller_init (default)\n");
    return 0;
}

MP_WEAK int mp_bluetooth_hci_controller_deinit(void) {
    DEBUG_printf("mp_bluetooth_hci_controller_deinit (default)\n");
    return 0;
}

MP_WEAK int mp_bluetooth_hci_controller_sleep_maybe(void) {
    DEBUG_printf("mp_bluetooth_hci_controller_sleep_maybe (default)\n");
    return 0;
}

MP_WEAK bool mp_bluetooth_hci_controller_woken(void) {
    DEBUG_printf("mp_bluetooth_hci_controller_woken (default)\n");
    return true;
}

MP_WEAK int mp_bluetooth_hci_controller_wakeup(void) {
    DEBUG_printf("mp_bluetooth_hci_controller_wakeup (default)\n");
    return 0;
}

#endif // MICROPY_PY_BLUETOOTH
