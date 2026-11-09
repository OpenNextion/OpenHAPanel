<script lang="ts">
    import type { getHassEntityPresentation } from '@/hooks/useHassEntity';
    import HassEntityIcon from './HassEntityIcon.svelte';

    let {
        entity,
        onclick,
        interactiveElement,
        compactPreview,
    }: {
        entity: ReturnType<typeof getHassEntityPresentation>;
        onclick?: () => void;
        interactiveElement: 'button' | 'div';
        compactPreview: boolean;
    } = $props();

    const badgeValue = $derived.by(() => {
        const value = entity.displayState;
        if (!entity.badgeUsesName || entity.isUnavailable) return value;
        const characters = Array.from(value);
        return characters.length > 10 ? `${characters.slice(0, 10).join('')}…` : value;
    });

    function handleKeydown(event: KeyboardEvent) {
        if (event.key !== 'Enter' && event.key !== ' ') return;
        event.preventDefault();
        onclick?.();
    }
</script>

{#snippet content()}
    <HassEntityIcon {entity} showBackground={false} showWarning={false} />
    <strong class="badge-value">{badgeValue}</strong>
{/snippet}

{#if onclick && interactiveElement === 'button'}
    <button class:compact-preview={compactPreview} class="hass-entity-row badge clickable" type="button" {onclick}>{@render content()}</button>
{:else if onclick}
    <div class:compact-preview={compactPreview} class="hass-entity-row badge clickable" role="button" tabindex="0" {onclick} onkeydown={handleKeydown}>{@render content()}</div>
{:else}
    <div class:compact-preview={compactPreview} class="hass-entity-row badge">{@render content()}</div>
{/if}
