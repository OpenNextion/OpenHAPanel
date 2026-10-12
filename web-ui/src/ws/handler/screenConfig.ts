import { setScreenConfigs } from '@/stores/screenConfig';
import type { IHassScreenConfigNotify } from '@/types/screenConfig';
import type { IResponse } from '@/types/websocket';

export const handleScreenConfigNotify = async (params: IResponse<IHassScreenConfigNotify>) => {
    setScreenConfigs(params.data.screens);
};
