export interface IHomeassistantStore {
    hassList: IHassInstanceItem[];
    scanning: boolean;
    /** Whether a Home Assistant instance has been configured. */
    connected: boolean;
    connectionInfo: IHassConnectionInfo | null;
    authorizationResult: IHassAuthorizationResult | null;
    oauthCompleting: boolean;
}

export interface IHassScanResultNotify {
    hassList: IHassInstanceItem[];
}

export interface IHassInstanceItem {
    instanceName: string;
    hostname: string;
    ipAddress: string;
    port: number;
    locationName: string;
    uuid: string;
    version: string;
    internalUrl: string;
    externalUrl: string;
}

export interface IHassStartOAuthResponse {
    authorizeUrl: string;
    expiresIn: number;
}

export interface IHassOAuthCallback {
    code: string;
    state: string;
}

export interface IHassConnectionInfo {
    configured: boolean;
    hassUrl?: string;
    longLivedToken?: string;
    tokenGeneratedAt?: number;
    enabled: boolean;
    deviceConnected: boolean;
    status?: string;
    reason?: string;
    changed?: boolean;
}

export interface IHassConnectionStatusNotify {
    configured: boolean;
    enabled: boolean;
    deviceConnected: boolean;
    status: string;
    reason?: string;
}

export interface IHassUpdateUrlResponse extends IHassConnectionInfo {
    changed: boolean;
    hassUrl: string;
}

export interface IHassDisableResponse {
    enabled: false;
    deviceConnected: false;
}

export type IHassAuthorizationResult =
    | ({ status: 'authorized' } & Omit<IHassConnectionInfo, 'configured'>)
    | { status: 'error'; reason: string };
