enum ETimerType {
    SERIAL_TIME = 'serialTime',
    TCP_TIME = 'tcpTime',
    BOOT_TIME = 'bootTime',
    CURRENT_TIME = 'currentTime',
}

export default ETimerType;

export type Updater = (initVal: number, elapsed: number) => number;

export interface TimerInfo {
    value: number;
    updater: Updater;
    startTimestamp: number;
    initialValue: number;
}
