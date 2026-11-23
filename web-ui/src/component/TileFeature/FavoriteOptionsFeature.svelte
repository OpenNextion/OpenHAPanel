<script lang="ts">
    import { getTileFavoriteVisibleCount } from '@/constant/tileFeaturePosition';
    import { deviceStore } from '@/stores/device';

    export interface Favorite { id: string; label: string; color?: string; position?: number }

    let {
        label,
        favorites,
        value,
        spansFullRow = false,
        disabled = false,
        readOnly = false,
        onSelect,
    }: {
        label: string;
        favorites: Favorite[];
        value?: string;
        spansFullRow?: boolean;
        disabled?: boolean;
        readOnly?: boolean;
        onSelect?: (id: string) => void;
    } = $props();

    const visibleCount = $derived(getTileFavoriteVisibleCount('light', $deviceStore.screenRotation.rotation === 90, spansFullRow));
    const visibleFavorites = $derived(favorites.slice(0, visibleCount));
</script>

<div class="tile-favorites" class:unavailable={disabled} role="group" aria-label={label}>
    {#if favorites.length}
        {#each visibleFavorites as favorite (favorite.id)}
            <button type="button" disabled={disabled || readOnly} class:color-swatch={Boolean(favorite.color)} class:selected={value === favorite.id} title={favorite.label} aria-label={favorite.label} aria-pressed={value === favorite.id} aria-disabled={disabled || readOnly} tabindex={disabled || readOnly ? -1 : 0} style={favorite.color ? `--favorite-color: ${favorite.color}` : ''} onclick={(event) => { event.stopPropagation(); if (!disabled && !readOnly) onSelect?.(favorite.id); }} onkeydown={(event) => event.stopPropagation()}>{favorite.color ? '' : (favorite.position !== undefined ? `${favorite.position}%` : favorite.label)}</button>
        {/each}
    {:else}
        <span class="empty">—</span>
    {/if}
</div>

<style lang="scss">
    .tile-favorites { display: flex; width: 100%; min-width: 0; min-height: var(--feature-height, 2.625rem); align-items: center; gap: 0.5rem; overflow: hidden; }
    button { min-width: 2rem; height: var(--feature-height, 2.625rem); padding: 0 0.45rem; flex: 1 1 0; border: 0; border-radius: var(--feature-border-radius, 0.75rem); background: var(--screen-icon-unavailable-background); color: var(--screen-text-primary); font: inherit; font-size: 0.75rem; cursor: pointer; }
    button.color-swatch { min-width: 0; padding: 0; border: 1px solid var(--screen-tile-border); background: var(--favorite-color); }
    button.selected { outline: 0.125rem solid var(--screen-focus-outline); outline-offset: -0.125rem; }
    button[aria-disabled='true'] { cursor: default; }
    .tile-favorites.unavailable button { cursor: not-allowed; }
    .tile-favorites.unavailable button.color-swatch { filter: grayscale(1) opacity(0.5); }
    .tile-favorites.unavailable button.selected { outline-color: var(--screen-feature-disabled-color); }
    .empty { display: grid; width: 100%; height: var(--feature-height, 2.625rem); place-items: center; border-radius: var(--feature-border-radius, 0.75rem); background: var(--screen-icon-unavailable-background); color: var(--screen-text-secondary); }
</style>
