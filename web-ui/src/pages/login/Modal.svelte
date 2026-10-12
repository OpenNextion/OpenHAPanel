<script lang="ts">
    import Modal from '@/component/Modal/Modal.svelte';
    import { useLogin, DEFAULT_FORGET_PASSWORD_COUNTDOWN, EModalType } from '@/hooks/useLogin.svelte';
    import { t } from 'svelte-i18n';
    import Button from '@/component/Button/Button.svelte';
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';
    import { wsConnected } from '@/ws/initWebsocket';

    const { loginPageState, setModal, toForgetPassword, toCancelForgetPassword } = useLogin();
    let countdown = $derived.by(() => loginPageState.forgetPassword.countdown);
    let modalVisible = $derived.by(() => loginPageState.modal.visible);
    let modalType = $derived.by(() => loginPageState.modal.type);
    let countdownText = $derived.by(() => {
        switch (countdown) {
            case 0:
                return $t('FIRMWARE_MANAGE.RETRY');
            default:
                return `${countdown}s`;
        }
    });

    const handleCancel = async () => {
        if (wsConnected()) {
            await toCancelForgetPassword();
        }
        setModal(false);
    };

    const confirm = () => {
        if (countdown === 0) {
            toForgetPassword();
        }
    };
</script>

<Modal {modalVisible}>
    {#snippet customSnippet()}
        <div class="modal-content">
            {#if modalType === EModalType.RESET_PASSWORD}
                <h5>{$t('common.tips')}</h5>
                <p>{$t('LOGIN.DOUBLE_CLICK_BTN', { values: { NUMBER: DEFAULT_FORGET_PASSWORD_COUNTDOWN } })}</p>
                <div class="btns">
                    <div class="btn-wrapper">
                        <Button iconUrl={countdown === 0 ? '' : 'loading.svg'} type="primary" onclick={confirm}>{countdownText}</Button>
                    </div>
                    <div class="btn-wrapper">
                        <Button onclick={handleCancel}>{$t('common.cancel')}</Button>
                    </div>
                </div>
            {:else if modalType === EModalType.OTHER_CLIENT_RESET_PASSWORD}
                <div class="d-flex justify-content-center icon-wrapper">
                    <SvgIcon name="iWarn.svg" />
                </div>
                <p>{$t('TIPS.OTHER_CLIENT_RESET_PASSWORD')}</p>
                <div class="btns" style="padding-top: 1.5rem">
                    <div class="btn-wrapper">
                        <Button type="primary" onclick={() => setModal(false)}>{$t('common.confirm')}</Button>
                    </div>
                </div>
            {/if}
        </div>
    {/snippet}
</Modal>

<style lang="scss">
    .modal-content {
        background: var(--bs-body-bg);
        border-radius: 0.75rem;
        padding: 1.5rem;
        text-align: center;
        min-width: 26rem;
        @media (max-width: 768px) {
            min-width: 21rem;
        }
        @media (max-width: 375px) {
            min-width: auto;
            width: 82.93vw;
        }
        h5 {
            line-height: 1.63rem;
            font-size: 1.13rem;
            color: var(--body-text-color-highlight);
            margin-bottom: 0.5rem;
            font-weight: bolder;
        }
        p {
            line-height: 1.5rem;
            font-size: 1rem;
            margin-bottom: 1.5rem;
            color: var(--body-text-color-highlight);
        }
        .icon-wrapper {
            font-size: 3.75rem;
            color: #f1524b;
            margin-bottom: 1.5rem;
        }
        .btns {
            width: 75%;
            margin: 0 auto;
            @media (max-width: 768px) {
                width: 100%;
            }
            .btn-wrapper {
                margin-bottom: 0.75rem;
                &:last-of-type {
                    margin-bottom: 0;
                }
                :global(.sf-icon) {
                    font-size: 2rem;
                    animation: global_keyframes_rotate 1.5s infinite linear;
                }
            }
        }
    }
</style>
