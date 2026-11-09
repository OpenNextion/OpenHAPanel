<script lang="ts">
    import { _ } from 'svelte-i18n';
    import { ActionButtonFeature, ToggleFeature } from '@/component/TileFeature';
    import type { getHassEntityPresentation } from '@/hooks/useHassEntity';
    import type { IHassScreenConfigFeature } from '@/types/screenConfig';

    let {
        entityId,
        entity,
        features,
        onToggle,
        onActivate,
    }: {
        entityId: string;
        entity: ReturnType<typeof getHassEntityPresentation>;
        features: IHassScreenConfigFeature[];
        onToggle?: (checked: boolean) => void;
        onActivate?: () => void;
    } = $props();

    // HA Tile toggle controls use action icons, independently of the entity's main icon.
    const TOGGLE_ICONS: Record<string, { on: string; off: string }> = {
        light: { on: 'mdi:lightbulb-on', off: 'mdi:lightbulb-off' },
        fan: { on: 'mdi:fan', off: 'mdi:fan-off' },
        siren: { on: 'mdi:volume-high', off: 'mdi:volume-off' },
    };

    function getToggleIcon(checked: boolean) {
        const domain = entityId.split('.')[0];
        const icons = TOGGLE_ICONS[domain] ?? { on: 'mdi:power', off: 'mdi:power-off' };
        return checked ? icons.on : icons.off;
    }

    const isOn = $derived(entity.stateObject?.state === 'on');
    const toneColor = $derived(`var(--screen-tone-${entity.preview.tone}-color)`);
    const toneBackground = $derived(`var(--screen-tone-${entity.preview.tone}-background)`);
</script>

<div class="tile-feature-list">
    {#each features as feature, index (`${feature.type}-${index}`)}
        <div class="tile-feature-item">
            {#if feature.type === 'toggle'}
                <ToggleFeature
                    checked={isOn}
                    label={entity.displayName}
                    icon={getToggleIcon(isOn)}
                    activeColor={toneColor}
                    activeIconColor="#fff"
                    inactiveColor="var(--screen-tone-ha-inactive-color)"
                    inactiveIconColor="#fff"
                    trackColor={isOn ? toneBackground : 'var(--screen-tone-ha-inactive-background)'}
                    readOnly={!onToggle}
                    onToggle={(checked) => onToggle?.(checked)}
                />
            {:else if feature.type === 'button'}
                <ActionButtonFeature
                    label={$_('page.screen.actionPress')}
                    readOnly={!onActivate}
                    onActivate={() => onActivate?.()}
                />
            {/if}
        </div>
    {/each}
</div>
