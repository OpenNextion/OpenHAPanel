#include "app.h"
#include "check_ram.h"
#include "cJSON.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "nvs_storage.h"
#include "nvs_flash.h"
#include "littlefs.h"
#include "esp_netif.h"
#include "esp_event.h"

static void dump_start_reason(void);
static void dump_ota_partition_info(void);
static const char *ota_image_state_to_string(esp_ota_img_states_t state);
static void setup_cjson_hooks(void);
static void *psram_malloc(size_t size);
static void psram_free(void *ptr);

static const char *TAG = "main";

#define FW_STATE_NVS_NS "esp32_cfg"
#define FW_STATE_NVS_KEY_ESP32_VERSION "esp32_ver"
#define FW_STATE_NVS_KEY_ESP32_TYPE "esp32_type"

static esp_err_t save_esp32_info_to_nvs(void)
{
    const nvs_storage_write_item_t items[] = {
        {
            .key = FW_STATE_NVS_KEY_ESP32_VERSION,
            .type = NVS_STORAGE_TYPE_STRING,
            .value.string = CONFIG_FIRMWARE_VERSION_STRING,
        },
        {
            .key = FW_STATE_NVS_KEY_ESP32_TYPE,
            .type = NVS_STORAGE_TYPE_STRING,
            .value.string = CONFIG_FIRMWARE_TYPE_STRING,
        },
    };
    return nvs_storage_write(FW_STATE_NVS_NS, items, sizeof(items) / sizeof(items[0]));
}
/**
 * @brief Initialize essential ESP32 system components including NVS, BT, event loop, and network stack.
 *
 * This function initializes the Non-Volatile Storage (NVS), releases unused Bluetooth memory,
 * creates the default event loop, and initializes the TCP/IP and Wi-Fi interfaces.
 */
static void esp_init(void)
{
    /* initialize the nvs component */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    nvs_stats_t nvs_stats = {0};
    nvs_get_stats(NULL, &nvs_stats);
    ESP_LOGI(TAG, "NVS Stats: Count: UsedEntries = (%d), FreeEntries = (%d), AllEntries = (%d)", nvs_stats.used_entries,
             nvs_stats.free_entries, nvs_stats.total_entries);

    ret = save_esp32_info_to_nvs();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save ESP32 firmware info: %s", esp_err_to_name(ret));
    }
    // ESP_ERROR_CHECK(esp_bt_mem_release(ESP_BT_MODE_CLASSIC_BT));
    /* initialize the default event loop */
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    /* Initialize the TCP/IP protocol stack and WI-FI interface */
    ESP_ERROR_CHECK(esp_netif_init());
    /* Initialize the LittleFS file system */
    ESP_ERROR_CHECK(little_file_system_init());
}

void app_main(void)
{
    dump_start_reason();
    ESP_LOGI(TAG, "Device model: %s", CONFIG_DEVICE_MODEL_STRING);
    setup_cjson_hooks();
    esp_init();
    dump_ota_partition_info();
    check_ram_timer_init();
    app_init();
}

/**
 * @brief Convert an ESP-IDF OTA image state into a log-friendly string.
 */
static const char *ota_image_state_to_string(esp_ota_img_states_t state)
{
    switch (state) {
    case ESP_OTA_IMG_NEW:
        return "new";
    case ESP_OTA_IMG_PENDING_VERIFY:
        return "pending_verify";
    case ESP_OTA_IMG_VALID:
        return "valid";
    case ESP_OTA_IMG_INVALID:
        return "invalid";
    case ESP_OTA_IMG_ABORTED:
        return "aborted";
    case ESP_OTA_IMG_UNDEFINED:
    default:
        return "undefined";
    }
}

/**
 * @brief Log the running, configured boot, next update, and last invalid OTA partitions.
 *
 * This is startup diagnostics only. It deliberately does not change OTA image
 * state; the firmware_update component performs first-boot verification.
 */
static void dump_ota_partition_info(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *boot = esp_ota_get_boot_partition();
    const esp_partition_t *next = esp_ota_get_next_update_partition(NULL);
    const esp_partition_t *last_invalid = esp_ota_get_last_invalid_partition();

    if (running) {
        ESP_LOGI(TAG, "Running OTA partition: label=%s address=0x%08lx size=%lu", running->label,
                 (unsigned long)running->address, (unsigned long)running->size);

        esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
        esp_err_t err = esp_ota_get_state_partition(running, &state);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Running OTA image state: %s", ota_image_state_to_string(state));
        } else {
            ESP_LOGW(TAG, "Unable to read running OTA image state: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGW(TAG, "Running OTA partition is unavailable");
    }

    if (boot) {
        ESP_LOGI(TAG, "Configured boot partition: label=%s address=0x%08lx", boot->label,
                 (unsigned long)boot->address);
    } else {
        ESP_LOGW(TAG, "Configured boot partition is unavailable");
    }

    if (next) {
        ESP_LOGI(TAG, "Next OTA partition: label=%s address=0x%08lx size=%lu", next->label,
                 (unsigned long)next->address, (unsigned long)next->size);
    } else {
        ESP_LOGE(TAG, "No writable OTA partition is available");
    }

    if (last_invalid) {
        ESP_LOGW(TAG, "Last invalid OTA partition: label=%s address=0x%08lx", last_invalid->label,
                 (unsigned long)last_invalid->address);
    }
}

/**
 * @brief Dump start reason.
 */
static void dump_start_reason(void)
{
    esp_reset_reason_t reason = esp_reset_reason();

    switch (reason) {
    case ESP_RST_POWERON:
        ESP_LOGE(TAG, "Reset reason: Power-on restart");
        break;
    case ESP_RST_EXT:
        ESP_LOGE(TAG, "Reset Reason: external pin (not applicable for ESP32)");
        break;
    case ESP_RST_SW:
        ESP_LOGE(TAG, "Reset Reason: Software Reboot (via esp_restart)");
        break;
    case ESP_RST_PANIC:
        ESP_LOGE(TAG, "Reset Reason: Software Abnormality/Panic Restart");
        break;
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:
        ESP_LOGE(TAG, "Reset Reason: Watch Dog Reset");
        break;
    case ESP_RST_DEEPSLEEP:
        ESP_LOGE(TAG, "Reset reason: Deep-sleep wake restart");
        break;
    case ESP_RST_BROWNOUT:
        ESP_LOGE(TAG, "Reset reason: undervoltage (browning) restart");
        break;
    case ESP_RST_SDIO:
        ESP_LOGE(TAG, "Reset reason: Reset over SDIO");
        break;
    case ESP_RST_USB:
        ESP_LOGE(TAG, "Reset reason: by USB peripheral");
        break;
    case ESP_RST_JTAG:
        ESP_LOGE(TAG, "Reset reason: by JTAG");
        break;
    default:
        ESP_LOGE(TAG, "Reset Reason: Unknown (%d)", reason);
    }
}

/**
 * @brief Set custom memory allocation hooks for cJSON using PSRAM-capable functions.
 */
static void setup_cjson_hooks(void)
{
    cJSON_Hooks hooks = {
        .malloc_fn = psram_malloc,
        .free_fn = psram_free,
    };
    cJSON_InitHooks(&hooks);
}

/**
 * @brief Allocate memory with preference for PSRAM; fallback to internal RAM if needed.
 *
 * @param size Size in bytes to allocate.
 * @return void* Pointer to the allocated memory, or NULL on failure.
 */
static void *psram_malloc(size_t size)
{
    /* PSRAM is tried first, and internal memory is rolled back if it fails */
    void *ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (ptr == NULL) {
        ESP_LOGI(TAG,
                 "PSRAM allocation failed! Requested size: %d bytes, Available memory: %d bytes, Largest free block: "
                 "%d bytes",
                 size, heap_caps_get_free_size(MALLOC_CAP_SPIRAM), heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));

        /* There may be an abnormality due to the unstable power supply, so try to distribute it again */
        ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (ptr == NULL) {
            /* Failed again to allocate space from internal memory */
            ESP_LOGW(TAG, "PSRAM error malloc MALLOC_CAP_INTERNAL");
            ptr = heap_caps_malloc(size, MALLOC_CAP_INTERNAL);
        }
    }

    return ptr;
}

/**
 * @brief Free memory previously allocated using ps_ram_malloc or ps_ram_realloc.
 *
 * @param ptr Pointer to the memory block to free.
 */
static void psram_free(void *ptr)
{
    heap_caps_free(ptr);
}
