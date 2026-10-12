import { EHassConnectionState, getHomeAssistantConnectionState } from '@/hooks/useHomeAssistant.svelte';
import type { IHassConnectionInfo } from '@/types/homeassistant';

export enum ETopTipsType {
    RESET_PASSWORD,
    RECONNECT,
    RECONNECT_SUCCESS,
    HASS_AUTHORIZATION_INVALID,
    HASS_CONNECTION_FAILED,
    HASS_CONNECTION_INTERRUPTED,
}

export const HASS_TOP_TIPS = new Set([
    ETopTipsType.HASS_AUTHORIZATION_INVALID,
    ETopTipsType.HASS_CONNECTION_FAILED,
    ETopTipsType.HASS_CONNECTION_INTERRUPTED,
]);

export function getHassConnectionTopTipsType(status: IHassConnectionInfo): ETopTipsType | undefined {
    switch (getHomeAssistantConnectionState(status)) {
        case EHassConnectionState.AUTHORIZATION_INVALID:
            return ETopTipsType.HASS_AUTHORIZATION_INVALID;
        case EHassConnectionState.DISABLED:
            return ETopTipsType.HASS_CONNECTION_FAILED;
        case EHassConnectionState.INTERRUPTED:
            return ETopTipsType.HASS_CONNECTION_INTERRUPTED;
        default:
            return undefined;
    }
}

interface TopTipsState {
    type?: ETopTipsType;
}

const topTips = $state<TopTipsState>({
    type: void 0,
});

const setTopTips = (params: Partial<TopTipsState>) => {
    const { type } = params;
    topTips.type = type;
};

export function useShowTopTips() {
    return {
        topTips,
        setTopTips,
    };
}
