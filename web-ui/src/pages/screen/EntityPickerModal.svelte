<script lang="ts">
    import { _ } from 'svelte-i18n';
    import HassEntityRow from '@/component/HassEntityRow/HassEntityRow.svelte';
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';
    import Modal from '@/component/Modal/Modal.svelte';
    import { addEntityToScreen, getScreenEntityIcon, searchScreenEntities, type TEntityPickerMode } from '@/hooks/useScreenConfiguration';
    import { haRegistryStore } from '@/stores/haRegistry';
    import { homeAssistantStore } from '@/stores/homeassistant';
    import { isHomeAssistantAvailable } from '@/hooks/useHomeAssistant.svelte';
    import { EMessageType, sendMessage } from '@/stores/message';
    import { isHassEntitySupported } from '@/constant';
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
        void $haRegistryStore.tree;
        return searchScreenEntities(query);
    });
    const selectedEntitySupported = $derived.by(() => {
        const entity = selectedEntity;
        if (!entity) return false;
        const state = $haRegistryStore.states.find((item) => item.entity_id === entity.id);
        return isHassEntitySupported(entity.id, state?.attributes?.device_class);
    });
    const DOMAIN_ICONS: Record<string, string> = {
        automation: 'mdi:robot',
        calendar: 'mdi:calendar',
        conversation: 'mdi:forum-outline',
        person: 'mdi:account',
        scene: 'mdi:palette',
        sun: 'mdi:white-balance-sunny',
        todo: 'mdi:format-list-checks',
        zone: 'mdi:map-marker-radius',
    };

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

    function addSelected() {
        if (screenIndex === null || !selectedEntity) return;
        if (!isHomeAssistantAvailable($homeAssistantStore.connectionInfo)) {
            sendMessage(EMessageType.ERROR, $_('page.screen.haConnectionAbnormal'));
            return;
        }
        if (!selectedEntitySupported) return;
        addEntityToScreen(screenIndex, mode, selectedEntity);
        onclose();
    }

    function selectEntity(entity: IHassRegistryTreeEntity) {
        selectedEntity = entity;
    }
</script>

{#snippet entityRow(entity: IHassRegistryTreeEntity)}
    <button class:selected={selectedEntity?.id === entity.id} class="tree-entity" type="button" onclick={() => selectEntity(entity)}>
        <SvgIcon name={getScreenEntityIcon(entity.id)} /><span>{entity.name}</span>
    </button>
{/snippet}
{#snippet deviceRow(device: IHassRegistryTreeDevice)}
    <details class="tree-node tree-device">
        <summary><span class="tree-chevron"><SvgIcon name="arrowRight.svg" /></span><SvgIcon name="mdi:devices" /><span>{device.name}</span></summary>
        <div class="tree-children">
            {#each device.children as child (child.id)}{@render deviceRow(child)}{/each}{#each device.entities as entity (entity.id)}{@render entityRow(entity)}{/each}
        </div>
    </details>
{/snippet}
{#snippet areaRow(area: IHassRegistryTreeArea)}
    <details class="tree-node tree-area">
        <summary><span class="tree-chevron"><SvgIcon name="arrowRight.svg" /></span><SvgIcon name="mdi:door" /><span>{area.name}</span></summary>
        <div class="tree-children">
            {#each area.devices as device (device.id)}{@render deviceRow(device)}{/each}{#each area.entities as entity (entity.id)}{@render entityRow(entity)}{/each}
        </div>
    </details>
{/snippet}
{#snippet domainRow(domain: IHassRegistryTreeDomain)}
    <details class="tree-node">
        <summary
            ><span class="tree-chevron"><SvgIcon name="arrowRight.svg" /></span><SvgIcon name={DOMAIN_ICONS[domain.domain] ?? getScreenEntityIcon(domain.entities[0].id)} /><span
                >{domain.name}</span
            ></summary
        >
        <div class="tree-children">
            {#each domain.entities as entity (entity.id)}{@render entityRow(entity)}{/each}
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
                        ><SvgIcon name="mdi:magnify" /><input bind:value={query} placeholder={$_('page.screen.searchEntity')} />
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
                                {#each results as result (result.entity.id)}{@render entityRow(result.entity)}{:else}<div class="entity-tree-status">
                                        {$_('page.screen.noEntities')}
                                    </div>{/each}
                            </div>
                        {:else}
                            {#each $haRegistryStore.tree.floors as floor (floor.id)}
                                <details class="tree-node tree-floor">
                                    <summary><span class="tree-chevron"><SvgIcon name="arrowRight.svg" /></span><SvgIcon name="mdi:layers" /><span>{floor.name}</span></summary>
                                    <div class="tree-children">
                                        {#each floor.areas as area (area.id)}{@render areaRow(area)}{/each}
                                    </div>
                                </details>
                            {/each}
                            {#if $haRegistryStore.tree.otherAreas.length}<details class="tree-node tree-floor">
                                    <summary
                                        ><span class="tree-chevron"><SvgIcon name="arrowRight.svg" /></span><SvgIcon name="mdi:view-grid" /><span
                                            >{$_('page.screen.otherAreas')}</span
                                        ></summary
                                    >
                                    <div class="tree-children">
                                        {#each $haRegistryStore.tree.otherAreas as area (area.id)}{@render areaRow(area)}{/each}
                                    </div>
                                </details>{/if}
                            {#if $haRegistryStore.tree.unassignedDevices.length || $haRegistryStore.tree.unassignedServices.length || $haRegistryStore.tree.unassignedEntityDomains.length || $haRegistryStore.tree.unassignedHelperDomains.length}
                                <section>
                                    <span class="tree-section-title">{$_('page.screen.unassigned')}</span>
                                    {#if $haRegistryStore.tree.unassignedEntityDomains.length}<details class="tree-node tree-unassigned-group">
                                            <summary
                                                ><span class="tree-chevron"><SvgIcon name="arrowRight.svg" /></span><SvgIcon name="mdi:shape" /><span
                                                    >{$_('page.screen.entities')}</span
                                                ></summary
                                            >
                                            <div class="tree-children">
                                                {#each $haRegistryStore.tree.unassignedEntityDomains as domain (domain.domain)}{@render domainRow(domain)}{/each}
                                            </div>
                                        </details>{/if}
                                    {#if $haRegistryStore.tree.unassignedHelperDomains.length}<details class="tree-node tree-unassigned-group">
                                            <summary
                                                ><span class="tree-chevron"><SvgIcon name="arrowRight.svg" /></span><SvgIcon name="mdi:toggle-switch" /><span
                                                    >{$_('page.screen.helpers')}</span
                                                ></summary
                                            >
                                            <div class="tree-children">
                                                {#each $haRegistryStore.tree.unassignedHelperDomains as domain (domain.domain)}{@render domainRow(domain)}{/each}
                                            </div>
                                        </details>{/if}
                                    {#if $haRegistryStore.tree.unassignedDevices.length}<details class="tree-node tree-unassigned-group">
                                            <summary
                                                ><span class="tree-chevron"><SvgIcon name="arrowRight.svg" /></span><SvgIcon name="mdi:puzzle" /><span
                                                    >{$_('page.screen.devices')}</span
                                                ></summary
                                            >
                                            <div class="tree-children">
                                                {#each $haRegistryStore.tree.unassignedDevices as device (device.id)}{@render deviceRow(device)}{/each}
                                            </div>
                                        </details>{/if}
                                    {#if $haRegistryStore.tree.unassignedServices.length}<details class="tree-node tree-unassigned-group">
                                            <summary
                                                ><span class="tree-chevron"><SvgIcon name="arrowRight.svg" /></span><SvgIcon name="mdi:home-assistant" /><span
                                                    >{$_('page.screen.services')}</span
                                                ></summary
                                            >
                                            <div class="tree-children">
                                                {#each $haRegistryStore.tree.unassignedServices as device (device.id)}{@render deviceRow(device)}{/each}
                                            </div>
                                        </details>{/if}
                                </section>
                            {/if}
                        {/if}
                    </div>
                </aside>
                <div class="entity-picker-detail">
                    {#if selectedEntity}
                        <div class="mobile-selected-entity">
                            <span>{selectedEntity.name}</span>
                            <button type="button" aria-label={$_('common.close')} onclick={() => (selectedEntity = null)}><SvgIcon name="mdi:close" /></button>
                        </div>
                        <span class="mobile-suggestions-title">{$_('page.screen.suggestions')}</span>
                    {/if}
                    {#if selectedEntity && selectedEntitySupported}<div class="entity-detail-preview">
                            <HassEntityRow
                                variant={mode}
                                entityId={selectedEntity.id}
                                name={selectedEntity.name}
                                onclick={addSelected}
                            />
                        </div>
                    {:else if selectedEntity}
                        <div class="unsupported-entity"><SvgIcon name="mdi:alert-circle" /><strong>{$_('page.screen.entityNotSupported')}</strong></div>
                    {:else}<i class="bi bi-plus-square selected-entity-icon"></i><strong>{$_('page.screen.pickEntityHint')}</strong>{/if}
                </div>
            </div>
        </div>
    {/snippet}
</Modal>

<style lang="scss">
    .entity-picker {
        width: min(820px, calc(100vw - 2rem));
        height: min(570px, calc(100vh - 2rem));
        overflow: hidden;
        border-radius: 0.65rem;
        background: var(--body-inner-level-2);
        color: var(--bs-body-color);
    }
    header {
        display: flex;
        height: 52px;
        padding: 0 1rem;
        align-items: center;
        gap: 0.75rem;
        border-bottom: 1px solid var(--bs-border-color);
    }
    header button {
        border: 0;
        background: transparent;
        color: inherit;
        font-size: 1.25rem;
    }
    .entity-picker-body {
        display: grid;
        height: calc(100% - 52px);
        grid-template-columns: 46% 54%;
    }
    aside {
        display: flex;
        min-height: 0;
        padding: 1rem 0 0;
        flex-direction: column;
        border-right: 1px solid var(--bs-border-color);
    }
    .entity-search {
        display: flex;
        margin: 0 1rem 0.75rem;
        padding: 0 0.75rem;
        align-items: center;
        gap: 0.6rem;
        border: 1px solid var(--bs-border-color);
        border-radius: 0.5rem;
    }
    input {
        min-width: 0;
        width: 100%;
        min-height: 40px;
        flex: 1;
        border: 0;
        outline: 0;
        background: transparent;
        color: inherit;
    }
    .entity-search button {
        display: inline-flex;
        padding: 0.25rem;
        flex: 0 0 auto;
        border: 0;
        background: transparent;
        color: var(--bs-secondary-color);
        font-size: 1rem;
    }
    .entity-tree {
        min-height: 0;
        padding-bottom: 1rem;
        overflow-y: auto;
        font-weight: 400;
    }
    .entity-tree-status {
        display: flex;
        padding: 1rem;
        align-items: center;
        justify-content: center;
        gap: 0.6rem;
        color: var(--bs-secondary-color);
        text-align: center;
    }
    .tree-node summary {
        display: flex;
        min-width: 0;
        min-height: 42px;
        padding: 0.55rem 0.8rem;
        align-items: center;
        gap: 0.65rem;
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
        background: color-mix(in srgb, var(--bs-primary) 12%, transparent);
    }
    .tree-chevron {
        display: inline-flex;
        flex: 0 0 auto;
        font-size: 0.72rem;
        transition: transform 0.15s ease;
    }
    .tree-node[open] > summary .tree-chevron {
        transform: rotate(90deg);
    }
    .tree-floor > summary,
    .tree-section-title {
        background: color-mix(in srgb, var(--bs-body-bg) 75%, transparent);
    }
    .tree-floor > summary {
        border-top: 1px solid var(--bs-border-color);
    }
    .tree-section-title {
        display: block;
        padding: 0.45rem 0.8rem;
        font-size: 0.8rem;
    }
    .tree-unassigned-group > summary {
        padding-left: 1rem;
    }
    .tree-children {
        min-width: 0;
        padding-left: 1rem;
    }
    .tree-entity {
        display: flex;
        width: 100%;
        min-height: 44px;
        margin: 0.125rem 0;
        padding: 0.45rem 0.8rem 0.45rem 1.75rem;
        align-items: center;
        gap: 0.7rem;
        border: 0;
        background: transparent;
        color: inherit;
        text-align: left;
    }
    .entity-search-results .tree-entity {
        padding-left: 0.8rem;
    }
    .tree-entity span {
        display: flex;
        min-width: 0;
        flex-direction: column;
    }
    .tree-entity > span {
        width: 100%;
        overflow: hidden;
        font-weight: 400;
        text-overflow: ellipsis;
        white-space: nowrap;
    }
    .entity-picker-detail {
        display: flex;
        padding: 2rem;
        align-items: center;
        justify-content: center;
        flex-direction: column;
        gap: 0.5rem;
        font-weight: 600;
        text-align: center;
    }
    .selected-entity-icon {
        color: var(--bs-primary);
        font-size: 1.6rem;
    }
    .entity-detail-preview {
        width: min(360px, 100%);
    }
    .mobile-selected-entity,
    .mobile-suggestions-title {
        display: none;
    }
    .unsupported-entity {
        display: flex;
        align-items: center;
        flex-direction: column;
        gap: 0.75rem;
        color: var(--bs-secondary-color);
    }
    .unsupported-entity :global(.sf-icon) {
        color: var(--bs-warning);
        font-size: 2rem;
    }
    @media (max-width: 700px) {
        .entity-picker {
            width: 100vw;
            max-width: 100%;
            height: min(82dvh, 44rem);
            border-radius: 1.25rem 1.25rem 0 0;
        }
        header,
        .entity-picker-body,
        aside,
        .entity-tree,
        .entity-picker-detail {
            min-width: 0;
            max-width: 100%;
        }
        header > span {
            min-width: 0;
            overflow: hidden;
            text-overflow: ellipsis;
            white-space: nowrap;
        }
        .entity-search {
            min-width: 0;
        }
        .tree-children {
            max-width: 100%;
            overflow: hidden;
        }
        .entity-picker-body {
            grid-template-columns: 1fr;
        }
        .entity-picker-detail {
            display: none;
        }
        .entity-picker-body.mobile-preview aside {
            display: none;
        }
        .entity-picker-body.mobile-preview .entity-picker-detail {
            display: flex;
            padding: 0;
            align-items: stretch;
            justify-content: flex-start;
            gap: 0;
            font-weight: 400;
            text-align: left;
        }
        .mobile-selected-entity {
            display: flex;
            min-height: 44px;
            padding: 0.55rem 0.75rem;
            align-items: center;
            justify-content: space-between;
            gap: 0.75rem;
            border-bottom: 1px solid var(--bs-border-color);
            background: color-mix(in srgb, var(--bs-body-bg) 65%, transparent);
        }
        .mobile-selected-entity > span {
            min-width: 0;
            overflow: hidden;
            text-overflow: ellipsis;
            white-space: nowrap;
        }
        .mobile-selected-entity button {
            display: inline-flex;
            padding: 0.2rem;
            flex: 0 0 auto;
            border: 0;
            background: transparent;
            color: inherit;
            font-size: 1.1rem;
        }
        .mobile-suggestions-title {
            display: block;
            padding: 0.7rem 0.75rem 0.45rem;
            color: var(--bs-secondary-color);
            font-size: 0.75rem;
        }
        .entity-detail-preview,
        .unsupported-entity {
            width: auto;
            max-width: calc(100% - 1.5rem);
            margin: 0 0.75rem;
            padding: 1.75rem 0.75rem;
            border-radius: 0.65rem;
            background: color-mix(in srgb, var(--bs-body-bg) 82%, transparent);
        }
    }
</style>
