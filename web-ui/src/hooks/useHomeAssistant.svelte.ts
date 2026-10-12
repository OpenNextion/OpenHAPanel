import { updateHomeassistantField } from '@/stores/homeassistant';
import type { IHassConnectionInfo, IHassStartOAuthResponse } from '@/types/homeassistant';
import { EWsMethods } from '@/types/websocket';
import sendForReply from '@/ws/sendForReply';

let connectionInfoRequest: Promise<IHassConnectionInfo | null> | null = null;

export enum EHassConnectionState {
    CONNECTED = 'connected',
    AUTHORIZATION_INVALID = 'authorizationInvalid',
    DISABLED = 'disabled',
    INTERRUPTED = 'interrupted',
}

export function getHomeAssistantConnectionState(
    connectionInfo: IHassConnectionInfo | null | undefined,
): EHassConnectionState | undefined {
    if (!connectionInfo?.configured) return undefined;
    if (connectionInfo.status === 'error' && connectionInfo.reason === 'authInvalid') {
        return EHassConnectionState.AUTHORIZATION_INVALID;
    }
    if (!connectionInfo.enabled) return EHassConnectionState.DISABLED;
    if (!connectionInfo.deviceConnected) return EHassConnectionState.INTERRUPTED;
    return EHassConnectionState.CONNECTED;
}

export function isHomeAssistantAvailable(connectionInfo: IHassConnectionInfo | null | undefined) {
    return getHomeAssistantConnectionState(connectionInfo) === EHassConnectionState.CONNECTED;
}

const getConnectionInfo = () => {
    if (connectionInfoRequest) return connectionInfoRequest;

    connectionInfoRequest = (async () => {
        const response = await sendForReply<Record<string, never>, IHassConnectionInfo>({
            method: EWsMethods.HASS_GET_CONNECTION_INFO,
            data: {},
        });
        if (response.error !== 0 || !response.data) return null;

        updateHomeassistantField('connectionInfo', response.data);
        updateHomeassistantField('connected', response.data.configured && response.data.enabled);
        return response.data;
    })().finally(() => {
        connectionInfoRequest = null;
    });

    return connectionInfoRequest;
};

export const startHomeAssistantOAuth = async (hassUrl: string) => {
    const redirectUri = `${window.location.origin}${window.location.pathname}`;
    const response = await sendForReply<{ hassUrl: string; redirectUri: string }, IHassStartOAuthResponse>({
        method: EWsMethods.HASS_START_OAUTH,
        data: { hassUrl, redirectUri },
    });
    if (response.error === 0 && response.data?.authorizeUrl) window.location.assign(response.data.authorizeUrl);
    return response;
};

export function useHomeAssistant() {
    return { getConnectionInfo, startHomeAssistantOAuth };
}
