/**
 * @file
 * @brief UART bootloader definitions.
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
 * @addtogroup lib_uart_bootloader UART Bootloader Library
 *
 * @details Definitions of the UART bootloader module.
 *
 * @attention FOR DEMO PURPOSES ONLY!!
 * @{
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#include "intelhex.h"

#include "uart_bootloader_err.h"
#include "uart_bootloader_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize the UART bootloader module
 *
 * @returns  error code representing the result of the action.
 */
esp_err_t uartbtl_init(void);

/** Enable the UART bootloader
 *
 * @returns  error code representing the result of the action.
 */
esp_err_t uartbtl_enable(void);

/** Disable the UART bootloader
 *
 * @returns  error code representing the result of the action.
 */
esp_err_t uartbtl_disable(void);

/** Request the connected uart chip to enter into programming mode
 *
 * @param[in]  full_duplex   true when bootloading shall be done in full duplex mode.
 * @param[in,out]  btl_baud  baudrate to be used during bootloader operations.
 * @param[in]  pin           chip tx pin to use when in full duplex mode
 * @param[in]  pattern_time  time to send the enter bootloader mode pattern (in ms).
 * @returns  error code representing the result of the action.
 */
uartbtl_err_t uartbtl_enterProgrammingMode(bool full_duplex,
                                           uint32_t *btl_baud,
                                           uint8_t pin,
                                           uint32_t pattern_time);

/** Request to unlock the chip memories
 *
 * @param[in]  flash_keys    flash protection keys for the chip.
 * @returns  error code representing the result of the action.
 */
uartbtl_err_t uartbtl_unlockMemories(uint32_t *flash_keys);

/** Request the connected uart chip to exit out of programming mode
 *
 * @returns  error code representing the result of the action.
 */
uartbtl_err_t uartbtl_exitProgrammingMode(void);

/** Program the Flash memory of a connected uart chip
 *
 * @param[in]  ihex  intel hex container to be programmed.
 * @returns  error code representing the result of the action.
 */
uartbtl_err_t uartbtl_programFlash(ihexContainer_t * ihex);

/** Verify the Flash memory of a connected uart chip
 *
 * @param[in]  ihex  intel hex container to be verified.
 * @returns  error code representing the result of the action.
 */
uartbtl_err_t uartbtl_verifyFlash(ihexContainer_t * ihex);

/** Program the NVRAM memory of a connected uart chip
 *
 * @param[in]  ihex  intel hex container to be programmed.
 * @returns  error code representing the result of the action.
 */
uartbtl_err_t uartbtl_programNvram(ihexContainer_t * ihex);

/** Verify the NVRAM memory of a connected uart chip
 *
 * @param[in]  ihex  intel hex container to be verified.
 * @returns  error code representing the result of the action.
 */
uartbtl_err_t uartbtl_verifyNvram(ihexContainer_t * ihex);

/** Detect which chip is connected and read its project specific info
 *
 * @param[in]  manpow            enable manual power cycling.
 * @param[out]  project_id       project ID of the connected chip.
 * @param[out]  project_info     project specific info reported by the chip.
 * @param[in]  project_info_len  length of the project_info buffer [nr of words].
 * @returns  error code representing the result of the action.
 */
uartbtl_err_t uartbtl_readChipInfo(bool manpow,
                                   uint16_t *project_id,
                                   uint16_t *project_info,
                                   size_t project_info_len);

/** Perform a full programming/verification action to the connected uart chip
 *
 * @param[in]  manpow       enable manual power cycling.
 * @param[in]  bitrate      baudrate to be used during bootloader operations.
 * @param[in]  fullduplex   true when bootloading shall be done in full duplex mode.
 * @param[in]  txpin        chip tx pin to use when in full duplex mode.
 * @param[in]  flash_keys   flash protection keys for the chip.
 * @param[in]  memory       memory type to perform action on.
 * @param[in]  action       action type to perform.
 * @param[in]  ihex         intel hex container to perform action with.
 * @returns  error code representing the result of the action.
 */
uartbtl_err_t uartbtl_doAction(bool manpow,
                               uint32_t bitrate,
                               bool fullduplex,
                               uint8_t txpin,
                               uint32_t flash_keys[4],
                               uartbtl_memory_t memory,
                               uartbtl_action_t action,
                               ihexContainer_t * ihex);

/** @} */

#ifdef __cplusplus
}
#endif
