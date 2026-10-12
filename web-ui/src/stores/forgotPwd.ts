import { cloneDeep, set } from 'lodash-es';
import { writable } from 'svelte/store';
import type { IForgotPwdNotify } from '@/types/auth';

interface IForgotPwd extends IForgotPwdNotify {}

const INIT_VALUE: IForgotPwd = {
    state: 'done',
};

export const forgotPwdStore = writable<IForgotPwd>(cloneDeep(INIT_VALUE));
export const forgotPwdNotifyStore = writable<IForgotPwdNotify | null>(null);

export function updatePwdField(field: keyof IForgotPwd, value: any) {
    forgotPwdStore.update((item) => {
        const cloneItem = cloneDeep(item);
        set(cloneItem, field, value);
        return cloneItem;
    });
}

export function notifyForgotPwd(params: IForgotPwdNotify) {
    forgotPwdNotifyStore.set(cloneDeep(params));
}
