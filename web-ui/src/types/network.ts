/** network mode */
export enum ENetworkMode {
    AP = 'AP',
    WIFI = 'WiFi',
    //ETHERNET = 'Ethernet',
}
export interface IWifiInfoNotify {
    networkMode: ENetworkMode[];
    internetConnected: boolean;
    mdnsDomain: string;
    //vpnStatus?: VpnStatus;
    //ethernet?: Ethernet;
    wifi?: Wifi;
    ap?: Ap;
}

interface Ap {
    enabled: boolean;
    defaultEnabled: boolean;
    internetThroughAp: boolean;
    apName: string;
    security: string;
    channel: number;
    clients: Clients;
    gateway: string;
    subnetMask: string;
    password: string;
    hideSsid: boolean;
    bandwidth: number;
}

interface Clients {
    current: number;
    max: number;
}

export interface Wifi {
    connected: boolean;
    ssid: string;
    security: string;
    channel: number;
    dhcpEnabled: boolean;
    ipAddress: string;
    subnetMask: string;
    gateway: string;
    speed: number;
    dns1: string;
    dns2: string;
    macAddress: string;
    ipAcquisition: EIPAcquisition;
}

export enum EIPAcquisition {
    DHCP = 'dhcp',
    STATIC = 'static',
}

/** wifi scan result notify */
export interface IWifiScanResultNotify {
    results: Result[];
}

export interface Result {
    bssid: string;
    ssid: string;
    security: string;
    channel: number;
    rssi: number;
}

export interface IConnectionStatusNotify {
    connectWithAp: boolean;
}
