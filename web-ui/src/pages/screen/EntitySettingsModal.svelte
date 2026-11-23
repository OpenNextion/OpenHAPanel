<script lang="ts">
    import { _ } from 'svelte-i18n';
    import { dragHandle, dragHandleZone, SHADOW_ITEM_MARKER_PROPERTY_NAME, type DndEvent, type Item as DndItem } from 'svelte-dnd-action';
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';
    import HassEntityRow from '@/component/HassEntityRow/HassEntityRow.svelte';
    import Modal from '@/component/Modal/Modal.svelte';
    import DropDown from '@/component/Dropdown/DropDown.svelte';
    import { getHassEntityPresentation, normalizeHassEntityAction } from '@/hooks/useHassEntity';
    import { getEditableTileFeatureDefinitions, getRenderableTileFeatures } from '@/constant/tileFeatureRules';
    import { getTileFeaturePosition, type TTileFeaturePosition } from '@/constant/tileFeaturePosition';
    import type { IHassScreenConfigFeature } from '@/types/screenConfig';
    import { haRegistryStore, haRegistryTreeStore } from '@/stores/haRegistry';
    import { EMessageType, sendMessage } from '@/stores/message';
    import { screenConfigStore, shouldShowScreenEntityIconBackground } from '@/stores/screenConfig';
    import {
        deleteScreenEntity,
        getScreenActionOptions,
        getScreenEntityActionSelections,
        saveScreenEntitySettings,
        searchScreenEntities,
        type TActionSelection,
        type TEntityPickerMode,
    } from '@/hooks/useScreenConfiguration';

    interface Props {
        visible: boolean;
        mode: TEntityPickerMode;
        screenIndex: number | null;
        entityId: string;
        onclose: () => void;
    }

    let { visible, mode, screenIndex, entityId, onclose }: Props = $props();
    let tapAction = $state<TActionSelection>('default');
    let holdAction = $state<TActionSelection>('default');
    let iconTapAction = $state<TActionSelection>('default');
    let activeTab = $state<'config' | 'layout'>('config');
    let fullWidth = $state(true);
    let draftEntityId = $state('');
    let entityQuery = $state('');
    let selectedFeatures = $state<IHassScreenConfigFeature[]>([]);
    let featureDndItems = $state<TFeatureItem[] | null>(null);
    let featureToAdd = $state('');
    let featuresPosition = $state<TTileFeaturePosition>('bottom');
    let initializedKey = '';
    type TFeatureItem = DndItem & { id: string; value: IHassScreenConfigFeature };
    const layoutTabAvailable = $derived(mode === 'entity');
    const entityResults = $derived.by(() => {
        void $haRegistryTreeStore;
        return searchScreenEntities(entityQuery);
    });
    const entityOptions = $derived(entityResults.map((result) => ({ label: result.displayName, value: result.entity.id })));
    const draftEntity = $derived(getHassEntityPresentation(draftEntityId, $haRegistryStore, { variant: mode }));
    const interactions = $derived(mode === 'badge' ? draftEntity.config?.badgeInteractions ?? draftEntity.interactions : draftEntity.interactions);
    const longPressSupported = $derived(interactions.hold.supported);
    const tapOptions = $derived(getScreenActionOptions(interactions.tap, $_));
    const holdOptions = $derived(getScreenActionOptions(interactions.hold, $_));
    const iconTapOptions = $derived(getScreenActionOptions(draftEntity.interactions.iconTap, $_));
    const tapLabel = $derived(tapOptions.find((option) => option.value === tapAction)?.label ?? tapAction);
    const holdLabel = $derived(holdOptions.find((option) => option.value === holdAction)?.label ?? holdAction);
    const iconTapLabel = $derived(iconTapOptions.find((option) => option.value === iconTapAction)?.label ?? iconTapAction);
    const draftEntitySupported = $derived(draftEntity.supported);
    const availableFeatureEditors = $derived(mode === 'entity' ? getEditableTileFeatureDefinitions(draftEntityId, draftEntity.stateObject) : []);
    const availableFeatureOptions = $derived(availableFeatureEditors
        .filter((feature) => !selectedFeatures.some((selected) => selected.type === feature.type))
        .map((feature) => ({ label: $_(feature.labelKey), value: feature.type })));
    const featureItems: TFeatureItem[] = $derived(featureDndItems ?? selectedFeatures.map((feature) => ({ id: feature.type, value: feature })));
    const previewFeatures = $derived(getRenderableTileFeatures(draftEntityId, draftEntity.stateObject, selectedFeatures));
    const configuredEntityContext = $derived(draftEntity.context);
    const previewActionConfig = $derived(
        iconTapAction === 'default' ? undefined : { icon_tap_action: { action: iconTapAction } },
    );

    $effect(() => {
        if (!layoutTabAvailable && activeTab === 'layout') activeTab = 'config';
    });

    $effect(() => {
        if (!visible || screenIndex === null || !entityId) {
            initializedKey = '';
            return;
        }
        const key = `${mode}:${screenIndex}:${entityId}`;
        if (initializedKey === key) return;
        initializedKey = key;
        activeTab = 'config';
        const currentCard = $screenConfigStore.screens
            .find((screen) => screen.index === screenIndex)
            ?.cards?.find((card) => card.data.entity === entityId);
        const cardFeatures = currentCard?.data.features ?? [];
        selectedFeatures = cardFeatures.map((feature) => ({ ...feature, data: { ...feature.data } }));
        featureDndItems = null;
        featureToAdd = '';
        featuresPosition = cardFeatures.length ? getTileFeaturePosition(currentCard?.data.features_position) : 'bottom';
        fullWidth = featuresPosition === 'inline' ? true : (currentCard?.data.full_width ?? true);
        const actions = getScreenEntityActionSelections(mode, screenIndex, entityId);
        const entity = getHassEntityPresentation(entityId, $haRegistryStore, { variant: mode });
        const entityInteractions = mode === 'badge' ? entity.config?.badgeInteractions ?? entity.interactions : entity.interactions;
        tapAction = normalizeHassEntityAction(actions.tap, entityId, $haRegistryStore, 'tap', mode);
        holdAction = entityInteractions.hold.supported ? normalizeHassEntityAction(actions.hold, entityId, $haRegistryStore, 'hold', mode) : 'default';
        iconTapAction = normalizeHassEntityAction(actions.iconTap, entityId, $haRegistryStore, 'iconTap');
        draftEntityId = entityId;
        entityQuery = '';
    });

    function save() {
        if (screenIndex === null || !entityId || !draftEntityId) return;
        saveScreenEntitySettings(
            mode,
            screenIndex,
            entityId,
            draftEntityId,
            draftEntity.displayName,
            tapAction,
            longPressSupported ? holdAction : 'default',
            iconTapAction,
            mode === 'entity' ? fullWidth : undefined,
            mode === 'entity' ? selectedFeatures : undefined,
            mode === 'entity' ? featuresPosition : undefined,
        );
        onclose();
    }

    function handleFeatureConsider(event: CustomEvent<DndEvent<TFeatureItem>>) {
        featureDndItems = event.detail.items;
    }

    function handleFeatureFinalize(event: CustomEvent<DndEvent<TFeatureItem>>) {
        selectedFeatures = event.detail.items
            .filter((item) => !item[SHADOW_ITEM_MARKER_PROPERTY_NAME])
            .map((item) => item.value);
        featureDndItems = null;
    }

    function addFeature(type: string) {
        if (!type || selectedFeatures.some((feature) => feature.type === type)) return;
        selectedFeatures = [...selectedFeatures, { type, data: {} }];
        featureDndItems = null;
        featureToAdd = '';
    }

    function removeFeature(type: string) {
        selectedFeatures = selectedFeatures.filter((feature) => feature.type !== type);
        featureDndItems = null;
        if (!selectedFeatures.length) featuresPosition = 'bottom';
    }

    function setFeaturesPosition(position: TTileFeaturePosition) {
        featuresPosition = position;
        if (position === 'inline') fullWidth = true;
    }

    function setFullWidth(value: boolean) {
        if (featuresPosition === 'inline') return;
        fullWidth = value;
    }

    function remove() {
        if (screenIndex === null || !entityId) return;
        deleteScreenEntity(mode, screenIndex, entityId);
        onclose();
    }

    function selectEntity(entityId: string, previousEntityId?: string) {
        const entity = entityResults.find((result) => result.entity.id === entityId)?.entity;
        if (!entity) return;
        if (!getHassEntityPresentation(entity.id, $haRegistryStore).supported) {
            sendMessage(EMessageType.ERROR, $_('page.screen.entityNotSupported'));
            return;
        }
        const changed = entity.id !== (previousEntityId ?? draftEntityId);
        draftEntityId = entity.id;
        entityQuery = '';
        if (changed) {
            tapAction = 'default';
            holdAction = 'default';
            iconTapAction = 'default';
            selectedFeatures = [];
            featuresPosition = 'bottom';
        }
    }
</script>

<Modal modalVisible={visible} footer={false} mobileSheet={true} setVisible={(nextVisible) => !nextVisible && onclose()}>
    {#snippet customSnippet()}
        <div class="entity-settings-dialog" role="dialog" aria-modal="true" aria-label={$_('page.screen.entitySettings')}>
            <header>
                <button type="button" aria-label={$_('common.close')} onclick={onclose}><SvgIcon name="mdi:close" /></button>
                <strong>{$_(mode === 'badge' ? 'page.screen.badgeConfiguration' : 'page.screen.tileCardConfiguration')}</strong>
            </header>
            <div class="entity-settings-dialog-content">
                <div class="entity-settings">
                    {#if layoutTabAvailable}
                        <div class="entity-settings-tabs" role="tablist">
                            <button class:active={activeTab === 'config'} type="button" role="tab" aria-selected={activeTab === 'config'} onclick={() => (activeTab = 'config')}>
                                {$_('page.screen.config')}
                            </button>
                            <button class:active={activeTab === 'layout'} type="button" role="tab" aria-selected={activeTab === 'layout'} onclick={() => (activeTab = 'layout')}>
                                {$_('page.screen.layout')}
                            </button>
                        </div>
                    {/if}
                    {#if !layoutTabAvailable || activeTab === 'config'}
                    <section class="configured-entity">
                        <span>{$_('page.screen.entity')}</span>
                        <DropDown
                            id="entity-settings-entity"
                            bind:value={draftEntityId}
                            options={entityOptions}
                            searchable={true}
                            searchInMenu={true}
                            bind:searchValue={entityQuery}
                            searchPlaceholder={$_('page.screen.searchEntity')}
                            noOptionsText={$_('page.screen.noEntities')}
                            ariaLabel={$_('page.screen.searchEntity')}
                            onchange={selectEntity}
                        >
                            <span class="configured-entity-value">
                                <SvgIcon name={draftEntity.icon} />
                                <span><strong>{draftEntity.displayName}</strong><small>{configuredEntityContext}</small></span>
                                <SvgIcon name="mdi:close" />
                                <SvgIcon name="arrowRight.svg" />
                            </span>
                            {#snippet optionSnippet(option)}
                                {@const optionEntity = getHassEntityPresentation(String(option.value), $haRegistryStore)}
                                {@const entity = entityResults.find((result) => result.entity.id === String(option.value))?.entity}
                                {@const device = entity?.deviceId ? $haRegistryStore.devices.find((item) => item.id === entity.deviceId) : undefined}
                                {@const domain = String(option.value).split('.', 1)[0] ?? ''}
                                {@const entityCategory = $haRegistryStore.domainTranslations[`component.${domain}.title`] ?? domain}
                                <SvgIcon name={optionEntity.icon} />
                                <span class="entity-search-option-copy">
                                    <strong>{entity?.raw.en?.trim() || optionEntity.displayName}</strong>
                                    <small>{device?.name_by_user || device?.name || optionEntity.context || option.value}</small>
                                </span>
                                <span class="entity-search-option-category">{entityCategory}</span>
                            {/snippet}
                        </DropDown>
                    </section>
                    <details class="entity-config-section" open>
                        <summary
                            ><span class="section-leading-icon"><SvgIcon name="interactions.svg" /></span><span>{$_('page.screen.interactions')}</span><span class="section-chevron"
                                ><SvgIcon name="arrowRight.svg" /></span
                            ></summary
                        >
                        <div class="interaction-fields">
                            <div class="behavior-field">
                                <span>{$_('page.screen.tapBehavior')}</span><DropDown bind:value={tapAction} options={tapOptions} ariaLabel={$_('page.screen.tapBehavior')}><span>{tapLabel}</span></DropDown>
                            </div>
                            {#if mode === 'entity'}
                                <div class="behavior-field">
                                    <span>{$_('page.screen.iconTapBehavior')}</span><DropDown bind:value={iconTapAction} options={iconTapOptions} ariaLabel={$_('page.screen.iconTapBehavior')}
                                        ><span>{iconTapLabel}</span></DropDown
                                    >
                                </div>
                            {/if}
                            {#if longPressSupported}
                                <div class="behavior-field">
                                    <span>{$_('page.screen.longPressBehavior')}</span><DropDown bind:value={holdAction} options={holdOptions} ariaLabel={$_('page.screen.longPressBehavior')}
                                        ><span>{holdLabel}</span></DropDown
                                    >
                                </div>
                            {/if}
                        </div>
                    </details>
                    {#if mode === 'entity'}
                        <details class="entity-config-section" open>
                            <summary
                                ><span class="section-leading-icon"><SvgIcon name="features.svg" /></span><span>{$_('page.screen.features')}</span><span class="section-chevron"
                                    ><SvgIcon name="arrowRight.svg" /></span
                                ></summary
                            >
                            {#if availableFeatureEditors.length}
                                {#if selectedFeatures.length}
                                    <div
                                        class="selected-feature-list"
                                        role="list"
                                        aria-label={$_('page.screen.features')}
                                        use:dragHandleZone={{ items: featureItems, type: 'entity-features', dropFromOthersDisabled: true, morphDisabled: true, dropTargetStyle: { outline: 'none' }, delayTouchStart: 250 }}
                                        onconsider={handleFeatureConsider}
                                        onfinalize={handleFeatureFinalize}
                                    >
                                        {#each featureItems as item (`${item.id}-${item[SHADOW_ITEM_MARKER_PROPERTY_NAME] ? 'shadow' : 'item'}`)}
                                            {@const feature = availableFeatureEditors.find((entry) => entry.type === item.value.type)}
                                            {#if feature}
                                                <div class="selected-feature-row" role="listitem">
                                                    <span use:dragHandle class="feature-drag-handle" aria-hidden="true"><SvgIcon name="mdi:drag-horizontal-variant" /></span>
                                                    <span class="selected-feature-label">{$_(feature.labelKey)}</span>
                                                <button class="remove-feature-button" type="button" aria-label={`${$_('page.screen.deleteEntity')}: ${$_(feature.labelKey)}`} onclick={() => removeFeature(feature.type)}><SvgIcon name="mdi:delete" /></button>
                                                </div>
                                            {/if}
                                        {/each}
                                    </div>
                                {/if}
                                <div class="add-feature-dropdown">
                                    <DropDown
                                        id="entity-settings-add-feature"
                                        bind:value={featureToAdd}
                                        options={availableFeatureOptions}
                                        disabled={!availableFeatureOptions.length}
                                        ariaLabel={$_('page.screen.addFeature')}
                                        onchange={addFeature}
                                    >
                                        <span class="add-feature-trigger"><SvgIcon name="mdi:plus" /><span>{$_('page.screen.addFeature')}</span></span>
                                    </DropDown>
                                </div>
                            {:else}
                                <div class="feature-placeholder" role="status">
                                    <SvgIcon name="mdi:alert-circle-outline" />
                                    <span>{$_('page.screen.comingSoon')}</span>
                                </div>
                            {/if}
                            {#if selectedFeatures.length}
                                <fieldset class="features-position-setting">
                                    <legend>{$_('page.screen.featuresPosition')}</legend>
                                    <div class="features-position-options">
                                        <label class:active={featuresPosition === 'bottom'} class="features-position-option">
                                            <span class="features-position-radio">
                                                <input type="radio" name="features-position" value="bottom" checked={featuresPosition === 'bottom'} onchange={() => setFeaturesPosition('bottom')} />
                                                <span class="features-position-radio-icon" aria-hidden="true"><SvgIcon name="mdi:circle" size="12px" /></span>
                                            </span>
                                            <span class="features-position-copy"><strong>{$_('page.screen.featuresPositionBottom')}</strong><small>{$_('page.screen.featuresPositionBottomHint')}</small></span>
                                            <svg class="features-position-example" viewBox="0 0 96 48" aria-hidden="true" focusable="false">
                                                <rect class="example-frame" x="1" y="1" width="94" height="46" rx="7" />
                                                <rect class="example-block" x="9" y="8" width="78" height="12" rx="2" />
                                                <rect class="example-block secondary" x="9" y="28" width="78" height="12" rx="2" />
                                            </svg>
                                        </label>
                                        <label class:active={featuresPosition === 'inline'} class="features-position-option">
                                            <span class="features-position-radio">
                                                <input type="radio" name="features-position" value="inline" checked={featuresPosition === 'inline'} onchange={() => setFeaturesPosition('inline')} />
                                                <span class="features-position-radio-icon" aria-hidden="true"><SvgIcon name="mdi:circle" size="12px" /></span>
                                            </span>
                                            <span class="features-position-copy"><strong>{$_('page.screen.featuresPositionInline')}</strong><small>{$_('page.screen.featuresPositionInlineHint')}</small></span>
                                            <svg class="features-position-example features-position-example-inline" viewBox="0 0 96 30" aria-hidden="true" focusable="false">
                                                <rect class="example-frame" x="1" y="1" width="94" height="28" rx="7" />
                                                <rect class="example-block" x="9" y="9" width="36" height="12" rx="2" />
                                                <rect class="example-block secondary" x="51" y="9" width="36" height="12" rx="2" />
                                            </svg>
                                        </label>
                                    </div>
                                </fieldset>
                            {/if}
                        </details>
                    {/if}
                    {:else}
                        <section class="layout-settings">
                            <div class="layout-setting-copy">
                                <strong>{$_('page.screen.fullWidth')}</strong>
                                <p>{$_('page.screen.fullWidthHint')}</p>
                            </div>
                            <span class="form-check form-switch">
                                <input
                                    class="form-check-input"
                                    type="checkbox"
                                    role="switch"
                                    aria-label={$_('page.screen.fullWidth')}
                                    checked={fullWidth}
                                    disabled={featuresPosition === 'inline'}
                                    onchange={(event) => setFullWidth(event.currentTarget.checked)}
                                />
                            </span>
                        </section>
                    {/if}
                </div>
                <div class="entity-settings-preview">
                    <div
                        class:badge-preview={mode === 'badge'}
                        class:entity-preview={mode === 'entity'}
                        class:full-width-preview={mode === 'entity' && fullWidth}
                        class:inline-preview={mode === 'entity' && previewFeatures.length > 0 && featuresPosition === 'inline'}
                        class="preview-surface"
                    >
                        {#if draftEntitySupported}
                            <HassEntityRow
                                variant={mode}
                                entityId={draftEntityId}
                                showIconBackground={shouldShowScreenEntityIconBackground(draftEntityId, previewActionConfig)}
                                features={mode === 'entity' ? previewFeatures : []}
                                {featuresPosition}
                                fullWidth={mode === 'entity' && fullWidth}
                            />
                        {:else}
                            <span class="unavailable-preview"><SvgIcon name={draftEntity.icon} />{$_('hassEntityState.unavailable')}</span>
                        {/if}
                    </div>
                </div>
            </div>
            <footer>
                <button class="delete-entity-button" type="button" onclick={remove}>{$_(mode === 'badge' ? 'page.screen.deleteBadge' : 'page.screen.deleteEntity')}</button>
                <button class="save-entity-button" type="button" onclick={save}>{$_('common.save')}</button>
            </footer>
        </div>
    {/snippet}
</Modal>

<style lang="scss">
    .entity-settings-dialog {
        --entity-field-bg: #f3f3f3;
        --entity-preview-bg: rgb(249, 249, 249);
        --interaction-menu-bg: #fff;
        --interaction-menu-border: rgba(25, 40, 60, 0.18);
        --feature-empty-background: rgb(225, 243, 252);
        --feature-add-background: rgb(223, 243, 252);
        --feature-add-color: rgb(0, 154, 199);
        --feature-add-text-color: rgb(20, 20, 20);
        box-sizing: border-box;
        display: flex;
        width: min(62.5rem, calc(100vw - 2rem));
        height: min(44.1875rem, calc(100dvh - 2rem));
        overflow: hidden;
        flex-direction: column;
        border-radius: 1.25rem;
        background: var(--body-inner-level-2);
        color: var(--bs-body-color);
        font-size: 0.875rem;
    }
    :global([data-bs-theme='dark']) .entity-settings-dialog {
        --entity-field-bg: #454545;
        --entity-preview-bg: rgba(17,17,17,1);
        --interaction-menu-bg: #303132;
        --interaction-menu-border: rgba(207, 220, 234, 0.85);
        --feature-empty-background: rgb(25, 43, 52);
        --feature-add-background: #003b4d;
        --feature-add-color: #00c8f8;
        --feature-add-text-color: rgb(225, 225, 225);
        --ha-color-fill-primary-normal-resting: #003b4d;
        --ha-color-on-primary-normal: #00c8f8;
        --body-inner-level-2: #303132;
        --bs-border-color: #515253;
    }
    header {
        display: flex;
        min-height: 4.0625rem;
        padding: 0 1.25rem;
        flex: 0 0 auto;
        align-items: center;
        gap: 0.75rem;
        border-bottom: 0.0625rem solid var(--bs-border-color);
    }
    header button {
        display: inline-flex;
        padding: 0;
        flex: 0 0 auto;
        border: 0;
        background: transparent;
        color: inherit;
        font-size: 1.5rem;
    }
    header strong {
        min-width: 0;
        overflow: hidden;
        font-size: 1.125rem;
        font-weight: 500;
        text-overflow: ellipsis;
        white-space: nowrap;
    }
    .entity-settings-dialog-content {
        display: grid;
        min-height: 0;
        flex: 1 1 auto;
        overflow-y: auto;
        grid-template-columns: 350px 650px;
        border-bottom: 0.0625rem solid var(--bs-border-color);
    }
    .entity-settings {
        min-width: 0;
        padding: 0.75rem;
    }
    .entity-settings-tabs {
        display: flex;
        height: 2.8125rem;
        margin: -0.75rem -0.75rem 1.25rem;
        border-bottom: 0.0625rem solid var(--bs-border-color);
    }
    .entity-settings-tabs button {
        width: 50%;
        padding: 0;
        color: inherit;
        border: 0;
        border-bottom: 0.0625rem solid transparent;
        background: transparent;
        font-size: 0.875rem;
    }
    .entity-settings-tabs button.active {
        border-bottom-color: var(--primary-button-bg);
        background: rgba(1, 119, 253, 0.12);
    }
    .configured-entity {
        position: relative;
        display: flex;
        margin-bottom: 1.25rem;
        flex-direction: column;
        gap: 0.625rem;
    }
    .configured-entity > span {
        font-size: 0.875rem;
    }
    .configured-entity :global(.dropdown) {
        width: 100%;
        min-width: 0;
    }
    .configured-entity :global(.dropdown-menu) {
        max-width: 100%;
        max-height: 18rem;
        overflow-y: auto;
        border-radius: 0.625rem;
    }
    .configured-entity :global(.dropdown-menu),
    .add-feature-dropdown :global(.dropdown-menu) {
        background: var(--body-inner-level-2);
    }
    .configured-entity :global(.custom-searchable-trigger) {
        min-height: 3.5rem;
    }
    .configured-entity :global(.searchable-trigger) {
        width: calc(100% - 1.5rem);
        min-height: 2.375rem;
        margin: 0.75rem 0.75rem 0.625rem;
        padding-inline: 0.75rem;
        border: 0.0625rem solid var(--bs-border-color);
        border-radius: 0.625rem;
        background: transparent;
    }
    .configured-entity :global(.searchable-trigger > .sf-icon) {
        flex: 0 0 auto;
        color: var(--bs-body-color);
        font-size: 1.25rem;
    }
    .configured-entity-value {
        display: grid;
        width: 100%;
        min-height: 3.5rem;
        min-width: 0;
        padding: 0.5rem 0.75rem;
        align-items: center;
        grid-template-columns: 1.25rem minmax(0, 1fr) 1rem 0.75rem;
        gap: 0.625rem;
        border-bottom: 0.0625rem solid var(--bs-secondary-color);
        border-radius: 0.5rem 0.5rem 0 0;
        background: var(--entity-field-bg);
    }
    .configured-entity-value > :global(.sf-icon:first-child) {
        color: var(--bs-secondary-color);
        font-size: 1.25rem;
    }
    .configured-entity-value > span {
        display: flex;
        min-width: 0;
        flex-direction: column;
    }
    .configured-entity-value strong,
    .configured-entity-value small {
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
    }
    .configured-entity-value strong {
        font-size: 0.875rem;
        font-weight: 400;
        line-height: 1.25rem;
    }
    .configured-entity-value small {
        color: var(--bs-secondary-color);
        font-size: 0.75rem;
        line-height: 1.125rem;
    }
    .configured-entity-value > :global(.sf-icon:nth-last-child(2)),
    .configured-entity-value > :global(.sf-icon:last-child) {
        color: var(--bs-secondary-color);
        font-size: 1rem;
    }
    .configured-entity-value > :global(.sf-icon:last-child) {
        font-size: 0.75rem;
        transform: rotate(90deg);
    }
    .configured-entity :global(.dropdown-item) {
        display: flex;
        min-height: 4rem;
        padding: 0.625rem 0.75rem;
        align-items: center;
        gap: 0.75rem;
        border-bottom: 0.0625rem solid var(--bs-border-color);
        color: inherit;
    }
    .entity-settings-dialog :global(.dropdown-item:hover),
    .entity-settings-dialog :global(.dropdown-item.active),
    .entity-settings-dialog :global(.dropdown-item:active) {
        background: var(--web-10, rgba(1, 119, 253, 0.1));
        color: var(--bs-body-color);
    }
    .configured-entity :global(.dropdown-item > .sf-icon) {
        width: 1.25rem;
        flex: 0 0 auto;
        justify-content: center;
        color: var(--bs-secondary-color);
        font-size: 1.25rem;
    }
    .configured-entity :global(.entity-search-option-copy) {
        display: flex;
        min-width: 0;
        flex: 1 1 auto;
        flex-direction: column;
        gap: 0.125rem;
    }
    .configured-entity :global(.entity-search-option-copy strong),
    .configured-entity :global(.entity-search-option-copy small) {
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
        font-weight: 400;
    }
    .configured-entity :global(.entity-search-option-copy small),
    .configured-entity :global(.entity-search-option-category) {
        color: var(--bs-secondary-color);
        font-size: 0.75rem;
    }
    .configured-entity :global(.entity-search-option-category) {
        flex: 0 0 auto;
    }
    .entity-config-section {
        margin-top: 1.25rem;
        border: 0.0625rem solid var(--bs-border-color);
        border-radius: 0.625rem;
    }
    .entity-config-section > summary {
        display: grid;
        min-height: 3.375rem;
        padding: 0.75rem;
        align-items: center;
        grid-template-columns: auto 1fr auto;
        gap: 0.625rem;
        cursor: pointer;
        list-style: none;
        font-size: 1rem;
    }
    .entity-config-section > summary::-webkit-details-marker {
        display: none;
    }
    .section-leading-icon,
    .section-chevron {
        display: inline-flex;
        font-size: 1.25rem;
    }
    .section-leading-icon {
        color: var(--bs-secondary-color);
    }
    .section-chevron {
        font-size: 0.875rem;
    }
    .section-chevron :global(.sf-icon) {
        transition: transform 0.15s ease;
        transform: rotate(90deg);
    }
    .entity-config-section[open] .section-chevron :global(.sf-icon) {
        transform: rotate(-90deg);
    }
    .interaction-fields {
        display: grid;
        padding: 0 0.625rem 0.75rem;
        gap: 1.25rem;
    }
    .behavior-field {
        display: flex;
        min-width: 0;
        min-height: 3.5rem;
        padding: 0.5rem 0.75rem 0.375rem;
        flex-direction: column;
        border-bottom: 0.0625rem solid var(--bs-secondary-color);
        border-radius: 0.5rem 0.5rem 0 0;
        background: var(--entity-field-bg);
        color: var(--bs-secondary-color);
        font-size: 0.75rem;
    }
    .interaction-fields :global(.dropdown) {
        width: 100%;
        min-width: 0;
        color: var(--bs-body-color);
        font-size: 0.875rem;
    }
    .interaction-fields :global(.dropdown-toggle) {
        min-height: 1.3125rem;
    }
    .interaction-fields :global(.dropdown-menu) {
        left: -0.75rem;
        width: calc(100% + 1.5rem) !important;
        max-width: none;
        max-height: 12rem;
        margin-top: 0.375rem;
        padding: 0;
        overflow-y: auto;
        border-radius: 0.75rem;
        background: var(--interaction-menu-bg);
    }
    .interaction-fields :global(.dropdown-item) {
        display: flex;
        min-height: 2.5rem;
        padding: 0 0.75rem;
        align-items: center;
        color: var(--bs-body-color);
        line-height: 1.25rem;
    }
    .interaction-fields :global(.label-content) {
        overflow: hidden;
    }
    .interaction-fields :global(.label-content > span) {
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
    }
    .interaction-fields :global(.arrow) {
        font-size: 0.625rem;
    }
    .feature-placeholder {
        display: flex;
        min-height: 2.5rem;
        margin: 0 0.75rem 0.75rem;
        padding: 0.5rem 0.625rem;
        align-items: center;
        gap: 0.5rem;
        border-radius: 0.25rem;
        background: var(--feature-empty-background);
        color: var(--feature-add-text-color);
    }
    .selected-feature-list {
        display: flex;
        margin-bottom: 0.5rem;
        padding: 0 0.2rem 0 0.75rem;
        flex-direction: column;
    }
    .selected-feature-row {
        display: flex;
        min-height: 2.75rem;
        min-width: 0;
        align-items: center;
        gap: 0.5rem;
    }
    .feature-drag-handle {
        display: flex;
        width: 1.5rem;
        flex: 0 0 1.5rem;
        align-items: center;
        justify-content: center;
        color: var(--bs-text-secondary);
        cursor: grab;
        touch-action: none;
    }
    .feature-drag-handle:active {
        cursor: grabbing;
    }
    .feature-drag-handle :global(svg) {
        width: 1.25rem;
        height: 1.25rem;
    }
    .selected-feature-label {
        min-width: 0;
        flex: 1 1 auto;
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
    }
    .remove-feature-button {
        display: grid;
        width: 2rem;
        height: 2rem;
        flex: 0 0 2rem;
        place-items: center;
        border: 0;
        border-radius: 0.25rem;
        background: transparent;
        color: var(--bs-secondary-color);
    }
    .remove-feature-button :global(svg) {
        width: 1.125rem;
        height: 1.125rem;
    }
    .add-feature-dropdown {
        width: max-content;
        max-width: 100%;
        margin: 0 0.75rem 0.75rem;
    }
    .add-feature-dropdown :global(.dropdown) {
        width: max-content;
        max-width: 100%;
    }
    .add-feature-dropdown :global(.dropdown.disabled) {
        opacity: 1;
        cursor: not-allowed;
    }
    .add-feature-dropdown :global(.dropdown.disabled .dropdown-trigger) {
        cursor: not-allowed;
    }
    .add-feature-dropdown :global(.dropdown-menu) {
        width: max-content !important;
        min-width: 100%;
        max-width: calc(100vw - 2rem);
        max-height: min(16rem, 40dvh);
        margin-top: 0;
        margin-bottom: 0.25rem;
        overflow-y: auto;
        top: auto !important;
        bottom: 100%;
    }
    .add-feature-dropdown :global(.dropdown-item) {
        white-space: normal;
        overflow-wrap: anywhere;
    }
    .add-feature-dropdown :global(.dropdown-trigger) {
        width: auto !important;
        height: auto !important;
    }
    .add-feature-dropdown :global(.dropdown-trigger .arrow) {
        display: none !important;
    }
    .add-feature-trigger {
        display: inline-flex;
		justify-content: center;
        min-height: 2rem;
        padding: 0.375rem 0.625rem;
        align-items: center;
        border-radius: 999px;
        background: var(--ha-color-fill-primary-normal-resting, var(--feature-add-background));
		color: var(--ha-color-on-primary-normal, var(--feature-add-color));
        font-size: 0.875rem;
        white-space: nowrap;
		font-weight: 500;
    }
    .add-feature-trigger :global(svg) {
        width: 1rem;
        height: 1rem;
    }
    .feature-placeholder :global(.sf-icon) {
        flex: 0 0 auto;
        color: var(--feature-icon-color);
        font-size: 1.25rem;
    }
    .features-position-setting {
        min-width: 0;
        margin: 0;
        padding: 1rem 0.75rem 0.75rem;
        border: 0;
    }
    .features-position-setting legend {
        float: none;
        width: auto;
        margin: 0 0 0.5rem;
        font-size: 0.875rem;
    }
    .features-position-options {
        display: grid;
        grid-template-columns: repeat(2, minmax(0, 1fr));
        gap: 0.5rem;
    }
    .features-position-option {
        display: grid;
        min-width: 0;
        min-height: 7.875rem;
        padding: 0.625rem;
        grid-template-columns: auto minmax(0, 1fr);
        align-content: start;
        gap: 0.25rem 0.5rem;
        border: 0.0625rem solid var(--bs-border-color);
        border-radius: 0.5rem;
        cursor: pointer;
    }
    .features-position-option.active {
        background: var(--screen-feature-position-active-background);
    }
    .features-position-radio {
        position: relative;
        display: block;
        width: 1.25rem;
        height: 1.25rem;
        margin: 0.125rem 0 0;
    }
    .features-position-radio input {
        display: block;
        width: 100%;
        height: 100%;
        margin: 0;
        appearance: none;
        border: 0.125rem solid var(--bs-secondary-color);
        border-radius: 50%;
        background: transparent;
        cursor: pointer;
    }
    .features-position-radio input:checked {
        border-color: var(--feature-add-color);
    }
    .features-position-radio-icon {
        position: absolute;
        inset: 0;
        display: flex;
        align-items: center;
        justify-content: center;
        color: var(--feature-add-color);
        pointer-events: none;
        visibility: hidden;
    }
    .features-position-radio input:checked + .features-position-radio-icon {
        visibility: visible;
    }
    .features-position-radio input:focus-visible {
        outline: 0.125rem solid var(--screen-tone-cyan-color, #00bcd4);
        outline-offset: 0.125rem;
    }
    .features-position-copy {
        display: flex;
        min-width: 0;
        flex-direction: column;
        gap: 0.25rem;
    }
    .features-position-copy strong {
        font-size: 0.875rem;
        font-weight: 500;
    }
    .features-position-copy small {
        color: var(--bs-secondary-color);
        font-size: 0.6875rem;
        line-height: 1rem;
    }
    .features-position-example {
        display: block;
        width: 6rem;
        height: 3rem;
        margin-top: 0.6875rem;
        grid-column: 1 / -1;
        justify-self: center;
    }
    .features-position-example-inline {
        height: 1.875rem;
        margin-top: 0.0625rem;
    }
    .features-position-example .example-frame {
        fill: var(--body-inner-level-2);
        stroke: var(--bs-border-color);
    }
    .features-position-example .example-block {
        fill: var(--feature-position-example-primary);
    }
    .features-position-example .example-block.secondary {
        fill: var(--feature-position-example-secondary);
    }
    .layout-settings {
        display: flex;
        align-items: center;
        justify-content: space-between;
        gap: 1rem;
    }
    .layout-setting-copy {
        min-width: 0;
    }
    .layout-setting-copy > strong {
        display: block;
        margin-bottom: 0.375rem;
        font-size: 1rem;
        font-weight: 500;
    }
    .layout-setting-copy > p {
        margin: 0;
        color: var(--bs-secondary-color);
        font-size: 0.75rem;
        line-height: 1.125rem;
    }
    .layout-settings .form-check {
        min-height: 0;
        margin: 0;
        padding: 0;
        flex: 0 0 auto;
    }
    .layout-settings .form-check-input {
        width: 3rem;
        height: 1.5rem;
        margin: 0;
        border: 0;
        cursor: pointer;
    }
    .layout-settings .form-check-input:disabled {
        cursor: not-allowed;
    }
    .entity-settings-preview {
        min-width: 0;
        padding: 0.75rem;
        border-left: 0.0625rem solid var(--bs-border-color);
    }
    .preview-surface {
        display: flex;
        width: 100%;
        min-height: 7.75rem;
        padding: 1.75rem 0.75rem;
        align-items: center;
        justify-content: center;
        border-radius: 0.75rem;
        background: var(--entity-preview-bg);
    }
    .preview-surface.inline-preview {
        align-items: stretch;
    }
    .preview-surface.entity-preview {
        display: grid;
        grid-template-columns: repeat(2, minmax(0, 1fr));
        align-items: start;
        gap: 0.75rem;
    }
    .preview-surface.entity-preview :global(.hass-entity-row) {
        width: 100%;
        max-width: 100%;
        margin-inline: 0;
    }
    .preview-surface.entity-preview.full-width-preview :global(.hass-entity-row),
    .preview-surface.entity-preview.full-width-preview .unavailable-preview {
        grid-column: 1 / -1;
    }
    @media (min-width: 43.8125rem) {
        .preview-surface.entity-preview {
            min-height: 0;
        }
    }
    .preview-surface.badge-preview {
        min-height: 5.25rem;
        padding-block: 1.6875rem;
    }
    .preview-surface :global(.hass-entity-row.badge) {
        min-height: 1.875rem;
        padding: 0.1875rem 0.5rem;
        border-radius: 1rem;
    }
    .preview-surface :global(.badge-value) {
        font-size: 0.875rem;
    }
    .unavailable-preview {
        display: inline-flex;
        min-height: 1.875rem;
        padding: 0.1875rem 0.5rem;
        align-items: center;
        gap: 0.25rem;
        border-radius: 1rem;
        background: #333;
        color: #f2f2f7;
        font-size: 0.875rem;
    }
    .unavailable-preview :global(.sf-icon) {
        color: #8e8e93;
        font-size: 1.125rem;
    }
    footer {
        display: flex;
        min-height: 5rem;
        padding: 1.25rem 1.125rem;
        flex: 0 0 auto;
        align-items: center;
        justify-content: flex-end;
        gap: 1.5rem;
    }
    footer button {
        width: 7.5rem;
        max-width: calc((100% - 1.5rem) / 2);
        min-height: 2.5rem;
        border: 0;
        border-radius: 1.25rem;
        font-size: 1rem;
    }
    .delete-entity-button {
        background: rgba(241, 82, 75, 0.2);
        color: #f1524b;
    }
    .save-entity-button {
        background: var(--primary-button-bg);
        color: white;
    }
    @media (max-width: 64.5rem) {
        .entity-settings-dialog-content {
            grid-template-columns: minmax(0, 7fr) minmax(0, 13fr);
        }
    }
    @media (max-width: 43.75rem) {
        .entity-settings-dialog {
            width: 100%;
            max-width: 100%;
            height: min(42.875rem, calc(100dvh - 1rem));
            border-radius: 1.25rem 1.25rem 0 0;
        }
        header {
            padding-inline: 0.75rem;
        }
        .entity-settings-dialog-content {
            grid-template-columns: minmax(0, 1fr);
            align-content: start;
            width: 100%;
            min-width: 0;
            border-bottom: 0;
        }
        .entity-settings {
            width: 100%;
            min-width: 0;
        }
        .entity-settings-tabs,
        .configured-entity,
        .entity-config-section {
            min-width: 0;
        }
        .entity-settings-preview {
            padding: 0 0.75rem 0.75rem;
            border-left: 0;
        }
        footer {
            justify-content: center;
            padding: 1.25rem 0.75rem calc(1.25rem + env(safe-area-inset-bottom));
        }
    }
</style>
