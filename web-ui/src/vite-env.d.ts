/// <reference types="svelte" />
/// <reference types="vite/client" />

interface ImportMetaEnv {
    readonly VITE_DEFAULT_NTP_SERVER_1?: string;
    readonly VITE_DEFAULT_NTP_SERVER_2?: string;
}

interface ImportMeta {
    readonly env: ImportMetaEnv;
}
