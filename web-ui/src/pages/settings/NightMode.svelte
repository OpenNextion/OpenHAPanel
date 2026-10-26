<script lang="ts">
    import { _ } from 'svelte-i18n';
    import { pop } from 'svelte-spa-router';
    import SettingPageHeader from '@/component/PageHeader/PageHeader.svelte';
    import Button from '@/component/Button/Button.svelte';
    import TimePicker from '@/component/TimePicker/TimePicker.svelte';
    import { ROUTES_PATH } from '@/constant';
    import { EMessageType, sendMessage } from '@/stores/message';
    import { deviceStore, updateDeviceField } from '@/stores/device';
    import { EWsMethods } from '@/types/websocket';
    import sendForReply from '@/ws/sendForReply';

    let enable = $state($deviceStore.nightMode.enable);
    let brightness = $state($deviceStore.nightMode.brightness);
    let from = $state($deviceStore.nightMode.from);
    let to = $state($deviceStore.nightMode.to);
    let loading = $state(false);

    $effect(() => {
        const settings = $deviceStore.nightMode;
        enable = settings.enable;
        brightness = settings.brightness;
        from = settings.from;
        to = settings.to;
    });

    function updateBrightness(value: number) {
        brightness = value;
    }

    async function save() {
        if (loading) return;
        loading = true;
        const settings = { enable, brightness, from, to };
        try {
            const response = await sendForReply< typeof settings >({
                method: EWsMethods.SET_NIGHT_MODE,
                data: settings,
            });
            if (response.error === 0) {
                updateDeviceField('nightMode', settings);
                sendMessage(EMessageType.SUCCESS, $_('TIPS.SAVED'));
                await pop();
            } else {
                sendMessage(EMessageType.ERROR, $_('TIPS.OPERATION_FAILED'));
            }
        } catch (error) {
            console.error('Failed to set night mode:', error);
            sendMessage(EMessageType.ERROR, $_('TIPS.OPERATION_FAILED'));
        } finally {
            loading = false;
        }
    }
</script>

<div class="page-container settings-detail night-mode-settings">
    <SettingPageHeader title={$_('page.settings.nightMode')} back={ROUTES_PATH.SETTINGS_DISPLAY} />
    <section class="night-mode-card">
        <header class:enabled={enable} class="night-mode-heading">
            <div>
                <h2>{$_('settingsDetail.enableNight')}</h2>
                <p>{$_('settingsDetail.nightHint')}</p>
            </div>
            <span class="form-check form-switch">
                <input
                    class="form-check-input"
                    type="checkbox"
                    role="switch"
                    bind:checked={enable}
                    disabled={loading}
                />
            </span>
        </header>

        {#if enable}
            <div class="night-mode-brightness" style={`--brightness: ${brightness}%`}>
                <span>{brightness}%</span>
                <input
                    aria-label={$_('settingsDetail.brightness')}
                    type="range"
                    min="1"
                    max="100"
                    value={brightness}
                    oninput={(event) => updateBrightness(Number(event.currentTarget.value))}
                    disabled={loading}
                />
            </div>

            <div class="night-mode-time-fields">
                <span class="night-mode-field-label">{$_('settingsDetail.from')}</span>
                <TimePicker label={$_('settingsDetail.from')} value={from} disabled={loading} onchange={(value) => (from = value)} />
                <span class="night-mode-field-label">{$_('settingsDetail.to')}</span>
                <TimePicker label={$_('settingsDetail.to')} value={to} disabled={loading} onchange={(value) => (to = value)} />
            </div>
        {/if}
    </section>

    <Button type="primary" class="settings-save" style="width: min(70%, 560px)" {loading} onclick={save}>{$_('common.save')}</Button>
</div>

<style>
    .night-mode-card {
        padding: 1rem;
        border-radius: 1rem;
        background: var(--body-inner-level-2);
    }
    .night-mode-heading {
        display: flex;
        align-items: center;
        justify-content: space-between;
        gap: 1rem;
    }
    .night-mode-heading.enabled {
        padding-bottom: 1rem;
        border-bottom: 1px solid var(--bs-border-color);
    }
    .night-mode-heading h2 {
        margin: 0;
        color: var(--body-text-color-highlight);
        font-size: 1rem;
        font-weight: 400;
        line-height: 1.4;
    }
    .night-mode-heading p {
        margin: 0.2rem 0 0;
        color: var(--body-text-color-grey);
        font-size: 0.75rem;
        line-height: 1.5;
    }
    .night-mode-heading .form-check {
        margin: 0;
        padding: 0;
        flex: 0 0 auto;
    }
    .night-mode-heading .form-check-input {
        float: none;
        margin: 0;
        cursor: pointer;
    }
    .night-mode-brightness {
        position: relative;
        display: flex;
        height: 2.75rem;
        margin: 1rem 0;
        padding: 0 0.75rem;
        overflow: hidden;
        align-items: center;
        color: #fff;
        border-radius: 0.75rem;
        background: var(--bs-tertiary-bg);
    }
    .night-mode-brightness::before {
        position: absolute;
        inset: 0 auto 0 0;
        width: var(--brightness, 30%);
        background: var(--primary-button-bg);
        content: '';
        transition: width 0.08s linear;
    }
    .night-mode-brightness > span {
        position: relative;
        z-index: 1;
        font-size: 0.875rem;
    }
    .night-mode-brightness > input {
        position: absolute;
        z-index: 2;
        inset: 0;
        width: 100%;
        opacity: 0;
        cursor: pointer;
    }
    .night-mode-time-fields {
        display: grid;
        grid-template-columns: minmax(0, 1fr) minmax(8rem, 11rem);
        gap: 0.5rem 1rem;
        align-items: center;
    }
    .night-mode-field-label {
        color: var(--body-text-color-highlight);
        font-size: 0.9rem;
    }
    @media (max-width: 480px) {
        .night-mode-card {
            padding: 0.875rem;
        }
        .night-mode-time-fields {
            grid-template-columns: minmax(0, 1fr) minmax(7rem, 9rem);
        }
    }
</style>
