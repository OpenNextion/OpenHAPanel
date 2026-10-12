import type { IResponse, IResponseErr } from '@/types/websocket';
import { EWsErrorCode, EWsMethods } from '@/types/websocket';
import { get } from 'svelte/store';
import { authStore, getLoginSession } from '@/stores/auth';
import { getWebsocket } from './initWebsocket';

const SESSION_READY_TIMEOUT = 30 * 1000;

/**
 * A persisted session is treated as authenticated only after ResumeSession
 * succeeds. This gate keeps all other requests behind that handshake.
 */
function waitForSessionReady(method: EWsMethods): Promise<boolean> {
    if (
        method === EWsMethods.RESUME_SESSION ||
        method === EWsMethods.EXCHANGE_PROVISIONING_HANDOFF ||
        !getLoginSession() ||
        get(authStore).sessionReady
    ) {
        return Promise.resolve(true);
    }

    return new Promise((resolve) => {
        let settled = false;
        let unsubscribe: (() => void) | undefined;
        const timer = setTimeout(() => finish(false), SESSION_READY_TIMEOUT);
        const finish = (ready: boolean) => {
            if (settled) return;
            settled = true;
            clearTimeout(timer);
            resolve(ready);
            queueMicrotask(() => unsubscribe?.());
        };

        unsubscribe = authStore.subscribe(({ sessionReady }) => {
            if (sessionReady) {
                finish(true);
            } else if (!getLoginSession()) {
                // ResumeSession failed and the stale local session was removed.
                finish(false);
            }
        });
    });
}

export default async function sendForReply<T, K = void>(params: IResponse<T>): Promise<IResponseErr<K>> {
    const { method, seq, data, chunk, chunkTotal } = params;

    if (!(await waitForSessionReady(method))) {
        return {
            method,
            error: EWsErrorCode.SESSION_NOT_READY,
        };
    }

    const ws = getWebsocket();
    if (!ws) {
        return {
            method,
            error: EWsErrorCode.WS_INSTANCE_NOT_EXIST,
        };
    }

    return await ws.sendForReply<T, K>({
        method,
        seq: seq ? seq : Date.now(),
        data,
        chunk,
        chunkTotal,
    });
}
