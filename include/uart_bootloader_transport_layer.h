/**
 * @file
 * @brief UART bootloader transport layer definitions.
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
 * @details Definitions of the UART bootloader transport layer module.
 *
 * @attention FOR DEMO PURPOSES ONLY!!
 * @{
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#include "uart_bootloader_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** UART bootloader commands enum */
typedef enum btlCommands_e {
    CMD_NONE = 0,                                   /**< Nothing todo */
    CMD_PROJECT_INFO = 1,                           /**< Project information request */
    CMD_CONFIG_PROT = 2,                            /**< Protocol configuration */
    CMD_REQUEST_SEED = 4,                           /**< Seed request */
    CMD_UNLOCK_PROT_MEM = 5,                        /**< Unlock protected memories */
    CMD_PROGR_KEYS = 6,                             /**< Programming keys */
    CMD_FLASH_WRITE = 8,                            /**< Flash write */
    CMD_FLASH_BIST = 9,                             /**< Flash verification */
    CMD_NVRAM_WRITE = 16,                           /**< NVRAM write */
    CMD_NVRAM_VERIFY = 17,                          /**< NVRAM verification */
    CMD_RESET = 64,                                 /**< Chip reset */
    CMD_STATUS_REPORT = 127,                        /**< Status report */
} btlCommands_t;                                    /**< Bootloader commands type */

/** Size of the frame header in number of words */
#define BTL_HEADER_SIZE 2

/** Maximum frame payload size in number of words */
#define BTL_PAYLOAD_SIZE 65

/** Bootloader message structure */
typedef union btlMessage_s {

    uint16_t rawU16[BTL_HEADER_SIZE + BTL_PAYLOAD_SIZE];  /**< raw message words */
    uint8_t raw[(BTL_HEADER_SIZE + BTL_PAYLOAD_SIZE) * sizeof(uint16_t)];  /**< raw message bytes */
    struct __attribute__((packed)) {
        uint8_t sync;                               /**< synchronization byte */
        uint8_t checksum;                           /**< checksum byte */
        uint8_t command;                            /**< command byte */
        uint8_t payload_length;                     /**< payload length */
        uint16_t payload[BTL_PAYLOAD_SIZE];         /**< payload */
    };
} btlMessage_t;                                     /**< Bootloader message type */

/** Initialize the UART bootloader transport layer module
 *
 * @returns  error code representing the result of the operation.
 */
esp_err_t uartbtltl_init(void);

/** Enable the UART transport layer
 *
 * all buffers are flushed, uart is configured in default bootloader mode (9.6kbps, half duplex)
 *
 * @returns  error code representing the result of the action.
 */
esp_err_t uartbtltl_enable(void);

/** Disable the UART transport layer
 *
 * @returns  error code representing the result of the action.
 */
esp_err_t uartbtltl_disable(void);

/** Set the baudrate and mode
 *
 * @param[in]  half_duplex  enable half duplex communication.
 * @param[in]  baudrate  new baudrate to be configured.
 * @returns  error code representing the result of the action.
 */
esp_err_t uartbtltl_setConfiguration(bool half_duplex, uint32_t baudrate);

/** Send the enter bootloader mode pattern on the UART bus
 *
 * @param[in]  pattern_time  time to send the pattern (in ms).
 * @returns  error code representing the result of the action.
 */
uartbtl_err_t uartbtltl_sendEnterBootloaderPattern(uint32_t pattern_time);

/** Send a bootloader message
 *
 * @param[in]  command      bootloader command identifier to sent.
 * @param[in]  payload      payload to be sent to the chip.
 * @param[in]  payload_len  length of the payload.
 * @returns  error code representing the result of the action.
 */
uartbtl_err_t uartbtltl_sendMessage(btlCommands_t command,
                                    const uint8_t *payload,
                                    size_t payload_len);

/** Wait for and read an incomming message
 *
 * @param[in]  command   bootloader command identifier to expect.
 * @param[out]  payload  payload received in the message.
 * @param[in]  timeout   timeout to wait for incomming message [us].
 * @param[in]  expected_bytes  expected number of bytes in incomming message.
 * @returns  error code representing the result of the action.
 */
uartbtl_err_t uartbtltl_receiveMessage(btlCommands_t command,
                                       uint8_t **payload,
                                       uint32_t timeout,
                                       size_t expected_bytes);

/** @} */

#ifdef __cplusplus
}
#endif
