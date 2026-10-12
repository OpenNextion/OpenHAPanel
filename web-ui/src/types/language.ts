enum ELanguage {
    ZH_CN = 'zh',
    EN_US = 'en',
}

export default ELanguage;

export interface ILanguageItem {
    label: string;
    value: ELanguage;
    loader: any;
}
