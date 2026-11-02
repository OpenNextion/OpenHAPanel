import { fromStore } from 'svelte/store';
import { languageStore, setLanguage } from '@/stores/language';
import type ELanguage from '@/types/language';

export function useLanguageSettings() {
    const language = fromStore(languageStore);
    const state = $state({ selected: language.current.language, loading: false });

    $effect(() => {
        const currentLanguage = language.current.language;
        if (!state.loading) state.selected = currentLanguage;
    });

    async function save(nextLanguage: ELanguage = state.selected): Promise<boolean> {
        if (state.loading) return false;
        if (nextLanguage === language.current.language) return true;

        state.selected = nextLanguage;
        state.loading = true;
        try {
            const { wsResponse } = await setLanguage(nextLanguage);
            if (wsResponse.error === 0) return true;
        } catch (error) {
            console.error('Failed to update language:', error);
        } finally {
            state.loading = false;
            state.selected = language.current.language;
        }
        return false;
    }

    return {
        state,
        get options() {
            return language.current.languageList.map(({ label, value }) => ({ label, value }));
        },
        save,
    };
}
