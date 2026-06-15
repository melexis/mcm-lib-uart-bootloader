/**
 * @file
 * @brief UART bootloader error code definitions.
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
 * @details Definitions of the UART bootloader error codes.
 *
 * @attention FOR DEMO PURPOSES ONLY!!
 * @{
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** UART bootloader error code enum */
typedef enum uartbtl_err_e {
    UART_BTL_OK = 0,                           /**< operation was successful */
    /* chip reported error codes */
    UART_BTL_FAIL_INVALID_FRAME_ID = -1,       /**< invalid frame identifier */
    UART_BTL_FAIL_CORRUPT_RESPONSE = -2,       /**< corrupted response */
    UART_BTL_FAIL_INVALID_PAGE_NR = -3,        /**< invalid page number */
    UART_BTL_FAIL_INVALID_CRC = -4,            /**< invalid checksum */
    UART_BTL_FAIL_INVALID_KEY = -16,           /**< invalid key received */
    UART_BTL_FAIL_INVALID_SEED = -17,          /**< seed is expired or not generated yet */
    UART_BTL_FAIL_MEM_PROTECTED = -18,         /**< memory is protected */
    UART_BTL_FAIL_FLASH_ERASE = -32,           /**< flash erase failed */
    UART_BTL_FAIL_FLASH_WRITE = -33,           /**< flash write failed */
    UART_BTL_FAIL_FLASH_CRC = -34,             /**< flash crc failed */
    UART_BTL_FAIL_NVRAM_ERASE = -48,           /**< NVRAM erase failed */
    UART_BTL_FAIL_NVRAM_WRITE = -49,           /**< NVRAM write failed */
    UART_BTL_FAIL_NVRAM_CRC = -50,             /**< NVRAM crc failed */
    UART_BTL_FAIL_UNKNOWN = -239,              /**< unknown error */
    /* bootloader app error codes */
    UART_BTL_FAIL_INTERNAL = -256,             /**< internal error */
    UART_BTL_FAIL_UNKNOWN_VERSION = -257,      /**< not supported bootloader protocol version */
    UART_BTL_FAIL_RX_TIMEOUT = -258,           /**< timeout occured during receiving */
    UART_BTL_FAIL_INVALID_RESP_CMD = -512,     /**< chip returned an unexpected command */
    UART_BTL_FAIL_INVALID_RESP_LEN = -513,     /**< chip returned an unexpected pl length */
    UART_BTL_FAIL_INVALID_HEX_FILE = -514,     /**< hex file could not be read */
    UART_BTL_FAIL_MISSING_DATA = -515,         /**< no data for the memory in the hex file */
    UART_BTL_FAIL_DATA_NOT_ALIGNED = -516,     /**< data in hex file is not page aligned */
    UART_BTL_FAIL_VERIFY_FAILED = -517,        /**< verification failed */
    UART_BTL_FAIL_CHIP_NOT_SUPPORTED = -519,   /**< connected chip is not supported */
    UART_BTL_FAIL_HEX_NOT_COMPATIBLE = -520,   /**< hex file is not compatible with connected chip */
    UART_BTL_FAIL_VSUP_LOW = -521,             /**< supply voltage too low */
    UART_BTL_FAIL_CHIP_POWERED = -522,         /**< chip is still powered and can not be reset */
} uartbtl_err_t;                               /**< UART bootloader error code type */

/** Convert a UART bootloader error code in a human readable message
 *
 * @param[in]  code  UART bootloader error code to translate.
 * @returns  A constant string describing the error.
 *           Returns "Unknown error" if the code is unrecognized.
 */
const char *uartbtl_err_to_string(uartbtl_err_t code);

/** @} */

#ifdef __cplusplus
}
#endif
