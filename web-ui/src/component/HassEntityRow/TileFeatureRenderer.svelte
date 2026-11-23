<script lang="ts">
    import { _ } from 'svelte-i18n';
    import { ActionButtonFeature, ActionGroupFeature, ColorPickerFeature, FavoriteOptionsFeature, OptionChoiceFeature, SliderFeature, StepControlFeature, ToggleFeature, TrendGraphFeature } from '@/component/TileFeature';
    import { getClimateTargetHumidity, getClimateTargetTemperature, getCoverActions, getCoverFavoriteCurrentPosition, getFanSpeedChoices, getHassCoverFavoriteOptions, getHassLightFavorites, getLightColor, getOptionPresentation, getSliderPresentation, getTrendPoints } from '@/constant/tileFeaturePresentation';
    import { getTileFeatureLabelKey, type THassTileFeatureType } from '@/constant/tileFeatureRules';
    import type { TTileFeaturePosition } from '@/constant/tileFeaturePosition';
    import type { getHassEntityPresentation } from '@/hooks/useHassEntity';
    import { haLightFavoritesStore } from '@/stores/haLightFavorites';
    import { haCoverFavoritesStore } from '@/stores/haCoverFavorites';
    import type { IHassScreenConfigFeature } from '@/types/screenConfig';

    let {
        entityId,
        entity,
        features,
        fullWidth,
        featuresPosition,
        onToggle,
        onActivate,
    }: {
        entityId: string;
        entity: ReturnType<typeof getHassEntityPresentation>;
        features: IHassScreenConfigFeature[];
        fullWidth: boolean;
        featuresPosition: TTileFeaturePosition;
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

    function featureLabel(type: string) {
        const key = getTileFeatureLabelKey(type);
        return key ? $_(key) : type;
    }

    function lightEffectIcon(effect: unknown) {
        if (effect === 'off') return 'mdi:star-off';
        return effect === false || effect === undefined || effect === null || effect === '' ? 'mdi:creation' : 'mdi:circle-medium';
    }

    const MODE_FALLBACK_ICONS: Partial<Record<THassTileFeatureType, string>> = {
        'climate-preset-modes': 'mdi:tune-variant',
        'climate-fan-modes': 'mdi:fan',
        'climate-swing-modes': 'mdi:arrow-oscillating',
        'climate-swing-horizontal-modes': 'mdi:arrow-oscillating',
        'fan-preset-modes': 'mdi:tune-variant',
    };

    function modeSelectIcon(type: THassTileFeatureType, model: ReturnType<typeof getOptionPresentation>, effect: unknown) {
        if (type === 'light-effect') return lightEffectIcon(effect);
        const selectedOption = model.options.find((option) => option.value === model.value);
        return selectedOption ? selectedOption.icon ?? 'mdi:circle-medium' : MODE_FALLBACK_ICONS[type];
    }

    const isOn = $derived(entity.stateObject?.state === 'on');
    const isOff = $derived(entity.stateObject?.state === 'off');
    const isLight = $derived(entityId.startsWith('light.'));
    const unavailable = $derived(entity.isUnavailable);
    const featureState = $derived(unavailable && entity.stateObject ? { ...entity.stateObject, state: 'unavailable' } : entity.stateObject);
    const toneColor = $derived(`var(--screen-tone-${entity.preview.tone}-color)`);
    const toneBackground = $derived(`var(--screen-tone-${entity.preview.tone}-background)`);
    const entityIconColor = $derived(unavailable && !isLight ? 'var(--screen-icon-unavailable)' : `var(--entity-icon-color, ${toneColor})`);
    const coverFavorites = $derived($haCoverFavoritesStore.get(entityId));
    const renderedFeatures = $derived(features.filter((feature) => {
        if (feature.type === 'cover-position-favorite') {
            return getHassCoverFavoriteOptions(coverFavorites?.favorite_positions).length > 0;
        }
        if (feature.type === 'cover-tilt-favorite') {
            return getHassCoverFavoriteOptions(coverFavorites?.favorite_tilt_positions).length > 0;
        }
        return true;
    }));
</script>

<div class="tile-feature-list" style={entity.iconStyle}>
    {#each renderedFeatures as feature, index (`${feature.type}-${index}`)}
        {@const coverFavoriteOptions = getHassCoverFavoriteOptions(feature.type === 'cover-tilt-favorite' ? coverFavorites?.favorite_tilt_positions : coverFavorites?.favorite_positions)}
        {@const spansFullRow = featuresPosition === 'bottom' ? fullWidth : index === renderedFeatures.length - 1 && renderedFeatures.length % 2 === 0}
        <div class="tile-feature-item" class:full-row={spansFullRow}>
            {#if feature.type === 'toggle'}
                {@const checked = isOn}
                <ToggleFeature
                    {checked}
                    label={entity.displayName}
                    icon={getToggleIcon(checked)}
                    activeColor={isLight ? entityIconColor : toneColor}
                    activeIconColor="#fff"
                    inactiveColor={isLight && unavailable ? entityIconColor : 'var(--screen-tone-ha-inactive-color)'}
                    inactiveIconColor="#fff"
                    trackColor={isLight && unavailable ? `var(--entity-icon-background, var(--screen-icon-unavailable-background))` : checked ? (isLight ? `var(--entity-icon-background, ${toneBackground})` : toneBackground) : 'var(--screen-tone-ha-inactive-background)'}
                    disabled={unavailable}
                    readOnly={!onToggle}
                    onToggle={(checked) => onToggle?.(checked)}
                />
            {:else if feature.type === 'button'}
                <ActionButtonFeature
                    label={$_('page.screen.actionPress')}
                    backgroundColor={entity.domain === 'scene' ? 'var(--screen-scene-button-background, var(--screen-tone-ha-inactive-background))' : undefined}
                    textColor={entity.domain === 'scene' ? 'var(--screen-scene-button-color, var(--screen-text-primary))' : undefined}
                    disabled={unavailable}
                    readOnly={!onActivate}
                    onActivate={() => onActivate?.()}
                />
            {:else if feature.type === 'fan-speed'}
                {@const speedChoices = getFanSpeedChoices(featureState)}
                {#if speedChoices}
                    <OptionChoiceFeature label={featureLabel(feature.type)} {...speedChoices} variant="fan-speed" activeColor={isOff ? 'var(--screen-tone-ha-inactive-color)' : 'var(--screen-tone-ha-fan-color)'} trackColor={isOff ? 'var(--screen-tone-ha-inactive-background)' : 'var(--screen-tone-ha-fan-background)'} disabled={unavailable} readOnly={!onActivate} onSelect={() => onActivate?.()} />
                {:else}
                    <SliderFeature label={featureLabel(feature.type)} {...getSliderPresentation(feature.type, featureState)} activeColor={unavailable ? 'var(--screen-feature-disabled-fill)' : 'var(--screen-tone-ha-fan-color)'} trackColor={unavailable ? 'var(--screen-feature-disabled-background)' : isOff ? 'var(--screen-tone-ha-inactive-background)' : 'var(--screen-tone-ha-fan-background)'} showActivePart={!isOff && !unavailable} disabled={unavailable} readOnly={!onActivate} onValueInput={() => onActivate?.()} />
                {/if}
            {:else if feature.type === 'light-brightness' || feature.type === 'light-color-temp' || feature.type === 'cover-position' || feature.type === 'cover-tilt-position'}
                {@const model = getSliderPresentation(feature.type as THassTileFeatureType, featureState)}
                <SliderFeature
                    label={featureLabel(feature.type)}
                    {...model}
                    variant={feature.type === 'light-brightness' ? 'filled' : feature.type === 'light-color-temp' ? 'color-temp' : feature.type === 'cover-position' ? 'cover' : 'default'}
                    activeColor={unavailable ? 'var(--screen-feature-disabled-fill)' : feature.type === 'light-brightness' ? entityIconColor : feature.type === 'cover-position' ? 'var(--screen-tone-ha-cover-color)' : undefined}
                    trackColor={unavailable ? 'var(--screen-feature-disabled-background)' : feature.type === 'light-brightness' ? `var(--entity-icon-background, ${toneBackground})` : feature.type === 'cover-position' ? 'var(--screen-tone-ha-cover-background)' : undefined}
                    showActivePart={!(feature.type === 'light-brightness' && (isOff || unavailable))}
                    disabled={unavailable}
                    readOnly={!onActivate}
                    onValueInput={() => onActivate?.()}
                />
            {:else if feature.type === 'light-color'}
                <ColorPickerFeature label={featureLabel(feature.type)} value={getLightColor(featureState)} disabled={unavailable} readOnly={!onActivate} onSelect={() => onActivate?.()} />
            {:else if feature.type === 'light-color-favorites'}
                <FavoriteOptionsFeature label={featureLabel(feature.type)} favorites={getHassLightFavorites($haLightFavoritesStore.get(entityId), featureState)} {spansFullRow} disabled={unavailable} readOnly={!onActivate} onSelect={() => onActivate?.()} />
            {:else if feature.type === 'target-temperature'}
                {@const temperature = getClimateTargetTemperature(featureState)}
                <div class="climate-target-controls" class:range={temperature.targets.length > 1}>
                    {#each temperature.targets as target (target.key)}
                        {#if feature.data?.style === 'slider'}
                            <SliderFeature label={target.key} value={target.value ?? target.min} min={target.min} max={target.max} step={target.step} valueText={target.value === undefined ? target.unit : `${target.value} ${target.unit}`} variant="filled" activeColor={unavailable || target.value === undefined ? 'var(--screen-feature-disabled-fill)' : toneColor} trackColor={unavailable || target.value === undefined ? 'var(--screen-feature-disabled-background)' : toneBackground} showActivePart={!unavailable && target.value !== undefined} disabled={unavailable || target.value === undefined} readOnly={!onActivate} onValueInput={() => onActivate?.()} />
                        {:else}
                            <StepControlFeature label={target.key === 'temperature' ? featureLabel(feature.type) : target.key} value={target.value} min={target.min} max={target.max} step={target.step} unit={target.unit} compact={temperature.targets.length > 1} roomy={fullWidth && temperature.targets.length > 1} narrow={featuresPosition === 'inline' && !spansFullRow && temperature.targets.length === 1} disabled={unavailable || target.value === undefined} readOnly={!onActivate} onChange={() => onActivate?.()} />
                        {/if}
                    {/each}
                </div>
            {:else if feature.type === 'target-humidity'}
                {@const humidity = getClimateTargetHumidity(featureState)}
                {#if feature.data?.style === 'buttons'}
                    <StepControlFeature label={featureLabel(feature.type)} value={humidity.value} min={humidity.min} max={humidity.max} step={humidity.step} unit="%" narrow={featuresPosition === 'inline' && !spansFullRow} disabled={unavailable} readOnly={!onActivate} onChange={() => onActivate?.()} />
                {:else}
                    <SliderFeature label={featureLabel(feature.type)} {...humidity} variant="filled" activeColor={unavailable ? 'var(--screen-feature-disabled-fill)' : toneColor} trackColor={unavailable ? 'var(--screen-feature-disabled-background)' : toneBackground} showActivePart={!unavailable} disabled={unavailable} readOnly={!onActivate} onValueInput={() => onActivate?.()} />
                {/if}
            {:else if feature.type === 'cover-position-favorite' || feature.type === 'cover-tilt-favorite'}
                {@const currentPosition = getCoverFavoriteCurrentPosition(feature.type, entity.stateObject)}
                <OptionChoiceFeature label={featureLabel(feature.type)} value={currentPosition === undefined ? undefined : String(currentPosition)} options={coverFavoriteOptions} variant="favorites" {spansFullRow} activeColor={toneColor} disabled={unavailable} readOnly={!onActivate} onSelect={() => onActivate?.()} />
            {:else if feature.type === 'cover-open-close' || feature.type === 'cover-tilt'}
                <ActionGroupFeature label={featureLabel(feature.type)} actions={getCoverActions(feature.type, featureState)} disabled={unavailable} readOnly={!onActivate} onAction={() => onActivate?.()} />
            {:else if feature.type === 'fan-oscillate'}
                <OptionChoiceFeature
                    label={featureLabel(feature.type)}
                    value={entity.stateObject?.attributes?.oscillating === true ? 'on' : 'off'}
                    options={[{ value: 'off', label: 'Off', icon: 'mdi:arrow-oscillating-off' }, { value: 'on', label: 'On', icon: 'mdi:arrow-oscillating' }]}
                    variant="fan-controls"
                    activeColor={toneColor}
                    disabled={unavailable}
                    readOnly={!onActivate}
                    onSelect={() => onActivate?.()}
                />
            {:else if feature.type === 'light-effect' || feature.type === 'climate-hvac-modes' || feature.type === 'climate-preset-modes' || feature.type === 'climate-fan-modes' || feature.type === 'climate-swing-modes' || feature.type === 'climate-swing-horizontal-modes' || feature.type === 'fan-preset-modes' || feature.type === 'fan-direction'}
                {@const model = getOptionPresentation(feature.type as THassTileFeatureType, featureState)}
                {#if feature.type === 'light-effect' && model.options.length === 0}
                    <span class="tile-feature-placeholder" aria-hidden="true"></span>
                {:else}
                    <OptionChoiceFeature label={featureLabel(feature.type)} {...model} variant={feature.type === 'fan-direction' ? 'fan-controls' : model.variant} selectIcon={modeSelectIcon(feature.type as THassTileFeatureType, model, featureState?.attributes?.effect)} placeholder={feature.type === 'light-effect' ? 'Effect' : undefined} selectLabel={feature.type === 'light-effect' ? 'Effect' : undefined} activeColor={toneColor} disabled={unavailable} readOnly={!onActivate} onSelect={() => onActivate?.()} />
                {/if}
            {:else if feature.type === 'trend-graph'}
                <TrendGraphFeature {entityId} label={entity.displayName} points={getTrendPoints(feature)} valueText={entity.displayState} lineColor={entityIconColor} />
            {/if}
        </div>
    {/each}
</div>

<style lang="scss">
    .climate-target-controls { display: flex; width: 100%; min-width: 0; gap: 0.5rem; }
    .climate-target-controls.range { gap: 0.25rem; }
    .climate-target-controls > :global(*) { min-width: 0; flex: 1 1 0; }
    .tile-feature-placeholder { display: block; width: 100%; height: var(--feature-height, 2.625rem); }
</style>
