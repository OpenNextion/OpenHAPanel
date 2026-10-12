import type { NtpServers, TimeZone } from '@/types/device';
import { cloneDeep, isEqual, merge } from 'lodash-es';
import { router } from 'svelte-spa-router';
interface ITime {
    tzInfo: TimeZone;
    ntpServers: NtpServers;
}

const INIT_VALUE: ITime = {
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
};

let timeInfo = $state<ITime>(cloneDeep(INIT_VALUE));

const handleLocationChange = () => {
    // Keep the draft while navigating between time-setting child routes.
    queueMicrotask(() => {
        if (router.location.startsWith('/settings/time')) return;
        initHooks();
    });
};

window.addEventListener('hashchange', handleLocationChange);

function setNtpServers(data: NtpServers) {
    timeInfo.ntpServers = cloneDeep(data);
}

function setTzInfo(tzInfo: TimeZone) {
    timeInfo.tzInfo = cloneDeep(tzInfo);
}

function updateNtpServers(value: Partial<NtpServers>) {
    merge(timeInfo.ntpServers, value);
}

function initHooks() {
    timeInfo.tzInfo = cloneDeep(INIT_VALUE.tzInfo);
    timeInfo.ntpServers = cloneDeep(INIT_VALUE.ntpServers);
}

function isEmptyHook() {
    return isEqual(timeInfo, INIT_VALUE);
}

export function useNtpServersHooks() {
    return {
        timeInfo,
        setNtpServers,
        updateNtpServers,
        initHooks,
        isEmptyHook,
        setTzInfo,
    };
}
