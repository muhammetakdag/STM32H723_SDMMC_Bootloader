/*
 * bootloader.c
 *
 *  Created on: 24 Şub 2026
 *      Author: MTA
 */

#include <string.h>

#include "fatfs.h"
#include "main.h"
#include "sha256.h"
#include "bootloader.h"


static FATFS g_file_system;
static FIL g_fw_file;
static FIL g_hash_file;
/*
 ==============================================================================
 	 	 	 	 	 ##### STATIC FUNCTION IMPLEMENTATIONS #####
 ==============================================================================

 */

/**
 * @brief This function reads the current firmware metadata from flash.
 * @attention Any modification in this function not recommended.
 * @param ptr_fw_metadata Firmware metadata structure pointer.
 * @param first_boot_flag Flag of first boot.
 */

static void s_read_current_fw_metadata(uint32_t *old_magic_val)
{
	FirmwareMetadata_t *ptr_fw_metadata = (FirmwareMetadata_t*) FW_START_ADDRESS;
	*old_magic_val = ptr_fw_metadata->u32_magic_number;
}

/**
 * @brief This function checks the readed firmware metadata.
 * @attention Any modification in this function not recommended.
 * @param ptr_fw_metadata Firmware metadata structure pointer.
 * @param ptr_update_valid_flag Flag of update valid.
 */

static void s_check_fw_metadata(uint8_t *ptr_new_fw_metadata,
		uint8_t *ptr_update_valid_flag, uint32_t *app_start_addr)
{
	FirmwareMetadata_t *incoming_metadata =
			(FirmwareMetadata_t*) ptr_new_fw_metadata;
	uint32_t new_magic_val = 0U;
	uint32_t old_magic_val = 0U;

	if ((NULL != ptr_new_fw_metadata) && (NULL != app_start_addr))
	{
		s_read_current_fw_metadata(&old_magic_val);

		memcpy(&new_magic_val, ptr_new_fw_metadata, sizeof(uint32_t));
		if (MAGIC_NUMBER == new_magic_val)
		{
			*ptr_update_valid_flag = 1U;
			*app_start_addr = incoming_metadata->u32_app_start_addr;
		}
	}
}

/**
 * @brief This function erase the flash.
 * @attention Any modification in this function not recommended.
 * @param None.
 * @retVal Status of operation.
 */
static HAL_StatusTypeDef s_erase_flash(void)
{
	HAL_StatusTypeDef status = HAL_OK;

	status = HAL_FLASH_Unlock();

	if (HAL_OK == status)
	{
		FLASH_EraseInitTypeDef flash_erase_init = {0U};
		uint32_t sector_error = 0U;

		flash_erase_init.TypeErase = FLASH_TYPEERASE_SECTORS;
		flash_erase_init.VoltageRange = FLASH_VOLTAGE_RANGE_3;
		flash_erase_init.Sector = FLASH_SECTOR_1;
		flash_erase_init.NbSectors = 7U;
		flash_erase_init.Banks = FLASH_BANK_1;

		status = HAL_FLASHEx_Erase(&flash_erase_init, &sector_error);

		if (0xFFFFFFFFU != sector_error)
		{
			status = HAL_ERROR;
			HAL_FLASH_Lock();
		}
	}
	HAL_FLASH_Lock();
	return status;
}

/**
 * @brief This function copy the binary from SD Card to flash.
 * @attention Any modification in this function not recommended.
 * @param None.
 * @retVal None.
 */

static void s_copy_fw_to_flash(void)
{
	UINT readed_fw_byte_count = 0U;
	UINT readed_hash_byte_count = 0U;
	uint32_t fw_file_size = 0U;
	uint8_t fw_metadata[META_DATA_SIZE] =
	{ 0U };
	uint8_t fw_buffer[FW_BUFFER_SIZE] __attribute__((aligned(32))) =
	{ 0U };
	uint32_t fw_start_addr = 0U;
	uint8_t is_update_valid = 0U;
	uint8_t fw_new_hash[32U] =
	{ 0U };

	mbedtls_sha256_context sha_ctx;
	uint8_t computed_hash[32U];

	FRESULT fw_file_status = f_open(&g_fw_file, FW_FILE_NAME, FA_READ);
	FRESULT hash_file_status = f_open(&g_hash_file, SHA256_FILE_NAME, FA_READ);

	HAL_StatusTypeDef status = HAL_OK;

	if (FR_OK == hash_file_status)
	{
		f_read(&g_hash_file, fw_new_hash, sizeof(fw_new_hash),
				&readed_hash_byte_count);
	}

	if (FR_OK == fw_file_status)
	{
		fw_file_size = f_size(&g_fw_file);

		// First of all read metadata from incoming binary file.
		if (FR_OK
				== f_read(&g_fw_file, fw_metadata, META_DATA_SIZE,
						&readed_fw_byte_count))
		{
			if ((META_DATA_SIZE == readed_fw_byte_count)
					&& (0U == is_update_valid))
			{
				s_check_fw_metadata(&fw_metadata[0U], &is_update_valid,
						&fw_start_addr);
			}
		}

		if (1U == is_update_valid)
		{
			// Start SHA256 computing for only app data.(FW Metadata is not including
			// the hash.)
			f_lseek(&g_fw_file, META_DATA_SIZE);
			mbedtls_sha256_init(&sha_ctx);
			mbedtls_sha256_starts_ret(&sha_ctx, 0U);

			status = HAL_FLASH_Unlock();
			if (HAL_OK == status)
			{
				while ((FR_OK
						== f_read(&g_fw_file, fw_buffer, sizeof(fw_buffer),
								&readed_fw_byte_count))
						&& (readed_fw_byte_count > 0U))
				{
					HAL_GPIO_WritePin(BOOTLOADER_PIN_GPIO_Port,
					BOOTLOADER_PIN_Pin, GPIO_PIN_SET);

					// Update hash every readed chunk.
					mbedtls_sha256_update_ret(&sha_ctx, fw_buffer,
							readed_fw_byte_count);

					if (readed_fw_byte_count % 32U != 0U)
					{
						uint32_t remaining_readed_data = 32U
								- (readed_fw_byte_count % 32U);
						memset(&fw_buffer[readed_fw_byte_count], 0xFFU,
								remaining_readed_data);
						readed_fw_byte_count += remaining_readed_data;
					}

					for (uint32_t i = 0U; i < readed_fw_byte_count; i += 32U)
					{
						if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD,
								fw_start_addr, (uint32_t) &fw_buffer[i])
								== HAL_OK)
						{
							fw_start_addr += 32U;
						}

						HAL_GPIO_WritePin(BOOTLOADER_PIN_GPIO_Port,
						BOOTLOADER_PIN_Pin, GPIO_PIN_RESET);
					}
				}

				// Complete the hash.
							mbedtls_sha256_finish_ret(&sha_ctx, computed_hash);
							mbedtls_sha256_free(&sha_ctx);
							// Write metadata to flash.
							if (0U == memcmp(fw_new_hash, computed_hash, 32U))
							{
								uint32_t metadata_start_addr = META_DATA_START_ADDRESS;
								for (uint32_t i = 0U; i < META_DATA_SIZE; i += 32U)
								{
									HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD,
											metadata_start_addr, (uint32_t) &fw_metadata[i]);
									metadata_start_addr += 32U;
								}
							}
			}
			else
			{
				HAL_FLASH_Lock();
			}
		}
		else
		{
			HAL_FLASH_Lock();
			f_close(&g_fw_file);
			f_close(&g_hash_file);

		}
	}

	HAL_FLASH_Lock();
	f_close(&g_fw_file);
	f_close(&g_hash_file);
}

/**
 * @brief This function mount the SD Card.
 * @param None.
 * @retVal None.
 */

static void s_mount_sd_card(void)
{
	if (FR_OK == f_mount(&g_file_system, "", 1U))
	{
		for (uint8_t i = 0U; i < 10U; ++i)
		{
			HAL_GPIO_WritePin(BOOTLOADER_PIN_GPIO_Port, BOOTLOADER_PIN_Pin,
					GPIO_PIN_SET);
			HAL_Delay(50U);
			HAL_GPIO_WritePin(BOOTLOADER_PIN_GPIO_Port, BOOTLOADER_PIN_Pin,
					GPIO_PIN_RESET);
			HAL_Delay(50U);
		}
	}
}

/**
 * @brief This function unmount the SD Card.
 * @param None.
 * @retVal None.
 */

static inline void s_unmount_sd_card(void)
{
	f_mount(NULL, "", 1U);
}

/*
 ==============================================================================
 ##### GLOBAL FUNCTION IMPLEMENTATIONS #####
 ==============================================================================

 */

/**
 * @brief This function jump to app start address.
 * @param None.
 * @retVal None.
 */

void jump_to_app(FirmwareMetadata_t *ptr_fw_metadata)
{
	void (*app_reset_handler)(
			void) = (void*)(*(
							(volatile uint32_t*)(ptr_fw_metadata->u32_app_start_addr + 4U)));

	__disable_irq();

	SysTick->CTRL = 0;
	SysTick->LOAD = 0;
	SysTick->VAL = 0;

	HAL_RCC_DeInit();
	HAL_DeInit();

	SCB_CleanInvalidateDCache();
	SCB_InvalidateICache();

	SCB->VTOR = ptr_fw_metadata->u32_app_start_addr;

	__set_MSP(*(volatile uint32_t*)ptr_fw_metadata->u32_app_start_addr);

	app_reset_handler();
}

/**
 * @brief This function runs the bootloader sequence.
 * @param None.
 * @retVal None.
 */
void run_bootloader(void)
{
	HAL_StatusTypeDef status = HAL_OK;

	s_mount_sd_card();
	status = s_erase_flash();

	if (HAL_OK == status)
	{
		s_copy_fw_to_flash();
		s_unmount_sd_card();
		HAL_Delay(100U);
		jump_to_app((FirmwareMetadata_t*) FW_START_ADDRESS);
	}
	else
	{
		/*TODO: Give error with led.*/
	}
}
