/** lan device notify */
export interface ILanDeviceNotify {
    devices: Device[];
}

export interface Device {
    id: string;
    name: string;
    mdnsDomain: string;
    ip: string;
}