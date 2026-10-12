import { writable } from 'svelte/store';
import type { IFirmwareUpdateStatusNotify } from '@/types/firmware';

const FIRMWARE_OPERATION_BLOCKED_KEY = 'firmwareOperationBlocked';

interface IFirmwareUpdateState extends Partial<IFirmwareUpdateStatusNotify> {
    operationBlocked: boolean;
}

const persistedOperationBlocked = localStorage.getItem(FIRMWARE_OPERATION_BLOCKED_KEY) === 'true';

export const firmwareUpdateStore = writable<IFirmwareUpdateState>({ operationBlocked: persistedOperationBlocked });

export function updateFirmwareStatus(status: IFirmwareUpdateStatusNotify) {
    localStorage.setItem(FIRMWARE_OPERATION_BLOCKED_KEY, String(status.operationBlocked));
    firmwareUpdateStore.set(status);
}

export function setFirmwareOperationBlocked(operationBlocked: boolean) {
    localStorage.setItem(FIRMWARE_OPERATION_BLOCKED_KEY, String(operationBlocked));
    firmwareUpdateStore.update((state) => ({ ...state, operationBlocked }));
}
