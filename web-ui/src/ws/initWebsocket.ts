import WebsocketService from './index';
import handleMessage from '@/ws/handler';
import { updateAuthField } from '@/stores/auth';

let Websocket: WebsocketService | null = null;

// clear cache after ws disconnect
const clearCache = () => {
    // Keep the current route visible while reconnecting. The next
    // AuthRequiredNotify decides whether the connection can be resumed or
    // must return to the login page.
    updateAuthField('authParams', null);
    updateAuthField('sessionReady', false);
};

export async function initWebsocket() {
    Websocket?.disconnect();
    Websocket = new WebsocketService();
    const initRes = await Websocket.connect();
    if (initRes.error !== 0) {
        console.log('init websocket fail with error: ', initRes);
        return initRes;
    }
    Websocket.on('message', handleMessage);
    Websocket.on('close', clearCache);
    return initRes;
}

export function getWebsocket() {
    return Websocket || null;
}

export function wsConnected() {
    return getWebsocket()?.ws?.readyState === WebSocket.OPEN;
}

export default Websocket;
