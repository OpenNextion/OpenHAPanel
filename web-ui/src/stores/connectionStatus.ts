import { writable, derived } from 'svelte/store';
import type { IConnectionStatusNotify } from '@/types/network';
import { cloneDeep, set } from 'lodash-es';

const INIT_VALUE: IConnectionStatusNotify = {
    connectWithAp: false,
};

export const connectionStatusStore = writable(cloneDeep(INIT_VALUE));

export const connectWithAp = derived(connectionStatusStore, (store) => store.connectWithAp);

export const update = (keypath: string, value: unknown) => {
    connectionStatusStore.update((store) => {
        set(store, keypath, value);
        return store;
    });
};
