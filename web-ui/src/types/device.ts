import type EThemeMode from '@/types//EThemeMode';

/** device info notify */
export interface IDeviceInfoNotify {
    name: string;
    id: string;
    bootTimestamp: number;
    currentTime: number;
    tzInfo: TimeZone;
    ntpServers: NtpServers;
    turboMode: boolean;
    language: string;
    brightness: number;
    idleTime: number;
    esp32?: {
        model: string;
        currentVersion: string;
    };
    //IEEEAddress: IEEEAddress;
}

export interface NtpServers {
    server1: string;
    server2: string;
}

export interface TimeZone {
    timeZone: string;
    /** utc offset for second */
    utcOffset: number;
    isDst: boolean;
    hasDst: boolean;
    nextDstShift?: any;
    nextUtcOffset?: any;
}

export interface IThemeModeNotify {
    currentMode: EThemeMode;
    followSystem: boolean;
}

export enum ERebootType {
    ESP32 = 'esp32',
}
