<script lang="ts">
    import { _ } from 'svelte-i18n';
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';
    import { deviceStore, updateTheme } from '@/stores/device';
    import { languageStore, setLanguage } from '@/stores/language';
    import { currentLanDevice } from '@/stores/lanDevice';
    import EThemeMode from '@/types/EThemeMode';
    import ELanguage from '@/types/language';
    import { GUEST_ABOUT, GUEST_FAQ } from '@/constant';
    import DropDown from '@/component/Dropdown/DropDown.svelte';

    interface Props {
        open?: boolean;
        onClose?: () => void;
    }

    let { open = false, onClose = () => undefined }: Props = $props();
    let changingTheme = $state(false);
    let changingLanguage = $state(false);
    let selectedLanguage = $state<ELanguage>($languageStore.language);
    const themeOptions = $derived([
        { label: 'settingsDetail.lightMode', icon: 'sun.svg', mode: EThemeMode.NORMAL, followSystem: false },
        { label: 'settingsDetail.darkMode', icon: 'night.svg', mode: EThemeMode.DARK, followSystem: false },
        { label: 'settingsDetail.followSystem', icon: 'sunNight.svg', mode: $deviceStore.currentMode, followSystem: true },
    ]);
    const languageOptions = [
        { label: '简体中文', value: ELanguage.ZH_CN },
        { label: 'English', value: ELanguage.EN_US },
    ];
    const selectedLanguageLabel = $derived(languageOptions.find((item) => item.value === selectedLanguage)?.label ?? selectedLanguage);
    const nextionLogo = $derived(
        $deviceStore.currentMode === EThemeMode.DARK ? 'nextionLogoMobileDark.svg' : 'nextionLogoMobileNormal.svg',
    );

    $effect(() => {
        if (changingLanguage) return;
        selectedLanguage = $languageStore.language;
    });

    function isThemeSelected(mode: EThemeMode, followSystem: boolean) {
        return followSystem ? $deviceStore.followSystem : !$deviceStore.followSystem && $deviceStore.currentMode === mode;
    }

    async function selectTheme(mode: EThemeMode, followSystem: boolean) {
        if (changingTheme || isThemeSelected(mode, followSystem)) return;
        changingTheme = true;
        try {
            await updateTheme({ currentMode: mode, followSystem });
        } finally {
            changingTheme = false;
        }
    }

    function handleThemeKeydown(event: KeyboardEvent, mode: EThemeMode, followSystem: boolean) {
        if (event.key !== 'Enter' && event.key !== ' ') return;
        event.preventDefault();
        void selectTheme(mode, followSystem);
    }

    async function changeLanguage(nextLanguage: ELanguage, previousLanguage: ELanguage = selectedLanguage) {
        if (changingLanguage) return;
        selectedLanguage = nextLanguage;
        changingLanguage = true;
        try {
            const { wsResponse } = await setLanguage(nextLanguage);
            if (wsResponse.error === 0) {
                return;
            }
            selectedLanguage = previousLanguage;
        } catch (error) {
            console.error('Failed to update language:', error);
            selectedLanguage = previousLanguage;
        } finally {
            changingLanguage = false;
        }
    }
</script>

<aside class:open class="guest-sidebar" aria-hidden={!open}>
    <button class="guest-sidebar-close" type="button" aria-label={$_('common.closeMenu')} onclick={onClose}><SvgIcon name="mdi:close" /></button>
    <div class="guest-device">
        <span class="guest-device-logo guest-device-logo-desktop"><SvgIcon name="nextionLogo.svg" /></span>
        <span class="guest-device-logo guest-device-logo-mobile"><SvgIcon name={nextionLogo} /></span>
        <div><strong>{$currentLanDevice?.name || '--'}</strong><small>{$currentLanDevice?.ip || '--'}</small></div>
    </div>

    <section class="guest-sidebar-section">
        <h2>{$_('page.settings.darkMode')}</h2>
        <div class="guest-theme-options">
            {#each themeOptions as item (item.label)}
                <div
                    class="theme-option"
                    class:selected={isThemeSelected(item.mode, item.followSystem)}
                    role="button"
                    tabindex={changingTheme ? -1 : 0}
                    aria-pressed={isThemeSelected(item.mode, item.followSystem)}
                    aria-disabled={changingTheme}
                    onclick={() => void selectTheme(item.mode, item.followSystem)}
                    onkeydown={(event) => handleThemeKeydown(event, item.mode, item.followSystem)}
                >
                    <span class="theme-preview"><SvgIcon name={item.icon} /></span>
                    <small>{$_(item.label)}</small>
                </div>
            {/each}
        </div>
    </section>

    <nav class="guest-sidebar-links" aria-label={$_('nav.main')}>
        <!--<label class="guest-language-select">
            <span>{$_('common.language')}</span>
            <DropDown bind:value={selectedLanguage} options={languageOptions} disabled={changingLanguage} onchange={changeLanguage} ariaLabel={$_('common.language')}>
                <span>{selectedLanguageLabel}</span>
            </DropDown>
        </label>-->
        <a href={GUEST_FAQ} target="_blank" rel="noreferrer"><span>{$_('guestSidebar.faq')}</span><SvgIcon name="blank.svg" /></a>
        <a href={GUEST_ABOUT} target="_blank" rel="noreferrer"><span>{$_('guestSidebar.about')}</span><SvgIcon name="blank.svg" /></a>
    </nav>
</aside>

{#if open}<button class="guest-sidebar-backdrop" type="button" aria-label={$_('common.closeMenu')} onclick={onClose}></button>{/if}

<style lang="scss">
    .guest-sidebar {
        position: fixed;
        inset: 0 auto 0 0;
        z-index: 1050;
        width: var(--sidebar-width);
        padding: 4.25rem 1.25rem 1.5rem;
        overflow-y: auto;
        color: var(--bs-body-color);
        background: var(--sidebar-background);
        box-shadow: 0.4rem 0 1.5rem rgba(0, 0, 0, 0.12);
        transform: translateX(-100%);
        transition: transform 0.25s ease;
    }
    .guest-sidebar.open { transform: translateX(0); }
    .guest-sidebar-close {
        position: absolute;
        top: 1.15rem;
        right: 1rem;
        display: grid;
        width: 2rem;
        height: 2rem;
        padding: 0;
        place-items: center;
        color: inherit;
        border: 0;
        background: transparent;
        font-size: 1.35rem;
    }
    .guest-device { display: flex; align-items: center; gap: 0.75rem; }
    .guest-device-logo { display: grid; width: 2.5rem; height: 2.5rem; flex: 0 0 auto; place-items: center; }
    .guest-device-logo-mobile { display: none; }
    .guest-device-logo :global(.sf-icon), .guest-device-logo :global(svg) { width: 100%; height: 100%; }
    .guest-device > div { display: flex; min-width: 0; flex-direction: column; }
    .guest-device strong { overflow: hidden; font-size: 0.85rem; text-overflow: ellipsis; white-space: nowrap; }
    .guest-device small { color: var(--body-text-color-grey); font-size: 0.7rem; }
    .guest-sidebar-section { margin-top: 2rem; }
    .guest-sidebar-section h2 { margin: 0 0 0.75rem; color: var(--body-text-color-grey); font-size: 0.85rem; font-weight: 500; }
    .guest-theme-options { display: grid; grid-template-columns: repeat(3, minmax(0, 1fr)); gap: 0.5rem; }
    .theme-option { position: relative; min-width: 0; color: inherit; cursor: pointer; }
    .theme-option[aria-disabled='true'] { cursor: default; opacity: 0.7; pointer-events: none; }
    .theme-preview { display: grid; width: 100%; aspect-ratio: 1.25; place-items: center; overflow: hidden; border: 2px solid transparent; border-radius: 0.65rem; background: var(--body-inner-level-2); font-size: 3.8rem; }
    .theme-preview :global(.sf-icon), .theme-preview :global(svg) { width: 100%; height: 100%; }
    .theme-option.selected .theme-preview { border-color: #0199df; }
    .theme-option.selected::after { position: absolute; top: 0.2rem; right: 0.25rem; content: '✓'; color: #0199df; font-size: 0.75rem; font-weight: 700; }
    .guest-theme-options small { display: block; margin-top: 0.4rem; overflow: hidden; font-size: 0.68rem; text-overflow: ellipsis; white-space: nowrap; }
    .guest-sidebar-links { display: grid; margin-top: 2rem; }
    .guest-sidebar-links a, .guest-sidebar-links label { display: flex; min-height: 48px; padding: 0; align-items: center; justify-content: space-between; color: var(--body-text-color-grey); border-bottom: 1px solid color-mix(in srgb, var(--bs-border-color) 55%, transparent); font-size: 0.85rem; text-decoration: none; }
    .guest-sidebar-links .guest-language-select { min-height: 0; padding-bottom: 0.9rem; align-items: stretch; flex-direction: column; gap: 0.75rem; }
    .guest-language-select :global(.dropdown) { width: 100%; min-height: 38px; color: var(--bs-body-color); border: 1px solid var(--bs-border-color); border-radius: 0.35rem; background: var(--body-inner-level-2); }
    .guest-language-select :global(.dropdown-toggle) { min-height: 38px; padding: 0.35rem 0.65rem; }
    .guest-language-select :global(.dropdown:focus-within) { border-color: #0199df; box-shadow: 0 0 0 1px #0199df; }
    .guest-sidebar-backdrop { position: fixed; inset: 0; z-index: 1045; border: 0; background: rgba(0, 0, 0, 0.35); }
    @media (min-width: 992px) { .guest-sidebar-backdrop { display: none; } }
    @media (max-width: 600px) {
        .guest-sidebar {
            width: 100vw;
            height: 100dvh;
            box-shadow: none;
        }
        .guest-device-logo-desktop { display: none; }
        .guest-device-logo-mobile { display: grid; width: 5.875rem; height: 1.875rem; }
    }
</style>
