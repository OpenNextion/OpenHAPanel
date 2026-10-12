import { updateNetworkField, updateNetworkForWifiOrScanResult } from '@/stores/network';
import type { IWifiInfoNotify, IWifiScanResultNotify } from '@/types/network';
//import type { ISecurityInfoNotify } from '@/lib/types/interface/methods/security';
import type { IResponse } from '@/types/websocket';
import type { IConnectionStatusNotify } from '@/types/network';
import { update as updateConnectionStatus } from '@/stores/connectionStatus';

export async function handleNetworkInfoNotify(params: IResponse<IWifiInfoNotify>) {
    console.log('network notify coming message', params);
    updateNetworkForWifiOrScanResult(params.data);
    updateNetworkField('infoReady', true);
}

export async function handleWifiScanResult(params: IResponse<IWifiScanResultNotify>) {
    console.log('wifi scan notify coming message', params);
    updateNetworkField('scanWifiLoading', false);
    updateNetworkField('results', params.data.results);
}

export async function handleConnectionStatusNotify(params: IResponse<IConnectionStatusNotify>) {
    updateConnectionStatus('connectWithAp', params.data.connectWithAp);
}

//export async function handleSecurityInfoNotify(params: IResponse<ISecurityInfoNotify>) {
//    console.log('security notify coming message', params);
//    updateNetworkField('security', params.data);
//}
