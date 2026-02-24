/*
 * bootloader.c
 *
 *  Created on: 24 Şub 2026
 *      Author: MTA
 */

#include <string.h>

#include "fatfs.h"
#include "main.h"

#define FW_START_ADDRESS 0x8020000U
#define FW_FILE_NAME "firmware.bin"
#define FW_BUFFER_SIZE 1024U
/*
  ==============================================================================
                      ##### USER DEFINE TYPES #####
  ==============================================================================

  */

/*
  ==============================================================================
                      ##### STATIC FUNCTION IMPLEMENTATIONS #####
  ==============================================================================

  */

static HAL_StatusTypeDef s_erase_flash(void) {
  HAL_StatusTypeDef status = HAL_OK;

  status = HAL_FLASH_Unlock();

  if (HAL_OK == status) {
    FLASH_EraseInitTypeDef flash_erase_init = 0U;
    uint32_t sector_error = 0U;

    flash_erase_init.TypeErase = FLASH_TYPEERASE_SECTORS;
    flash_erase_init.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    flash_erase_init.Sector = FLASH_SECTOR_1;
    flash_erase_init.NbSectors = 7U;

    status = HAL_FLASHEx_Erase(&flash_erase_init, &sector_error);

    if (0xFFFFFFFFU != sector_error) {
      status = HAL_ERROR;
    }
  }

  return status;
}

static void s_copy_fw_to_flash(void) {
  UINT readed_byte_count = 0U;
  uint32_t fw_file_size = 0U;

  uint8_t fw_buffer[FW_BUFFER_SIZE] __attribute__((aligned(32))) = {0};
  uint32_t fw_start_addr = FW_START_ADDRESS;

  if (FR_OK == f_open(&g_file, FW_FILE_NAME, FA_READ)) {
    fw_file_size = f_size(&g_file);

    while ((FR_OK == f_read(&g_file, fw_buffer, sizeof(fw_buffer),
                            &readed_byte_count)) &&
           (readed_byte_count > 0)) {
      /*TODO: Make status led on*/

      if (readed_byte_count % 32U != 0) {
        uint32_t remaining_readed_data = 32U - (readed_byte_count % 32U);
        memset(&fw_buffer[readed_byte_count], 0xFFU, remaining_readed_data);
        readed_byte_count += remaining_readed_data;
      }

      for (uint32_t i = 0U; i < readed_byte_count; i += 32U) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, fw_start_addr,
                              (uint32_t)&fw_buffer[i]) == HAL_OK) {
          fw_start_addr += 32U;
        }

        /*TODO: Make status led off*/
      }
    }

    HAL_FLASH_Lock();
    f_close(&g_file);
  }
}

static void s_mount_sd_card(void) {
  if (FR_OK == f_mount(&g_file_system, "", 1U)) {
    for (uint8_t i = 0U; i < 10U; ++i) {
      /*TODO: Make status led on*/
      HAL_Delay(50U);
      /*TODO: Make status led off*/
      HAL_Delay(50U);
    }
  }
}

static void s_unmount_sd_card(void) { f_mount(NULL, "", 1U); }

static void s_jump_to_app(void) {
  void (*app_reset_handler)(void) =
      (void*)(*((volatile uint32_t*)(ETX_APP_FLASH_ADDR + 4U)));

  __disable_irq();

  SysTick->CTRL = 0;
  SysTick->LOAD = 0;
  SysTick->VAL = 0;

  HAL_RCC_DeInit();
  HAL_DeInit();

  SCB_CleanInvalidateDCache();
  SCB_InvalidateICache();

  SCB->VTOR = FW_START_ADDRESS;

  __set_MSP(*(volatile uint32_t*)ETX_APP_FLASH_ADDR);

  app_reset_handler();
}
