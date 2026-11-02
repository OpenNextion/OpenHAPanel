import type Env from '@/types/Env';
import type { ILanguageItem } from '@/types/language';
import ELanguage from '@/types/language';
import enMessages from '@/assets/i18n/en.json';
import deMessages from '@/assets/i18n/de.json';
import frMessages from '@/assets/i18n/fr.json';
import ruMessages from '@/assets/i18n/ru.json';
import viMessages from '@/assets/i18n/vi.json';
import daMessages from '@/assets/i18n/da.json';
import trMessages from '@/assets/i18n/tr.json';
import esMessages from '@/assets/i18n/es.json';
import itMessages from '@/assets/i18n/it.json';
import ptBRMessages from '@/assets/i18n/pt-BR.json';
import koMessages from '@/assets/i18n/ko.json';

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
        label: 'English',
        value: ELanguage.EN_US,
        loader: async () => enMessages,
    },
    { label: 'Deutsch', value: ELanguage.DE, loader: async () => deMessages },
    { label: 'Français', value: ELanguage.FR, loader: async () => frMessages },
    { label: 'Русский', value: ELanguage.RU, loader: async () => ruMessages },
    { label: 'Tiếng Việt', value: ELanguage.VI, loader: async () => viMessages },
    { label: 'Dansk', value: ELanguage.DA, loader: async () => daMessages },
    { label: 'Türkçe', value: ELanguage.TR, loader: async () => trMessages },
    { label: 'Español', value: ELanguage.ES, loader: async () => esMessages },
    { label: 'Italiano', value: ELanguage.IT, loader: async () => itMessages },
    { label: 'Português (Brasil)', value: ELanguage.PT_BR, loader: async () => ptBRMessages },
    //{ label: '한국어', value: ELanguage.KO, loader: async () => koMessages },
];
