import { cloneDeep, merge, omit, set } from 'lodash-es';
import { get, writable, derived } from 'svelte/store';
import type { IDeviceInfoNotify, IThemeModeNotify } from '@/types/device';
import EThemeMode from '@/types/EThemeMode';
import sendForReply from '@/ws/sendForReply';
import { ERROR_CODE } from '@/constant';
import { EWsMethods } from '@/types/websocket';

interface IDevice extends IDeviceInfoNotify, IThemeModeNotify {
    debugLogEnabled: boolean;
}

type TBootstrapTheme = 'light' | 'dark';

const darkModeMediaQuery = window.matchMedia('(prefers-color-scheme: dark)');
const systemPrefersDark = darkModeMediaQuery.matches;
const cachedTheme = localStorage.getItem('theme');
const cachedFollowSystemValue = localStorage.getItem('themeFollowSystem');
const cachedFollowSystem = cachedFollowSystemValue === null ? true : cachedFollowSystemValue === 'true';
const initialBootstrapTheme: TBootstrapTheme = cachedFollowSystem
    ? systemPrefersDark
        ? 'dark'
        : 'light'
    : cachedTheme === 'dark' || cachedTheme === 'light'
      ? cachedTheme
      : systemPrefersDark
        ? 'dark'
        : 'light';

const INIT_VALUE: IDevice = {
    name: '',
    id: '',
    bootTimestamp: 0,
    currentTime: 0,
    tzInfo: {
        timeZone: '',
        utcOffset: 0,
        isDst: false,
        hasDst: false,
        nextDstShift: null,
        nextUtcOffset: null,
    },
    ntpServers: {
        server1: '',
        server2: '',
    },
    turboMode: false,
    language: 'en',
    currentMode: initialBootstrapTheme === 'dark' ? EThemeMode.DARK : EThemeMode.NORMAL,
    followSystem: cachedFollowSystem,
    debugLogEnabled: false,
    brightness: 50,
    idleTime: 300,
};

export const deviceStore = writable<IDevice>(cloneDeep(INIT_VALUE), () => {
    // init timezone
    const tz = 0 - new Date().getTimezoneOffset();
    const utcOffset = tz * 60;
    INIT_VALUE.tzInfo.utcOffset = utcOffset;
    deviceStore.set(INIT_VALUE);
});

export function updateDeviceField(field: keyof IDevice, value: any) {
    deviceStore.update((item) => {
        const cloneItem = cloneDeep(item);
        set(cloneItem, field, value);
        return cloneItem;
    });
}

export function updateDeviceInfoOrThemeMode(value: Partial<IDeviceInfoNotify | IThemeModeNotify>) {
    deviceStore.update((item) => {
        const cloneItem = cloneDeep(item);
        return merge(cloneItem, value);
    });
}

export function updateDeviceInfo(value: IDeviceInfoNotify) {
    const localThemeFields = ['currentMode', 'followSystem'];
    deviceStore.update((item) => merge(cloneDeep(item), omit(value, localThemeFields)));
}

/** get Bootstrap theme mode */
export function getBootstrapMode() {
    return document.documentElement.getAttribute('data-bs-theme') as unknown as 'light' | 'dark';
}

//export async function updateTurboMode(enable: boolean) {
//    const response = await sendForReply({
//        method: EWsMethods.SET_TURBO_MODE,
//        data: { enable },
//    });
//    console.log('updateTurboMode response: ', response);
//    return response;
//}

export async function updateTheme(data: { currentMode: EThemeMode; followSystem: boolean }) {
    const currentMode = data.followSystem ? (darkModeMediaQuery.matches ? EThemeMode.DARK : EThemeMode.NORMAL) : data.currentMode;
    updateDeviceInfoOrThemeMode({ currentMode, followSystem: data.followSystem });
    return { error: ERROR_CODE.SUCCESS };
}

export async function updateDeviceName(name: string) {
    const result = await sendForReply<{ name: string }>({
        method: EWsMethods.SET_DEVICE_NAME,
        data: { name },
    });
    if (result.error === ERROR_CODE.SUCCESS) updateDeviceField('name', name);
    return result;
}

export async function updateDeviceLanguage(language: string) {
    const result = await sendForReply<{ language: string }>({
        method: EWsMethods.SET_LANGUAGE,
        data: { language },
    });
    if (result.error === ERROR_CODE.SUCCESS) updateDeviceField('language', language);
    return result;
}

export async function updateDebugLog(enable: boolean) {
    const result = await sendForReply<{ enable: boolean }>({
        method: EWsMethods.SET_DEBUG_LOG,
        data: { enable },
    });
    if (result.error === ERROR_CODE.SUCCESS) updateDeviceField('debugLogEnabled', enable);
    return result;
}

//export async function setIEEE(customAddress?: string) {
//    const data = {};
//    if (customAddress) set(data, 'customAddress', customAddress);
//    const result = await sendForReply({ method: EWsMethods.SET_IEEE_ADDRESS, data });
//    if (result.error === EWsErrorCode.SUCCESS) {
//        deviceStore.update((store) => {
//            if (customAddress) {
//                store.IEEEAddress.customAddress = customAddress;
//            } else {
//                delete store.IEEEAddress.customAddress;
//            }
//            return store;
//        });
//    }
//    return result;
//}

const themeInfo = derived(deviceStore, (store) => {
    return {
        currentMode: store.currentMode,
        followSystem: store.followSystem,
    };
});

const setDataBsTheme = (theme: TBootstrapTheme) => {
    if (window.document.documentElement.getAttribute('data-bs-theme') !== theme) {
        window.document.documentElement.setAttribute('data-bs-theme', theme);
    }
    localStorage.setItem('theme', theme);
    document.querySelector('meta[name="theme-color"]')?.setAttribute('content', theme === 'dark' ? '#111827' : '#f4f5f9');
};

themeInfo.subscribe((store) => {
    const { followSystem, currentMode } = store;
    localStorage.setItem('themeFollowSystem', String(followSystem));
    if (followSystem) {
        const systemMode = darkModeMediaQuery.matches ? EThemeMode.DARK : EThemeMode.NORMAL;
        if (currentMode !== systemMode) {
            updateDeviceField('currentMode', systemMode);
            return;
        }
        setDataBsTheme(systemMode === EThemeMode.DARK ? 'dark' : 'light');
        return;
    }
    setDataBsTheme(currentMode === EThemeMode.DARK ? 'dark' : 'light');
});

const syncSystemTheme = (matches = darkModeMediaQuery.matches) => {
    const { followSystem } = get(deviceStore);
    if (!followSystem) return;
    updateDeviceInfoOrThemeMode({
        currentMode: matches ? EThemeMode.DARK : EThemeMode.NORMAL,
    });
};

const handleDarkModeChange = (event: MediaQueryListEvent) => syncSystemTheme(event.matches);
darkModeMediaQuery.addEventListener('change', handleDarkModeChange);

window.addEventListener('focus', () => syncSystemTheme());
document.addEventListener('visibilitychange', () => {
    if (document.visibilityState === 'visible') syncSystemTheme();
});
