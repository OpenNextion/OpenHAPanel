import { writable } from 'svelte/store';

export interface IHassCoverFavorites {
    favorite_positions?: readonly number[];
    favorite_tilt_positions?: readonly number[];
}

// Missing lists use HA defaults; an explicit empty list hides the favorite control.
export const haCoverFavoritesStore = writable<ReadonlyMap<string, IHassCoverFavorites>>(new Map());

export function setHassCoverFavorites(favorites: ReadonlyMap<string, IHassCoverFavorites>) {
    haCoverFavoritesStore.set(favorites);
}

export function clearHassCoverFavorites() {
    haCoverFavoritesStore.set(new Map());
}
