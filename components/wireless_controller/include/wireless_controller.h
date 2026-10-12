#ifdef __cplusplus
extern "C" {
#endif

#ifndef OPEN_NEXTION_WIRELESS_CONTROLLER_H
#define OPEN_NEXTION_WIRELESS_CONTROLLER_H

typedef enum
{
	WIRELESS_CONTROLLER_WORK_MODE_COOR = 0,
	WIRELESS_CONTROLLER_WORK_MODE_ROUTER,
	WIRELESS_CONTROLLER_WORK_MODE_THREAD,
	WIRELESS_CONTROLLER_WORK_MODE_CUSTOM,
	WIRELESS_CONTROLLER_WORK_MODE_INVALID,
} wireless_controller_work_mode_t;

typedef struct
{
	uint32_t baud;
	char version[100];
	uint8_t is_stable;
	bool hw_flow_ctrl;
} wireless_controller_fw_info_t;

typedef struct
{
	char custom_eui64[24];
	char factory_eui64[24];
} wireless_controller_ieee_info_t;

typedef struct
{
	wireless_controller_work_mode_t current_work_mode;
	wireless_controller_fw_info_t cur_fw_info;
	wireless_controller_ieee_info_t ieee_info;
} wireless_controller_work_status_t;

// char* wireless_controller_get_customer_file_name();

// void wireless_controller_save_customer_name_to_file(char* customer_name);

// void wireless_controller_load_ieee_from_file();

// void mg24_save_mg24_ieee_info_to_file();

// esp_err_t wireless_controller_load_config_from_nvs(void);

// esp_err_t wireless_controller_save_config_to_nvs(void);

// void wireless_controller_work_mode_init(void);

#endif // OPEN_NEXTION_WIRELESS_CONTROLLER_H

#ifdef __cplusplus
}
#endif
