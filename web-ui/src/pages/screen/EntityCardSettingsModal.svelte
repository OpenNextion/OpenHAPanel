<script lang="ts">
    import { _ } from 'svelte-i18n';
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';
    import HassEntityRow from '@/component/HassEntityRow/HassEntityRow.svelte';
    import Modal from '@/component/Modal/Modal.svelte';
    import DropDown from '@/component/Dropdown/DropDown.svelte';
    import { isHassEntityLongPressSupported, isHassEntitySupported } from '@/constant';
    import { haRegistryStore } from '@/stores/haRegistry';
    import { homeAssistantStore } from '@/stores/homeassistant';
    import { isHomeAssistantAvailable } from '@/hooks/useHomeAssistant.svelte';
    import { EMessageType, sendMessage } from '@/stores/message';
    import {
        deleteScreenCard,
        getScreenActionOptions,
        getScreenCardActionSelections,
        getScreenEntityDefaultActions,
        getScreenEntityName,
        getScreenEntityIcon,
        saveScreenCardSettings,
        searchScreenEntities,
        type TActionSelection,
    } from '@/hooks/useScreenConfiguration';

    interface Props {
        visible: boolean;
        screenIndex: number | null;
        entityId: string;
        onclose: () => void;
    }

    let { visible, screenIndex, entityId, onclose }: Props = $props();
    let tapAction = $state<TActionSelection>('default');
    let holdAction = $state<TActionSelection>('default');
    let iconTapAction = $state<TActionSelection>('default');
    let draftEntityId = $state('');
    let draftEntityName = $state('');
    let entityQuery = $state('');
    let initializedKey = '';
    const TOGGLE_UNSUPPORTED_DOMAINS = new Set(['sensor', 'binary_sensor', 'event']);
    const entityResults = $derived.by(() => {
        void $haRegistryStore.tree;
        return searchScreenEntities(entityQuery);
    });
    const entityOptions = $derived(entityResults.map((result) => ({ label: result.entity.name, value: result.entity.id })));
    const entityDomain = $derived(draftEntityId.split('.')[0] ?? '');
    const toggleSupported = $derived(!TOGGLE_UNSUPPORTED_DOMAINS.has(entityDomain));
    const draftDeviceClass = $derived($haRegistryStore.states.find((item) => item.entity_id === draftEntityId)?.attributes?.device_class);
    const longPressSupported = $derived(isHassEntityLongPressSupported(draftEntityId, draftDeviceClass));
    const defaultActions = $derived(getScreenEntityDefaultActions(draftEntityId));
    const tapOptions = $derived(getScreenActionOptions(defaultActions.tap).filter((option) => toggleSupported || option.value !== 'toggle'));
    const holdOptions = $derived(getScreenActionOptions(defaultActions.hold).filter((option) => toggleSupported || option.value !== 'toggle'));
    const iconTapOptions = $derived(getScreenActionOptions(defaultActions.iconTap).filter((option) => toggleSupported || option.value !== 'toggle'));
    const tapLabel = $derived(tapOptions.find((option) => option.value === tapAction)?.label ?? tapAction);
    const holdLabel = $derived(holdOptions.find((option) => option.value === holdAction)?.label ?? holdAction);
    const iconTapLabel = $derived(iconTapOptions.find((option) => option.value === iconTapAction)?.label ?? iconTapAction);
    const draftEntitySupported = $derived.by(() => {
        const state = $haRegistryStore.states.find((item) => item.entity_id === draftEntityId);
        return isHassEntitySupported(draftEntityId, state?.attributes?.device_class);
    });
    const configuredEntityContext = $derived.by(() => {
        const entity = $haRegistryStore.entities.find((item) => (item.ei ?? item.entity_id) === draftEntityId);
        const deviceId = entity?.di ?? entity?.device_id;
        const device = deviceId ? $haRegistryStore.devices.find((item) => item.id === deviceId) : undefined;
        const areaId = entity?.ai ?? entity?.area_id ?? device?.area_id;
        const area = areaId ? $haRegistryStore.areas.find((item) => item.area_id === areaId) : undefined;
        return [area?.name, device?.name_by_user ?? device?.name].filter(Boolean).join(' › ') || draftEntityId;
    });

    function normalizeActionForEntity(action: TActionSelection, targetEntityId: string) {
        const domain = targetEntityId.split('.')[0] ?? '';
        return action === 'toggle' && TOGGLE_UNSUPPORTED_DOMAINS.has(domain) ? 'default' : action;
    }

    $effect(() => {
        if (!visible || screenIndex === null || !entityId) {
            initializedKey = '';
            return;
        }
        const key = `${screenIndex}:${entityId}`;
        if (initializedKey === key) return;
        initializedKey = key;
        const actions = getScreenCardActionSelections(screenIndex, entityId);
        const entityDeviceClass = $haRegistryStore.states.find((item) => item.entity_id === entityId)?.attributes?.device_class;
        tapAction = normalizeActionForEntity(actions.tap, entityId);
        holdAction = isHassEntityLongPressSupported(entityId, entityDeviceClass) ? normalizeActionForEntity(actions.hold, entityId) : 'default';
        iconTapAction = normalizeActionForEntity(actions.iconTap, entityId);
        draftEntityId = entityId;
        draftEntityName = getScreenEntityName(entityId);
        entityQuery = '';
    });

    function save() {
        if (screenIndex === null || !entityId || !draftEntityId) return;
        if (!isHomeAssistantAvailable($homeAssistantStore.connectionInfo)) {
            sendMessage(EMessageType.ERROR, $_('page.screen.haConnectionAbnormal'));
            return;
        }
        saveScreenCardSettings(screenIndex, entityId, draftEntityId, tapAction, longPressSupported ? holdAction : 'default', iconTapAction);
        onclose();
    }

    function remove() {
        if (screenIndex === null || !entityId) return;
        deleteScreenCard(screenIndex, entityId);
        onclose();
    }

    function selectEntity(entityId: string, previousEntityId?: string) {
        const entity = entityResults.find((result) => result.entity.id === entityId)?.entity;
        if (!entity) return;
        const state = $haRegistryStore.states.find((item) => item.entity_id === entity.id);
        if (!isHassEntitySupported(entity.id, state?.attributes?.device_class)) {
            sendMessage(EMessageType.ERROR, $_('page.screen.entityNotSupported'));
            return;
        }
        const changed = entity.id !== (previousEntityId ?? draftEntityId);
        draftEntityId = entity.id;
        draftEntityName = entity.name;
        entityQuery = '';
        if (changed) {
            tapAction = 'default';
            holdAction = 'default';
            iconTapAction = 'default';
        }
    }
</script>

<Modal modalVisible={visible} footer={false} mobileSheet={true} setVisible={(nextVisible) => !nextVisible && onclose()}>
    {#snippet customSnippet()}
        <div class="entity-card-dialog" role="dialog" aria-modal="true" aria-label={$_('page.screen.entitySettings')}>
            <header>
                <button type="button" aria-label={$_('common.close')} onclick={onclose}><SvgIcon name="mdi:close" /></button>
                <strong>Tile card configuration</strong>
            </header>
            <div class="entity-card-dialog-content">
                <div class="entity-card-settings">
                    <section class="configured-entity">
                        <span>Entity</span>
                        <DropDown
                            id="entity-card-entity"
                            bind:value={draftEntityId}
                            options={entityOptions}
                            searchable={true}
                            bind:searchValue={entityQuery}
                            searchPlaceholder={$_('page.screen.searchEntity')}
                            noOptionsText={$_('page.screen.noEntities')}
                            ariaLabel={$_('page.screen.searchEntity')}
                            onchange={selectEntity}
                        >
                            <span class="configured-entity-value">
                                <SvgIcon name={getScreenEntityIcon(draftEntityId)} />
                                <span><strong>{draftEntityName || getScreenEntityName(draftEntityId)}</strong><small>{configuredEntityContext}</small></span>
                                <SvgIcon name="arrowRight.svg" />
                            </span>
                            {#snippet optionSnippet(option)}
                                <SvgIcon name={getScreenEntityIcon(String(option.value))} />
                                <span><strong>{option.label}</strong><small>{option.value}</small></span>
                            {/snippet}
                        </DropDown>
                    </section>
                    <details class="card-config-section" open>
                        <summary
                            ><span class="section-leading-icon"><SvgIcon name="mdi:gesture-tap-button" /></span><span>Interactions</span><span class="section-chevron"
                                ><SvgIcon name="arrowRight.svg" /></span
                            ></summary
                        >
                        <div class="interaction-fields">
                            <div class="behavior-field"
                                ><span>Tap behavior</span><DropDown bind:value={tapAction} options={tapOptions} ariaLabel="Tap behavior"><span>{tapLabel}</span></DropDown></div
                            >
                            {#if longPressSupported}
                                <div class="behavior-field"
                                    ><span>Long press behavior</span><DropDown bind:value={holdAction} options={holdOptions} ariaLabel="Long press behavior"
                                        ><span>{holdLabel}</span></DropDown
                                    ></div
                                >
                            {/if}
                            <div class="behavior-field"
                                ><span>Icon tap behavior</span><DropDown bind:value={iconTapAction} options={iconTapOptions} ariaLabel="Icon tap behavior"
                                    ><span>{iconTapLabel}</span></DropDown
                                ></div
                            >
                        </div>
                    </details>
                    <details class="card-config-section">
                        <summary
                            ><span class="section-leading-icon"><SvgIcon name="mdi:format-list-bulleted-square" /></span><span>Features</span><span class="section-chevron"
                                ><SvgIcon name="arrowRight.svg" /></span
                            ></summary
                        >
                        <div class="feature-placeholder"><SvgIcon name="mdi:message-text-outline" /><span>{$_('page.screen.comingSoon')}</span></div>
                    </details>
                </div>
                <div class="entity-card-preview">
                    <span class="preview-title">{$_('page.screen.suggestions')}</span>
                    <div class="preview-surface">
                        {#if draftEntitySupported}
                            <HassEntityRow entityId={draftEntityId} name={draftEntityName || getScreenEntityName(draftEntityId)} />
                        {:else}
                            <span class="unavailable-preview"><SvgIcon name={getScreenEntityIcon(draftEntityId)} />Unavailable</span>
                        {/if}
                    </div>
                </div>
            </div>
            <footer>
                <button class="delete-card-button" type="button" onclick={remove}>{$_('page.screen.deleteEntity')}</button>
                <button class="save-card-button" type="button" onclick={save}>{$_('common.save')}</button>
            </footer>
        </div>
    {/snippet}
</Modal>

<style lang="scss">
    .entity-card-dialog {
        width: min(744px, calc(100vw - 2rem));
        overflow: hidden;
        border: 1px solid var(--bs-border-color);
        border-radius: 0.75rem;
        background: var(--body-inner-level-2);
        color: var(--bs-body-color);
    }
    header {
        display: flex;
        height: 50px;
        padding: 0 0.9rem;
        align-items: center;
        gap: 0.75rem;
        border-bottom: 1px solid var(--bs-border-color);
    }
    header button {
        display: inline-flex;
        padding: 0;
        border: 0;
        background: transparent;
        color: inherit;
        font-size: 1.25rem;
    }
    .entity-card-dialog-content {
        display: grid;
        min-height: 390px;
        grid-template-columns: 56% 44%;
    }
    .entity-card-settings {
        padding: 1rem;
    }
    .configured-entity {
        position: relative;
        display: flex;
        margin-bottom: 0.75rem;
        flex-direction: column;
        gap: 0.15rem;
    }
    .configured-entity > span {
        margin-bottom: 0.35rem;
        font-size: 1rem;
    }
    .configured-entity :global(.dropdown) {
        width: 100%;
        border-radius: 0.75rem;
    }
    .configured-entity :global(.dropdown-menu) {
        max-height: 220px;
        overflow-y: auto;
    }
    .configured-entity-value {
        display: grid;
        width: 100%;
        min-height: 4rem;
        min-width: 0;
        padding: 0.5rem 0.75rem;
        align-items: center;
        grid-template-columns: 2rem minmax(0, 1fr) 1rem;
        gap: 0.75rem;
        color: rgba(48, 49, 50, 1);
        border-bottom: 1px solid rgba(142, 142, 147, 0.28);
        border-radius: 0.75rem 0.75rem 0 0;
        background: rgba(245, 245, 245, 1);
    }
    :global([data-bs-theme='dark']) .configured-entity-value {
        color: #e5e5e5;
        border-bottom-color: rgba(225, 225, 225, 0.35);
        background: rgba(72, 72, 72, 1);
    }
    .configured-entity-value > :global(.sf-icon:first-child) {
        display: inline-flex;
        width: 1.75rem;
        height: 1.75rem;
        align-items: center;
        justify-content: center;
        color: rgba(142, 142, 147, 1);
        font-size: 1.5rem;
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
        font-size: 1rem;
        font-weight: 400;
        line-height: 1.4;
    }
    .configured-entity-value small {
        color: rgba(142, 142, 147, 1);
        font-size: 0.875rem;
        line-height: 1.35;
    }
    .configured-entity-value > :global(.sf-icon:last-child) {
        color: rgba(142, 142, 147, 1);
        transform: rotate(90deg);
    }
    .configured-entity :global(.dropdown-item) {
        display: flex;
        min-height: 48px;
        padding: 0.45rem 0.7rem;
        align-items: center;
        gap: 0.65rem;
        color: inherit;
    }
    .configured-entity :global(.dropdown-item > span) {
        display: flex;
        min-width: 0;
        flex-direction: column;
    }
    .configured-entity :global(.dropdown-item strong),
    .configured-entity :global(.dropdown-item small) {
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
    }
    .configured-entity :global(.dropdown-item small) {
        color: var(--bs-secondary-color);
    }
    .card-config-section {
        margin-top: 0.6rem;
        border: 1px solid var(--bs-border-color);
        border-radius: 0.35rem;
    }
    .card-config-section > summary {
        display: grid;
        min-height: 50px;
        padding: 0.7rem;
        align-items: center;
        grid-template-columns: auto 1fr auto;
        gap: 0.65rem;
        cursor: pointer;
        list-style: none;
    }
    .card-config-section > summary::-webkit-details-marker {
        display: none;
    }
    .section-leading-icon,
    .section-chevron {
        display: inline-flex;
        color: var(--bs-secondary-color);
        font-size: 1.15rem;
    }
    .section-chevron :global(.sf-icon) {
        transition: transform 0.15s ease;
        transform: rotate(90deg);
    }
    .card-config-section[open] .section-chevron :global(.sf-icon) {
        transform: rotate(-90deg);
    }
    .interaction-fields {
        display: grid;
        padding: 0 0.5rem 0.75rem;
        gap: 0.65rem;
    }
    .interaction-fields .behavior-field {
        display: flex;
        padding: 0.35rem 0.55rem 0;
        flex-direction: column;
        background: var(--bs-body-bg);
        color: var(--bs-secondary-color);
        font-size: 0.75rem;
    }
    .interaction-fields :global(.dropdown) {
        width: 100%;
        min-height: 30px;
        border-bottom: 1px solid var(--bs-border-color);
        background: transparent;
        color: var(--bs-body-color);
        font-size: 0.9rem;
    }
    .interaction-fields :global(.dropdown-toggle) {
        min-height: 30px;
    }
    .feature-placeholder {
        display: flex;
        min-height: 78px;
        padding: 0 1rem 1rem;
        align-items: center;
        justify-content: center;
        gap: 0.65rem;
        color: var(--bs-body-color);
        font-size: 1rem;
    }
    .feature-placeholder :global(.sf-icon) {
        font-size: 1.65rem;
    }
    .entity-card-preview {
        padding: 1.25rem 1rem;
        border-left: 1px solid var(--bs-border-color);
        background: var(--bs-body-bg);
    }
    .preview-title {
        display: block;
        margin-bottom: 0.75rem;
        color: var(--bs-secondary-color);
        font-size: 1rem;
    }
    .preview-surface {
        display: flex;
        width: 100%;
        min-height: 115px;
        align-items: center;
        justify-content: center;
        border-radius: 0.75rem;
        background: rgba(24, 24, 24, 1);
    }
    .unavailable-preview {
        display: inline-flex;
        min-height: 40px;
        padding: 0.4rem 0.9rem;
        align-items: center;
        gap: 0.4rem;
        border-radius: 999px;
        background: rgba(58, 58, 58, 1);
        color: #f2f2f7;
        font-size: 1rem;
    }
    .unavailable-preview :global(.sf-icon) {
        color: #8e8e93;
        font-size: 1.2rem;
    }
    footer {
        display: flex;
        padding: 1.25rem 11%;
        justify-content: center;
        gap: min(14%, 6rem);
    }
    footer button {
        width: min(180px, 42%);
        min-height: 40px;
        color: white;
        border: 0;
        border-radius: 0.45rem;
    }
    .delete-card-button {
        background: #ef8388;
    }
    .save-card-button {
        background: var(--primary-button-bg);
    }
    @media (max-width: 700px) {
        .entity-card-dialog {
            display: flex;
            width: 100vw;
            max-width: 100%;
            height: min(88dvh, 46rem);
            max-height: 100dvh;
            overflow: hidden;
            flex-direction: column;
            border-radius: 1.25rem 1.25rem 0 0;
        }
        header,
        .entity-card-dialog-content,
        .entity-card-settings,
        .configured-entity,
        .entity-card-preview,
        .preview-surface {
            min-width: 0;
            max-width: 100%;
        }
        header strong {
            min-width: 0;
            overflow: hidden;
            text-overflow: ellipsis;
            white-space: nowrap;
        }
        .entity-card-dialog-content {
            flex: 1 1 auto;
            min-height: 0;
            overflow-x: hidden;
            overflow-y: auto;
            grid-template-columns: 1fr;
        }
        .configured-entity :global(.dropdown),
        .configured-entity :global(.dropdown-menu),
        .interaction-fields :global(.dropdown),
        .interaction-fields :global(.dropdown-menu) {
            max-width: 100%;
        }
        .entity-card-preview {
            min-height: 100px;
            border-top: 1px solid var(--bs-border-color);
            border-left: 0;
        }
        footer {
            flex: 0 0 auto;
            padding: 1rem 1rem calc(1rem + env(safe-area-inset-bottom));
            gap: 1rem;
        }
        footer button {
            width: 50%;
        }
    }
</style>
