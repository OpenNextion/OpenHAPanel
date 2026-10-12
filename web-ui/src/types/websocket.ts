import type { IAuthNotify, IForgotPwdNotify } from '@/types/auth';
import type { IConnectionStatusNotify, IWifiInfoNotify, IWifiScanResultNotify } from '@/types/network';
import type { IDeviceInfoNotify } from './device';
import type { IHassAuthorizationResult, IHassConnectionStatusNotify, IHassScanResultNotify } from '@/types/homeassistant';
import type { IHassScreenConfigNotify } from './screenConfig';
import type { IFirmwareUpdateStatusNotify } from './firmware';
import type { ILanDeviceNotify } from './lanDevice';

/** websocket methods */
export enum EWsMethods {
    // ============= Auth status =============
    AUTH_REQUIRED_NOTIFY = 'Sys.AuthRequiredNotify',

    //// ============= Set password =============
    SET_PASSWORD = 'Sys.SetPassword',
    //SET_PASSWORD_RSP = 'Sys.SetPasswordRsp',

    //// ============= Login auth =============
    SET_LOGIN_STATE = 'Sys.SetLoginState',
    //SET_LOGIN_STATE_RSP = 'Sys.SetLoginStateRsp',
    RESUME_SESSION = 'Sys.ResumeSession',
    RESUME_SESSION_RSP = 'Sys.ResumeSessionRsp',
    EXCHANGE_PROVISIONING_HANDOFF = 'Sys.ExchangeProvisioningHandoff',
    EXCHANGE_PROVISIONING_HANDOFF_RSP = 'Sys.ExchangeProvisioningHandoffRsp',
    LOGOUT = 'Sys.Logout',
    LOGOUT_RSP = 'Sys.LogoutRsp',

    //// ============= Forget password =============
    SET_FORGOT_PASSWORD_STATE = 'Sys.SetForgotPasswordState',
    SET_FORGOT_PASSWORD_STATE_RSP = 'Sys.SetForgotPasswordStateRsp',
    FORGOT_PASSWORD_NOTIFY = 'Sys.ForgotPasswordNotify',

    //// ============= Device information =============
    DEVICE_INFO_NOTIFY = 'Device.InfoNotify',
    FIRMWARE_UPDATE_STATUS_NOTIFY = 'Device.FirmwareUpdateStatusNotify',
    LAN_DEVICE_NOTIFY = 'Device.LanDevicesNotify',
    SET_DEVICE_NAME = 'Device.SetName',
    SET_DEVICE_NAME_RSP = 'Device.SetNameRsp',
    SET_TIME_ZONE = 'Device.SetTimeZone',
    SET_TIME_ZONE_RSP = 'Device.SetTimeZoneRsp',
    GET_TIME_ZONE = 'Device.GetTimeZone',
    GET_TIME_ZONE_RSP = 'Device.DetectTimeZoneRsp',
    SET_NTP_SERVERS = 'Device.SetNtpServers',
    SET_NTP_SERVERS_RSP = 'Device.SetNtpServersRsp',
    SET_LANGUAGE = 'Device.SetLanguage',
    SET_LANGUAGE_RSP = 'Device.SetLanguageRsp',
    REBOOT_DEVICE = 'Device.RebootDevice',
    REBOOT_DEVICE_RSP = 'Device.RebootDeviceRsp',
    SET_BRIGHTNESS = 'Device.SetBrightness',
    SET_BRIGHTNESS_RSP = 'Device.SetBrightnessRsp',
    SET_IDLE_TIME = 'Device.SetScreenLock',
    SET_IDLE_TIME_RSP = 'Device.SetScreenLockRsp',
    //SET_IDENTIFY = 'Device.Identify',
    //SET_IDENTIFY_RSP = 'Device.IdentifyRsp',
    //SET_IEEE_ADDRESS = 'Device.SetIEEAddress',
    //SET_IEEE_ADDRESS_RSP = 'Device.SetIEEAddressRsp',

    //// ============= Device indicator =============
    //INDICATOR_NOTIFY = 'Device.IndicatorNotify',
    //SET_ALL_INDICATORS_OFF = 'Device.SetAllIndicatorsOff',
    //SET_ALL_INDICATORS_OFF_RSP = 'Device.SetAllIndicatorsOffRsp',
    //SET_NIGHT_MODE = 'Device.SetNightMode',
    //SET_NIGHT_MODE_RSP = 'Device.SetNightModeRsp',
    //SET_INDICATOR = 'Device.SetIndicator',
    //SET_INDICATOR_RSP = 'Device.SetIndicatorRsp',
    //PREVIEW_INDICATOR_EFFECT = 'Device.PreviewIndicatorEffect',
    //PREVIEW_INDICATOR_EFFECT_RSP = 'Device.PreviewIndicatorEffectRsp',

    //// ============= Notification =============
    //GET_NOTIFICATION_HISTORY = 'Device.GetNotificationHistory',
    //GET_NOTIFICATION_HISTORY_RSP = 'Device.GetNotificationHistoryRsp',
    //NOTIFICATION_NOTIFY = 'Device.NotificationNotify',
    //DELETE_ALL_NOTIFICATIONS = 'Device.ClearNotificationHistory',
    //DELETE_ALL_NOTIFICATIONS_RSP = 'Device.ClearNotificationHistoryRsp',

    //// ============= Debug log management =============
    SET_DEBUG_LOG = 'Device.SetDebugLog',
    SET_DEBUG_LOG_RSP = 'Device.SetDebugLogRsp',
    //DEBUG_LOG_NOTIFY = 'Device.DebugLogNotify',

    // ============= Device network management=============
    NETWORK_INFO_NOTIFY = 'Network.InfoNotify',
    SET_MDNS_DOMAIN = 'Network.SetMdnsDomain',
    SET_MDNS_DOMAIN_RSP = 'Network.SetMdnsDomainRsp',
    //SET_VPN = 'Network.SetVpn',
    //SET_VPN_RSP = 'Network.SetVpnRsp',
    //SET_ETHERNET_CONFIG = 'Network.SetEthernetConfig',
    //SET_ETHERNET_CONFIG_RSP = 'Network.SetEthernetConfigRsp',
    SET_WIFI_CONFIG = 'Network.SetWiFiConfig',
    SET_WIFI_CONFIG_RSP = 'Network.SetWiFiConfigRsp',
    SWITCH_WIFI = 'Network.SwitchWiFi',
    SWITCH_WIFI_RSP = 'Network.SwitchWiFiRsp',
    SET_AP_CONFIG = 'Network.SetApConfig',
    SET_AP_CONFIG_RSP = 'Network.SetApConfigRsp',
    SET_AP_SSID = 'Network.SetApSsid',
    SET_AP_PASSWORD = 'Network.SetApPassword',
    SCAN_WIFI = 'Network.ScanWiFi',
    SCAN_WIFI_RSP = 'Network.ScanWiFiRsp',
    WIFI_SCAN_RESULT_NOTIFY = 'Network.WiFiScanResultNotify',
    CONNECTION_STATUS_NOTIFY = 'Network.ConnectionStatusNotify',
    SCAN_HA_SERVER = 'Network.ScanHassInstance',

    // ============= Home Assistant authorization =============
    SCAN_HA_SERVER_RESP = 'Network.ScanHassInstanceRsp',
    SCAN_HA_SERVER_RESULT_NOTIFY = 'Network.HassInstanceScanResultNotify',
    HASS_START_OAUTH = 'Hass.StartOAuth',
    HASS_START_OAUTH_RESP = 'Hass.StartOAuthRsp',
    HASS_COMPLETE_OAUTH = 'Hass.CompleteOAuth',
    HASS_COMPLETE_OAUTH_RESP = 'Hass.CompleteOAuthRsp',
    HASS_AUTHORIZATION_RESULT_NOTIFY = 'Hass.AuthorizationResultNotify',
    HASS_GET_CONNECTION_INFO = 'Hass.GetConnectionInfo',
    HASS_GET_CONNECTION_INFO_RESP = 'Hass.GetConnectionInfoRsp',
    HASS_UPDATE_URL = 'Hass.UpdateUrl',
    HASS_UPDATE_URL_RESP = 'Hass.UpdateUrlRsp',
    HASS_DISABLE = 'Hass.Disable',
    HASS_DISABLE_RESP = 'Hass.DisableRsp',
    HASS_ACTIVATE = 'Hass.Activate',
    HASS_ACTIVATE_RESP = 'Hass.ActivateRsp',
    HASS_CONNECTION_STATUS_NOTIFY = 'Hass.ConnectionStatusNotify',

    // ============= Device network management=============
    SCREEN_SETTING_NOTIFY = 'Device.ScreenConfigNotify',
    SET_SCREEN_SETTING = 'Device.SetScreenConfig',
    SET_SCREEN_SETTING_RESP = 'Device.SetScreenConfigRsp',
}

export interface IResponse<T> {
    method: EWsMethods;
    ts?: number;
    seq?: number;
    data: T;
    /** TODO: reserver key, may delete in the end */
    chunk?: number;
    /** TODO: reserver key, may delete in the end */
    chunkTotal?: number;
}

export interface IResponseErr<T = void> {
    error: number;
    method: EWsMethods;
    data?: T;
}

export interface IPendingMsgFunc {
    resolve: (response: any) => void;
    reject: (error: any) => void;
    timer: ReturnType<typeof setTimeout>;
}

export interface IWsConnectRes {
    error: EWsErrorCode;
    msg?: string;
}

export enum EWsErrorCode {
    SUCCESS = 0,
    INIT_CONNECT_ERROR = 601,
    INIT_CONNECT_TIMEOUT = 602,
    WS_INSTANCE_NOT_EXIST = 603,
    SESSION_NOT_READY = 604,
}

export type TMethodParams = {
    [EWsMethods.AUTH_REQUIRED_NOTIFY]: IResponse<IAuthNotify>;
    [EWsMethods.FORGOT_PASSWORD_NOTIFY]: IResponse<IForgotPwdNotify>;
    [EWsMethods.DEVICE_INFO_NOTIFY]: IResponse<IDeviceInfoNotify>;
    [EWsMethods.FIRMWARE_UPDATE_STATUS_NOTIFY]: IResponse<IFirmwareUpdateStatusNotify>;
    [EWsMethods.LAN_DEVICE_NOTIFY]: IResponse<ILanDeviceNotify>;
    //[EWsMethods.INDICATOR_NOTIFY]: IResponse<IDeviceIndicatorNotify>;
    //[EWsMethods.NOTIFICATION_NOTIFY]: IResponse<INotificationNotify>;
    //[EWsMethods.DEBUG_LOG_NOTIFY]: IResponse<IDebugLogNotify>;
    //[EWsMethods.WEBHOOK_LIST_NOTIFY]: IResponse<IWebhookListNotify>;
    //[EWsMethods.REMOTE_INFO_NOTIFY]: IResponse<IRemoteInfoNotify>;
    [EWsMethods.NETWORK_INFO_NOTIFY]: IResponse<IWifiInfoNotify>;
    [EWsMethods.WIFI_SCAN_RESULT_NOTIFY]: IResponse<IWifiScanResultNotify>;
    [EWsMethods.SCAN_HA_SERVER_RESULT_NOTIFY]: IResponse<IHassScanResultNotify>;
    [EWsMethods.HASS_AUTHORIZATION_RESULT_NOTIFY]: IResponse<IHassAuthorizationResult>;
    [EWsMethods.HASS_CONNECTION_STATUS_NOTIFY]: IResponse<IHassConnectionStatusNotify>;
    //[EWsMethods.SECURITY_INFO_NOTIFY]: IResponse<ISecurityInfoNotify>;
    //[EWsMethods.MQTT_INFO_NOTIFY]: IResponse<IMqttInfoNotify>;
    //[EWsMethods.MG24_INFO_NOTIFY]: IResponse<IMg24InfoNotify>;
    //[EWsMethods.FIRMWARE_INFO_NOTIFY]: IResponse<IFirmwareInfoNotify>;
    //[EWsMethods.CHANNEL_HEALTH_NOTIFY]: IResponse<IChannelHealthNotify>;
    //[EWsMethods.SET_LOGIN_STATE_RSP]: IResponseErr<never>;
    //// [EWsMethods.SET_FORGOT_PASSWORD_STATE_RSP]: IResponseErr<never>,
    //// [EWsMethods.SET_PASSWORD_RSP]: IResponseErr<never>,
    //[EWsMethods.UPLOAD_FLASH_NOTIFY]: IResponse<IFlashNotify>;
    [EWsMethods.CONNECTION_STATUS_NOTIFY]: IResponse<IConnectionStatusNotify>;
    [EWsMethods.SCREEN_SETTING_NOTIFY]: IResponse<IHassScreenConfigNotify>;
};
