import { cloneDeep } from 'lodash-es';
import { writable } from 'svelte/store';
import type { IHassConnectionInfo, IHomeassistantStore } from '@/types/homeassistant';
import type { IHassOAuthCallback } from '@/types/homeassistant';

const INIT_VALUE: IHomeassistantStore = {
    hassList: [],
    scanning: false,
    connected: false,
    connectionInfo: null,
    authorizationResult: null,
    oauthCompleting: false,
};

export const pendingHassOAuthCallback = writable<IHassOAuthCallback | null>(null);

export function captureHassOAuthCallback() {
    const params = new URLSearchParams(window.location.search);
    const code = params.get('code');
    const state = params.get('state');
    if (!code || !state) return false;

    pendingHassOAuthCallback.set({ code, state });
    window.history.replaceState({}, document.title, window.location.pathname);
    return true;
}

export const homeAssistantStore = writable(cloneDeep(INIT_VALUE));

// update local state
export const updateHomeassistantField = (field: keyof IHomeassistantStore, value: any) => {
    homeAssistantStore.update((item) => {
        const cloneItem = cloneDeep(item);
        return { ...cloneItem, [field]: cloneDeep(value) };
    });
};

export function updateHassConnectionInfo(connectionInfo: Partial<IHassConnectionInfo>) {
    homeAssistantStore.update((item) => {
        const nextConnectionInfo = { ...item.connectionInfo, ...connectionInfo } as IHassConnectionInfo;
        return {
            ...item,
            connected: Boolean(nextConnectionInfo.configured && nextConnectionInfo.enabled),
            connectionInfo: cloneDeep(nextConnectionInfo),
        };
    });
}
