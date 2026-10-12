import { cloneDeep } from 'lodash-es';
import { writable } from 'svelte/store';
import type { IHassScreenActionConfig, IHassScreenConfigBadge, IHassScreenConfigCard, IHassScreenConfigItem } from '@/types/screenConfig';

interface IScreenConfigStore {
    screens: IHassScreenConfigItem[];
}

const INIT_VALUE: IScreenConfigStore = {
    screens: [],
};

export const screenConfigStore = writable<IScreenConfigStore>(cloneDeep(INIT_VALUE));

export function setScreenConfigs(screens: IHassScreenConfigItem[]) {
    screenConfigStore.set({ screens: cloneDeep(screens) });
}

export function updateScreenConfigs(screens: IHassScreenConfigItem[]) {
    screenConfigStore.update((store) => ({ ...store, screens: cloneDeep(screens) }));
}

export function updateScreenTitle(index: number, title: string) {
    screenConfigStore.update((store) => ({
        ...store,
        screens: store.screens.map((screen) => (screen.index === index ? { ...screen, title } : screen)),
    }));
}

export function removeScreenConfig(index: number) {
    screenConfigStore.update((store) => ({
        ...store,
        screens: store.screens.filter((screen) => screen.index !== index).map((screen, nextIndex) => ({ ...screen, index: nextIndex })),
    }));
}

export function reorderScreenConfigs(sourceIndex: number, targetIndex: number) {
    if (sourceIndex === targetIndex) return;
    screenConfigStore.update((store) => {
        const screens = cloneDeep(store.screens);
        const sourcePosition = screens.findIndex((screen) => screen.index === sourceIndex);
        const targetPosition = screens.findIndex((screen) => screen.index === targetIndex);
        if (sourcePosition < 0 || targetPosition < 0) return store;

        const [movedScreen] = screens.splice(sourcePosition, 1);
        screens.splice(targetPosition, 0, movedScreen);
        return {
            ...store,
            screens: screens.map((screen, index) => ({ ...screen, index })),
        };
    });
}

export function reorderScreenCards(index: number, sourceEntityId: string, targetEntityId: string) {
    if (sourceEntityId === targetEntityId) return;
    screenConfigStore.update((store) => ({
        ...store,
        screens: store.screens.map((screen) => {
            if (screen.index !== index) return screen;
            const cards = cloneDeep(screen.cards ?? []);
            const sourcePosition = cards.findIndex((card) => card.data.entity === sourceEntityId);
            const targetPosition = cards.findIndex((card) => card.data.entity === targetEntityId);
            if (sourcePosition < 0 || targetPosition < 0) return screen;

            const [movedCard] = cards.splice(sourcePosition, 1);
            cards.splice(targetPosition, 0, movedCard);
            return { ...screen, cards };
        }),
    }));
}

export function addScreenCard(index: number, card: IHassScreenConfigCard) {
    screenConfigStore.update((store) => ({
        ...store,
        screens: store.screens.map((screen) => {
            if (screen.index !== index) return screen;
            const cards = screen.cards ?? [];
            if (cards.some((item) => item.data.entity === card.data.entity)) return screen;
            return { ...screen, cards: [cloneDeep(card), ...cards] };
        }),
    }));
}

export function addScreenBadge(index: number, badge: IHassScreenConfigBadge) {
    screenConfigStore.update((store) => ({
        ...store,
        screens: store.screens.map((screen) => {
            if (screen.index !== index) return screen;
            const badges = screen.badges ?? [];
            if (badges.some((item) => item.data.entity === badge.data.entity)) return screen;
            return { ...screen, badges: [...badges, cloneDeep(badge)] };
        }),
    }));
}

export function removeScreenCard(index: number, entityId: string) {
    screenConfigStore.update((store) => ({
        ...store,
        screens: store.screens.map((screen) =>
            screen.index === index ? { ...screen, cards: (screen.cards ?? []).filter((card) => card.data.entity !== entityId) } : screen,
        ),
    }));
}

export function updateScreenCardActionConfig(index: number, entityId: string, actionConfig: IHassScreenActionConfig) {
    screenConfigStore.update((store) => ({
        ...store,
        screens: store.screens.map((screen) =>
            screen.index === index
                ? {
                      ...screen,
                      cards: (screen.cards ?? []).map((card) =>
                          card.data.entity === entityId ? { ...card, data: { ...card.data, actionConfig: cloneDeep(actionConfig) } } : card,
                      ),
                  }
                : screen,
        ),
    }));
}

export function updateScreenCard(index: number, currentEntityId: string, entityId: string, actionConfig: IHassScreenActionConfig) {
    screenConfigStore.update((store) => ({
        ...store,
        screens: store.screens.map((screen) =>
            screen.index === index
                ? {
                      ...screen,
                      cards: (screen.cards ?? []).map((card) =>
                          card.data.entity === currentEntityId
                              ? { ...card, data: { ...card.data, entity: entityId, actionConfig: cloneDeep(actionConfig) } }
                              : card,
                      ),
                  }
                : screen,
        ),
    }));
}
