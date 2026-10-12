import { cloneDeep } from 'lodash-es';
import { writable } from 'svelte/store';

interface IMessage {
    showMessage: boolean;
    type: EMessageType;
    message: string;
}

export enum EMessageType {
    SUCCESS = 'success',
    WARN = 'warn',
    ERROR = 'error',
}

const INIT_VALUE: IMessage = {
    showMessage: false,
    type: EMessageType.SUCCESS,
    message: '',
};

export const messageStore = writable<IMessage>(cloneDeep(INIT_VALUE));
let timer: NodeJS.Timeout | null = null;

export function closeMessage() {
    messageStore.set(cloneDeep(INIT_VALUE));
    if (timer) clearTimeout(timer);
}

// update message
export function sendMessage(type = EMessageType.SUCCESS, message: string) {
    if (timer) clearTimeout(timer);
    messageStore.set({ showMessage: true, type, message });
    timer = setTimeout(() => {
        closeMessage();
    }, 3 * 1000);
}
