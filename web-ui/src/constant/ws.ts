import { ENV_MODE } from '@/constant';
import { type TMethodParams } from '@/types/websocket';
import { EWsMethods } from '@/types/websocket';
import Env from '@/types/Env';
import { handleAuthRequiredNotify } from '@/ws/handler/authHandler';
import { handleConnectionStatusNotify, handleNetworkInfoNotify, handleWifiScanResult } from '@/ws/handler/network';
import { handleDeviceInfoNotify } from '@/ws/handler/device';
import { handleHassAuthorizationResultNotify, handleHassConnectionStatusNotify, handleScanHassInstanceNotify } from '@/ws/handler/homeassistant';
import { handleScreenConfigNotify } from '@/ws/handler/screenConfig';
import { handleForgotPwdNotify } from '@/ws/handler/forgotPwd';
import { handleFirmwareUpdateStatusNotify } from '@/ws/handler/firmware';
import { handleLanDeviceNotify } from '@/ws/handler/lanDevice';

const wsProtocol = window.location.protocol === 'https:' ? 'wss://' : 'ws://';

/** initiate address  */
export const WS_ADDRESS =
    ENV_MODE === Env.PROD
        ? `${wsProtocol}${window.location.host}/api/ws`
        : `${wsProtocol}${ENV_MODE === Env.STAGING ? window.location.hostname : import.meta.env.VITE_WS_ADDRESS}/api/ws`;

type IMethod2Func = { [K in keyof TMethodParams]: Service<K> };
type Service<K extends keyof TMethodParams> = (params: TMethodParams[K]) => Promise<void>;

export const METHOD2FUNC: IMethod2Func = {
    [EWsMethods.AUTH_REQUIRED_NOTIFY]: handleAuthRequiredNotify,
    [EWsMethods.FORGOT_PASSWORD_NOTIFY]: handleForgotPwdNotify,
    [EWsMethods.DEVICE_INFO_NOTIFY]: handleDeviceInfoNotify,
    [EWsMethods.FIRMWARE_UPDATE_STATUS_NOTIFY]: handleFirmwareUpdateStatusNotify,
    [EWsMethods.LAN_DEVICE_NOTIFY]: handleLanDeviceNotify,
    //[EWsMethods.INDICATOR_NOTIFY]: handleIndicatorNotify,
    //[EWsMethods.NOTIFICATION_NOTIFY]: handleNotificationNotify,
    //[EWsMethods.DEBUG_LOG_NOTIFY]: handleDebugLogNotify,
    //[EWsMethods.WEBHOOK_LIST_NOTIFY]: handleWebhookListNotify,
    //[EWsMethods.REMOTE_INFO_NOTIFY]: handleRemoteNotify,
    [EWsMethods.NETWORK_INFO_NOTIFY]: handleNetworkInfoNotify,
    [EWsMethods.WIFI_SCAN_RESULT_NOTIFY]: handleWifiScanResult,
    [EWsMethods.SCAN_HA_SERVER_RESULT_NOTIFY]: handleScanHassInstanceNotify,
    [EWsMethods.HASS_AUTHORIZATION_RESULT_NOTIFY]: handleHassAuthorizationResultNotify,
    [EWsMethods.HASS_CONNECTION_STATUS_NOTIFY]: handleHassConnectionStatusNotify,
    //[EWsMethods.SECURITY_INFO_NOTIFY]: handleSecurityInfoNotify,
    //[EWsMethods.MQTT_INFO_NOTIFY]: handleMqttNotify,
    //[EWsMethods.MG24_INFO_NOTIFY]: handleMg24InfoNotify,
    //[EWsMethods.FIRMWARE_INFO_NOTIFY]: handleFirmwareInfoNotify,
    //[EWsMethods.CHANNEL_HEALTH_NOTIFY]: handleChannelHealthNotify,
    //[EWsMethods.SET_LOGIN_STATE_RSP]: sendAfterVerify,
    // [EWsMethods.SET_FORGOT_PASSWORD_STATE_RSP]: sendAfterVerify,
    // [EWsMethods.SET_PASSWORD_RSP]: sendAfterVerify,
    //[EWsMethods.UPLOAD_FLASH_NOTIFY]: handleFlashNotify,
    [EWsMethods.CONNECTION_STATUS_NOTIFY]: handleConnectionStatusNotify,
    [EWsMethods.SCREEN_SETTING_NOTIFY]: handleScreenConfigNotify,
};
