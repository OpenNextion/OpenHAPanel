import type { Unsubscriber } from 'svelte/store';
import { setHassRegistryData, setHassRegistryError, setHassRegistryLoading } from '@/stores/haRegistry';
import { homeAssistantStore } from '@/stores/homeassistant';
import type {
    IHassAreaRegistryEntry,
    IHassDeviceRegistryEntry,
    IHassFloorRegistryEntry,
    IHassStateEntry,
    THassEntityRegistryDisplayResult,
} from '@/types/haRegistry';
import type { IHassConnectionInfo } from '@/types/homeassistant';

export interface IHassCommand {
    type: string;
    id?: number;
    [key: string]: unknown;
}

export interface IHassResultMessage<TResult = unknown> {
    type: 'result';
    id: number;
    success?: boolean;
    result?: TResult;
    error?: unknown;
}

export interface IHassEventMessage<TEvent = unknown> {
    type: 'event';
    id: number;
    event: TEvent;
}

type THassIncomingMessage =
    | { type: 'auth_required'; ha_version?: string }
    | { type: 'auth_ok'; ha_version?: string }
    | { type: 'auth_invalid'; message?: string }
    | IHassResultMessage
    | IHassEventMessage
    | { type: string; [key: string]: unknown };

interface IPendingRequest {
    resolve: (message: IHassResultMessage) => void;
    reject: (reason: Error) => void;
    timer: ReturnType<typeof setTimeout>;
}

type THassEventListener = (message: IHassEventMessage) => void;

const RECONNECT_DELAY = 5_000;
const REGISTRY_RETRY_DELAY = 5_000;
const AUTH_TIMEOUT = 10_000;
const MESSAGE_TIMEOUT = 10_000;

function getHassWebSocketUrl(hassUrl: string) {
    try {
        const url = new URL(hassUrl);
        if (url.protocol !== 'http:' && url.protocol !== 'https:') return null;
        url.protocol = url.protocol === 'https:' ? 'wss:' : 'ws:';
        const basePath = url.pathname.replace(/\/+$/, '');
        url.pathname = `${basePath}/api/websocket`;
        url.search = '';
        url.hash = '';
        return url.toString();
    } catch {
        return null;
    }
}

class HomeAssistantWebSocket {
    private socket: WebSocket | null = null;
    private unsubscribe: Unsubscriber | null = null;
    private reconnectTimer: ReturnType<typeof setTimeout> | null = null;
    private registryRetryTimer: ReturnType<typeof setTimeout> | null = null;
    private authTimer: ReturnType<typeof setTimeout> | null = null;
    private connectionInfo: IHassConnectionInfo | null = null;
    private connectionKey = '';
    private rejectedConnectionKey = '';
    private authenticated = false;
    private nextMessageId = 1;
    private pendingRequests = new Map<number, IPendingRequest>();
    private eventListeners = new Map<number, Set<THassEventListener>>();
    private registryLoadGeneration = 0;
    private registryLoadPromise: Promise<void> | null = null;

    start() {
        if (this.unsubscribe) return;
        this.unsubscribe = homeAssistantStore.subscribe(({ connectionInfo }) => {
            this.syncConnection(connectionInfo);
        });
    }

    stop() {
        this.unsubscribe?.();
        this.unsubscribe = null;
        this.connectionInfo = null;
        this.connectionKey = '';
        this.rejectedConnectionKey = '';
        this.clearReconnectTimer();
        this.disconnect();
    }

    isAuthenticated() {
        return this.authenticated;
    }

    refreshRegistryData() {
        const connectionKey = this.connectionKey;
        if (!this.authenticated || !connectionKey) return Promise.resolve(false);
        if (this.registryLoadPromise) return this.registryLoadPromise.then(() => true);

        this.clearRegistryRetryTimer();
        const loadPromise = this.loadRegistryData(connectionKey);
        this.registryLoadPromise = loadPromise;
        return loadPromise.finally(() => {
            if (this.registryLoadPromise === loadPromise) this.registryLoadPromise = null;
        }).then(() => true);
    }

    sendMessage(message: IHassCommand) {
        const socket = this.getAuthenticatedSocket();
        const id = this.reserveMessageId(message.id);
        socket.send(JSON.stringify({ ...message, id }));
        return id;
    }

    sendForReply<TResult = unknown>(message: IHassCommand, timeout = MESSAGE_TIMEOUT) {
        const socket = this.getAuthenticatedSocket();
        const id = this.reserveMessageId(message.id);
        if (this.pendingRequests.has(id)) return Promise.reject(new Error(`Home Assistant request id ${id} is already pending`));

        return new Promise<IHassResultMessage<TResult>>((resolve, reject) => {
            const timer = setTimeout(() => {
                this.pendingRequests.delete(id);
                reject(new Error(`Home Assistant request ${id} timed out`));
            }, timeout);
            this.pendingRequests.set(id, {
                resolve: (response) => resolve(response as IHassResultMessage<TResult>),
                reject,
                timer,
            });

            try {
                socket.send(JSON.stringify({ ...message, id }));
            } catch (error) {
                clearTimeout(timer);
                this.pendingRequests.delete(id);
                reject(error instanceof Error ? error : new Error('Failed to send the Home Assistant request'));
            }
        });
    }

    onEvent<TEvent = unknown>(id: number, listener: (message: IHassEventMessage<TEvent>) => void) {
        const listeners = this.eventListeners.get(id) ?? new Set<THassEventListener>();
        const normalizedListener = listener as THassEventListener;
        listeners.add(normalizedListener);
        this.eventListeners.set(id, listeners);

        return () => {
            const currentListeners = this.eventListeners.get(id);
            currentListeners?.delete(normalizedListener);
            if (!currentListeners?.size) this.eventListeners.delete(id);
        };
    }

    private syncConnection(connectionInfo: IHassConnectionInfo | null) {
        this.connectionInfo = connectionInfo;
        const hassUrl = connectionInfo?.hassUrl?.trim();
        const token = connectionInfo?.longLivedToken?.trim();
        const webSocketUrl = hassUrl ? getHassWebSocketUrl(hassUrl) : null;

        if (!connectionInfo?.enabled || !token || !webSocketUrl) {
            this.connectionKey = '';
            this.rejectedConnectionKey = '';
            this.clearReconnectTimer();
            this.disconnect();
            return;
        }

        const nextConnectionKey = `${webSocketUrl}\n${token}`;
        if (nextConnectionKey === this.connectionKey && (this.socket || this.reconnectTimer)) return;

        this.clearReconnectTimer();
        this.disconnect();
        this.connectionKey = nextConnectionKey;
        if (this.rejectedConnectionKey !== nextConnectionKey) this.connect(webSocketUrl, token, nextConnectionKey);
    }

    private connect(webSocketUrl: string, token: string, connectionKey: string) {
        if (!this.connectionInfo?.enabled || connectionKey !== this.connectionKey) return;

        let socket: WebSocket;
        try {
            socket = new WebSocket(webSocketUrl);
        } catch {
            this.scheduleReconnect();
            return;
        }
        this.socket = socket;
        this.authenticated = false;
        this.nextMessageId = 1;

        socket.addEventListener('open', () => {
            if (this.socket !== socket) return;
            this.clearAuthTimer();
            this.authTimer = setTimeout(() => {
                if (this.socket === socket && !this.authenticated) socket.close();
            }, AUTH_TIMEOUT);
        });

        socket.addEventListener('message', (event) => {
            if (this.socket !== socket) return;
            this.handleMessage(socket, token, connectionKey, event.data);
        });

        socket.addEventListener('close', () => {
            if (this.socket !== socket) return;
            this.socket = null;
            this.authenticated = false;
            this.clearAuthTimer();
            this.clearMessageMatching(new Error('Home Assistant WebSocket disconnected'));
            if (this.rejectedConnectionKey !== connectionKey) this.scheduleReconnect();
        });

        socket.addEventListener('error', () => {
            if (this.socket === socket) socket.close();
        });
    }

    private handleMessage(socket: WebSocket, token: string, connectionKey: string, rawData: unknown) {
        if (typeof rawData !== 'string') return;

        let parsed: unknown;
        try {
            parsed = JSON.parse(rawData) as unknown;
        } catch {
            return;
        }

        const messages = Array.isArray(parsed) ? parsed : [parsed];
        messages.forEach((message) => {
            if (!message || typeof message !== 'object' || !('type' in message)) return;
            this.routeMessage(socket, token, connectionKey, message as THassIncomingMessage);
        });
    }

    private routeMessage(socket: WebSocket, token: string, connectionKey: string, message: THassIncomingMessage) {
        switch (message.type) {
            case 'auth_required':
                if (socket.readyState === WebSocket.OPEN) {
                    socket.send(JSON.stringify({ type: 'auth', access_token: token }));
                }
                break;
            case 'auth_ok':
                this.clearAuthTimer();
                this.authenticated = true;
                this.rejectedConnectionKey = '';
                void this.refreshRegistryData();
                break;
            case 'auth_invalid':
                this.clearAuthTimer();
                this.authenticated = false;
                this.rejectedConnectionKey = connectionKey;
                socket.close();
                break;
            case 'result': {
                if (!('id' in message) || typeof message.id !== 'number') break;
                const pendingRequest = this.pendingRequests.get(message.id);
                if (!pendingRequest) break;
                clearTimeout(pendingRequest.timer);
                this.pendingRequests.delete(message.id);
                pendingRequest.resolve(message as IHassResultMessage);
                break;
            }
            case 'event': {
                if (!('id' in message) || typeof message.id !== 'number') break;
                const listeners = this.eventListeners.get(message.id);
                if (!listeners) break;
                listeners.forEach((listener) => {
                    try {
                        listener(message as IHassEventMessage);
                    } catch (error) {
                        console.error('Failed to handle a Home Assistant event:', error);
                    }
                });
                break;
            }
        }
    }

    private scheduleReconnect() {
        this.clearReconnectTimer();
        const connectionInfo = this.connectionInfo;
        const hassUrl = connectionInfo?.hassUrl?.trim();
        const token = connectionInfo?.longLivedToken?.trim();
        const webSocketUrl = hassUrl ? getHassWebSocketUrl(hassUrl) : null;
        const connectionKey = this.connectionKey;
        if (!connectionInfo?.enabled || !token || !webSocketUrl || !connectionKey) return;

        this.reconnectTimer = setTimeout(() => {
            this.reconnectTimer = null;
            if (this.connectionKey === connectionKey && this.rejectedConnectionKey !== connectionKey) {
                this.connect(webSocketUrl, token, connectionKey);
            }
        }, RECONNECT_DELAY);
    }

    private disconnect() {
        this.registryLoadGeneration += 1;
        this.registryLoadPromise = null;
        this.clearRegistryRetryTimer();
        this.clearAuthTimer();
        this.authenticated = false;
        this.clearMessageMatching(new Error('Home Assistant WebSocket disconnected'));
        const socket = this.socket;
        this.socket = null;
        if (!socket) return;
        socket.close();
    }

    private async loadRegistryData(connectionKey: string) {
        const generation = ++this.registryLoadGeneration;
        setHassRegistryLoading(true);
        try {
            const language = document.documentElement.lang || navigator.language || 'en';
            const hassLanguage = language.toLowerCase().startsWith('zh') ? 'zh-Hans' : language;
            const [stateResponse, entityResponse, deviceResponse, areaResponse, floorResponse, translationResponse] = await Promise.all([
                this.sendForReply<IHassStateEntry[]>({ type: 'get_states' }),
                this.sendForReply<THassEntityRegistryDisplayResult>({ type: 'config/entity_registry/list_for_display' }),
                this.sendForReply<IHassDeviceRegistryEntry[]>({ type: 'config/device_registry/list' }),
                this.sendForReply<IHassAreaRegistryEntry[]>({ type: 'config/area_registry/list' }),
                this.sendForReply<IHassFloorRegistryEntry[]>({ type: 'config/floor_registry/list' }),
                this.sendForReply<{ resources: Record<string, string> }>({ type: 'frontend/get_translations', language: hassLanguage, category: 'title' }),
            ]);
            if (generation !== this.registryLoadGeneration || connectionKey !== this.connectionKey || !this.authenticated) return;

            const responses = [stateResponse, entityResponse, deviceResponse, areaResponse, floorResponse];
            const failedResponse = responses.find((response) => response.success === false);
            if (failedResponse) throw new Error(`Home Assistant registry request ${failedResponse.id} failed`);

            setHassRegistryData({
                states: stateResponse.result ?? [],
                entities: Array.isArray(entityResponse.result) ? entityResponse.result : (entityResponse.result?.entities ?? []),
                devices: deviceResponse.result ?? [],
                areas: areaResponse.result ?? [],
                floors: floorResponse.result ?? [],
                domainTranslations: translationResponse.success === false ? {} : (translationResponse.result?.resources ?? {}),
            });
        } catch (error) {
            if (generation !== this.registryLoadGeneration || connectionKey !== this.connectionKey) return;
            console.error('Failed to load Home Assistant registries:', error);
            setHassRegistryError(error instanceof Error ? error.message : 'Failed to load Home Assistant registries');
            this.scheduleRegistryReload(connectionKey);
        }
    }

    private scheduleRegistryReload(connectionKey: string) {
        this.clearRegistryRetryTimer();
        if (!this.authenticated || connectionKey !== this.connectionKey) return;
        this.registryRetryTimer = setTimeout(() => {
            this.registryRetryTimer = null;
            if (this.authenticated && connectionKey === this.connectionKey) void this.refreshRegistryData();
        }, REGISTRY_RETRY_DELAY);
    }

    private clearReconnectTimer() {
        if (this.reconnectTimer) clearTimeout(this.reconnectTimer);
        this.reconnectTimer = null;
    }

    private clearRegistryRetryTimer() {
        if (this.registryRetryTimer) clearTimeout(this.registryRetryTimer);
        this.registryRetryTimer = null;
    }

    private clearAuthTimer() {
        if (this.authTimer) clearTimeout(this.authTimer);
        this.authTimer = null;
    }

    private getAuthenticatedSocket() {
        if (!this.authenticated || !this.socket || this.socket.readyState !== WebSocket.OPEN) {
            throw new Error('Home Assistant WebSocket is not authenticated');
        }
        return this.socket;
    }

    private reserveMessageId(requestedId?: number) {
        if (requestedId !== undefined) {
            if (!Number.isInteger(requestedId) || requestedId <= 0) throw new Error('Home Assistant message id must be a positive integer');
            this.nextMessageId = Math.max(this.nextMessageId, requestedId + 1);
            return requestedId;
        }

        while (this.pendingRequests.has(this.nextMessageId) || this.eventListeners.has(this.nextMessageId)) this.nextMessageId += 1;
        return this.nextMessageId++;
    }

    private clearMessageMatching(reason: Error) {
        this.pendingRequests.forEach(({ reject, timer }) => {
            clearTimeout(timer);
            reject(reason);
        });
        this.pendingRequests.clear();
        this.eventListeners.clear();
    }
}

export const homeAssistantWebSocket = new HomeAssistantWebSocket();

export function initHomeAssistantWebSocket() {
    homeAssistantWebSocket.start();
    return homeAssistantWebSocket;
}
