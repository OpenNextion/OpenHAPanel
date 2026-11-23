import { get, type Unsubscriber } from 'svelte/store';
import { t } from 'svelte-i18n';
import { haRegistryStore, setHassRegistryData, setHassRegistryError, setHassRegistryLoading, updateHassEntityState, updateHassRegistryData } from '@/stores/haRegistry';
import { clearHassLightFavorites, setHassLightFavorites } from '@/stores/haLightFavorites';
import { clearHassCoverFavorites, setHassCoverFavorites, type IHassCoverFavorites } from '@/stores/haCoverFavorites';
import { appendHassHistoryStream, beginHassEntityHistory, clearHassHistory, failHassEntityHistory } from '@/stores/haHistory';
import { homeAssistantStore } from '@/stores/homeassistant';
import { languageStore } from '@/stores/language';
import type {
    IHassAreaRegistryEntry,
    IHassDeviceRegistryEntry,
    IHassEntityRegistryEntry,
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

interface IHassStateChangedEvent {
    data?: {
        entity_id?: string;
        new_state?: IHassStateEntry | null;
    };
}

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
    private unsubscribeLanguage: Unsubscriber | null = null;
    private reconnectTimer: ReturnType<typeof setTimeout> | null = null;
    private registryRetryTimer: ReturnType<typeof setTimeout> | null = null;
    private registryResourceTimers = new Map<'entities' | 'devices' | 'areas' | 'floors', ReturnType<typeof setTimeout>>();
    private authTimer: ReturnType<typeof setTimeout> | null = null;
    private connectionInfo: IHassConnectionInfo | null = null;
    private connectionKey = '';
    private rejectedConnectionKey = '';
    private authenticated = false;
    private nextMessageId = 1;
    private pendingRequests = new Map<number, IPendingRequest>();
    private historyStreamSubscriptions = new Map<string, { id: number | null; off?: () => void; listeners: Set<() => void> }>();
    private eventListeners = new Map<number, Set<THassEventListener>>();
    private registryLoadGeneration = 0;
    private registryLoadPromise: Promise<void> | null = null;
    private stateChangesDuringRegistryLoad = new Map<string, IHassStateEntry | null>();
    private stateChangedSubscriptionId: number | null = null;
    private entityRegistrySubscriptionId: number | null = null;
    private deviceRegistrySubscriptionId: number | null = null;
    private areaRegistrySubscriptionId: number | null = null;
    private floorRegistrySubscriptionId: number | null = null;
    private loadedTranslationsLanguage: string | null = null;
    private translationRequests = new Map<string, Promise<void>>();

    start() {
        if (this.unsubscribe) return;
        this.unsubscribe = homeAssistantStore.subscribe(({ connectionInfo }) => {
            this.syncConnection(connectionInfo);
        });
        this.unsubscribeLanguage = languageStore.subscribe(({ language }) => {
            if (!this.authenticated || language === this.loadedTranslationsLanguage) return;
            void this.refreshHassTranslations(language);
        });
    }

    stop() {
        this.unsubscribe?.();
        this.unsubscribe = null;
        this.unsubscribeLanguage?.();
        this.unsubscribeLanguage = null;
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

    subscribeEntityHistory(entityId: string) {
        let subscription = this.historyStreamSubscriptions.get(entityId);
        if (!subscription) {
            subscription = { id: null, listeners: new Set() };
            this.historyStreamSubscriptions.set(entityId, subscription);
            beginHassEntityHistory(entityId);
        }
        const listener = () => {};
        subscription.listeners.add(listener);
        this.startEntityHistoryStream(entityId, subscription);
        return () => {
            const current = this.historyStreamSubscriptions.get(entityId);
            if (!current) return;
            current.listeners.delete(listener);
            if (current.listeners.size) return;
            if (current.id !== null && this.authenticated) {
                try { this.sendMessage({ type: 'unsubscribe_events', subscription: current.id }); } catch { /* disconnect in progress */ }
            }
            current.off?.();
            this.historyStreamSubscriptions.delete(entityId);
        };
    }

    private startEntityHistoryStream(entityId: string, subscription: { id: number | null; off?: () => void; listeners: Set<() => void> }) {
        if (!this.authenticated || subscription.id !== null || subscription.listeners.size === 0) return;
        const startTime = new Date(Date.now() - 24 * 60 * 60 * 1000).toISOString();
        try {
            const id = this.reserveMessageId();
            const response = this.sendForReply({
                id, type: 'history/stream', entity_ids: [entityId], start_time: startTime,
                minimal_response: true, significant_changes_only: true, no_attributes: true,
            });
            subscription.id = id;
            void response.then((message) => {
                if (message.success === false) throw new Error('History stream subscription failed');
                appendHassHistoryStream(entityId, [], true);
            }).catch((error) => {
                if (subscription.id === id) subscription.id = null;
                failHassEntityHistory(entityId);
                console.warn(`Failed to subscribe to Home Assistant history for ${entityId}:`, error);
                window.setTimeout(() => {
                    if (this.historyStreamSubscriptions.get(entityId) === subscription) {
                        this.startEntityHistoryStream(entityId, subscription);
                    }
                }, 5_000);
            });
            subscription.off = this.onEvent<{ states?: Record<string, Array<{ s?: string; state?: string; lu?: number; last_updated?: string }>> }>(id, ({ event }) => {
                const states = event?.states?.[entityId];
                if (!Array.isArray(states)) return;
                const points = states.flatMap((state) => {
                    const rawValue = typeof state.s === 'string' ? state.s : state.state;
                    const value = rawValue?.trim() ? Number(rawValue) : NaN;
                    const parsed = Date.parse(state.last_updated ?? '');
                    const timestamp = Number.isFinite(parsed) ? parsed : (typeof state.lu === 'number' ? state.lu * 1000 : NaN);
                    return Number.isFinite(value) && Number.isFinite(timestamp) ? [{ value, timestamp }] : [];
                });
                appendHassHistoryStream(entityId, points);
            });
        } catch (error) {
            subscription.id = null;
            failHassEntityHistory(entityId);
            console.warn(`Failed to subscribe to Home Assistant history for ${entityId}:`, error);
        }
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
            this.stateChangedSubscriptionId = null;
            this.entityRegistrySubscriptionId = null;
            this.deviceRegistrySubscriptionId = null;
            this.areaRegistrySubscriptionId = null;
            this.floorRegistrySubscriptionId = null;
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
                this.subscribeToHassEvents();
                this.historyStreamSubscriptions.forEach((subscription, entityId) => {
                    subscription.id = null;
                    this.startEntityHistoryStream(entityId, subscription);
                });
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
        this.stateChangedSubscriptionId = null;
        this.entityRegistrySubscriptionId = null;
        this.deviceRegistrySubscriptionId = null;
        this.areaRegistrySubscriptionId = null;
        this.floorRegistrySubscriptionId = null;
        this.loadedTranslationsLanguage = null;
        this.translationRequests.clear();
        this.historyStreamSubscriptions.forEach((subscription) => { subscription.id = null; });
        this.registryResourceTimers.forEach((timer) => clearTimeout(timer));
        this.registryResourceTimers.clear();
        this.stateChangesDuringRegistryLoad.clear();
        clearHassLightFavorites();
        clearHassCoverFavorites();
        clearHassHistory();
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
        this.stateChangesDuringRegistryLoad.clear();
        setHassRegistryLoading(true);
        try {
            const language = get(languageStore).language;
            const [stateResponse, entityResponse, deviceResponse, areaResponse, floorResponse, translationResponse] = await Promise.all([
                this.sendForReply<IHassStateEntry[]>({ type: 'get_states' }),
                this.sendForReply<THassEntityRegistryDisplayResult>({ type: 'config/entity_registry/list_for_display' }),
                this.sendForReply<IHassDeviceRegistryEntry[]>({ type: 'config/device_registry/list' }),
                this.sendForReply<IHassAreaRegistryEntry[]>({ type: 'config/area_registry/list' }),
                this.sendForReply<IHassFloorRegistryEntry[]>({ type: 'config/floor_registry/list' }),
                this.sendForReply<{ resources: Record<string, string> }>({ type: 'frontend/get_translations', language, category: 'title' }),
            ]);
            if (generation !== this.registryLoadGeneration || connectionKey !== this.connectionKey || !this.authenticated) return;

            const responses = [stateResponse, entityResponse, deviceResponse, areaResponse, floorResponse];
            const failedResponse = responses.find((response) => response.success === false);
            if (failedResponse) throw new Error(`Home Assistant registry request ${failedResponse.id} failed`);

            const statesById = new Map((stateResponse.result ?? []).map((state) => [state.entity_id, state]));
            this.stateChangesDuringRegistryLoad.forEach((state, entityId) => {
                if (state) statesById.set(entityId, state);
                else statesById.delete(entityId);
            });

            const activeLanguage = get(languageStore).language;
            const translationsAreCurrent = translationResponse.success !== false && language === activeLanguage;
            setHassRegistryData({
                states: [...statesById.values()],
                entities: Array.isArray(entityResponse.result) ? entityResponse.result : (entityResponse.result?.entities ?? []),
                devices: deviceResponse.result ?? [],
                areas: areaResponse.result ?? [],
                floors: floorResponse.result ?? [],
                domainTranslations: translationsAreCurrent ? (translationResponse.result?.resources ?? {}) : get(haRegistryStore).domainTranslations,
            });
            if (translationsAreCurrent) this.loadedTranslationsLanguage = language;
            else if (language !== activeLanguage) void this.refreshHassTranslations(activeLanguage);

            const favoriteEntityIds = [...statesById.keys()].filter((entityId) => entityId.startsWith('light.') || entityId.startsWith('cover.'));
            await this.loadEntityFavorites(connectionKey, generation, favoriteEntityIds);
        } catch (error) {
            if (generation !== this.registryLoadGeneration || connectionKey !== this.connectionKey) return;
            console.error('Failed to load Home Assistant registries:', error);
            setHassRegistryError(get(t)('TIPS.HA_REGISTRY_LOAD_FAILED'));
            this.scheduleRegistryReload(connectionKey);
        }
    }

    private async loadEntityFavorites(connectionKey: string, generation: number, entityIds: string[]) {
        if (!entityIds.length) {
            setHassLightFavorites(new Map());
            setHassCoverFavorites(new Map());
            return;
        }

        try {
            const response = await this.sendForReply<Record<string, IHassEntityRegistryEntry | null>>({
                type: 'config/entity_registry/get_entries',
                entity_ids: entityIds,
            });
            if (generation !== this.registryLoadGeneration || connectionKey !== this.connectionKey || !this.authenticated) return;
            if (response.success === false) throw new Error('Home Assistant entity registry get_entries request failed');

            const lightFavorites = new Map<string, readonly unknown[]>();
            const coverFavorites = new Map<string, IHassCoverFavorites>();
            for (const entityId of entityIds) {
                const options = response.result?.[entityId]?.options;
                const colors = options?.light?.favorite_colors;
                if (Array.isArray(colors)) lightFavorites.set(entityId, colors);
                const positions = options?.cover?.favorite_positions;
                const tiltPositions = options?.cover?.favorite_tilt_positions;
                if (Array.isArray(positions) || Array.isArray(tiltPositions)) {
                    coverFavorites.set(entityId, {
                        ...(Array.isArray(positions) ? { favorite_positions: positions } : {}),
                        ...(Array.isArray(tiltPositions) ? { favorite_tilt_positions: tiltPositions } : {}),
                    });
                }
            }
            setHassLightFavorites(lightFavorites);
            setHassCoverFavorites(coverFavorites);
        } catch (error) {
            if (generation !== this.registryLoadGeneration || connectionKey !== this.connectionKey || !this.authenticated) return;
            console.warn('Failed to load Home Assistant entity favorites:', error);
        }
    }

    private subscribeToHassEvents() {
        if (!this.authenticated) return;

        if (this.stateChangedSubscriptionId === null) {
            const id = this.sendMessage({ type: 'subscribe_events', event_type: 'state_changed' });
            this.stateChangedSubscriptionId = id;
            this.onEvent<IHassStateChangedEvent>(id, ({ event }) => {
                const entityId = event?.data?.entity_id;
                if (typeof entityId !== 'string') return;
                const newState = event.data?.new_state ?? null;
                if (this.registryLoadPromise) this.stateChangesDuringRegistryLoad.set(entityId, newState);
                updateHassEntityState(entityId, newState);
            });
        }

        if (this.entityRegistrySubscriptionId === null) {
            const id = this.sendMessage({ type: 'subscribe_events', event_type: 'entity_registry_updated' });
            this.entityRegistrySubscriptionId = id;
            this.onEvent(id, () => this.scheduleRegistryResourceRefresh('entities'));
        }

        if (this.deviceRegistrySubscriptionId === null) {
            const id = this.sendMessage({ type: 'subscribe_events', event_type: 'device_registry_updated' });
            this.deviceRegistrySubscriptionId = id;
            this.onEvent(id, () => this.scheduleRegistryResourceRefresh('devices'));
        }

        if (this.areaRegistrySubscriptionId === null) {
            const id = this.sendMessage({ type: 'subscribe_events', event_type: 'area_registry_updated' });
            this.areaRegistrySubscriptionId = id;
            this.onEvent(id, () => this.scheduleRegistryResourceRefresh('areas'));
        }

        if (this.floorRegistrySubscriptionId === null) {
            const id = this.sendMessage({ type: 'subscribe_events', event_type: 'floor_registry_updated' });
            this.floorRegistrySubscriptionId = id;
            this.onEvent(id, () => this.scheduleRegistryResourceRefresh('floors'));
        }
    }

    private scheduleRegistryResourceRefresh(resource: 'entities' | 'devices' | 'areas' | 'floors') {
        const pendingTimer = this.registryResourceTimers.get(resource);
        if (pendingTimer) clearTimeout(pendingTimer);
        this.registryResourceTimers.set(resource, setTimeout(() => {
            this.registryResourceTimers.delete(resource);
            void this.refreshRegistryResource(resource);
        }, 100));
    }

    private async refreshRegistryResource(resource: 'entities' | 'devices' | 'areas' | 'floors') {
        const connectionKey = this.connectionKey;
        if (!this.authenticated || !connectionKey) return;
        try {
            if (this.registryLoadPromise) await this.registryLoadPromise;
            if (!this.authenticated || connectionKey !== this.connectionKey) return;
            const command = {
                entities: { type: 'config/entity_registry/list_for_display' },
                devices: { type: 'config/device_registry/list' },
                areas: { type: 'config/area_registry/list' },
                floors: { type: 'config/floor_registry/list' },
            }[resource];
            const response = await this.sendForReply(command);
            if (!this.authenticated || connectionKey !== this.connectionKey) return;
            if (response.success === false) throw new Error(`Home Assistant ${resource} registry request failed`);

            if (resource === 'entities') {
                const result = response.result as THassEntityRegistryDisplayResult | undefined;
                updateHassRegistryData({ entities: Array.isArray(result) ? result : (result?.entities ?? []) });
                const current = get(haRegistryStore);
                const favoriteEntityIds = current.states.map(({ entity_id }) => entity_id).filter((entityId) => entityId.startsWith('light.') || entityId.startsWith('cover.'));
                await this.loadEntityFavorites(connectionKey, this.registryLoadGeneration, favoriteEntityIds);
                return;
            }

            if (resource === 'devices') updateHassRegistryData({ devices: (response.result as IHassDeviceRegistryEntry[] | undefined) ?? [] });
            else if (resource === 'areas') updateHassRegistryData({ areas: (response.result as IHassAreaRegistryEntry[] | undefined) ?? [] });
            else updateHassRegistryData({ floors: (response.result as IHassFloorRegistryEntry[] | undefined) ?? [] });
        } catch (error) {
            if (!this.authenticated || connectionKey !== this.connectionKey) return;
            console.warn(`Failed to refresh Home Assistant ${resource} registry:`, error);
            this.scheduleRegistryReload(connectionKey);
        }
    }

    private refreshHassTranslations(language: string) {
        const connectionKey = this.connectionKey;
        if (!this.authenticated || !connectionKey || language === this.loadedTranslationsLanguage) return Promise.resolve();
        const pendingRequest = this.translationRequests.get(language);
        if (pendingRequest) return pendingRequest;

        const request = this.loadHassTranslations(language, connectionKey).finally(() => {
            if (this.translationRequests.get(language) === request) this.translationRequests.delete(language);
        });
        this.translationRequests.set(language, request);
        return request;
    }

    private async loadHassTranslations(language: string, connectionKey: string) {
        try {
            const response = await this.sendForReply<{ resources: Record<string, string> }>({
                type: 'frontend/get_translations',
                language,
                category: 'title',
            });
            if (!this.authenticated || connectionKey !== this.connectionKey || language !== get(languageStore).language) return;
            if (response.success === false) throw new Error('Home Assistant translations request failed');
            updateHassRegistryData({ domainTranslations: response.result?.resources ?? {} });
            this.loadedTranslationsLanguage = language;
        } catch (error) {
            if (!this.authenticated || connectionKey !== this.connectionKey) return;
            console.warn('Failed to refresh Home Assistant translations:', error);
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
