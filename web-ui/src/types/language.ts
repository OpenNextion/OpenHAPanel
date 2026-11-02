enum ELanguage {
    EN_US = 'en',
    DE = 'de',
    FR = 'fr',
    RU = 'ru',
    VI = 'vi',
    DA = 'da',
    TR = 'tr',
    ES = 'es',
    IT = 'it',
    PT_BR = 'pt-BR',
    KO = 'ko',
}

export default ELanguage;

export interface ILanguageItem {
    label: string;
    value: ELanguage;
    loader?: () => Promise<Record<string, unknown>>;
}
