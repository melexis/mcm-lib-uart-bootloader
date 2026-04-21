/**
 * @file
 * @brief UART bootloader module.
 * @internal
 *
 * @copyright (C) 2023-2026 Melexis N.V.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @endinternal
 *
 * @ingroup lib_uart_bootloader
 *
 * @details Implementations of the UART bootloader module.
 *
 * @attention FOR DEMO PURPOSES ONLY!!
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "sdkconfig.h"

#include "esp_err.h"
#include "esp_log.h"
#include "driver/uart.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "uart_bootloader_err.h"
#include "uart_bootloader_ext.h"

#include "uart_bootloader_transport_layer.h"

static const char *TAG = "uart-btl-tl";

static TaskHandle_t btlTaskHandle;
static SemaphoreHandle_t taskGotoSuspend;

static QueueHandle_t btlQueue;

static const uint32_t UartBtlDefaultBaud = 9600u;

static bool UartBtlHalfDuplex = true;

/** sync byte value */
#define BTL_SYNC_BYTE 0x55u

/** Bootloader uart receive task
 *
 * @param[in]  arg  arguments for the task
 */
static void uartbtltl_task(void *arg);

/** Calculate the checksum for a bootloader message
 *
 * @param[in]  data  message data to calculate checksum for.
 * @param[in]  data_len  length of data to calculate.
 * @returns  calculated checksum value.
 */
static uint8_t uartbtltl_btlCalcChecksum(const uint16_t *data, size_t data_len);

/** Receive task handler for the UART bootloader transport layer
 *
 * @param[in]  buffer  UART received data buffer.
 * @param[in]  wr_ptr  current pointer at which the buffer is being written.
 * @returns  new wr_ptr value after execution of this method.
 */
static int uartbtltl_receiveTask(uint8_t * buffer, int wr_ptr);


static void uartbtltl_task(void *arg) {
    (void)arg;
    static int wr_ptr = 0;
    uint8_t *buffer = (uint8_t*) malloc(256);

    while (1) {
        wr_ptr = uartbtltl_receiveTask(buffer, wr_ptr);

        if (xSemaphoreTake(taskGotoSuspend, 0) == pdTRUE) {
            vTaskSuspend(NULL);
            uart_flush_input(CONFIG_UART_BOOTLOADER_UART_PORT_NUM);
            wr_ptr = 0;
            xSemaphoreGive(taskGotoSuspend);
        }
    }

    free(buffer);
}

static uint8_t uartbtltl_btlCalcChecksum(const uint16_t *data, size_t data_len) {
    uint32_t checksum = 0u;

    for (size_t cnt = 0; cnt < data_len; cnt++) {
        checksum += data[cnt];
        if (checksum > 0xFFFFu) {
            checksum += 1u;
            checksum &= 0xFFFFu;
        }
    }

    checksum = ((checksum >> 8) & 0xFFu) + (checksum & 0xFFu);
    if (checksum > 0xFFu) {
        checksum += 1u;
    }

    return ((uint8_t)checksum) ^ 0xFFu;
}

static int uartbtltl_receiveTask(uint8_t * buffer, int wr_ptr) {
    static bool btl_sync_det = false;

    if ((wr_ptr > 0) && (!btl_sync_det)) {
        /* some data received, search for sync byte */
        int sync_pos = 0;
        for (; sync_pos < wr_ptr; sync_pos++) {
            if (buffer[sync_pos] == BTL_SYNC_BYTE) {
                break;
            }
        }
        if (sync_pos < wr_ptr) {
            /* potential start of frame found */
            btl_sync_det = true;
            memcpy(&buffer[0], &buffer[sync_pos], wr_ptr - sync_pos);
            wr_ptr -= sync_pos;
        } else {
            /* drop all received crap */
            wr_ptr = 0;
        }
    }

    int bytes_to_receive = (BTL_HEADER_SIZE * sizeof(uint16_t)) - wr_ptr;
    if (wr_ptr >= BTL_HEADER_SIZE) {
        const int payload_len = ((btlMessage_t*)&buffer[0])->payload_length;
        if (payload_len <= sizeof(((btlMessage_t*)0)->payload)) {
            /* length seems feasible, continue */
            bytes_to_receive = ((BTL_HEADER_SIZE + payload_len) * sizeof(uint16_t)) - wr_ptr;
        } else {
            /* this is not gonna work, drop invalid sync */
            memcpy(&buffer[0], &buffer[1], wr_ptr - 1);
            wr_ptr -= 1;
        }
    }

    if (bytes_to_receive <= 0) {
        /* complete message received */
        const btlMessage_t* rxMessage = (btlMessage_t*)&buffer[0];
        const int msg_len = rxMessage->payload_length + BTL_HEADER_SIZE;

        ESP_LOG_BUFFER_HEX_LEVEL(TAG, rxMessage->raw, msg_len * sizeof(uint16_t), ESP_LOG_DEBUG);

        if (uartbtltl_btlCalcChecksum(rxMessage->rawU16, msg_len) != 0x00u) {
            /* checksum incorrect, drop invalid sync byte */
            memcpy(&buffer[0], &buffer[1], wr_ptr - 1);
            wr_ptr -= 1;
        } else {
            /* checksum is correct */
            if (xQueueSend(btlQueue, (void*)rxMessage, (TickType_t)0) != pdTRUE) {
                ESP_LOGE(TAG, "Message could not be enqueued");
            }

            /* drop message from buffer */
            if (wr_ptr > msg_len * sizeof(uint16_t)) {
                memcpy(&buffer[0], &buffer[msg_len * sizeof(uint16_t)], wr_ptr - (msg_len * sizeof(uint16_t)));
                wr_ptr -= msg_len * sizeof(uint16_t);
            } else {
                wr_ptr = 0;
            }
        }

        btl_sync_det = false;
    } else {
        wr_ptr += uart_read_bytes(CONFIG_UART_BOOTLOADER_UART_PORT_NUM,
                                  &buffer[wr_ptr],
                                  bytes_to_receive,
                                  50 / portTICK_PERIOD_MS);
    }

    return wr_ptr;
}

esp_err_t uartbtltl_init(void) {
    esp_err_t retval = ESP_FAIL;
    btlQueue = xQueueCreate(5, sizeof(btlMessage_t));
    if (btlQueue != NULL) {
        retval = ESP_OK;
        taskGotoSuspend = xSemaphoreCreateBinary();
        xTaskCreate(uartbtltl_task, "uartbtltl_task", 1536 * 2, NULL, configMAX_PRIORITIES - 1, &btlTaskHandle);
        xSemaphoreGive(taskGotoSuspend);
    }
    return retval;
}

esp_err_t uartbtltl_enable(void) {
    /* flush buffers */
    xQueueReset(btlQueue);

    /* resume task */
    vTaskResume(btlTaskHandle);
    xSemaphoreTake(taskGotoSuspend, 100);

    return uartbtltl_setConfiguration(true, UartBtlDefaultBaud);
}

esp_err_t uartbtltl_disable(void) {
    xSemaphoreGive(taskGotoSuspend);
    return ESP_OK;
}

esp_err_t uartbtltl_setConfiguration(bool half_duplex, uint32_t baudrate) {
    UartBtlHalfDuplex = half_duplex;
    uart_config_t uart_config = {
        .baud_rate = baudrate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1_5,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    return uartbtl_phyEnable(&uart_config, UartBtlHalfDuplex);
}

uartbtl_err_t uartbtltl_sendEnterBootloaderPattern(uint32_t pattern_time) {
    const uint8_t pattern[4] = {'L', 'X', '0', ' '};
    size_t bytes_in_time = UartBtlDefaultBaud / 1000.0 / 10.5 * pattern_time;
    if (bytes_in_time < 3 * sizeof(pattern)) {
        bytes_in_time = 3 * sizeof(pattern);
    }
    uint8_t *buffer = (uint8_t*) malloc(bytes_in_time);
    for (size_t i = 0; i < bytes_in_time; i = i + sizeof(pattern)) {
        size_t tocopy = sizeof(pattern);
        if (i + tocopy > bytes_in_time) {
            tocopy = bytes_in_time - i;
        }
        memcpy(&buffer[i], &pattern[0], tocopy);
    }
    ESP_ERROR_CHECK(uartbtl_phySetHalfDuplexTx());
    uart_set_baudrate(CONFIG_UART_BOOTLOADER_UART_PORT_NUM, UartBtlDefaultBaud);
    uart_write_bytes(CONFIG_UART_BOOTLOADER_UART_PORT_NUM, (const void *)buffer, sizeof(pattern));
    uartbtl_chipPower(true);
    uart_write_bytes_with_break(CONFIG_UART_BOOTLOADER_UART_PORT_NUM, (const void *)buffer, bytes_in_time, 12u);
    uart_wait_tx_done(CONFIG_UART_BOOTLOADER_UART_PORT_NUM, (pattern_time + 20u) / portTICK_PERIOD_MS);
    ESP_ERROR_CHECK(uartbtl_phySetHalfDuplexRx());
    free(buffer);
    return UART_BTL_OK;
}

uartbtl_err_t uartbtltl_sendMessage(btlCommands_t command,
                                    const uint8_t *payload,
                                    size_t payload_len) {
    uartbtl_err_t retval = UART_BTL_FAIL_UNKNOWN;

    if ((payload_len == 0) || (payload != NULL)) {
        btlMessage_t reqMessage;

        memset(reqMessage.raw, 0, sizeof(btlMessage_t));
        reqMessage.sync = BTL_SYNC_BYTE;
        reqMessage.command = (uint8_t)command;
        reqMessage.payload_length = payload_len / sizeof(uint16_t);
        memcpy(reqMessage.payload, payload, payload_len);
        reqMessage.checksum = uartbtltl_btlCalcChecksum(reqMessage.rawU16,
                                                        reqMessage.payload_length + BTL_HEADER_SIZE);

        ESP_LOG_BUFFER_HEX_LEVEL(TAG,
                                 reqMessage.raw,
                                 (reqMessage.payload_length + BTL_HEADER_SIZE) * sizeof(uint16_t),
                                 ESP_LOG_DEBUG);
        if (UartBtlHalfDuplex) {
            ESP_ERROR_CHECK(uartbtl_phySetHalfDuplexTx());
        }
        uart_write_bytes_with_break(CONFIG_UART_BOOTLOADER_UART_PORT_NUM,
                                    (const void *)reqMessage.raw,
                                    (reqMessage.payload_length + BTL_HEADER_SIZE) * sizeof(uint16_t),
                                    12u);
        uart_wait_tx_done(CONFIG_UART_BOOTLOADER_UART_PORT_NUM, pdMS_TO_TICKS(100));
        if (UartBtlHalfDuplex) {
            ESP_ERROR_CHECK(uartbtl_phySetHalfDuplexRx());
        }
        retval = UART_BTL_OK;
    }

    return retval;
}

uartbtl_err_t uartbtltl_receiveMessage(btlCommands_t command,
                                       uint8_t **payload,
                                       uint32_t timeout,
                                       size_t expected_bytes) {
    uartbtl_err_t retval = UART_BTL_FAIL_RX_TIMEOUT;
    btlMessage_t new_msg;
    uint32_t extra_margin = 20u;
    uint32_t rx_timeout = (timeout >= 15 ? timeout : 15) +
                          ((expected_bytes + 5u) * uartbtl_phyGetCurrentBaudrate() / 1000u / 10u) +
                          extra_margin;

    if (xQueueReceive(btlQueue, new_msg.raw, pdMS_TO_TICKS(rx_timeout)) == pdTRUE) {
        if (new_msg.command != CMD_STATUS_REPORT) {
            if (new_msg.command != (uint8_t)command) {
                retval = UART_BTL_FAIL_INVALID_RESP_CMD;
            } else {
                retval = UART_BTL_OK;
                if ((payload != NULL) && (new_msg.payload_length > 0)) {
                    uint8_t * data = calloc(1, new_msg.payload_length * sizeof(uint16_t));
                    if (data == NULL) {
                        retval = UART_BTL_FAIL_INTERNAL;
                    } else {
                        retval = new_msg.payload_length * sizeof(uint16_t);
                        memcpy(data, new_msg.payload, retval);
                        *payload = data;
                    }
                }
            }
        } else {
            /* chip reports an error */
            if (new_msg.payload_length == 1) {
                uint8_t error = new_msg.payload[0] >> 8;
                if (error > CMD_NONE) {
                    ESP_LOGE(TAG, "chip reported error %d", error);
                    retval = -error;
                } else {
                    /* this probably was a status report request */
                    retval = UART_BTL_OK;
                }
            } else {
                retval = UART_BTL_FAIL_INVALID_PL_LEN;
            }
        }
    }

    return retval;
}

esp_err_t __attribute__((weak)) uartbtl_phyEnable(uart_config_t * config, bool half_duplex) {
    (void)config;
    (void)half_duplex;
    return ESP_OK;
}

uint32_t __attribute__((weak)) uartbtl_phyGetCurrentBaudrate(void) {
    return 0u;
}

esp_err_t __attribute__((weak)) uartbtl_phySetHalfDuplexTx(void) {
    return ESP_OK;
}

esp_err_t __attribute__((weak)) uartbtl_phySetHalfDuplexRx(void) {
    return ESP_OK;
}

void __attribute__((weak)) uartbtl_chipPower(bool enable) {
    (void)enable;
}
