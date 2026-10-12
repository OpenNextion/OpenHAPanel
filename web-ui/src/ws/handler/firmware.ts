import { updateFirmwareStatus } from '@/stores/firmwareUpdate';
import type { IFirmwareUpdateStatusNotify } from '@/types/firmware';
import type { IResponse } from '@/types/websocket';
import { getWebsocket } from '@/ws/initWebsocket';
import { wsReconnectManager } from '@/ws/wsReconnect';

let rebootTimer: ReturnType<typeof setTimeout> | null = null;

export async function handleFirmwareUpdateStatusNotify(params: IResponse<IFirmwareUpdateStatusNotify>) {
    updateFirmwareStatus(params.data);

    if (params.data.state !== 'rebooting') return;
    if (rebootTimer) clearTimeout(rebootTimer);
    rebootTimer = setTimeout(() => {
        rebootTimer = null;
        getWebsocket()?.disconnect();
        wsReconnectManager.start();
    }, 2000);
}
