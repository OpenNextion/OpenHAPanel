<script lang="ts">
    import { _ } from 'svelte-i18n';
    import HassEntityRow from '@/component/HassEntityRow/HassEntityRow.svelte';
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';
    import Modal from '@/component/Modal/Modal.svelte';
    import { addEntityToScreen, searchScreenEntities, type TEntityPickerMode } from '@/hooks/useScreenConfiguration';
    import { getHassEntityPresentation } from '@/hooks/useHassEntity';
    import { getTileFeatureSuggestions } from '@/constant/tileFeatureRules';
    import type { IHassScreenConfigFeature } from '@/types/screenConfig';
    import { haRegistryStore, haRegistryTreeStore } from '@/stores/haRegistry';
    import { homeAssistantStore } from '@/stores/homeassistant';
    import { isHomeAssistantAvailable } from '@/hooks/useHomeAssistant.svelte';
    import type { IHassRegistryTreeArea, IHassRegistryTreeDevice, IHassRegistryTreeDomain, IHassRegistryTreeEntity } from '@/types/haRegistry';
    import { homeAssistantWebSocket } from '@/wsForHA';

    interface Props {
        visible: boolean;
        mode: TEntityPickerMode;
        screenIndex: number | null;
        onclose: () => void;
    }
    let { visible, mode, screenIndex, onclose }: Props = $props();
    let query = $state('');
    let selectedEntity = $state<IHassRegistryTreeEntity | null>(null);
    let wasVisible = false;
    const results = $derived.by(() => {
        void $haRegistryTreeStore;
        return searchScreenEntities(query);
    });
    const selectedEntityPresentation = $derived(selectedEntity ? getHassEntityPresentation(selectedEntity.id, $haRegistryStore, { variant: mode }) : null);
    const selectedEntitySupported = $derived(selectedEntityPresentation?.supported ?? false);
    const selectedEntitySuggestions = $derived(selectedEntity ? getTileFeatureSuggestions(selectedEntity.id, $haRegistryStore.statesById.get(selectedEntity.id)) : []);

    $effect(() => {
        if (visible && !wasVisible) {
            query = '';
            selectedEntity = null;
            if (
                isHomeAssistantAvailable($homeAssistantStore.connectionInfo) &&
                ($haRegistryStore.states.length === 0 || Boolean($haRegistryStore.error))
            ) {
                void homeAssistantWebSocket.refreshRegistryData();
            }
        }
        wasVisible = visible;
    });

    function addSelected(features: IHassScreenConfigFeature[] = []) {
        if (screenIndex === null || !selectedEntity) return;
        if (!selectedEntitySupported) return;
        addEntityToScreen(screenIndex, mode, selectedEntity, features);
        onclose();
    }

    function selectEntity(entity: IHassRegistryTreeEntity) {
        selectedEntity = entity;
    }
</script>

{#snippet entityRow(entity: IHassRegistryTreeEntity, searchResult = false, depth = 0)}
    {@const presentation = getHassEntityPresentation(entity.id, $haRegistryStore)}
    {@const device = searchResult ? $haRegistryStore.devices.find((device) => device.id === entity.deviceId) : undefined}
    {@const domain = entity.id.split('.', 1)[0] ?? ''}
    {@const entityCategory = $haRegistryStore.domainTranslations[`component.${domain}.title`] ?? domain}
    <button class:selected={selectedEntity?.id === entity.id} class="tree-entity" style:--tree-depth={depth} type="button" onclick={() => selectEntity(entity)}>
        <SvgIcon name={presentation.icon} />
        <span class="tree-entity-copy">
            <span class="tree-entity-name">{entity.raw.en?.trim() || presentation.displayName}</span>
            {#if searchResult}<small>{device?.name_by_user || device?.name || presentation.context || entity.id}</small>{/if}
        </span>
        {#if searchResult}<span class="entity-select-label">{entityCategory}</span>{/if}
    </button>
{/snippet}
{#snippet deviceRow(device: IHassRegistryTreeDevice, depth = 0)}
    <details class="tree-node tree-device">
        <summary style:--tree-depth={depth}><span class="tree-chevron"><SvgIcon name="arrowRight.svg" /></span><span>{device.name}</span></summary>
        <div class="tree-children">
            {#each device.children as child (child.id)}{@render deviceRow(child, depth + 1)}{/each}{#each device.entities as entity (entity.id)}{@render entityRow(entity, false, depth + 1)}{/each}
        </div>
    </details>
{/snippet}
{#snippet areaRow(area: IHassRegistryTreeArea)}
    <details class="tree-node tree-area">
        <summary><span class="tree-chevron"><SvgIcon name="arrowRight.svg" /></span><span>{area.name}</span></summary>
        <div class="tree-children">
            {#each area.devices as device (device.id)}{@render deviceRow(device, 1)}{/each}{#each area.entities as entity (entity.id)}{@render entityRow(entity, false, 1)}{/each}
        </div>
    </details>
{/snippet}
{#snippet domainRow(domain: IHassRegistryTreeDomain)}
    {@const icon = getHassEntityPresentation(domain.entities[0].id, $haRegistryStore).icon}
    <details class="tree-node">
        <summary style:--tree-depth={1}
            ><span class="tree-chevron"><SvgIcon name="arrowRight.svg" /></span><SvgIcon name={icon} /><span
                >{domain.name}</span
            ></summary
        >
        <div class="tree-children">
            {#each domain.entities as entity (entity.id)}{@render entityRow(entity, false, 2)}{/each}
        </div>
    </details>
{/snippet}

<Modal modalVisible={visible} footer={false} mobileSheet={true} setVisible={(nextVisible) => !nextVisible && onclose()}>
    {#snippet customSnippet()}
        <div class="entity-picker" role="dialog" aria-modal="true" aria-label={$_('page.screen.entityPickerTitle')}>
            <header>
                <button type="button" aria-label={$_('common.close')} onclick={onclose}><SvgIcon name="mdi:close" /></button><span
                    >{$_(mode === 'badge' ? 'page.screen.addBadgeTitle' : 'page.screen.entityPickerTitle')}</span
                >
            </header>
            <div class:mobile-preview={selectedEntity !== null} class="entity-picker-body">
                <aside>
                    <label class="entity-search"
                        ><SvgIcon name="mdi:magnify" /><input bind:value={query} aria-label={$_('page.screen.searchEntity')} placeholder={$_('page.screen.searchEntity')} oninput={() => (selectedEntity = null)} />
                        {#if query}<button type="button" aria-label={$_('common.clear')} title={$_('common.clear')} onclick={() => (query = '')}
                                ><SvgIcon name="mdi:close" /></button
                            >{/if}
                    </label>
                    <div class="entity-tree">
                        {#if $haRegistryStore.loading}<div class="entity-tree-status">
                                <span class="spinner-border spinner-border-sm"></span>{$_('page.screen.loadingEntities')}
                            </div>
                        {:else if $haRegistryStore.error}<div class="entity-tree-status text-danger">{$haRegistryStore.error}</div>
                        {:else if query.trim()}
                            <div class="entity-search-results">
                                {#each results as result (result.entity.id)}{@render entityRow(result.entity, true)}{:else}
                                    <div class="entity-tree-status entity-empty-state">
                                        <SvgIcon name="mdi:magnify" />
                                        <span>{$_('page.screen.noEntityMatchesSearch')}</span>
                                    </div>
                                {/each}
                            </div>
                        {:else}
                            {#each $haRegistryTreeStore.floors as floor (floor.id)}
                                <section>
                                    <span class="tree-section-title">{floor.name}</span>
                                    <div>
                                        {#each floor.areas as area (area.id)}{@render areaRow(area)}{/each}
                                    </div>
                                </section>
                            {/each}
                            {#if $haRegistryTreeStore.otherAreas.length}<section>
                                    <span class="tree-section-title">{$_('page.screen.otherAreas')}</span>
                                    <div>
                                        {#each $haRegistryTreeStore.otherAreas as area (area.id)}{@render areaRow(area)}{/each}
                                    </div>
                                </section>{/if}
                            {#if $haRegistryTreeStore.unassignedDevices.length || $haRegistryTreeStore.unassignedServices.length || $haRegistryTreeStore.unassignedEntityDomains.length || $haRegistryTreeStore.unassignedHelperDomains.length}
                                <section>
                                    <span class="tree-section-title">{$_('page.screen.unassigned')}</span>
                                    {#if $haRegistryTreeStore.unassignedEntityDomains.length}<details class="tree-node tree-unassigned-group">
                                            <summary
                                                ><span class="tree-chevron"><SvgIcon name="arrowRight.svg" /></span><span
                                                    >{$_('page.screen.entities')}</span
                                                ></summary
                                            >
                                            <div class="tree-children">
                                                {#each $haRegistryTreeStore.unassignedEntityDomains as domain (domain.domain)}{@render domainRow(domain)}{/each}
                                            </div>
                                        </details>{/if}
                                    {#if $haRegistryTreeStore.unassignedHelperDomains.length}<details class="tree-node tree-unassigned-group">
                                            <summary
                                                ><span class="tree-chevron"><SvgIcon name="arrowRight.svg" /></span><span
                                                    >{$_('page.screen.helpers')}</span
                                                ></summary
                                            >
                                            <div class="tree-children">
                                                {#each $haRegistryTreeStore.unassignedHelperDomains as domain (domain.domain)}{@render domainRow(domain)}{/each}
                                            </div>
                                        </details>{/if}
                                    {#if $haRegistryTreeStore.unassignedDevices.length}<details class="tree-node tree-unassigned-group">
                                            <summary
                                                ><span class="tree-chevron"><SvgIcon name="arrowRight.svg" /></span><span
                                                    >{$_('page.screen.devices')}</span
                                                ></summary
                                            >
                                            <div class="tree-children">
                                                {#each $haRegistryTreeStore.unassignedDevices as device (device.id)}{@render deviceRow(device, 1)}{/each}
                                            </div>
                                        </details>{/if}
                                    {#if $haRegistryTreeStore.unassignedServices.length}<details class="tree-node tree-unassigned-group">
                                            <summary
                                                ><span class="tree-chevron"><SvgIcon name="arrowRight.svg" /></span><span
                                                    >{$_('page.screen.services')}</span
                                                ></summary
                                            >
                                            <div class="tree-children">
                                                {#each $haRegistryTreeStore.unassignedServices as device (device.id)}{@render deviceRow(device, 1)}{/each}
                                            </div>
                                        </details>{/if}
                                </section>
                            {/if}
                        {/if}
                    </div>
                </aside>
                <div class:has-selection={selectedEntity !== null} class="entity-picker-detail">
                    {#if selectedEntity}
                        <span class="mobile-selected-title">{$_('page.screen.selectedEntity')}</span>
                        <div class="mobile-selected-entity">
                            <SvgIcon name={selectedEntityPresentation?.icon ?? 'mdi:help-circle-outline'} />
                            <span>{selectedEntityPresentation?.displayName}</span>
                            <button type="button" aria-label={$_('common.clear')} onclick={() => (selectedEntity = null)}><SvgIcon name="mdi:close" /></button>
                        </div>
                        <span class="mobile-suggestions-title">{$_('page.screen.suggestions')}</span>
                    {/if}
                    {#if selectedEntity}<span class="preview-title">{$_('page.screen.suggestions')}</span>{/if}
                    <div
                        class:badge-preview={mode === 'badge'}
                        class:multiple-tile-previews={mode === 'entity' && selectedEntitySuggestions.length > 1}
                        class:unsupported-preview={selectedEntity !== null && !selectedEntitySupported}
                        class="preview-surface"
                    >
                        {#if selectedEntity && selectedEntitySupported}
                            {#if mode === 'entity' && selectedEntitySuggestions.length > 1}
                                <div class="tile-suggestions">
                                    {#each selectedEntitySuggestions as suggestion (suggestion.id)}
                                        <div class="tile-suggestion">
                                            <span class="tile-suggestion-title">Tile{suggestion.labelKey ? ` - ${$_(suggestion.labelKey)}` : ''}</span>
                                            <HassEntityRow
                                                entityId={selectedEntity.id}
                                                features={suggestion.features}
                                                onclick={() => addSelected(suggestion.features)}
                                                onFeatureToggle={() => addSelected(suggestion.features)}
                                                onFeatureActivate={() => addSelected(suggestion.features)}
                                            />
                                        </div>
                                    {/each}
                                </div>
                            {:else}
                                <div class:single-tile-preview={mode === 'entity'} class="entity-detail-preview">
                                    <HassEntityRow variant={mode} entityId={selectedEntity.id} onclick={() => addSelected()} />
                                </div>
                            {/if}
                        {:else if selectedEntity}
                            <div class="unsupported-entity"><strong>{$_('page.screen.entityNotSupported')}</strong></div>
                        {:else}<span class="pick-entity-placeholder">{$_('page.screen.pickEntityHint')}</span>{/if}
                    </div>
                </div>
            </div>
        </div>
    {/snippet}
</Modal>

<style lang="scss">
    .entity-picker {
        --entity-selection-bg: var(--body-inner-level-2);
        --selected-preview-bg: rgba(250, 250, 250, 1);
        --unsupported-preview-bg: #f8f8f8;
        --picker-mobile-section-bg: rgb(241, 241, 241);
        --picker-text: var(--bs-body-color);
        --picker-muted: var(--body-text-color-grey, var(--bs-secondary-color));
        --picker-border: var(--bs-border-color);
        --picker-danger: #dc3545;
        --picker-title-size: 1.125rem;
        --picker-body-size: 1rem;
        --picker-detail-size: 0.875rem;
        --picker-caption-size: 0.75rem;
        box-sizing: border-box;
        display: flex;
        width: min(62.5rem, calc(100vw - 2rem));
        height: min(39.1875rem, calc(100dvh - 2rem));
        overflow: hidden;
        flex-direction: column;
        border-radius: 1.25rem;
        background: var(--body-inner-level-2);
        color: var(--picker-text);
        font-size: var(--picker-detail-size);
    }
    :global([data-bs-theme='dark']) .entity-picker {
        --entity-selection-bg: #454545;
        --selected-preview-bg: rgba(17, 17, 17, 1);
        --unsupported-preview-bg: #1d1d1d;
        --picker-mobile-section-bg: transparent;
        --body-inner-level-2: #303132;
        --bs-border-color: #515253;
        --picker-text: var(--body-text-color-highlight, var(--bs-body-color));
        --picker-muted: var(--body-text-color-grey, var(--bs-secondary-color));
        --picker-border: #515253;
        --picker-danger: #ff6b63;
    }
    header {
        display: flex;
        min-width: 0;
        min-height: 4.0625rem;
        padding: 0 1.25rem;
        flex: 0 0 auto;
        align-items: center;
        gap: 0.75rem;
        border-bottom: 0.0625rem solid var(--picker-border);
        color: var(--picker-text);
        font-size: var(--picker-title-size);
        font-weight: 500;
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
    header > span {
        min-width: 0;
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
    }
    .entity-picker-body {
        display: grid;
        min-height: 0;
        flex: 1 1 auto;
        grid-template-columns: 350px 650px;
    }
    aside {
        display: flex;
        min-width: 0;
        min-height: 0;
        padding-top: 1.125rem;
        flex-direction: column;
    }
    .entity-search {
        display: flex;
        min-width: 0;
        min-height: 3rem;
        margin: 0 0.75rem 0.75rem;
        padding: 0 0.75rem;
        flex: 0 0 auto;
        align-items: center;
        gap: 0.625rem;
        border: 0.0625rem solid var(--picker-border);
        border-radius: 0.625rem;
        font-size: var(--picker-body-size);
    }
    .entity-search:focus-within {
        border-color: var(--primary-button-bg);
    }
    .entity-search > :global(.sf-icon) {
        flex: 0 0 auto;
        font-size: 1.5rem;
    }
    input {
        min-width: 0;
        width: 100%;
        padding: 0;
        flex: 1;
        border: 0;
        outline: 0;
        background: transparent;
        color: inherit;
        font: inherit;
    }
    input::placeholder {
        color: var(--picker-muted);
    }
    .entity-search button {
        display: inline-flex;
        padding: 0;
        flex: 0 0 auto;
        border: 0;
        background: transparent;
        color: var(--picker-muted);
        font-size: 1.25rem;
    }
    .entity-tree {
        min-height: 0;
        flex: 1 1 auto;
        padding-bottom: 0.75rem;
        overflow: auto;
        overscroll-behavior: contain;
        font-weight: 400;
    }
    .entity-tree-status {
        display: flex;
        padding: 1rem;
        align-items: center;
        justify-content: center;
        gap: 0.625rem;
        color: var(--picker-muted);
        text-align: center;
    }
    .entity-empty-state {
        padding: 1.25rem 0.75rem;
        flex-direction: column;
        gap: 0.5rem;
        color: inherit;
        font-size: var(--picker-body-size);
    }
    .entity-empty-state :global(.sf-icon) {
        font-size: 2.5rem;
    }
    .tree-node summary {
        display: flex;
        min-width: 0;
        min-height: 3rem;
        padding: 0.625rem 0.75rem 0.625rem calc(0.75rem + var(--tree-depth, 0) * 1.25rem);
        align-items: center;
        gap: 0.75rem;
        font-size: var(--picker-body-size);
        cursor: pointer;
        list-style: none;
    }
    .tree-node summary > span:last-child {
        min-width: 0;
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
    }
    .tree-node summary::-webkit-details-marker {
        display: none;
    }
    .tree-node summary:hover,
    .tree-entity:hover,
    .tree-entity.selected {
        background: color-mix(in srgb, var(--primary-button-bg) 12%, transparent);
    }
    .tree-entity.selected {
        color: var(--primary-button-bg);
    }
    .tree-chevron {
        display: inline-flex;
        width: 1.5rem;
        flex: 0 0 auto;
        justify-content: center;
        font-size: 0.875rem;
        transition: transform 0.15s ease;
    }
    .tree-node[open] > summary .tree-chevron {
        transform: rotate(90deg);
    }
    .tree-section-title {
        display: flex;
        min-height: 3rem;
        padding: 0.625rem 0.75rem;
        align-items: center;
        background: var(--bs-body-bg);
        font-size: var(--picker-body-size);
    }
    .tree-entity {
        display: flex;
        width: 100%;
        min-width: 0;
        min-height: 3rem;
        padding: 0.625rem 0.75rem 0.625rem calc(0.75rem + var(--tree-depth, 0) * 1.25rem);
        align-items: center;
        gap: 0.75rem;
        border: 0;
        background: transparent;
        color: inherit;
        font-size: var(--picker-body-size);
        text-align: left;
    }
    .tree-entity > :global(.sf-icon) {
        width: 1.25rem;
        flex: 0 0 auto;
        justify-content: center;
        color: var(--picker-muted);
        font-size: 1.25rem;
    }
    .tree-entity-copy {
        display: flex;
        min-width: 0;
        flex: 1 1 auto;
        flex-direction: column;
        gap: 0.125rem;
    }
    .tree-entity-name,
    .tree-entity-copy small {
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
    }
    .tree-entity-copy small,
    .entity-select-label {
        color: var(--picker-muted);
        font-size: var(--picker-caption-size);
    }
    .entity-select-label {
        flex: 0 0 auto;
    }
    .entity-search-results {
        border-top: 0.0625rem solid var(--picker-border);
    }
    .entity-search-results .tree-entity {
        min-height: 4rem;
        border-bottom: 0.0625rem solid var(--picker-border);
    }
    .entity-picker-detail {
        display: flex;
        min-width: 0;
        min-height: 0;
        padding: 0.75rem;
        overflow-y: auto;
        flex-direction: column;
        border-left: 0.0625rem solid var(--picker-border);
    }
    .preview-title {
        display: block;
        margin-bottom: 0.5rem;
        color: var(--picker-muted);
        font-size: var(--picker-detail-size);
    }
    .preview-surface {
        display: flex;
        width: 100%;
        min-height: 0;
        padding: 0.75rem;
        flex: 1 1 auto;
        align-items: center;
        justify-content: center;
    }
    .has-selection .preview-surface {
        min-height: 7.75rem;
        padding: 1.75rem 0.75rem;
        flex: 0 0 auto;
        border-radius: 0.75rem;
        background: var(--selected-preview-bg);
    }
    .has-selection .preview-surface.unsupported-preview {
        background: var(--unsupported-preview-bg);
    }
    .has-selection .preview-surface.badge-preview {
        min-height: 5.25rem;
        padding-block: 1.6875rem;
    }
    .has-selection .preview-surface.multiple-tile-previews {
        padding: 0;
        background: transparent;
    }
    .tile-suggestions {
        display: grid;
        width: 100%;
        min-width: 0;
        grid-template-columns: repeat(2, minmax(0, 1fr));
        gap: 0.75rem;
    }
    .tile-suggestion {
        display: flex;
        min-width: 0;
        min-height: 10rem;
        padding: 0.75rem;
        flex-direction: column;
        align-items: center;
        justify-content: center;
        gap: 1rem;
        border: 0.0625rem solid var(--picker-border);
        border-radius: 0.75rem;
        background: var(--selected-preview-bg);
    }
    .tile-suggestion-title {
        color: var(--picker-text);
        text-align: center;
    }
    .tile-suggestion :global(.hass-entity-row) {
        width: 100%;
    }
    .preview-surface :global(.hass-entity-row.badge) {
        min-height: 1.875rem;
        padding: 0.1875rem 0.5rem;
        border-radius: 1rem;
    }
    .preview-surface :global(.badge-value) {
        font-size: 0.875rem;
    }
    .pick-entity-placeholder {
        color: var(--picker-text);
        font-size: var(--picker-body-size);
        text-align: center;
    }
    .entity-detail-preview {
        display: flex;
        width: 100%;
        min-width: 0;
        justify-content: center;
    }
    .entity-detail-preview.single-tile-preview {
        display: grid;
        grid-template-columns: repeat(2, minmax(0, 1fr));
    }
    .entity-detail-preview.single-tile-preview :global(.hass-entity-row) {
        width: 100%;
        margin-inline: 0;
    }
    .mobile-selected-title,
    .mobile-selected-entity,
    .mobile-suggestions-title {
        display: none;
    }
    .unsupported-entity {
        display: flex;
        align-items: center;
        flex-direction: column;
        gap: 0.75rem;
        color: var(--picker-danger);
        text-align: center;
    }
    .unsupported-entity :global(.sf-icon) {
        color: var(--bs-warning);
        font-size: 2rem;
    }
    @media (max-width: 64.5rem) {
        .entity-picker-body {
            grid-template-columns: minmax(0, 7fr) minmax(0, 13fr);
        }
    }
    @media (max-width: 43.75rem) {
        .entity-detail-preview.single-tile-preview {
            grid-template-columns: minmax(0, 1fr);
        }
        .entity-detail-preview.single-tile-preview :global(.hass-entity-row) {
            width: min(100%, 17.5rem);
            margin-inline: auto;
        }
        .tile-suggestions {
            grid-template-columns: minmax(0, 1fr);
        }
        .entity-picker {
            width: 100%;
            max-width: 100%;
            height: min(42.875rem, calc(100dvh - 1rem));
            border-radius: 1.25rem 1.25rem 0 0;
        }
        header {
            padding-inline: 0.75rem;
        }
        .entity-picker-body {
            width: 100%;
            min-width: 0;
            grid-template-columns: minmax(0, 1fr);
        }
        .entity-search {
            margin-inline: 0.875rem;
        }
        .entity-picker-detail,
        .entity-picker-body.mobile-preview aside {
            display: none;
        }
        .entity-picker-body.mobile-preview .entity-picker-detail {
            display: flex;
            padding: 0 0 0.875rem;
            border-left: 0;
        }
        .entity-picker-detail > .preview-title {
            display: none;
        }
        .mobile-selected-title,
        .mobile-suggestions-title {
            display: block;
            padding: 0.75rem 0.875rem 0.5rem;
            flex: 0 0 auto;
            background: var(--picker-mobile-section-bg);
            color: var(--picker-muted);
            font-size: var(--picker-detail-size);
        }
        .mobile-suggestions-title {
            margin-bottom: 0.5rem;
        }
        .mobile-selected-entity {
            display: flex;
            min-height: 3rem;
            padding: 0.625rem 0.875rem;
            flex: 0 0 auto;
            align-items: center;
            gap: 0.75rem;
            background: var(--entity-selection-bg);
        }
        .mobile-selected-entity > :global(.sf-icon) {
            flex: 0 0 auto;
            color: var(--picker-muted);
            font-size: 1.125rem;
        }
        .mobile-selected-entity > span {
            min-width: 0;
            flex: 1 1 auto;
            overflow: hidden;
            text-overflow: ellipsis;
            white-space: nowrap;
        }
        .mobile-selected-entity button {
            display: inline-flex;
            padding: 0;
            flex: 0 0 auto;
            border: 0;
            background: transparent;
            color: inherit;
            font-size: 1.5rem;
        }
        .has-selection .preview-surface {
            width: auto;
            margin-inline: 0.875rem;
        }
    }
</style>
