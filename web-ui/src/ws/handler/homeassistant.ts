import type { IResponse } from '@/types/websocket';
import type { IHassAuthorizationResult, IHassConnectionStatusNotify, IHassScanResultNotify } from '@/types/homeassistant';
import { get } from 'svelte/store';
import { homeAssistantStore, updateHomeassistantField } from '@/stores/homeassistant';
import { getHassConnectionTopTipsType, HASS_TOP_TIPS, useShowTopTips } from '@/hooks/useShowTopTips.svelte';

const { topTips, setTopTips } = useShowTopTips();

function showConnectionStatusTips(status: IHassConnectionStatusNotify) {
    const type = getHassConnectionTopTipsType(status);
    if (type !== undefined) {
        setTopTips({ type });
        return;
    }

    if (topTips.type !== undefined && HASS_TOP_TIPS.has(topTips.type)) setTopTips({ type: undefined });
}

export const handleScanHassInstanceNotify = async (params: IResponse<IHassScanResultNotify>) => {
    updateHomeassistantField('hassList', params.data.hassList);
    updateHomeassistantField('scanning', false);
};

export const handleHassAuthorizationResultNotify = async (params: IResponse<IHassAuthorizationResult>) => {
    const result = params.data;
    updateHomeassistantField('authorizationResult', result);
    updateHomeassistantField('oauthCompleting', false);

    if (result.status === 'authorized') {
        updateHomeassistantField('connected', result.enabled);
        updateHomeassistantField('connectionInfo', {
            configured: true,
            hassUrl: result.hassUrl,
            longLivedToken: result.longLivedToken,
            tokenGeneratedAt: result.tokenGeneratedAt,
            enabled: result.enabled,
            deviceConnected: result.deviceConnected,
        });
    } else {
        updateHomeassistantField('connected', false);
        updateHomeassistantField('connectionInfo', null);
    }
};

export const handleHassConnectionStatusNotify = async (params: IResponse<IHassConnectionStatusNotify>) => {
    const status = params.data;
    const currentConnectionInfo = get(homeAssistantStore).connectionInfo;

    updateHomeassistantField('connected', status.configured && status.enabled);
    updateHomeassistantField(
        'connectionInfo',
        status.configured
            ? {
                  ...currentConnectionInfo,
                  ...status,
              }
            : status,
    );
    showConnectionStatusTips(status);
};
