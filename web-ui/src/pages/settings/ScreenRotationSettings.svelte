<script lang="ts">
    import { _ } from 'svelte-i18n';
    import { pop } from 'svelte-spa-router';
    import Button from '@/component/Button/Button.svelte';
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';
    import SettingPageHeader from '@/component/PageHeader/PageHeader.svelte';
    import { ROUTES_PATH } from '@/constant';
    import { deviceStore } from '@/stores/device';
    import { EMessageType, sendMessage } from '@/stores/message';
    import { EWsMethods } from '@/types/websocket';
    import sendForReply from '@/ws/sendForReply';

    type ScreenRotation = 0 | 90;
    let selectedRotation = $state<ScreenRotation>($deviceStore.screenRotation.rotation);
    let loading = $state(false);

    $effect(() => {
        if (!loading) selectedRotation = $deviceStore.screenRotation.rotation;
    });

    async function save() {
        if (loading) return;
        loading = true;
        try {
            const response = await sendForReply<{ rotation: ScreenRotation }>({
                method: EWsMethods.SET_SCREEN_ROTATION,
                data: { rotation: selectedRotation },
            });
            if (response.error === 0) {
                sendMessage(EMessageType.SUCCESS, $_('TIPS.SAVED'));
                await pop();
            } else {
                sendMessage(EMessageType.ERROR, $_('TIPS.OPERATION_FAILED'));
            }
        } catch (error) {
            console.error('Failed to set screen rotation:', error);
            sendMessage(EMessageType.ERROR, $_('TIPS.OPERATION_FAILED'));
        } finally {
            loading = false;
        }
    }
</script>

<div class="page-container settings-detail screen-rotation-settings">
    <SettingPageHeader title={$_('page.settings.rotation')} />

    <div class="rotation-options" role="radiogroup" aria-label={$_('page.settings.rotation')}>
        {#each [{ value: 0 as ScreenRotation, label: 'settingsDetail.portrait', orientation: 'portrait' }, { value: 90 as ScreenRotation, label: 'settingsDetail.landscape', orientation: 'landscape' }] as option}
            <!-- svelte-ignore a11y_click_events_have_key_events -->
            <div
                class:selected={selectedRotation === option.value}
                class="rotation-option"
                role="radio"
                aria-checked={selectedRotation === option.value}
                aria-disabled={loading}
                tabindex={selectedRotation === option.value ? 0 : -1}
                onclick={() => !loading && (selectedRotation = option.value)}
            >
                <span class="option-title">{$_(option.label)}</span>
                <span class={`device-preview ${option.orientation}`} aria-hidden="true">
                    <span class="preview-status">
                        <span>{$_('page.screen.defaultSplit')}</span>
                        <span class="preview-status-icons">
                            <SvgIcon name="ha.svg" />
                            <SvgIcon name="wifi_4.svg" />
                            <b>19:20</b>
                        </span>
                    </span>
                    <span class="preview-badges"><i></i><i></i><i></i></span>
                    <span class="preview-entities">
                        <i></i><i></i><i></i>
                    </span>
                </span>
                {#if selectedRotation === option.value}
                    <span class="selected-check" aria-hidden="true"><SvgIcon name="themeCheck.svg" /></span>
                {/if}
            </div>
        {/each}
    </div>

    <Button type="primary" class="settings-save" style="width: min(70%, 560px)" {loading} onclick={save}>{$_('common.save')}</Button>
</div>

<style>
    .rotation-options {
        display: grid;
        grid-template-columns: repeat(2, minmax(0, 1fr));
        gap: 1rem;
    }
    .rotation-option {
        position: relative;
        display: flex;
        min-width: 0;
        min-height: 21.5rem;
        padding: 1rem;
        flex-direction: column;
        align-items: flex-start;
        justify-content: space-between;
        color: var(--body-text-color-highlight);
        border: 2px solid transparent;
        border-radius: 0.875rem;
        background: var(--body-inner-level-2);
        text-align: left;
        cursor: pointer;
    }
    .rotation-option.selected {
        border-color: var(--primary-button-bg, rgba(1, 119, 253, 1));
    }
    .rotation-option[aria-disabled='true'] {
        cursor: wait;
        opacity: 0.75;
    }
    .option-title {
        font-size: 1rem;
        font-weight: 500;
    }
    .device-preview {
        display: flex;
        max-width: 100%;
        padding: 0.4rem;
        overflow: hidden;
        flex-direction: column;
        gap: 0.35rem;
        align-self: center;
        border-radius: 0.65rem;
        background: #000;
        aspect-ratio: 2 / 3;
    }
    .device-preview.portrait {
        width: min(62%, 11rem);
    }
    .device-preview.landscape {
        width: min(100%, 20rem);
        aspect-ratio: 3 / 2;
        align-self: center;
        margin-block: auto;
    }
    .preview-status {
        display: flex;
        min-width: 0;
        align-items: center;
        justify-content: space-between;
        gap: 0.25rem;
        color: #fff;
        font-size: clamp(0.4rem, 1.2vw, 0.65rem);
        white-space: nowrap;
    }
    .preview-status-icons {
        display: inline-flex;
        flex: 0 0 auto;
        align-items: center;
        gap: 0.2em;
    }
    .preview-status-icons :global(.sf-icon > svg) {
        width: 1em;
        height: 1em;
    }
    .preview-status-icons b {
        font-size: 1.1em;
        font-weight: 500;
    }
    .preview-badges {
        display: flex;
        gap: 0.2rem;
    }
    .preview-badges i {
        width: 22%;
        height: 0.55rem;
        border-radius: 999px;
        background: #292d31;
    }
    .preview-entities {
        display: grid;
        min-height: 0;
        flex: 1;
        grid-template-columns: 1fr;
        gap: 0.25rem;
    }
    .landscape .preview-entities {
        grid-template-columns: repeat(2, minmax(0, 1fr));
    }
    .preview-entities i {
        min-height: 0.75rem;
        border-radius: 0.4rem;
        background: #292d31;
    }
    .selected-check {
        position: absolute;
        top: 0;
        right: 0;
        display: inline-flex;
        width: 31px;
        height: 31px;
    }
    .selected-check :global(.sf-icon),
    .selected-check :global(svg) {
        width: 100%;
        height: 100%;
    }
    .settings-save {
        display: block;
        margin: 1.25rem auto 0;
    }
    @media (max-width: 480px) {
        .rotation-options {
            gap: 0.625rem;
        }
        .rotation-option {
            min-height: 17rem;
            padding: 0.75rem;
        }
        .device-preview.portrait {
            width: 74%;
            margin-block: auto;
        }
    }
</style>
