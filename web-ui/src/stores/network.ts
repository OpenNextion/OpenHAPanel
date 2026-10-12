import { cloneDeep, merge, set, get as lodashGet } from 'lodash-es';
import { writable, get, derived } from 'svelte/store';
//import type { Config, IWifiInfoNotify, IWifiScanResultNotify, Result } from '@/types/network';
//import type { ISecurityInfoNotify } from '../types/interface/methods/security';
import sendForReply from '@/ws/sendForReply';
import { EWsMethods } from '@/types/websocket';
import { encryptData } from '../utils/encryptContent';
import { ENetworkMode, type IWifiInfoNotify, type IWifiScanResultNotify, type Result, type Wifi } from '@/types/network';
import { t } from 'svelte-i18n';
import { EMessageType, sendMessage } from './message';
import { ERROR_CODE } from '@/constant';

interface INetwork extends IWifiInfoNotify, IWifiScanResultNotify {
    //security: ISecurityInfoNotify;
    scanWifiLoading: boolean;
    connectWifiLoading: boolean;
    infoReady: boolean;
}

const INIT_VALUE: INetwork = {
    networkMode: [],
    internetConnected: false,
    mdnsDomain: '',
    //vpnStatus: {
    //    connected: false,
    //    enabled: false,
    //    config: {
    //        localIp: '',
    //        privateKey: '',
    //        publicKey: '',
    //        endpointPort: '0',
    //        endpoint: '',
    //    },
    //},
    results: [],
    //security: {
    //    ipWhitelistEnabled: false,
    //    ipWhitelist: [],
    //},
    scanWifiLoading: false,
    connectWifiLoading: false,
    infoReady: false,
};

export const networkStore = writable<INetwork>(cloneDeep(INIT_VALUE));

export type WifiIpDraft = Pick<Wifi, 'ipAddress' | 'subnetMask' | 'gateway' | 'dns1' | 'dns2'>;

const EMPTY_WIFI_IP_DRAFT: WifiIpDraft = {
    ipAddress: '',
    subnetMask: '',
    gateway: '',
    dns1: '',
    dns2: '',
};

export const wifiIpDraft = writable<WifiIpDraft>(cloneDeep(EMPTY_WIFI_IP_DRAFT));
let wifiIpDraftInitialized = false;

export function initializeWifiIpDraft(wifiConfig?: Wifi, force = false) {
    if (wifiIpDraftInitialized && !force) return;
    wifiIpDraft.set({
        ipAddress: wifiConfig?.ipAddress ?? '',
        subnetMask: wifiConfig?.subnetMask ?? '',
        gateway: wifiConfig?.gateway ?? '',
        dns1: wifiConfig?.dns1 ?? '',
        dns2: wifiConfig?.dns2 ?? '',
    });
    wifiIpDraftInitialized = true;
}

export function updateWifiIpDraft(field: keyof WifiIpDraft, value: string) {
    wifiIpDraft.update((draft) => ({ ...draft, [field]: value }));
}

export const networkModeText = derived(networkStore, (store) => {
    const NETWORK_MODE_TEXT_MAPPING = {
        [ENetworkMode.AP]: 'OVERVIEW.NETWORK_MODE.AP',
        [ENetworkMode.WIFI]: 'OVERVIEW.NETWORK_MODE.WIFI',
        //[ENetworkMode.ETHERNET]: 'OVERVIEW.NETWORK_MODE.ETHERNET',
    };
    const getT = get(t);
    return store.networkMode
        .map((mode) => {
            if (mode === ENetworkMode.WIFI) {
                return `${getT(NETWORK_MODE_TEXT_MAPPING[mode])}`;
            }
            return getT(NETWORK_MODE_TEXT_MAPPING[mode]);
        })
        .join('+');
});

//export const ethernet = derived(networkStore, (store) => store.ethernet!);

export const wifi = derived(networkStore, (store) => store.wifi!);

/** scan wifi timeout */
const scanWifiLoading = derived(networkStore, (store) => store.scanWifiLoading);
let timer: ReturnType<typeof setTimeout> | null = null;
scanWifiLoading.subscribe((value) => {
    if (value) {
        if (timer) {
            clearTimeout(timer);
            timer = null;
        }
        timer = setTimeout(() => {
            updateNetworkField('scanWifiLoading', false);
            timer = null;
        }, 30 * 1000);
    }
});

export function updateNetworkField(field: keyof INetwork, value: any) {
    networkStore.update((item) => {
        const cloneItem = cloneDeep(item);
        set(cloneItem, field, value);
        return cloneItem;
    });
}

export function updateNetworkByKeypath(keypath: string | string[], value: any) {
    networkStore.update((store) => {
        const _store = cloneDeep(store);
        set(_store, keypath, value);
        return _store;
    });
}

export function updateNetworkForWifiOrScanResult(value: IWifiInfoNotify | IWifiScanResultNotify) {
    networkStore.update((item) => {
        const cloneItem = cloneDeep(item);
        if (lodashGet(value, 'results', null)) cloneItem.results = [];
        const networkMode = lodashGet(value, 'networkMode', null);
        if (networkMode) cloneItem.networkMode = networkMode;
        return merge(cloneItem, value);
    });
}

export const wifiSetupRequired = derived(networkStore, (store) => store.infoReady && !store.wifi?.ssid?.trim());

export type WifiConnectionErrorKey = 'networkPage.wifiCredentialsIncorrect' | 'networkPage.wifiConnectionFailed';

export function getWifiConnectionErrorKey(error?: number | null): WifiConnectionErrorKey | null {
    if (error === ERROR_CODE.SUCCESS) return null;
    if (error === ERROR_CODE.WIFI_AUTH_FAILED) return 'networkPage.wifiCredentialsIncorrect';
    return 'networkPage.wifiConnectionFailed';
}

/** ---------- Ap ------------------ */
export async function setApConfig(params: Partial<IWifiInfoNotify['ap']>) {
    if (!params) {
        return {
            method: EWsMethods.SET_AP_CONFIG,
            error: -1,
            msg: 'params must not empty!',
        };
    }
    const response = await sendForReply({
        method: EWsMethods.SET_AP_CONFIG,
        data: params,
    });
    console.log('set Ap Config: ', response, params);
    if (response.error === 0) {
        Object.entries(params).forEach(([key, value]) => {
            updateNetworkByKeypath(`ap.${key}`, value);
        });
    }
    return response;
}

export async function updateApSsid(ssid: string) {
    const response = await sendForReply({
        method: EWsMethods.SET_AP_SSID,
        data: { ssid },
    });
    console.log('updateApSsid response: ', response);
    if (response.error === 0) updateNetworkByKeypath('ap.apName', ssid);
    return response;
}

export async function updateApPassword(password: string) {
    const { iv, content } = encryptData(JSON.stringify({ password }));
    const response = await sendForReply({
        method: EWsMethods.SET_AP_PASSWORD,
        data: { iv, content },
    });
    console.log('updateApPassword response: ', response);
    if (response.error === 0) updateNetworkByKeypath('ap.password', password);
    return response;
}

///** ------------- Ethernet ----------- */
//export async function setEthernetConfig(params: Partial<IWifiInfoNotify['ethernet']>) {
//    if (!params) {
//        return {
//            method: EWsMethods.SET_ETHERNET_CONFIG,
//            error: -1,
//            msg: 'params must not empty!',
//        };
//    }
//    const response = await sendForReply({
//        method: EWsMethods.SET_ETHERNET_CONFIG,
//        data: params,
//    });
//    console.log('set Ethernet Config: ', response, params);
//    if (response.error === 0) {
//        Object.entries(params).forEach(([key, value]) => {
//            updateNetworkByKeypath(`ethernet.${key}`, value);
//        });
//    }
//    return response;
//}

/** ------------- WLAN --------------- */
export async function scanWifi(duration: number) {
    updateNetworkField('scanWifiLoading', true);
    const response = await sendForReply({
        method: EWsMethods.SCAN_WIFI,
        data: { duration },
    });
    if (response.error !== 0) {
        updateNetworkField('scanWifiLoading', false);
        const getT = get(t);
        const msgMap = {
            [ERROR_CODE.SCAN_IN_PROGRESS]: getT('TIPS.SCAN_IN_PROGRESS'),
        };
        const msg = msgMap[response.error] || getT('TIPS.OPERATION_FAILED');
        if (msg) sendMessage(EMessageType.ERROR, msg);
    }
    console.log('scanWifi response: ', response);
    return response;
}

export async function switchWifi(wifi: Result, password: string, keepLoadingOnSuccess = false) {
    try {
        updateNetworkField('connectWifiLoading', true);
        const { ssid, bssid } = wifi;
        const { iv, content } = encryptData(JSON.stringify({ ssid, password, bssid: bssid || '00:00:00:00:00:00' }));
        const response = await sendForReply<{ iv: string; content: string }, { handoffTicket?: string; expiresIn?: number }>({
            method: EWsMethods.SWITCH_WIFI,
            data: { iv, content },
        });
        if (response.error === 0) {
            updateNetworkByKeypath('wifi.ssid', ssid);
        }
        if (response.error !== 0 || !keepLoadingOnSuccess) {
            updateNetworkField('connectWifiLoading', false);
        }
        return response;
    } catch (error) {
        updateNetworkField('connectWifiLoading', false);
        console.error('switchWifi error: ', error);
        return null;
    }
}

export async function setWifiConfig(params: Partial<IWifiInfoNotify['wifi']>) {
    if (!params) {
        return {
            method: EWsMethods.SET_WIFI_CONFIG,
            error: -1,
            msg: 'params must not empty!',
        };
    }
    const response = await sendForReply({
        method: EWsMethods.SET_WIFI_CONFIG,
        data: params,
    });
    console.log('setWifiConfig response: ', response, 'params: ', params);
    if (response.error === 0) {
        Object.entries(params).forEach(([key, value]) => {
            updateNetworkByKeypath(`wifi.${key}`, value);
        });
    }
    return response;
}

export async function updateMdnsDomain(mdnsDomain: string) {
    const response = await sendForReply<{ mdnsDomain: string }>({
        method: EWsMethods.SET_MDNS_DOMAIN,
        data: { mdnsDomain },
    });
    if (response.error === ERROR_CODE.SUCCESS) updateNetworkField('mdnsDomain', mdnsDomain);
    return response;
}

///** vpn */
//export async function setVpn(data: { enabled: boolean; config: Config }) {
//    const response = await sendForReply({ method: EWsMethods.SET_VPN, data });
//    console.log('setVpn response: ', response);
//    if (response.error === 0) {
//        const { enabled, config } = data;
//        updateNetworkByKeypath('vpnStatus.enabled', enabled);
//        Object.entries(config).forEach(([key, value]) => {
//            updateNetworkByKeypath(`vpnStatus.config.${key}`, value);
//        });
//    }
//    return response;
//}
