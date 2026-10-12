import { updateDeviceInfo } from '@/stores/device';
//import { esp32Version } from '@/stores/firmware';
import { applyDeviceLanguage } from '@/stores/language';
import { registerTimer } from '@/stores/timer';
import ETimerType from '@/types/timer';
import type { IDeviceInfoNotify } from '@/types/device';
import type { IResponse } from '@/types/websocket';
import { get, has } from 'lodash-es';
import { get as getSvelteStoreValue } from 'svelte/store';

export async function handleDeviceInfoNotify(params: IResponse<IDeviceInfoNotify>) {
    console.log('device info notify coming message', params);
    updateDeviceInfo(params.data);
    if (has(params.data, 'language')) {
        applyDeviceLanguage(params.data.language);
    }

    //const esp32CurrentVersion = get(params.data, ['esp32', 'currentVersion']);
    //if (esp32CurrentVersion) {
    //    const esp32VersionValue = getSvelteStoreValue(esp32Version);
    //    if (!esp32VersionValue || (esp32VersionValue && esp32VersionValue !== esp32CurrentVersion)) {
    //        esp32Version.set(esp32CurrentVersion);
    //        globalThis.location.reload();
    //    }
    //}

    //const bootTs = get(params.data, ['bootTimestamp']);
    //if (bootTs) {
    //    registerTimer(ETimerType.BOOT_TIME, bootTs, (initial, elapsed) => {
    //        return initial + elapsed;
    //    });
    //}

    const currentTime = get(params.data, ['currentTime']);
    if (currentTime) {
        registerTimer(ETimerType.CURRENT_TIME, currentTime, (initial, elapsed) => {
            return initial + elapsed * 1000;
        });
    }
}
