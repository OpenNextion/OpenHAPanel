import { cloneDeep, set } from 'lodash-es';
import { derived, writable } from 'svelte/store';
import type { ILanDeviceNotify } from '@/types/lanDevice';

interface ILanDevice extends ILanDeviceNotify {}

const INIT_VALUE: ILanDevice = {
    devices: [],
};

export const lanDeviceStore = writable<ILanDevice>(cloneDeep(INIT_VALUE));
export const currentLanDevice = derived(lanDeviceStore, ({ devices }) => {
    if (devices.length === 1) return devices[0];
    if (devices.length === 0) return undefined;

    const normalizeHostname = (value: string) => value.trim().toLowerCase().replace(/^\[|\]$/g, '').replace(/\.$/, '');
    const currentHostname = normalizeHostname(window.location.hostname);
    return devices.find(
        ({ ip, mdnsDomain }) => normalizeHostname(ip) === currentHostname || normalizeHostname(mdnsDomain) === currentHostname,
    );
});

export function updateLanDeviceField(field: keyof ILanDevice, value: any) {
    lanDeviceStore.update((item) => {
        const cloneItem = cloneDeep(item);
        set(cloneItem, field, value);
        return cloneItem;
    });
}
