<script lang="ts">
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';
    import type { getHassEntityPresentation } from '@/hooks/useHassEntity';

    let {
        entity,
        showBackground,
        showWarning,
    }: {
        entity: ReturnType<typeof getHassEntityPresentation>;
        showBackground: boolean;
        showWarning: boolean;
    } = $props();
</script>

<span
    class:unavailable={entity.isUnavailable}
    class:no-icon-background={!showBackground}
    class="entity-icon tone-{entity.preview.tone}"
    style={entity.iconStyle}
>
    <SvgIcon name={entity.icon} />
    {#if entity.climateActionIndicator}
        <span class="climate-action-indicator action-{entity.climateActionIndicator.tone}" aria-hidden="true">
            <SvgIcon name={entity.climateActionIndicator.icon} />
        </span>
    {/if}
    {#if showWarning && entity.isUnavailable}<span class="entity-warning" aria-hidden="true">!</span>{/if}
</span>
