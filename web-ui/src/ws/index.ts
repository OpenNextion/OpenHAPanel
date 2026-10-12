import type { IWsConnectRes, IResponse, IResponseErr, IPendingMsgFunc } from '@/types/websocket';
import { wsReconnectManager } from './wsReconnect';
import { EWsErrorCode } from '@/constant/errorCode';
import { authStore } from '@/stores/auth';
import { isString } from 'lodash-es';
import { get } from 'svelte/store';
import { WS_ADDRESS } from '@/constant/ws';
import { ERROR_CODE } from '@/constant';
import { setFirmwareOperationBlocked } from '@/stores/firmwareUpdate';

type EventCallback = (ev: Event) => void;
type CloseEventCallback = (ev: CloseEvent) => void;
type MessageEventCallback = (ev: MessageEvent) => void;
type AllEventCallback = EventCallback | CloseEventCallback | MessageEventCallback;

const decoder = new TextDecoder('utf-8');

export const SEND_MSG_TIMEOUT = 30 * 1000;

export default class WebsocketService {
    ws: WebSocket | null = null;
    private connectResolve: ((value: IWsConnectRes) => void) | null = null;
    private connectPromise: Promise<IWsConnectRes> | null = null;
    private pendingMessages: Map<number, IPendingMsgFunc> = new Map();
    private eventHandlers: Record<string, Array<AllEventCallback>> = {
        open: [],
        message: [],
        close: [],
        error: [],
    };
    constructor() {}
    async connect(): Promise<IWsConnectRes> {
        if (this.ws?.readyState === WebSocket.OPEN) {
            return { error: EWsErrorCode.SUCCESS };
        }

        // Several components may request data while the application-level
        // socket is still opening. Reuse that connection instead of replacing
        // it with another WebSocket instance.
        if (this.connectPromise) {
            return this.connectPromise;
        }

        this.connectPromise = new Promise((resolve) => {
            let timer: ReturnType<typeof setTimeout> | null = null;
            const finish = (result: IWsConnectRes) => {
                if (timer) clearTimeout(timer);
                this.connectResolve = null;
                resolve(result);
            };

            try {
                this.ws = new WebSocket(WS_ADDRESS);
                this.ws.binaryType = 'arraybuffer';
                if (!this.ws) {
                    console.error('WebSocket not existed');
                    finish({ error: EWsErrorCode.INIT_CONNECT_ERROR });
                    return;
                }

                this.ws.onopen = this.onOpen.bind(this);
                this.ws.onclose = this.onClose.bind(this);
                this.ws.onerror = this.onError.bind(this);
                this.ws.onmessage = this.onMessage.bind(this);

                timer = setTimeout(() => {
                    finish({ error: EWsErrorCode.INIT_CONNECT_TIMEOUT });
                }, SEND_MSG_TIMEOUT);

                this.connectResolve = finish;
            } catch (error) {
                console.error('WebSocket connect error', error);
                finish({ error: EWsErrorCode.INIT_CONNECT_ERROR });
            }
        });

        try {
            return await this.connectPromise;
        } finally {
            this.connectPromise = null;
        }
    }
    sendForReply<T, K>(data: IResponse<T>): Promise<IResponseErr<K>> {
        return new Promise(async (resolve, reject) => {
            // 发现还未建立长连接则直接重连
            if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
                const connectResult = await this.connect();
                if (connectResult.error !== EWsErrorCode.SUCCESS || this.ws?.readyState !== WebSocket.OPEN) {
                    resolve({ method: data.method, error: connectResult.error });
                    return;
                }
            }

            if (!data.seq) {
                reject(new Error("Message must contain a 'seq' field."));
                return;
            }

            const sequence = data.seq;
            // 超时处理
            const timer = setTimeout(() => {
                if (this.pendingMessages.has(sequence)) {
                    this.pendingMessages.get(sequence)?.reject(new Error(`Request timed out after ${SEND_MSG_TIMEOUT}ms`));
                    this.pendingMessages.delete(sequence);
                }
            }, SEND_MSG_TIMEOUT);

            // 注册回调等待响应
            this.pendingMessages.set(sequence, { resolve, reject, timer });

            // 发送消息
            console.log(`WebSocket send: ${data.method} (seq: ${data.seq})`);
            this.ws!.send(JSON.stringify(data));
        });
    }

    // 关闭 WebSocket 连接
    disconnect() {
        if (this.ws) {
            this.ws.close();
            this.ws = null;
            this.pendingMessages.clear();
            this.eventHandlers.open = [];
            this.eventHandlers.message = [];
            this.eventHandlers.error = [];
        } else {
            console.warn('WebSocket is not connected.');
        }
    }

    on(event: 'open', callback: EventCallback): void;
    on(event: 'message', callback: MessageEventCallback): void;
    on(event: 'close', callback: CloseEventCallback): void;
    on(event: 'error', callback: EventCallback): void;
    on(event: 'open' | 'message' | 'close' | 'error', callback: AllEventCallback) {
        if (this.eventHandlers[event]) {
            this.eventHandlers[event].push(callback);
        } else {
            console.warn(`Unknown event type: ${event}`);
        }
    }

    off(event: 'open', callback: EventCallback): void;
    off(event: 'message', callback: MessageEventCallback): void;
    off(event: 'close', callback: CloseEventCallback): void;
    off(event: 'error', callback: EventCallback): void;
    off(event: 'open' | 'message' | 'close' | 'error', callback: AllEventCallback) {
        if (this.eventHandlers[event]) {
            this.eventHandlers[event] = this.eventHandlers[event].filter((cb) => cb !== callback);
        }
    }

    private dispatchEvent(event: 'open', ev: Event): void;
    private dispatchEvent(event: 'message', ev: MessageEvent): void;
    private dispatchEvent(event: 'close', ev: CloseEvent): void;
    private dispatchEvent(event: 'error', ev: Event): void;
    private dispatchEvent(event: 'open' | 'message' | 'close' | 'error', ev: Event & CloseEvent & MessageEvent<any>) {
        if (this.eventHandlers[event]) {
            this.eventHandlers[event].forEach((callback) => callback(ev));
        }
    }

    private reconnect() {
        const { passwordInitRequired, connectionLimitReached } = get(authStore);
        if (passwordInitRequired || connectionLimitReached || wsReconnectManager.status) return;
        wsReconnectManager.start();
    }

    private onClose(ev: CloseEvent) {
        if (this.connectResolve) {
            this.connectResolve({ error: EWsErrorCode.INIT_CONNECT_ERROR });
            this.connectResolve = null;
        }
        console.log('WebSocket closed with event: ', ev);
        this.dispatchEvent('close', ev);
        this.eventHandlers.close = [];
        this.reconnect();
    }

    private onOpen(ev: Event) {
        console.log('WebSocket opened with event: ', ev);
        wsReconnectManager.close();
        if (this.connectResolve) {
            this.connectResolve({ error: EWsErrorCode.SUCCESS });
            this.connectResolve = null;
        }
        this.dispatchEvent('open', ev);
    }

    private onError(ev: Event) {
        if (this.connectResolve) {
            this.connectResolve({ error: EWsErrorCode.INIT_CONNECT_ERROR });
            this.connectResolve = null;
        }
        console.log('WebSocket error with event: ', ev);
        this.dispatchEvent('error', ev);
    }

    private onMessage(ev: MessageEvent) {
        try {
            const text = isString(ev.data) ? ev.data : decoder.decode(ev.data);
            const message = JSON.parse(text);
            console.log(`WebSocket message: ${message.method ?? 'unknown'} (seq: ${message.seq ?? '-'})`);

            if (message.error === ERROR_CODE.OTA_IN_PROGRESS) {
                setFirmwareOperationBlocked(true);
            }

            if (message.seq && this.pendingMessages.has(message.seq)) {
                const { resolve, timer } = this.pendingMessages.get(message.seq)!;
                clearTimeout(timer);
                resolve(message);
                this.pendingMessages.delete(message.seq);
            }

            // @ts-ignore
            this.dispatchEvent('message', { data: text });
        } catch (err) {
            console.error(err, 'ws onMessage error');
        }
    }
}
