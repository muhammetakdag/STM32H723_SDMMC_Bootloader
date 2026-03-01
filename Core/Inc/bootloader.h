/*
 * bootloader.h
 *
 *  Created on: 1 Mar 2026
 *      Author: MTA
 */

#ifndef INC_BOOTLOADER_H_
#define INC_BOOTLOADER_H_

#define FW_START_ADDRESS 0x8020000U
#define META_DATA_START_ADDRESS FW_START_ADDRESS
#define MAGIC_NUMBER_START_ADDRESS (FW_START_ADDRESS)
#define FW_FILE_NAME "firmware.bin"
#define SHA256_FILE_NAME "SHA256_hash.bin"
#define FW_BUFFER_SIZE 1024U
#define META_DATA_SIZE 512UL
#define MAGIC_NUMBER 0x4F594D41U

typedef struct __attribute__((packed)) _FirmwareMetadata_t
{
	uint32_t u32_magic_number;
	uint8_t u8_sha_256[32U];
	uint32_t u32_app_start_addr;
	uint32_t u32_app_end_addr;
	uint32_t u8_fw_version[2U];
	uint8_t u8_hw_version[2U];
	uint8_t u8_app_type;
	char c_build_date[12U];
	char c_build_time[9U];
	uint32_t u32_resereved[5U];
} FirmwareMetadata_t;

void run_bootloader(void);
void jump_to_app(FirmwareMetadata_t *ptr_fw_metadata);

#endif /* INC_BOOTLOADER_H_ */
