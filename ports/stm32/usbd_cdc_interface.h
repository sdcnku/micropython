/*
 * This file is part of the MicroPython project, http://micropython.org/
 */
#ifndef MICROPY_INCLUDED_STM32_USBD_CDC_INTERFACE_H
#define MICROPY_INCLUDED_STM32_USBD_CDC_INTERFACE_H

/**
  ******************************************************************************
  * @file    USB_Device/CDC_Standalone/Inc/usbd_cdc_interface.h
  * @author  MCD Application Team
  * @version V1.0.1
  * @date    26-February-2014
  * @brief   Header for usbd_cdc_interface.c file.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT(c) 2014 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software
  * distributed under the License is distributed on an "AS IS" BASIS,
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */

#define USBD_CDC_RX_DATA_SIZE (512) // this must be 2 or greater, and a power of 2
#define USBD_CDC_TX_DATA_SIZE (512) // I think this can be any value (was 2048)

#define DBG_MAX_PACKET      (64)

typedef struct _usbd_cdc_itf_t {
    usbd_cdc_msc_hid_state_t *usbd; // the parent USB device

    uint8_t rx_packet_buf[CDC_DATA_MAX_PACKET_SIZE]; // received data from USB OUT endpoint is stored in this buffer
    uint8_t rx_user_buf[USBD_CDC_RX_DATA_SIZE]; // received data is buffered here until the user reads it
    volatile uint16_t rx_buf_put; // circular buffer index
    uint16_t rx_buf_get; // circular buffer index

    uint8_t tx_buf[USBD_CDC_TX_DATA_SIZE]; // data for USB IN endpoind is stored in this buffer
    uint16_t tx_buf_ptr_in; // increment this pointer modulo USBD_CDC_TX_DATA_SIZE when new data is available
    volatile uint16_t tx_buf_ptr_out; // increment this pointer modulo USBD_CDC_TX_DATA_SIZE when data is drained
    uint16_t tx_buf_ptr_out_shadow; // shadow of above
    uint8_t tx_buf_ptr_wait_count; // used to implement a timeout waiting for low-level USB driver
    uint8_t tx_need_empty_packet; // used to flush the USB IN endpoint if the last packet was exactly the endpoint packet size

    uint32_t baudrate;
    volatile uint8_t dev_is_connected; // indicates if we are connected
    volatile uint8_t dbg_mode_enabled;
    volatile uint32_t dbg_last_packet;
    volatile uint32_t dbg_xfer_length;
    uint8_t dbg_xfer_buffer[DBG_MAX_PACKET];
} usbd_cdc_itf_t;

static inline int usbd_cdc_is_connected(usbd_cdc_itf_t *cdc) {
    return cdc->dev_is_connected;
}

static inline int usbd_cdc_debug_mode_enabled(usbd_cdc_itf_t *cdc) {
    return cdc->dbg_mode_enabled;
}

int usbd_cdc_tx_half_empty(usbd_cdc_itf_t *cdc);
int usbd_cdc_tx(usbd_cdc_itf_t *cdc, const uint8_t *buf, uint32_t len, uint32_t timeout);
void usbd_cdc_tx_always(usbd_cdc_itf_t *cdc, const uint8_t *buf, uint32_t len);

int usbd_cdc_rx_num(usbd_cdc_itf_t *cdc);
int usbd_cdc_rx(usbd_cdc_itf_t *cdc, uint8_t *buf, uint32_t len, uint32_t timeout);

uint32_t usbd_cdc_tx_buf_len(usbd_cdc_itf_t *cdc);
uint8_t *usbd_cdc_tx_buf(usbd_cdc_itf_t *cdc, uint32_t bytes);

#endif // MICROPY_INCLUDED_STM32_USBD_CDC_INTERFACE_H
