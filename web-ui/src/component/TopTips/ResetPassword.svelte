<script lang="ts">
    import SvgIcon from '../Icon/SvgIcon.svelte';
    import { useLogin, DEFAULT_RESET_PASSWORD_COUNTDOWN } from '@/hooks/useLogin.svelte';
    import { t } from 'svelte-i18n';

    const { loginPageState } = useLogin();
    const leftTime = $derived.by(() => {
        return {
            MINUTE: DEFAULT_RESET_PASSWORD_COUNTDOWN / 60,
            SECOND: loginPageState.forgetPassword.resetCountdown
        }
    });
</script>

<div class="d-flex justify-content-start align-items-center top-tips">
    <SvgIcon name="bell.svg" />
    { $t('LOGIN.PLE_RESET_PASSWORD_IN_TIME', { values: leftTime }) }
</div>

<style lang="scss">
$color: #EF961D;
.top-tips {
    background: rgba($color, 0.2);
    color: $color;
    padding: 0 1rem;
}
</style>
