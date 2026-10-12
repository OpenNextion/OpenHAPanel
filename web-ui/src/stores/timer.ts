import { derived, get, writable } from 'svelte/store';
import type ETimerType from '@/types/timer';
import type { TimerInfo, Updater } from '@/types/timer';
import { deviceStore } from '@/stores/device';
import { get as lodashGet } from 'lodash-es';

interface TimerStore {
    timers: Record<string, TimerInfo>;
    timer: ReturnType<typeof setInterval> | null;
    routerTimer: ReturnType<typeof setInterval> | null;
}

export const timerStore = writable<TimerStore>({
    timers: {},
    timer: null,
    routerTimer: null,
});

export function registerTimer(field: string, initialValue: number, updater: Updater) {
    const startTimestamp = Date.now();
    const store = get(timerStore);
    // 检查是否需要启动定时器
    if (!store.timer) {
        syncTime();
    }
    timerStore.update((s) => {
        const newStore = {
            ...s,
            timers: {
                ...s.timers,
                [field]: { value: initialValue, updater, startTimestamp, initialValue },
            },
        };
        if (!s.timer) syncTime();
        return newStore;
    });
}

export function offTimer(field: ETimerType) {
    timerStore.update((store) => {
        if (store.timers[field]) {
            delete store.timers[field];
        }
        return store;
    });
}

export function syncTime() {
    timerStore.update((s) => {
        if (s.timer) {
            clearInterval(s.timer);
            s.timer = null;
        }
        const timer = setInterval(() => {
            timerStore.update((state) => {
                const newTimers = { ...state.timers };
                for (const key in newTimers) {
                    const elapsed = Math.floor((Date.now() - newTimers[key].startTimestamp) / 1000);
                    newTimers[key].value = newTimers[key].updater(newTimers[key].initialValue, elapsed);
                }
                return { ...state, timers: newTimers };
            });
        }, 1000);
        return { ...s, timer };
    });
}

export const finialOffset = derived(timerStore, ($timerStore) => {
    const { utcOffset, nextUtcOffset, nextDstShift, hasDst } = get(deviceStore).tzInfo;
    const time = lodashGet($timerStore, ['timers', 'currentTime', 'value'], '');
    if (!hasDst) return utcOffset / 60;
    const offset = time >= nextDstShift ? nextUtcOffset / 60 : utcOffset / 60;
    return offset;
});

export const updateRouterTimer = (timerId: ReturnType<typeof setInterval> | null) => {
    timerStore.update((store) => {
        if (store.routerTimer) {
            clearInterval(store.routerTimer);
        }
        return { ...store, routerTimer: timerId };
    });
};
