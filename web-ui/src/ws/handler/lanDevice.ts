import { updateLanDeviceField } from '@/stores/lanDevice';
import type { ILanDeviceNotify } from '@/types/lanDevice';
import type { IResponse } from '@/types/websocket';

export async function handleLanDeviceNotify(params: IResponse<ILanDeviceNotify>) {
    console.log('lan device notify coming message', params);
    updateLanDeviceField('devices', params.data.devices);
}
