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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "intelhex.h"
#include "mlx_chip.h"
#include "mlx_crc.h"

#include "uart_bootloader_err.h"
#include "uart_bootloader_ext.h"
#include "uart_bootloader_transport_layer.h"
#include "uart_bootloader_types.h"

#include "uart_bootloader.h"

static const char *TAG = "uart-btl";

static uint32_t keyCalculation(uint32_t seed, uint32_t *flash_prot_key) {
    uint32_t hashed = seed;
    uint32_t carry;

    hashed ^= flash_prot_key[0];
    for (uint8_t i = 0u; i < 7u; i++) {
        carry = (hashed >> 31) & 1u;
        hashed = (hashed << 1) + carry;
        if (carry == 1u) {
            hashed -= flash_prot_key[2];
            hashed ^= flash_prot_key[1];
        }
        hashed += flash_prot_key[3];
    }

    return hashed;
}

static uartbtl_err_t uartbtl_cmdProjectId(uint16_t *project_id,
                                          uint8_t *btl_size,
                                          uint8_t *btl_version,
                                          uint16_t *project_info,
                                          size_t project_info_len) {
    uartbtl_err_t retval;

    retval = uartbtltl_sendMessage(CMD_PROJECT_INFO, NULL, 0);

    if (retval == UART_BTL_OK) {
        uint8_t *response = NULL;
        retval = uartbtltl_receiveMessage(CMD_PROJECT_INFO, &response, 10u, 8u);

        if (retval >= 0) {
            if (retval >= 4) {
                size_t nr_words = (retval - 4) / sizeof(uint16_t);
                if (project_id != NULL) {
                    *project_id = *((uint16_t*)&response[0]);
                }
                if (btl_size != NULL) {
                    *btl_size = response[3];
                }
                if (btl_version != NULL) {
                    *btl_version = response[2];
                }
                if ((nr_words > 0) && (project_info != NULL) && (project_info_len > 0)) {
                    if (nr_words > project_info_len) {
                        nr_words = project_info_len;
                    }
                    memcpy(project_info, &response[4], nr_words * sizeof(uint16_t));
                    retval = nr_words;
                } else {
                    retval = UART_BTL_OK;
                }
            } else {
                retval = UART_BTL_FAIL_INVALID_RESP_LEN;
            }
        }
        free(response);
    }

    return retval;
}

static int baudrateToChipBaudId(uint32_t *baudrate) {
    int retval = -1;
    if (*baudrate <= 9600u) {
        *baudrate = 9600u;
        retval = 0;
    } else if (*baudrate <= 19200u) {
        *baudrate = 19200u;
        retval = 1;
    } else if (*baudrate <= 38400u) {
        *baudrate = 38400u;
        retval = 2;
    } else if (*baudrate <= 57600u) {
        *baudrate = 57600u;
        retval = 3;
    } else if (*baudrate <= 115200u) {
        *baudrate = 115200u;
        retval = 4;
    } else if (*baudrate <= 230400u) {
        *baudrate = 230400u;
        retval = 5;
    } else if (*baudrate <= 460800u) {
        *baudrate = 460800u;
        retval = 6;
    }
    return retval;
}

static uartbtl_err_t uartbtl_cmdConfig(bool full_duplex, uint32_t *baudrate, uint8_t pin) {
    uartbtl_err_t retval;
    uint8_t payload[4];
    payload[0] = baudrateToChipBaudId(baudrate);
    payload[1] = (uint8_t)full_duplex;
    payload[2] = pin;
    payload[3] = 0xFFu;

    retval = uartbtltl_sendMessage(CMD_CONFIG_PROT, payload, sizeof(payload));

    if (retval == UART_BTL_OK) {
        retval = uartbtltl_setConfiguration(!full_duplex, *baudrate);
    }

    if (retval == UART_BTL_OK) {
        uint8_t *response = NULL;
        retval = uartbtltl_receiveMessage(CMD_CONFIG_PROT, &response, 10u, 0u);
        if (retval > 0) {
            retval = UART_BTL_FAIL_INVALID_RESP_LEN;
        }
        free(response);
    }

    return retval;
}

static uartbtl_err_t uartbtl_cmdRequestSeed(uint32_t *seed) {
    uartbtl_err_t retval;

    retval = uartbtltl_sendMessage(CMD_REQUEST_SEED, NULL, 0);

    if (retval == UART_BTL_OK) {
        uint8_t *response = NULL;
        retval = uartbtltl_receiveMessage(CMD_REQUEST_SEED, &response, 10u, 4u);
        if (retval >= 0) {
            if (retval != 4) {
                retval = UART_BTL_FAIL_INVALID_RESP_LEN;
            } else if (seed != NULL) {
                *seed = *(uint32_t*)response;
                retval = UART_BTL_OK;
            }
        }
        free(response);
    }

    return retval;
}

static uartbtl_err_t uartbtl_cmdUnlockProtMemories(uint32_t key) {
    uartbtl_err_t retval;

    retval = uartbtltl_sendMessage(CMD_UNLOCK_PROT_MEM, (uint8_t*)&key, 4);

    if (retval == UART_BTL_OK) {
        uint8_t *response = NULL;
        retval = uartbtltl_receiveMessage(CMD_UNLOCK_PROT_MEM, &response, 10u, 0u);
        if (retval > 0) {
            retval = UART_BTL_FAIL_INVALID_RESP_LEN;
        }
        free(response);
    }

    return retval;
}

static uartbtl_err_t uartbtl_cmdProgKeys(const uint16_t *keys, size_t keys_len) {
    uartbtl_err_t retval;

    retval = uartbtltl_sendMessage(CMD_PROGR_KEYS, (const uint8_t*)keys, keys_len * sizeof(uint16_t));

    if (retval == UART_BTL_OK) {
        uint8_t *response = NULL;
        retval = uartbtltl_receiveMessage(CMD_PROGR_KEYS, &response, 10u, 0u);
        if (retval > 0) {
            retval = UART_BTL_FAIL_INVALID_RESP_LEN;
        }
        free(response);
    }

    return retval;
}

static uartbtl_err_t uartbtl_cmdFlashPageWrite(int page_nr,
                                               const uint8_t *data,
                                               size_t data_len,
                                               uint32_t resp_timeout) {
    uartbtl_err_t retval = UART_BTL_FAIL_UNKNOWN;

    if (data != NULL) {
        uint8_t *frame_data = malloc(data_len + 2);
        if (frame_data != NULL) {
            frame_data[0] = (uint8_t)(page_nr & 0xFFu);
            frame_data[1] = (uint8_t)(page_nr >> 8);
            memcpy(&frame_data[2], data, data_len);
            retval = uartbtltl_sendMessage(CMD_FLASH_WRITE, frame_data, data_len + 2);

            if (retval == UART_BTL_OK) {
                uint8_t *response = NULL;
                retval = uartbtltl_receiveMessage(CMD_FLASH_WRITE, &response, resp_timeout, 0u);
                if (retval > 0) {
                    retval = UART_BTL_FAIL_INVALID_RESP_LEN;
                }
                free(response);
            }
        }
        free(frame_data);
    }

    return retval;
}

static uartbtl_err_t uartbtl_cmdFlashBist(uint32_t *bist) {
    uartbtl_err_t retval;

    retval = uartbtltl_sendMessage(CMD_FLASH_BIST, NULL, 0);

    if (retval == UART_BTL_OK) {
        uint8_t *response = NULL;
        retval = uartbtltl_receiveMessage(CMD_FLASH_BIST, &response, 10u, 4u);
        if (retval >= 0) {
            if (retval == 4) {
                *bist = *((uint32_t*)response);
                retval = UART_BTL_OK;
            } else {
                retval = UART_BTL_FAIL_INVALID_RESP_LEN;
            }
        }
        free(response);
    }

    return retval;
}

static uartbtl_err_t uartbtl_cmdWriteNvram(int page_nr, const uint8_t *data, size_t data_len) {
    uartbtl_err_t retval = UART_BTL_FAIL_UNKNOWN;

    if (data != NULL) {
        uint8_t *frame_data = malloc(data_len + 2);
        if (frame_data != NULL) {
            frame_data[0] = page_nr;
            frame_data[1] = 0x00u;
            memcpy(&frame_data[2], data, data_len);
            retval = uartbtltl_sendMessage(CMD_NVRAM_WRITE, frame_data, data_len + 2);

            if (retval == UART_BTL_OK) {
                uint8_t *response = NULL;
                retval = uartbtltl_receiveMessage(CMD_NVRAM_WRITE, &response, 25u, 0u);
                if (retval > 0) {
                    retval = UART_BTL_FAIL_INVALID_RESP_LEN;
                }
                free(response);
            }
        }
        free(frame_data);
    }

    return retval;
}

static uartbtl_err_t uartbtl_cmdVerifyNvram(int start_page_nr, int nr_pages, uint8_t *crc) {
    uartbtl_err_t retval = UART_BTL_FAIL_UNKNOWN;
    uint16_t data[2];
    data[0] = start_page_nr;
    data[1] = nr_pages;
    retval = uartbtltl_sendMessage(CMD_NVRAM_VERIFY, (uint8_t*)data, sizeof(data));

    if (retval == UART_BTL_OK) {
        uint8_t *response = NULL;
        retval = uartbtltl_receiveMessage(CMD_NVRAM_VERIFY, &response, 10u, 2u);
        if (retval >= 0) {
            if (retval == 2) {
                *crc = response[0];
                retval = UART_BTL_OK;
            } else {
                retval = UART_BTL_FAIL_INVALID_RESP_LEN;
            }
        }
        free(response);
    }

    return retval;
}

static uartbtl_err_t uartbtl_cmdRestart(void) {
    uartbtl_err_t retval = uartbtltl_sendMessage(CMD_RESET, NULL, 0);
    (void)uartbtltl_receiveMessage(CMD_RESET, NULL, 15u, 0u);
    return retval;
}

static uartbtl_err_t uartbtl_cmdProgrammingStatus(void) {
    uartbtl_err_t retval = UART_BTL_FAIL_UNKNOWN;
    retval = uartbtltl_sendMessage(CMD_STATUS_REPORT, NULL, 0);

    if (retval == UART_BTL_OK) {
        uint8_t *response = NULL;
        retval = uartbtltl_receiveMessage(CMD_STATUS_REPORT, &response, 15u, 0u);
        if (retval > 0) {
            retval = UART_BTL_FAIL_INVALID_RESP_LEN;
        }
        free(response);
    }

    return retval;
}


esp_err_t uartbtl_init(void) {
    return uartbtltl_init();
}

esp_err_t uartbtl_enable(void) {
    return uartbtltl_enable();
}

esp_err_t uartbtl_disable(void) {
    return uartbtltl_disable();
}

uartbtl_err_t uartbtl_enterProgrammingMode(bool full_duplex,
                                           uint32_t *baudrate,
                                           uint8_t pin,
                                           uint32_t pattern_time) {
    uartbtl_err_t retval = UART_BTL_OK;

    ESP_LOGI(TAG, "ENTER uartbtl_enterProgrammingMode");

    if (retval >= 0) {
        retval = uartbtltl_sendEnterBootloaderPattern(pattern_time);
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    if (retval >= 0) {
        retval = uartbtl_cmdConfig(full_duplex, baudrate, pin);
    }

    if (retval >= 0) {
        ESP_LOGI(TAG, "switched to baudrate %ld", *baudrate);
    }

    return retval;
}

uartbtl_err_t uartbtl_unlockMemories(uint32_t *flash_keys) {
    uartbtl_err_t retval = UART_BTL_OK;

    ESP_LOGI(TAG, "ENTER uartbtl_unlockMemories");

    uint32_t seed;
    if (retval >= 0) {
        retval = uartbtl_cmdRequestSeed(&seed);
    }

    if (retval >= 0) {
        ESP_LOGI(TAG, "chip seed %ld", seed);
        retval = uartbtl_cmdUnlockProtMemories(keyCalculation(seed, flash_keys));
    }

    if (retval >= 0) {
        ESP_LOGI(TAG, "chip memories are unlocked");
    }

    return retval;
}

uartbtl_err_t uartbtl_exitProgrammingMode(void) {
    ESP_LOGI(TAG, "ENTER uartbtl_exitProgrammingMode");
    return uartbtl_cmdRestart();
}

uartbtl_err_t uartbtl_programFlash(ihexContainer_t * ihex) {
    uartbtl_err_t retval;

    if (ihex != NULL) {
        uint16_t project_id;
        uint8_t btl_size = 0xFFu;
        uint8_t btl_version = 0u;

        ESP_LOGI(TAG, "ENTER uartbtl_programFlash");

        retval = uartbtl_cmdProjectId(&project_id, &btl_size, &btl_version, NULL, 0);

        if (btl_version != 0u) {
            retval = UART_BTL_FAIL_UNKNOWN_VERSION;
        }

        const mlx_chip_t * chip = mlxchip_get_camcu_chip(project_id);

        if ((chip == NULL) || (chip->bootloaders.uart_loader == NULL)) {
            retval = UART_BTL_FAIL_CHIP_NOT_SUPPORTED;
        } else if (retval >= 0) {
            ESP_LOGI(TAG, "Programming %s with btl size=%d", chip->name, btl_size);

            const mlx_flash_t * memory = chip->memories.flash;
            uint32_t flashEnd = memory->start + memory->length - 1;
            uint32_t flashStart = memory->start;
            uint32_t flashPageSize = memory->page;
            uint32_t flashSectorSize = memory->sector;

            if ((intelhex_minAddress(ihex) > flashEnd) ||
                (intelhex_maxAddress(ihex) < (flashStart + (btl_size * flashSectorSize)))) {
                ESP_LOGE(TAG, "No data for this memory in the hex file");
                retval = UART_BTL_FAIL_MISSING_DATA;
            } else {
                /* compare hex project_id vs chip */
                uint16_t hex_id;
                (void)intelhex_get(ihex, flashStart + (btl_size * flashSectorSize) + 6u, (uint8_t*)&hex_id, 2);
                hex_id ^= 0xA55Au;
                if (hex_id != project_id) {
                    ESP_LOGE(TAG, "Hex file (%i) not compatible with chip (%i)", hex_id, project_id);
                    retval = UART_BTL_FAIL_HEX_NOT_COMPATIBLE;
                } else {
                    if (chip->bootloaders.uart_loader->prog_keys->values != NULL) {
                        const uint16_t * keys = chip->bootloaders.uart_loader->prog_keys->values;
                        retval = uartbtl_cmdProgKeys(keys, chip->bootloaders.uart_loader->prog_keys->length);
                    }

                    uint32_t erase_time = memory->length / memory->erase_unit * memory->erase_time;
                    uint32_t write_time = memory->write_time;
                    int page_nr = 1u;
                    uint32_t curaddr = flashStart + (btl_size * flashSectorSize) + (page_nr * flashPageSize);
                    while ((curaddr <= flashEnd) && (retval >= UART_BTL_OK)) {
                        uint8_t page[flashPageSize];
                        (void)intelhex_getFilled(ihex, curaddr, page, flashPageSize);
                        retval = uartbtl_cmdFlashPageWrite(page_nr,
                                                           page,
                                                           flashPageSize,
                                                           page_nr == 1 ? erase_time : write_time);
                        curaddr += flashPageSize;
                        page_nr++;
                    }

                    if (retval == UART_BTL_OK) {
                        /* write last page which is first in Flash */
                        uint8_t page[flashPageSize];
                        (void)intelhex_getFilled(ihex, flashStart + (btl_size * flashSectorSize), page, flashPageSize);
                        retval = uartbtl_cmdFlashPageWrite(0, page, flashPageSize, write_time);
                    }

                    if (retval == UART_BTL_OK) {
                        retval = uartbtl_cmdProgrammingStatus();
                    }
                }
            }
        }
    } else {
        retval = UART_BTL_FAIL_INVALID_HEX_FILE;
    }

    return retval;
}

uartbtl_err_t uartbtl_verifyFlash(ihexContainer_t * ihex) {
    uartbtl_err_t retval;

    if (ihex != NULL) {
        uint8_t btl_size = 0xFFu;
        uint8_t btl_version = 0u;
        uint16_t project_id;
        uint32_t bist_chip = 0xFFFFFFFFu;
        uint32_t bist_calc = 1u;

        ESP_LOGI(TAG, "ENTER uartbtl_verifyFlash");

        retval = uartbtl_cmdProjectId(&project_id, &btl_size, &btl_version, NULL, 0);

        if (btl_version != 0u) {
            retval = UART_BTL_FAIL_UNKNOWN_VERSION;
        }

        const mlx_chip_t * chip = mlxchip_get_camcu_chip(project_id);
        if ((chip == NULL) || (chip->bootloaders.uart_loader == NULL)) {
            retval = UART_BTL_FAIL_CHIP_NOT_SUPPORTED;
        }

        if (retval >= 0) {
            retval = uartbtl_cmdFlashBist(&bist_chip);
        }

        if (retval >= 0) {
            const mlx_flash_t * memory = chip->memories.flash;
            uint32_t flashEnd = memory->start + memory->length - 1;
            uint32_t flashStart = memory->start;
            uint32_t flashSectorSize = memory->sector;

            if ((intelhex_minAddress(ihex) > flashEnd) ||
                (intelhex_maxAddress(ihex) < (flashStart + (btl_size * flashSectorSize)))) {
                ESP_LOGE(TAG, "No data for this memory in the hex file");
                retval = UART_BTL_FAIL_MISSING_DATA;
            } else {
                /* calculate bist over hex file */
                size_t mem_len = (flashEnd + 1) - (flashStart + (btl_size * flashSectorSize));
                uint8_t *memory = malloc(mem_len);
                if (memory != NULL) {
                    (void)intelhex_getFilled(ihex, flashStart + (btl_size * flashSectorSize), memory, mem_len);
                    bist_calc = crc_calc24bitCrc((uint16_t*)memory, mem_len / 2, 1u);
                }
                free(memory);
            }
        }

        if (retval == UART_BTL_OK) {
            if (bist_chip != bist_calc) {
                ESP_LOGE(TAG, "Flash BIST chip %ld != hex %ld", bist_chip, bist_calc);
                retval = UART_BTL_FAIL_VERIFY_FAILED;
            }
        }
    } else {
        retval = UART_BTL_FAIL_INVALID_HEX_FILE;
    }

    return retval;
}

uartbtl_err_t uartbtl_programNvram(ihexContainer_t * ihex) {
    uartbtl_err_t retval;

    if (ihex != NULL) {
        uint16_t project_id;
        uint8_t btl_version = 0u;

        ESP_LOGI(TAG, "ENTER uartbtl_programNvram");

        retval = uartbtl_cmdProjectId(&project_id, NULL, &btl_version, NULL, 0);

        if (btl_version != 0u) {
            retval = UART_BTL_FAIL_UNKNOWN_VERSION;
        }

        const mlx_chip_t * chip = mlxchip_get_camcu_chip(project_id);

        if ((chip == NULL) || (chip->bootloaders.uart_loader == NULL)) {
            retval = UART_BTL_FAIL_CHIP_NOT_SUPPORTED;
        } else if (retval >= 0) {
            uint32_t nvramStart = chip->memories.nv_memory->start;
            uint32_t nvramEnd = chip->memories.nv_memory->start + chip->memories.nv_memory->writeable - 1;
            uint32_t nvramPageSize = chip->memories.nv_memory->page;
            if ((intelhex_minAddress(ihex) > nvramEnd) || (intelhex_maxAddress(ihex) < nvramStart)) {
                ESP_LOGE(TAG, "No data for this memory in the hex file");
                retval = UART_BTL_FAIL_MISSING_DATA;
            } else {
                uint32_t startaddr = intelhex_minAddress(ihex);
                if (startaddr < nvramStart) {
                    startaddr = nvramStart;
                }
                uint32_t endaddr = intelhex_maxAddress(ihex);
                if (endaddr > nvramEnd) {
                    endaddr = nvramEnd;
                }

                if ((((startaddr - nvramStart) % nvramPageSize) != 0u) ||
                    (((endaddr + 1 - nvramStart) % nvramPageSize) != 0u)) {
                    retval = UART_BTL_FAIL_DATA_NOT_ALIGNED;
                } else {
                    if (chip->bootloaders.uart_loader->prog_keys->values != NULL) {
                        const uint16_t * keys = chip->bootloaders.uart_loader->prog_keys->values;
                        retval = uartbtl_cmdProgKeys(keys, chip->bootloaders.uart_loader->prog_keys->length);
                    }

                    for (uint32_t curaddr = startaddr;
                         (curaddr <= endaddr) && (retval >= 0);
                         curaddr += nvramPageSize) {
                        uint8_t page[nvramPageSize];
                        size_t size = intelhex_get(ihex, curaddr, page, nvramPageSize);
                        if (size == nvramPageSize) {
                            int page_nr = (curaddr - nvramStart) / nvramPageSize;
                            retval = uartbtl_cmdWriteNvram(page_nr, page, size);
                        } else if (intelhex_countBytesInRange(ihex, curaddr, nvramPageSize) != 0) {
                            /* the page was not empty */
                            retval = UART_BTL_FAIL_DATA_NOT_ALIGNED;
                        }
                    }

                    if (retval == UART_BTL_OK) {
                        retval = uartbtl_cmdProgrammingStatus();
                    }
                }
            }
        }
    } else {
        retval = UART_BTL_FAIL_INVALID_HEX_FILE;
    }

    return retval;
}

uartbtl_err_t uartbtl_verifyNvram(ihexContainer_t * ihex) {
    uartbtl_err_t retval;

    if (ihex != NULL) {
        uint16_t project_id;
        uint8_t btl_version = 0u;

        ESP_LOGI(TAG, "ENTER uartbtl_verifyNvram");

        retval = uartbtl_cmdProjectId(&project_id, NULL, &btl_version, NULL, 0);

        if (btl_version != 0u) {
            retval = UART_BTL_FAIL_UNKNOWN_VERSION;
        }

        const mlx_chip_t * chip = mlxchip_get_camcu_chip(project_id);

        if ((chip == NULL) || (chip->bootloaders.uart_loader == NULL)) {
            retval = UART_BTL_FAIL_CHIP_NOT_SUPPORTED;
        }

        if (retval >= 0) {
            uint32_t nvramStart = chip->memories.nv_memory->start;
            uint32_t nvramEnd = chip->memories.nv_memory->start + chip->memories.nv_memory->writeable - 1;
            uint32_t nvramPageSize = chip->memories.nv_memory->page;
            if ((intelhex_minAddress(ihex) > nvramEnd) || (intelhex_maxAddress(ihex) < nvramStart)) {
                ESP_LOGE(TAG, "No data for this memory in the hex file");
                retval = UART_BTL_FAIL_MISSING_DATA;
            } else {
                uint32_t startaddr = intelhex_minAddress(ihex);
                if (startaddr < nvramStart) {
                    startaddr = nvramStart;
                }
                startaddr &= ~(nvramPageSize - 1u);   /* start has to be page aligned */
                uint32_t endaddr = intelhex_maxAddress(ihex);
                if (endaddr > nvramEnd) {
                    endaddr = nvramEnd;
                }

                /* compare chip crc with hex crc for each in hex available page */
                for (uint32_t curaddr = startaddr; (curaddr <= endaddr) && (retval >= 0); curaddr += nvramPageSize) {
                    uint8_t crc = 0u;
                    retval = uartbtl_cmdVerifyNvram((curaddr - nvramStart) / nvramPageSize, 1, &crc);
                    if (retval >= UART_BTL_OK) {
                        uint8_t memory[nvramPageSize];
                        (void)intelhex_getFilled(ihex, curaddr, memory, nvramPageSize);
                        if (crc != crc_calcPageChecksum((uint16_t*)memory, nvramPageSize / 2)) {
                            retval = UART_BTL_FAIL_VERIFY_FAILED;
                            break;
                        }
                    }
                }
            }
        }
    } else {
        retval = UART_BTL_FAIL_INVALID_HEX_FILE;
    }

    return retval;
}

uartbtl_err_t uartbtl_readChipInfo(bool manpow,
                                   uint16_t *project_id,
                                   uint16_t *project_info,
                                   size_t project_info_len) {
    uartbtl_err_t uartstat = UART_BTL_OK;

    ESP_LOGI(TAG, "ENTER uartbtl_readChipInfo");

    uint32_t pattern_time = 50u;
    if (manpow) {
        /* TODO bring back? STR had issues here but why? does chip not stay in btl for pattern?
         * pattern_time = 1000u;
         */
        pattern_time = 100u;
    } else {
        if (uartbtl_chipPowered()) {
            uartbtl_chipPower(false);
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }

        int32_t vsup = uartbtl_getVSupply();
        if (vsup < 4500u) {
            ESP_LOGE(TAG, "Vsup level is too low %i mV", (int)vsup);
            uartstat = UART_BTL_FAIL_VSUP_LOW;
        }

        int32_t vchip = uartbtl_getVChip();
        if (vchip > 4500u) {
            ESP_LOGE(TAG, "Vchip level is too high %i mV, chip not properly reset", (int)vchip);
            uartstat = UART_BTL_FAIL_CHIP_POWERED;
        }
    }

    if (uartstat == UART_BTL_OK) {
        uartstat = uartbtltl_sendEnterBootloaderPattern(pattern_time);
        vTaskDelay(10 / portTICK_PERIOD_MS);

        if (uartstat >= 0) {
            uartstat = uartbtl_cmdProjectId(project_id, NULL, NULL, project_info, project_info_len);
        }

        (void)uartbtl_exitProgrammingMode();
    }

    return uartstat;
}

uartbtl_err_t uartbtl_doAction(bool manpow,
                               uint32_t bitrate,
                               bool fullduplex,
                               uint8_t txpin,
                               uint32_t flash_keys[4],
                               uartbtl_memory_t memory,
                               uartbtl_action_t action,
                               ihexContainer_t * ihex) {
    uartbtl_err_t uartstat = UART_BTL_OK;

    if (ihex != NULL) {
        uint32_t pattern_time = 50u;
        if (manpow) {
            /* TODO bring back? STR had issues here but why? does chip not stay in btl for pattern?
             * pattern_time = 1000u;
             */
            pattern_time = 100u;
        } else {
            if (uartbtl_chipPowered()) {
                uartbtl_chipPower(false);
                vTaskDelay(100 / portTICK_PERIOD_MS);
            }

            int32_t vsup = uartbtl_getVSupply();
            if (vsup < 4500u) {
                ESP_LOGE(TAG, "Vsup level is too low %i mV", (int)vsup);
                uartstat = UART_BTL_FAIL_VSUP_LOW;
            }

            /* wait for chip voltage to decrease */
            int32_t vchip;
            for (int i = 0; i < 10; i++) {
                vchip = uartbtl_getVChip();
                if (vchip < 4500u) {
                    break;
                }
                vTaskDelay(50 / portTICK_PERIOD_MS);
            }
            if (vchip > 4500u) {
                ESP_LOGE(TAG, "Vchip level is too high %i mV, chip not properly reset", (int)vchip);
                uartstat = UART_BTL_FAIL_CHIP_POWERED;
            }
        }

        if (uartstat == UART_BTL_OK) {
            uartstat = uartbtl_enterProgrammingMode(fullduplex, &bitrate, txpin, pattern_time);

            if (uartstat == UART_BTL_OK) {
                if (flash_keys != NULL) {
                    uartstat = uartbtl_unlockMemories(flash_keys);
                }

                if (memory == UARTBTL_MEM_FLASH) {
                    if (uartstat == UART_BTL_OK) {
                        if (action == UARTBTL_ACT_PROGRAM) {
                            uartstat = uartbtl_programFlash(ihex);
                        } else if (action == UARTBTL_ACT_VERIFY) {
                            uartstat = uartbtl_verifyFlash(ihex);
                        }
                    }
                } else if (memory == UARTBTL_MEM_NVRAM) {
                    /* some EEPROM actions might be possible without a successful unlock, as pages are locked separately */
                    if (action == UARTBTL_ACT_PROGRAM) {
                        uartstat = uartbtl_programNvram(ihex);
                    } else if (action == UARTBTL_ACT_VERIFY) {
                        uartstat = uartbtl_verifyNvram(ihex);
                    }
                }
            }

            (void)uartbtl_exitProgrammingMode();
        }

        if (!manpow) {
            uartbtl_chipPower(false);
        }
    } else {
        uartstat = UART_BTL_FAIL_INVALID_HEX_FILE;
    }

    return uartstat;
}

void __attribute__((weak)) uartbtl_chipPower(bool enable) {
    (void)enable;
}

bool __attribute__((weak)) uartbtl_chipPowered(void) {
    return false;
}

int32_t __attribute__((weak)) uartbtl_getVSupply(void) {
    return -1;
}

int32_t __attribute__((weak)) uartbtl_getVChip(void) {
    return -1;
}
