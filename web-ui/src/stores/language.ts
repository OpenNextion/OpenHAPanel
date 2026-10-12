import { cloneDeep, set } from 'lodash-es';
import { get, writable } from 'svelte/store';
import ELanguage from '@/types/language';
import { locale, register } from 'svelte-i18n';
import { LANGUAGE_LIST, ROUTES_PATH } from '@/constant';
import type { ILanguageItem } from '@/types/language';
import sendForReply from '@/ws/sendForReply';
import { EWsMethods } from '@/types/websocket';

interface ILanguage {
    language: ELanguage;
    languageList: ILanguageItem[];
}

function normalizeLanguage(value: unknown): ELanguage | null {
    if (typeof value !== 'string') return null;
    const normalized = value.toLowerCase();
    if (normalized === ELanguage.EN_US || normalized.startsWith('en-')) return ELanguage.EN_US;
    if (normalized === ELanguage.ZH_CN || normalized.startsWith('zh-')) return ELanguage.ZH_CN;
    return null;
}

function resolveLanguage() {
    return ELanguage.EN_US;
}

const INIT_VALUE: ILanguage = {
    language: resolveLanguage(),
    languageList: LANGUAGE_LIST,
};

export const languageStore = writable<ILanguage>(cloneDeep(INIT_VALUE), () => initEtc());

function initEtc() {
    const language = resolveLanguage();
    console.log('initEtc: ', language);
    languageStore.set({ language, languageList: LANGUAGE_LIST });
}

languageStore.subscribe((etc) => {
    locale.set(etc.language);
    switchLanguage(etc.language);
});

export function resetMessage() {
    languageStore.set({ ...cloneDeep(INIT_VALUE), language: resolveLanguage() });
}

export function updateEtcField(field: keyof ILanguage, value: any) {
    languageStore.update((etc) => {
        const cloneWifi = cloneDeep(etc);
        set(cloneWifi, field, value);
        return cloneWifi;
    });
}

/** Keep this client and its persisted preference in sync with a device language notification. */
export function applyDeviceLanguage(language: unknown) {
    const normalizedLanguage = normalizeLanguage(language);
    if (!normalizedLanguage) return;
    updateEtcField('language', normalizedLanguage);
}

export async function setLanguage(language: ELanguage) {
    const wsResponse = await sendForReply({
        method: EWsMethods.SET_LANGUAGE,
        data: { language },
    });
    let languageLoaderAvailable = false;
    try {
        const langConfig = get(languageStore).languageList.find((item) => item.value === language);
        if (langConfig) {
            await langConfig.loader();
            languageLoaderAvailable = true;
        }
    } catch {
        languageLoaderAvailable = false;
    }
    if (wsResponse.error === 0) {
        updateEtcField('language', language);
    }
    return {
        wsResponse,
        languageLoaderAvailable,
    };
}

async function switchLanguage(langTag: ELanguage) {
    const langConfig = get(languageStore).languageList.find((config) => config.value === langTag);
    if (langConfig) {
        register(langTag, async () => {
            try {
                const response = await langConfig.loader();
                return response;
            } catch (error) {
                console.log('Get language fail !', error);
                locale.set(ELanguage.EN_US);
                return;
            }
        });
    }
    locale.set(langTag);
}
