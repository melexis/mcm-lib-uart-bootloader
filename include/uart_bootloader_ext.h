/**
 * @file
 * @brief UART bootloader external method definitions.
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
 * @details Definitions of the UART bootloader external methods.
 *
 * @attention FOR DEMO PURPOSES ONLY!!
 * @{
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "driver/uart.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Library callout to en/disable the chip power
 *
 * @param[in]  enable  whether to enable the chip power.
 */
void uartbtl_chipPower(bool enable);

/** Library callout to check whether the chip is powered
 *
 * @retval  true  chip is currently powered.
 * @retval  false  otherwise
 */
bool uartbtl_chipPowered(void);

/** Library callout to get the input supply voltage level (voltage before power switch)
 *
 * @retval  input supply voltage level in mV
 */
int32_t uartbtl_getVSupply(void);

/** Library callout to get the chip supply voltage level (voltage after power switch)
 *
 * @retval  chip supply voltage level in mV
 */
int32_t uartbtl_getVChip(void);

/** Library callout to request to enable the UART physical layer
 *
 * @param[in]  config  UART configuration to apply.
 * @param[in]  half_duplex  whether or not the UART shall be configured in half duplex.
 * @returns  error code representing the result of the action.
 */
esp_err_t uartbtl_phyEnable(uart_config_t * config, bool half_duplex);

/** Library callout to get the currently configured UART baudrate
 *
 * @returns  currently configured UART baudrate in bps
 */
uint32_t uartbtl_phyGetCurrentBaudrate(void);

/** Library callout to request the UART physical layer to switch to half duplex TX mode
 *
 * @returns  error code representing the result of the action.
 */
esp_err_t uartbtl_phySetHalfDuplexTx(void);

/** Library callout to request the UART physical layer to switch to half duplex RX mode
 *
 * @returns  error code representing the result of the action.
 */
esp_err_t uartbtl_phySetHalfDuplexRx(void);

/** @} */

#ifdef __cplusplus
}
#endif
