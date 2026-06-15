/**
 * @file
 * @brief UART bootloader error code module.
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
 * @details Implementations of the UART bootloader error code module.
 *
 * @attention FOR DEMO PURPOSES ONLY!!
 */
#include <stddef.h>

#include "uart_bootloader_err.h"

/** getter for the size of an array in number of items of the array type */
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

static const struct {
    uartbtl_err_t code;
    const char *name;
} uartbtl_error_codes_dict[] = {
    {UART_BTL_OK, "operation was successful"},
    /* chip reported error codes */
    {UART_BTL_FAIL_INVALID_FRAME_ID, "invalid frame identifier"},
    {UART_BTL_FAIL_CORRUPT_RESPONSE, "corrupted response"},
    {UART_BTL_FAIL_INVALID_PAGE_NR, "invalid page number"},
    {UART_BTL_FAIL_INVALID_CRC, "invalid checksum"},
    {UART_BTL_FAIL_INVALID_KEY, "invalid key received"},
    {UART_BTL_FAIL_INVALID_SEED, "seed is expired or not generated yet"},
    {UART_BTL_FAIL_MEM_PROTECTED, "memory is protected"},
    {UART_BTL_FAIL_FLASH_ERASE, "flash erase failed"},
    {UART_BTL_FAIL_FLASH_WRITE, "flash write failed"},
    {UART_BTL_FAIL_FLASH_CRC, "flash crc failed"},
    {UART_BTL_FAIL_NVRAM_ERASE, "NVRAM erase failed"},
    {UART_BTL_FAIL_NVRAM_WRITE, " NVRAM write failed"},
    {UART_BTL_FAIL_NVRAM_CRC, "NVRAM crc failed"},
    {UART_BTL_FAIL_UNKNOWN, "unknown error"},
    /* bootloader app error codes */
    {UART_BTL_FAIL_INTERNAL, "internal error"},
    {UART_BTL_FAIL_UNKNOWN_VERSION, "not supported bootloader protocol version"},
    {UART_BTL_FAIL_RX_TIMEOUT, "timeout occured during receiving"},
    {UART_BTL_FAIL_INVALID_RESP_CMD, "chip returned an unexpected command"},
    {UART_BTL_FAIL_INVALID_RESP_LEN, "chip returned an unexpected pl length"},
    {UART_BTL_FAIL_INVALID_HEX_FILE, "hex file could not be read"},
    {UART_BTL_FAIL_MISSING_DATA, "no data for the memory in the hex file"},
    {UART_BTL_FAIL_DATA_NOT_ALIGNED, "data in hex file is not page aligned"},
    {UART_BTL_FAIL_VERIFY_FAILED, "verification failed"},
    {UART_BTL_FAIL_CHIP_NOT_SUPPORTED, "connected chip is not supported"},
    {UART_BTL_FAIL_HEX_NOT_COMPATIBLE, "hex file is not compatible with connected chip"},
    {UART_BTL_FAIL_VSUP_LOW, "supply voltage too low"},
    {UART_BTL_FAIL_CHIP_POWERED, "chip is still powered and can not be reset"},
};

const char *uartbtl_err_to_string(uartbtl_err_t code) {
    for (size_t i = 0; i < ARRAY_SIZE(uartbtl_error_codes_dict); ++i) {
        if (uartbtl_error_codes_dict[i].code == code) {
            return uartbtl_error_codes_dict[i].name;
        }
    }

    return "unknown error";
}
