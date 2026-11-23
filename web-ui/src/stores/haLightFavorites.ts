import { writable } from 'svelte/store';

// A missing entry means HA should use its capability-based default colors.
// An explicit empty array means the user removed all favorite colors.
export const haLightFavoritesStore = writable<ReadonlyMap<string, readonly unknown[]>>(new Map());

export function setHassLightFavorites(favorites: ReadonlyMap<string, readonly unknown[]>) {
    haLightFavoritesStore.set(favorites);
}

export function clearHassLightFavorites() {
    haLightFavoritesStore.set(new Map());
}
