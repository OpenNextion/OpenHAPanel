<script lang="ts">
    import { _ } from 'svelte-i18n';
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';
    import HassEntityRow from '@/component/HassEntityRow/HassEntityRow.svelte';
    import Modal from '@/component/Modal/Modal.svelte';
    import DropDown from '@/component/Dropdown/DropDown.svelte';
    import { getHassEntityPresentation, normalizeHassEntityAction } from '@/hooks/useHassEntity';
    import { haRegistryStore } from '@/stores/haRegistry';
    import { EMessageType, sendMessage } from '@/stores/message';
    import { shouldShowScreenEntityIconBackground } from '@/stores/screenConfig';
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
    let draftEntityId = $state('');
    let entityQuery = $state('');
    let initializedKey = '';
    const entityResults = $derived.by(() => {
        void $haRegistryStore.tree;
        return searchScreenEntities(entityQuery);
    });
    const entityOptions = $derived(entityResults.map((result) => ({ label: result.displayName, value: result.entity.id })));
    const draftEntity = $derived(getHassEntityPresentation(draftEntityId, $haRegistryStore));
    const interactions = $derived(mode === 'badge' ? draftEntity.config?.badgeInteractions ?? draftEntity.interactions : draftEntity.interactions);
    const longPressSupported = $derived(interactions.hold.supported);
    const tapOptions = $derived(getScreenActionOptions(interactions.tap));
    const holdOptions = $derived(getScreenActionOptions(interactions.hold));
    const iconTapOptions = $derived(getScreenActionOptions(draftEntity.interactions.iconTap));
    const tapLabel = $derived(tapOptions.find((option) => option.value === tapAction)?.label ?? tapAction);
    const holdLabel = $derived(holdOptions.find((option) => option.value === holdAction)?.label ?? holdAction);
    const iconTapLabel = $derived(iconTapOptions.find((option) => option.value === iconTapAction)?.label ?? iconTapAction);
    const draftEntitySupported = $derived(draftEntity.supported);
    const configuredEntityContext = $derived(draftEntity.context);
    const previewActionConfig = $derived(
        iconTapAction === 'default' ? undefined : { icon_tap_action: { action: iconTapAction } },
    );

    $effect(() => {
        if (!visible || screenIndex === null || !entityId) {
            initializedKey = '';
            return;
        }
        const key = `${mode}:${screenIndex}:${entityId}`;
        if (initializedKey === key) return;
        initializedKey = key;
        const actions = getScreenEntityActionSelections(mode, screenIndex, entityId);
        const entity = getHassEntityPresentation(entityId, $haRegistryStore);
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
        );
        onclose();
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
        }
    }
</script>

<Modal modalVisible={visible} footer={false} mobileSheet={true} setVisible={(nextVisible) => !nextVisible && onclose()}>
    {#snippet customSnippet()}
        <div class="entity-settings-dialog" role="dialog" aria-modal="true" aria-label={$_('page.screen.entitySettings')}>
            <header>
                <button type="button" aria-label={$_('common.close')} onclick={onclose}><SvgIcon name="mdi:close" /></button>
                <strong>{mode === 'badge' ? 'Badge configuration' : 'Tile card configuration'}</strong>
            </header>
            <div class="entity-settings-dialog-content">
                <div class="entity-settings">
                    <section class="configured-entity">
                        <span>Entity</span>
                        <DropDown
                            id="entity-settings-entity"
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
                                <SvgIcon name={draftEntity.icon} />
                                <span><strong>{draftEntity.displayName}</strong><small>{configuredEntityContext}</small></span>
                                <SvgIcon name="arrowRight.svg" />
                            </span>
                            {#snippet optionSnippet(option)}
                                {@const optionEntity = getHassEntityPresentation(String(option.value), $haRegistryStore)}
                                <SvgIcon name={optionEntity.icon} />
                                <span><strong>{option.label}</strong><small>{option.value}</small></span>
                            {/snippet}
                        </DropDown>
                    </section>
                    <details class="entity-config-section" open>
                        <summary
                            ><span class="section-leading-icon"><SvgIcon name="mdi:gesture-tap-button" /></span><span>Interactions</span><span class="section-chevron"
                                ><SvgIcon name="arrowRight.svg" /></span
                            ></summary
                        >
                        <div class="interaction-fields">
                            <div class="behavior-field">
                                <span>Tap behavior</span><DropDown bind:value={tapAction} options={tapOptions} ariaLabel="Tap behavior"><span>{tapLabel}</span></DropDown>
                            </div>
                            {#if mode === 'entity'}
                                <div class="behavior-field">
                                    <span>Icon tap behavior</span><DropDown bind:value={iconTapAction} options={iconTapOptions} ariaLabel="Icon tap behavior"
                                        ><span>{iconTapLabel}</span></DropDown
                                    >
                                </div>
                            {/if}
                            {#if longPressSupported}
                                <div class="behavior-field">
                                    <span>Long press behavior</span><DropDown bind:value={holdAction} options={holdOptions} ariaLabel="Long press behavior"
                                        ><span>{holdLabel}</span></DropDown
                                    >
                                </div>
                            {/if}
                        </div>
                    </details>
                    {#if mode === 'entity'}
                        <details class="entity-config-section">
                            <summary
                                ><span class="section-leading-icon"><SvgIcon name="mdi:format-list-bulleted-square" /></span><span>Features</span><span class="section-chevron"
                                    ><SvgIcon name="arrowRight.svg" /></span
                                ></summary
                            >
                            <div class="feature-placeholder"><SvgIcon name="mdi:message-text-outline" /><span>{$_('page.screen.comingSoon')}</span></div>
                        </details>
                    {/if}
                </div>
                <div class="entity-settings-preview">
                    <span class="preview-title">{$_('page.screen.suggestions')}</span>
                    <div class="preview-surface">
                        {#if draftEntitySupported}
                            <HassEntityRow
                                variant={mode}
                                entityId={draftEntityId}
                                showIconBackground={shouldShowScreenEntityIconBackground(draftEntityId, previewActionConfig)}
                            />
                        {:else}
                            <span class="unavailable-preview"><SvgIcon name={draftEntity.icon} />Unavailable</span>
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
        box-sizing: border-box;
        width: min(50rem, calc(100vw - 2rem));
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
    .entity-settings-dialog-content {
        display: grid;
        min-height: 390px;
        grid-template-columns: 24.9375rem minmax(0, 1fr);
        border-bottom: 1px solid var(--bs-border-color);
    }
    .entity-settings {
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
    .entity-config-section {
        margin-top: 0.6rem;
        border: 1px solid var(--bs-border-color);
        border-radius: 0.35rem;
    }
    .entity-config-section > summary {
        display: grid;
        min-height: 50px;
        padding: 0.7rem;
        align-items: center;
        grid-template-columns: auto 1fr auto;
        gap: 0.65rem;
        cursor: pointer;
        list-style: none;
    }
    .entity-config-section > summary::-webkit-details-marker {
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
    .entity-config-section[open] .section-chevron :global(.sf-icon) {
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
    .entity-settings-preview {
        padding: 1.25rem 1rem;
        border-left: 1px solid var(--bs-border-color);
        background: var(--body-inner-level-2);
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
        //min-height: 115px;
        padding: 1.4rem 3rem;
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
    .delete-entity-button {
        background: #ef8388;
    }
    .save-entity-button {
        background: var(--primary-button-bg);
    }
    @media (max-width: 700px) {
        .entity-settings-dialog {
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
        .entity-settings-dialog-content,
        .entity-settings,
        .configured-entity,
        .entity-settings-preview,
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
        .entity-settings-dialog-content {
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
        .entity-settings-preview {
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
