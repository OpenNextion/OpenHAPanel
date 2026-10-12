export interface IFirmwareUpdateStatusNotify {
    active: boolean;
    operationBlocked: boolean;
    source: string;
    state: string;
    progress: number;
    targetVersion: string;
}
