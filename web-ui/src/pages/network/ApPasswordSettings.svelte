<script lang="ts">
    import { _ } from 'svelte-i18n';
    import { pop } from 'svelte-spa-router';
    import NetworkPageHeader from '@/component/PageHeader/PageHeader.svelte';
    import { networkStore, updateApPassword } from '@/stores/network';
    import Button from '@/component/Button/Button.svelte';
    import ApInUseConfirmModal from './ApInUseConfirmModal.svelte';
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';
    import { REG_EXP_PASSWORD_LETTER_AND_NUMBER } from '@/constant/regRex';
    let password = $state('');
    let confirmation = $state('');
    let showPassword = $state(false);
    let showConfirmation = $state(false);
    let loading = $state(false);
    let confirmVisible = $state(false);
    let passwordValid = $derived(password.length >= 8 && REG_EXP_PASSWORD_LETTER_AND_NUMBER.test(password));
    function togglePasswordVisibility(target: 'password' | 'confirmation', event?: KeyboardEvent) {
        if (event && event.key !== 'Enter' && event.key !== ' ') return;
        event?.preventDefault();
        if (target === 'password') showPassword = !showPassword;
        else showConfirmation = !showConfirmation;
    }
    async function requestSave() {
        if (!passwordValid || password !== confirmation || loading) return;
        if (($networkStore.ap?.clients.current ?? 0) > 0) {
            confirmVisible = true;
            return;
        }
        await save();
    }
    async function save() {
        if (!passwordValid || password !== confirmation || loading) return;
        loading = true;
        try {
            const response = await updateApPassword(password);
            if (response.error === 0) {
                confirmVisible = false;
                await pop();
            }
        } catch (error) {
            console.error('Failed to update AP password:', error);
        } finally {
            loading = false;
        }
    }
</script>

<div class="page-container network-page network-detail">
    <NetworkPageHeader title={$_('networkPage.editApPassword')} back="/network/ap" />
    <div class="network-form">
        <label for="ap-password">{$_('networkPage.newPassword')}</label>
        <div class="password-input">
            <input id="ap-password" class="form-control" type={showPassword ? 'text' : 'password'} bind:value={password} /><span
                class="input-icon-action"
                role="button"
                tabindex="0"
                onclick={() => togglePasswordVisibility('password')}
                onkeydown={(event) => togglePasswordVisibility('password', event)}
                aria-label={$_('networkPage.togglePassword')}><SvgIcon name={showPassword ? 'eyeInvisible.svg' : 'eye.svg'} /></span
            >
        </div>
        {#if password && !passwordValid}<small class="text-danger">{$_('networkPage.passwordLetterNumberRequired')}</small>{/if}
        <label for="ap-password-confirmation">{$_('networkPage.confirmPassword')}</label>
        <div class="password-input">
            <input id="ap-password-confirmation" class="form-control" type={showConfirmation ? 'text' : 'password'} bind:value={confirmation} /><span
                class="input-icon-action"
                role="button"
                tabindex="0"
                onclick={() => togglePasswordVisibility('confirmation')}
                onkeydown={(event) => togglePasswordVisibility('confirmation', event)}
                aria-label={$_('networkPage.togglePassword')}><SvgIcon name={showConfirmation ? 'eyeInvisible.svg' : 'eye.svg'} /></span
            >
        </div>
        {#if confirmation && password !== confirmation}<small class="text-danger">{$_('TIPS.PLE_INPUT_SAME_PASSWORD')}</small>{/if}
    </div>
    <Button
        type="primary"
        class="settings-save"
        style="width: min(70%, 560px)"
        disabled={!passwordValid || password !== confirmation}
        {loading}
        onclick={requestSave}>{$_('common.save')}</Button
    >
</div>

<ApInUseConfirmModal visible={confirmVisible} {loading} confirm={save} cancel={() => (confirmVisible = false)} />
