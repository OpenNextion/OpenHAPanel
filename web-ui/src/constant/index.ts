import type Env from '@/types/Env';
import type { ILanguageItem } from '@/types/language';
import ELanguage from '@/types/language';
import enMessages from '@/assets/i18n/en.json';
import zhCNMessages from '@/assets/i18n/zh-cn.json';

export * from './route';
export * from './errorCode';
export * from './svgMap';
export * from './mdiIcons';
export * from './regRex';
export * from './timezone';
export * from './linkAddress';
export * from './hassEntity';
export * from './wifi';


/** current env mode */
export const ENV_MODE = import.meta.env.MODE as Env;

/** init vconsole */
export const ENV_VCONSOLE = import.meta.env.VITE_VCONSOLE as ('1' | '0');

export const NAME_MAX_LENGTH = 30;

export const PASSWORD_MAX_BYTE_SIZE = 63;

export const PASSWORD_MAX_LENGTH = 30;

export const MDNS_DOMAIN_INPUT_MAX_LENGTH = 30;

export const NTP_SERVER_MAX_LENGTH = 30;


/** support language list */
export const LANGUAGE_LIST: ILanguageItem[] = [
    {
        label: '简体中文',
        value: ELanguage.ZH_CN,
        loader: async () => zhCNMessages,
    },
    {
        label: 'English',
        value: ELanguage.EN_US,
        loader: async () => enMessages,
    },
];
