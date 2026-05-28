/**
 * @file
 * @brief UART bootloader types definitions.
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
 * @details Definitions of the UART bootloader types module.
 *
 * @attention FOR DEMO PURPOSES ONLY!!
 * @{
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** UART bootloader memory types enum */
typedef enum uartbtl_memory_e {
    UARTBTL_MEM_NVRAM = 0,              /**< non volatile memory */
    UARTBTL_MEM_FLASH,                  /**< flash memory */
    UARTBTL_MEM_FLASH_CS,               /**< flash configuration memory */
    UARTBTL_MEM_INVALID = 255           /**< invalid memory */
} uartbtl_memory_t;                     /**< UART bootloader memory type */

/** UART bootloader action types enum */
typedef enum uartbtl_action_e {
    UARTBTL_ACT_PROGRAM = 0,            /**< program memory */
    UARTBTL_ACT_VERIFY,                 /**< verify memory */
    UARTBTL_ACT_INVALID = 255           /**< invalid action */
} uartbtl_action_t;                     /**< UART bootloader action type */

/** @} */

#ifdef __cplusplus
}
#endif
