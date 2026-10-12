import { notifyForgotPwd, updatePwdField } from '@/stores/forgotPwd';
import type { IForgotPwdNotify } from '@/types/auth';
import type { IResponse } from '@/types/websocket';

export async function handleForgotPwdNotify(params: IResponse<IForgotPwdNotify>) {
    console.log('forgot pwd notify coming message', params);
    updatePwdField('state', params.data.state);
    notifyForgotPwd(params.data);
}
