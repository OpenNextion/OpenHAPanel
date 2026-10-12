#ifdef __cplusplus
extern "C" {


#endif

#ifndef OPEN_NEXTION_COMPONENTS_WEBSERVER_INCLUDE_WEBSERVER_METHOD_API_H
#define OPEN_NEXTION_COMPONENTS_WEBSERVER_INCLUDE_WEBSERVER_METHOD_API_H

#include "cJSON.h"
#include <esp_http_server.h>

/* msg type */
typedef enum webMsgType
{
	WEB_MSG_TYPE_CONNECT, /* connection type */
	WEB_MSG_TYPE_DATA, /* data type */
	WEB_MSG_TYPE_MAX
} webMsgType_t;

/* websocket message */
typedef struct
{
	httpd_handle_t hd;
	int fd;
	double seq;
} async_resp_arg;

/* websocket message */
typedef struct
{
	webMsgType_t msgType;
	async_resp_arg resp_arg;
	char* data;
	size_t len;
} websocketMsg_t;

/* method api handle */
typedef esp_err_t (* webserver_method_api_handle)(cJSON* data, async_resp_arg* now_resp_arg);

/* method api strategy */
typedef struct
{
	const char* method;
	webserver_method_api_handle handler;
} webserver_method_api_strategy;

#define WEB_API_ERROR_OK (0)
#define WEB_API_ERROR_FAIL (-1)
#define WEB_API_ERROR_PARAM_LEN (-4)
#define WEB_API_ERROR_AUTH (2)
#define WEB_API_ERROR_WIFI_AUTH_FAILED (420)
#define WEB_API_ERROR_WIFI_CONNECT_FAILED (421)
#define WEB_API_ERROR_WEB_DEBUG_ALREADY_ON (900) /* Other pages are in the process of debugging log operations */

#define WEB_API_ERROR_SET_FORGET_PASSWORD (901) /* Other pages are in the process of resetting passwords！ */

#define WEB_API_ERROR_OTA_IS_DOING (904)                /* the device is ota do not repeat the operation */
#define WEB_API_ERROR_WHITE_LIST_IP_ALREADY_EXIST (907) /* the ip address of the whitelist exists */
#define WEB_API_ERROR_TCP_PORT_ALREADY_EXIST (908)
#define WEB_API_ERROR_SESSION_INVALID (910)
#define WEB_API_ERROR_SESSION_EXPIRED (911)
#define WEB_API_ERROR_SESSION_TIME_UNAVAILABLE (912)
#define WEB_API_ERROR_SESSION_TOO_MANY_ATTEMPTS (913)
#define WEB_API_ERROR_SESSION_STORAGE (914)
#define WEB_API_ERROR_HANDOFF_INVALID (915)

#define WEB_API_KEY_METHOD "method"
#define WEB_API_KEY_SEQ "seq"
#define WEB_API_KEY_DATA "data"
#define WEB_API_KEY_ERROR "error"
#define WEB_API_KEY_IV "iv"
#define WEB_API_KEY_CONTENT "content"
#define WEB_API_KEY_CUSTOMER_NAME "customName"
#define WEB_API_KEY_NAME "username"
#define WEB_API_KEY_REALM "realm"
#define WEB_API_KEY_NONCE "nonce"
#define WEB_API_KEY_C_NONCE "cnonce"
#define WEB_API_KEY_QOP "qop"
#define WEB_API_KEY_RSP "response"
#define WEB_API_KEY_C_ALGORITHM "algorithm"
#define WEB_API_KEY_PASSWORD "password"
#define WEB_API_KEY_TS "ts"

#define WEB_API_METHOD_SYS_EXCHANGE_PROVISIONING_HANDOFF "Sys.ExchangeProvisioningHandoff"
#define WEB_API_METHOD_SYS_EXCHANGE_PROVISIONING_HANDOFF_RSP "Sys.ExchangeProvisioningHandoffRsp"
#define WEB_API_KEY_CHUNK "chunk"
#define WEB_API_KEY_CHUNK_TOTAL "chunkTotal"
#define WEB_API_KEY_FW_TYPE "firmwareType"
#define WEB_API_KEY_FW_VERSION "firmwareVersion"
#define WEB_API_KEY_TARGET "target"
#define WEB_API_KEY_PROGRESS "progress"
#define WEB_API_KEY_STATUS "status"
#define WEB_API_KEY_AUTO_OTA_ENABLE "autoUpdateEnabled"
#define WEB_API_KEY_CHECKSUM "checksum"
#define WEB_API_KEY_TOTAL_SIZE "totalSize"
#define WEB_API_KEY_LAST_CHK_TIME "lastCheckTime"
#define WEB_API_KEY_LATEST_STB_VER "latestStableVersion"
#define WEB_API_KEY_LATEST_BETA_VER "latestBetaVersion"
#define WEB_API_KEY_ESP32 "esp32"
#define WEB_API_KEY_WIRELESS_CONTROLLER_ZIGBEE "wirelessControllerZigbee"
#define WEB_API_KEY_WIRELESS_CONTROLLER_THREAD "wirelessControllerThread"
#define WEB_API_KEY_WIRELESS_CONTROLLER_ROUTER "wirelessControllerZigbeeRouter"
#define WEB_API_KEY_SUCCESS "success"
#define WEB_API_KEY_FAILED "failed"
#define WEB_API_KEY_PROCESSING "processing"
#define WEB_API_KEY_WIRELESS_CONTROLLER "wirelessController"
#define WEB_API_KEY_ENABLE "enable"
#define WEB_API_KEY_LOG_CONTENT "logContent"
#define WEB_API_KEY_STABLE "stable"
#define WEB_API_KEY_BETA "beta"
#define WEB_API_KEY_ZIGBEE "zigbee"
#define WEB_API_KEY_OPEN_THREAD "openthread"
#define WEB_API_KEY_ROUTER "zigbeerouter"
#define WEB_API_KEY_VERSION "version"
#define WEB_API_KEY_CUSTOM "custom"
#define WEB_API_KEY_MODE "mode"
#define WEB_API_KEY_BAUD_RATE "baudRate"

/*-- Method: Sys. --*/
#define WEB_API_METHOD_SYS_AUTH_NTY "Sys.AuthRequiredNotify"
#define WEB_API_METHOD_SYS_SET_LOGIN_STATE "Sys.SetLoginState"
#define WEB_API_METHOD_SYS_SET_LOGIN_STATE_RSP "Sys.SetLoginStateRsp"
#define WEB_API_METHOD_SYS_SET_PASSWORD "Sys.SetPassword"
#define WEB_API_METHOD_SYS_SET_PASSWORD_RSP "Sys.SetPasswordRsp"
#define WEB_API_METHOD_SYS_SET_FORGOT_PASSWORD "Sys.SetForgotPasswordState"
#define WEB_API_METHOD_SYS_SET_FORGOT_PASSWORD_RSP "Sys.SetForgotPasswordStateRsp"
#define WEB_API_METHOD_SYS_FOTGET_PASS_NOTIFY "Sys.ForgotPasswordNotify"
#define WEB_API_METHOD_SYS_RESUME_SESSION "Sys.ResumeSession"
#define WEB_API_METHOD_SYS_RESUME_SESSION_RSP "Sys.ResumeSessionRsp"
#define WEB_API_METHOD_SYS_LOGOUT "Sys.Logout"
#define WEB_API_METHOD_SYS_LOGOUT_RSP "Sys.LogoutRsp"
#define WEB_API_METHOD_SYS_REVOKE_ALL_SESSIONS "Sys.RevokeAllSessions"
#define WEB_API_METHOD_SYS_REVOKE_ALL_SESSIONS_RSP "Sys.RevokeAllSessionsRsp"

/* -- Method: Security. -- */
#define WEB_API_METHOD_SECU_SET_IP_WHITE_LIST "Security.SetIpWhitelist"
#define WEB_API_METHOD_SECU_SET_IP_WHITE_LIST_RSP "Security.SetIpWhitelistRsp"
#define WEB_API_METHOD_SECU_INFO_NOTIFY "Security.InfoNotify"

/* -- Method: ChannelHealth. -- */
#define WEB_API_METHOD_HEALTH_SET_SCAN "ChannelHealth.SetScan"
#define WEB_API_METHOD_HEALTH_SCAN_RSP "ChannelHealth.SetScanRsp"
#define WEB_API_METHOD_HEALTH_NOTIFY "ChannelHealth.Notify"

/* -- Method: Device. -- */
#define WEB_API_METHOD_REBOOT_DEVICE "Device.RebootDevice"
#define WEB_API_METHOD_REBOOT_DEVICE_RSP "Device.RebootDeviceRsp"
#define WEB_API_METHOD_RESET_DEVICE "Device.ResetDevice"
#define WEB_API_METHOD_RESET_DEVICE_RSP "Device.ResetDeviceRsp"
#define WEB_API_METHOD_SET_NAME "Device.SetName"
#define WEB_API_METHOD_SET_NAME_RSP "Device.SetNameRsp"
#define WEB_API_METHOD_SET_BRIGHTNESS "Device.SetBrightness"
#define WEB_API_METHOD_SET_BRIGHTNESS_RSP "Device.SetBrightnessRsp"
#define WEB_API_METHOD_SET_NIGHT_MODE "Device.SetNightMode"
#define WEB_API_METHOD_SET_NIGHT_MODE_RSP "Device.SetNightModeRsp"
#define WEB_API_METHOD_SET_SCREEN_LOCK "Device.SetScreenLock"
#define WEB_API_METHOD_SET_SCREEN_LOCK_RSP "Device.SetScreenLockRsp"
#define WEB_API_METHOD_LANGUAGE_NOTIFY "Device.LanguageNotify"
#define WEB_API_METHOD_SET_LANGUAGE "Device.SetLanguage"
#define WEB_API_METHOD_SET_LANGUAGE_RSP "Device.SetLanguageRsp"
#define WEB_API_METHOD_SET_THEME_MODE "Device.SetThemeMode"
#define WEB_API_METHOD_SET_THEME_MODE_RSP "Device.SetThemeModeRsp"
#define WEB_API_METHOD_SCREEN_CONFIG_NOTIFY "Device.ScreenConfigNotify"
#define WEB_API_METHOD_SET_SCREEN_CONFIG "Device.SetScreenConfig"
#define WEB_API_METHOD_SET_SCREEN_CONFIG_RSP "Device.SetScreenConfigRsp"
#define WEB_API_METHOD_SET_ALL_INDICATOR_OFF "Device.SetAllIndicatorsOff"
#define WEB_API_METHOD_SET_ALL_INDICATOR_OFF_RSP "Device.SetAllIndicatorsOffRsp"
#define WEB_API_METHOD_SET_NIGHT_MODE "Device.SetNightMode"
#define WEB_API_METHOD_SET_NIGHT_MODE_RSP "Device.SetNightModeRsp"
#define WEB_API_METHOD_SET_INDICATOR "Device.SetIndicator"
#define WEB_API_METHOD_SET_INDICATOR_RSP "Device.SetIndicatorRsp"
#define WEB_API_METHOD_PREVIEW_INDICATOR_EFFECT "Device.PreviewIndicatorEffect"
#define WEB_API_METHOD_PREVIEW_INDICATOR_EFFECT_RSP "Device.PreviewIndicatorEffectRsp"
#define WEB_API_METHOD_IDENTIFY "Device.Identify"
#define WEB_API_METHOD_IDENTIFY_RSP "Device.IdentifyRsp"
#define WEB_API_METHOD_GET_NTFY_HIS "Device.GetNotificationHistory"
#define WEB_API_METHOD_GET_NTFY_HIS_RSP "Device.GetNotificationHistoryRsp"
#define WEB_API_METHOD_CLEAR_NTFY_HIS "Device.ClearNotificationHistory"
#define WEB_API_METHOD_CLEAR_NTFY_HIS_RSP "Device.ClearNotificationHistoryRsp"
#define WEB_API_METHOD_NTFY "Device.NotificationNotify"
#define WEB_API_METHOD_UP_ESP32_FW "Device.UploadEsp32Firmware"
#define WEB_API_METHOD_UP_ESP32_FW_RSP "Device.UploadEsp32FirmwareRsp"
#define WEB_API_METHOD_FLASH_NTFY "Device.FlashNotify"
#define WEB_API_METHOD_SET_AUTO_UPDATE "Device.SetAutoUpdate"
#define WEB_API_METHOD_SET_AUTO_UPDATE_RSP "Device.SetAutoUpdateRsp"
#define WEB_API_METHOD_SET_FW_LAST_CHK_INFO "Device.SetFWLastCheckInfo"
#define WEB_API_METHOD_SET_FW_LAST_CHK_INFO_RSP "Device.SetFWLastCheckInfoRsp"

#define WEB_API_METHOD_DEBUG_LOG "Device.SetDebugLog"
#define WEB_API_METHOD_DEBUG_LOG_RSP "Device.SetDebugLogRsp"
#define WEB_API_METHOD_DEBUG_LOG_NTFY "Device.DebugLogNotify"
#define WEB_API_METHOD_DEVICE_INFO_NOTIFY "Device.InfoNotify"
#define WEB_API_METHOD_FW_INFO_NOTIFY "Device.FirmwareInfoNotify"
#define WEB_API_METHOD_FIRMWARE_UPDATE_STATUS_NOTIFY "Device.FirmwareUpdateStatusNotify"
#define WEB_API_METHOD_MDNS_NOTIFY "Device.LanDevicesNotify"
#define WEB_API_METHOD_INDICATOR_NOTIFY "Device.IndicatorNotify"
#define WEB_API_METHOD_THEME_MODE_NOTIFY "Device.ThemeModeNotify"
#define WEB_API_METHOD_DOWNLOAD_BACKUP "Device.DownloadBackup"
#define WEB_API_METHOD_DOWNLOAD_BACKUP_RSP "Device.DownloadBackupRsp"
#define WEB_API_METHOD_UPLOAD_BACKUP "Device.UploadBackup"
#define WEB_API_METHOD_UPLOAD_BACKUP_RSP "Device.UploadBackupRsp"
#define WEB_API_METHOD_SET_TIMEZONE "Device.SetTimeZone"
#define WEB_API_METHOD_SET_TIMEZONE_RSP "Device.SetTimeZoneRsp"
#define WEB_API_METHOD_GET_TIMEZONE "Device.GetTimeZone"
#define WEB_API_METHOD_GET_TIMEZONE_RSP "Device.GetTimeZoneRsp"
#define WEB_API_METHOD_SET_NTP_SERVER "Device.SetNtpServers"
#define WEB_API_METHOD_SET_NTP_SERVER_RSP "Device.SetNtpServersRsp"
#define WEB_API_METHOD_WEBHOOK_SET "Device.SetWebhook"
#define WEB_API_METHOD_WEBHOOK_SET_RSP "Device.SetWebhookRsp"
#define WEB_API_METHOD_WEBHOOK_DEL "Device.DelWebhook"
#define WEB_API_METHOD_WEBHOOK_DEL_RSP "Device.DelWebhookRsp"
#define WEB_API_METHOD_WEBHOOK_TEST "Device.TestWebhook"
#define WEB_API_METHOD_WEBHOOK_TEST_RSP "Device.TestWebhookRsp"
#define WEB_API_METHOD_WEBHOOK_LIST_NOTIFY "Device.WebhookListNotify"
#define WEB_API_METHOD_IEEE_SET "Device.SetIEEAddress"
#define WEB_API_METHOD_IEEE_SET_RSP "Device.SetIEEAddressRsp"

#define WEB_API_METHOD_RESET_WIRELESS_CONTROLLER_ROUTER "Device.ResetWirelessControllerRouterMode"
#define WEB_API_METHOD_RESET_WIRELESS_CONTROLLER_ROUTER_RSP "Device.ResetWirelessControllerRouterModeRsp"
#define WEB_API_METHOD_WIRELESS_CONTROLLER_INFO_NOTIFY "Device.WirelessControllerInfoNotify"
#define WEB_API_METHOD_SET_WIRELESS_CONTROLLER_INFO "Device.SetWirelessControllerInfo"
#define WEB_API_METHOD_SET_WIRELESS_CONTROLLER_INFO_RSP "Device.SetWirelessControllerInfoRsp"

#define WEB_API_METHOD_UP_WIRELESS_CONTROLLER_FW "Device.UploadWirelessControllerFirmware"
#define WEB_API_METHOD_UP_WIRELESS_CONTROLLER_FW_RSP "Device.UploadWirelessControllerFirmwareRsp"

/* -- Method: Network -- */
#define WEB_API_METHOD_NET_SET_MDNS_DOMAIN "Network.SetMdnsDomain"
#define WEB_API_METHOD_NET_SET_MDNS_DOMAIN_RSP "Network.SetMdnsDomainRsp"
#define WEB_API_METHOD_NET_SET_VPN "Network.SetVpn"
#define WEB_API_METHOD_NET_SET_VPN_RSP "Network.SetVpnRsp"
#define WEB_API_METHOD_NET_SCAN "Network.ScanWiFi"
#define WEB_API_METHOD_NET_SCAN_HASS_INSTANCE "Network.ScanHassInstance"
#define WEB_API_METHOD_NET_SWITCH_WIFI "Network.SwitchWiFi"
#define WEB_API_METHOD_NET_SET_WIFI_CONFIG "Network.SetWiFiConfig"
#define WEB_API_METHOD_NET_SET_ETH_CONFIG "Network.SetEthernetConfig"
#define WEB_API_METHOD_NET_SET_AP_PASS "Network.SetApPassword"
#define WEB_API_METHOD_NET_SET_AP_CONFIG "Network.SetApConfig"
#define WEB_API_METHOD_NET_INFO_NOTIFY "Network.InfoNotify"
#define WEB_API_METHOD_NET_SET_AP_PASS_RSP "Network.SetApPasswordRsp"
#define WEB_API_METHOD_NET_SCAN_RSP "Network.ScanWiFiRsp"
#define WEB_API_METHOD_NET_SCAN_HASS_INSTANCE_RSP "Network.ScanHassInstanceRsp"
#define WEB_API_METHOD_NET_SET_ETH_CONFIG_RSP "Network.SetEthernetConfigRsp"
#define WEB_API_METHOD_NET_SET_WIFI_CONFIG_RSP "Network.SetWiFiConfigRsp"
#define WEB_API_METHOD_NET_SET_AP_CONFIG_RSP "Network.SetApConfigRsp"
#define WEB_API_METHOD_NET_SWITCH_WIFI_RSP "Network.SwitchWiFiRsp"
#define WEB_API_METHOD_NET_WIFI_SCAN_RES_NOTIFY "Network.WiFiScanResultNotify"
#define WEB_API_METHOD_NET_HASS_INSTANCE_SCAN_RES_NOTIFY "Network.HassInstanceScanResultNotify"
#define WEB_API_METHOD_NET_CONN_STATUS_NOTIFY "Network.ConnectionStatusNotify"
#define WEB_API_METHOD_NET_SET_AP_SSID "Network.SetApSsid"
#define WEB_API_METHOD_NET_SET_AP_SSID_RSP "Network.SetApSsidRsp"

/* -- Method: Home Assistant -- */
#define WEB_API_METHOD_HASS_START_OAUTH "Hass.StartOAuth"
#define WEB_API_METHOD_HASS_START_OAUTH_RSP "Hass.StartOAuthRsp"
#define WEB_API_METHOD_HASS_COMPLETE_OAUTH "Hass.CompleteOAuth"
#define WEB_API_METHOD_HASS_COMPLETE_OAUTH_RSP "Hass.CompleteOAuthRsp"
#define WEB_API_METHOD_HASS_AUTH_RESULT_NOTIFY "Hass.AuthorizationResultNotify"
#define WEB_API_METHOD_HASS_GET_CONNECTION_INFO "Hass.GetConnectionInfo"
#define WEB_API_METHOD_HASS_GET_CONNECTION_INFO_RSP "Hass.GetConnectionInfoRsp"
#define WEB_API_METHOD_HASS_DISABLE "Hass.Disable"
#define WEB_API_METHOD_HASS_DISABLE_RSP "Hass.DisableRsp"
#define WEB_API_METHOD_HASS_ACTIVATE "Hass.Activate"
#define WEB_API_METHOD_HASS_ACTIVATE_RSP "Hass.ActivateRsp"
#define WEB_API_METHOD_HASS_UPDATE_URL "Hass.UpdateUrl"
#define WEB_API_METHOD_HASS_UPDATE_URL_RSP "Hass.UpdateUrlRsp"
#define WEB_API_METHOD_HASS_CONNECTION_STATUS_NOTIFY "Hass.ConnectionStatusNotify"
#define WEB_API_METHOD_HASS_REMOVE_AUTH "Hass.RemoveAuthorization"
#define WEB_API_METHOD_HASS_REMOVE_AUTH_RSP "Hass.RemoveAuthorizationRsp"

/* -- Method: Mqtt -- */
#define WEB_API_METHOD_MQTT_NOTIFY "Mqtt.InfoNotify"
#define WEB_API_METHOD_MQTT_SET_STATE "Mqtt.SetState"
#define WEB_API_METHOD_MQTT_SET_STATE_RSP "Mqtt.SetStateRsp"
#define WEB_API_METHOD_MQTT_SET_BROKER_CONFIG "Mqtt.SetBrokerConfig"
#define WEB_API_METHOD_MQTT_SET_BROKER_CONFIG_RSP "Mqtt.SetBrokerConfigRsp"
#define WEB_API_METHOD_MQTT_UPLOAD_CAPEM "Mqtt.UploadCaPem"
#define WEB_API_METHOD_MQTT_UPLOAD_CAPEM_RSP "Mqtt.UploadCaPemRsp"
#define WEB_API_METHOD_MQTT_UPLOAD_CLIENT_CERT "Mqtt.UploadClientCert"
#define WEB_API_METHOD_MQTT_UPLOAD_CLIENT_CERT_RSP "Mqtt.UploadClientCertRsp"
#define WEB_API_METHOD_MQTT_UPLOAD_CLIENT_KEY "Mqtt.UploadClientKey"
#define WEB_API_METHOD_MQTT_UPLOAD_CLIENT_KEY_RSP "Mqtt.UploadClientKeyRsp"
#define WEB_API_METHOD_MQTT_DEL_CAPEM "Mqtt.DelCaPem"
#define WEB_API_METHOD_MQTT_DEL_CAPEM_RSP "Mqtt.DelCaPemRsp"
#define WEB_API_METHOD_MQTT_DEL_CLIENT_CERT "Mqtt.DelClientCert"
#define WEB_API_METHOD_MQTT_DEL_CLIENT_CERT_RSP "Mqtt.DelClientCertRsp"
#define WEB_API_METHOD_MQTT_DEL_CLIENT_KEY "Mqtt.DelClientKey"
#define WEB_API_METHOD_MQTT_DEL_CLIENT_KEY_RSP "Mqtt.DelClientKeyRsp"
#define WEB_API_METHOD_MQTT_GET_PASSWORD "Mqtt.GetPassword"
#define WEB_API_METHOD_MQTT_GET_PASSWORD_RSP "Mqtt.GetPasswordRsp"

/* -- Method: UsbPassthrough -- */
#define WEB_API_METHOD_USB_PASSTHROUGH_SET_DEVICE "USBPassthrough.SetPassthroughDevice"
#define WEB_API_METHOD_USB_PASSTHROUGH_SET_DEVICE_RSP "USBPassthrough.SetPassthroughDeviceRsp"
#define WEB_API_METHOD_USB_PASSTHROUGH_NOTIFY "USBPassthrough.InfoNotify"

#define WEB_API_METHOD_REMOTE "Remote"
#define WEB_API_METHOD_WEBHOOK "Webhook"

void esp32_app_restart();

#endif // OPEN_NEXTION_COMPONENTS_WEBSERVER_INCLUDE_WEBSERVER_METHOD_API_H

#ifdef __cplusplus
}
#endif
