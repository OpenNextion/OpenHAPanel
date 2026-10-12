<script lang="ts">
    import Button from '@/component/Button/Button.svelte';
    import FormItem from '@/component/Form/FormItem.svelte';
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';
    import Input from '@/component/Input/Input.svelte';
    import { t } from 'svelte-i18n';
    import { onDestroy, onMount } from 'svelte';
    import { useShowTopTips, ETopTipsType } from '@/hooks/useShowTopTips.svelte';
    import { useLogin } from '@/hooks/useLogin.svelte';
    import { calculateUtf8ByteSize } from '@/utils/commonUtils';
    import { PASSWORD_MAX_BYTE_SIZE, PASSWORD_MAX_LENGTH, REG_EXP_PASSWORD_LETTER_AND_NUMBER } from '@/constant';
    import { EMessageType, sendMessage } from '@/stores/message';

    const { topTips, setTopTips } = useShowTopTips();
    const { toCancelForgetPassword, setPassword } = useLogin();

    let formState = $state({ password: '', _password: '' });
    let passwordError = $derived.by(() => {
        const { password } = formState;
        if (!password) return false;
        return password.length < 8 || !REG_EXP_PASSWORD_LETTER_AND_NUMBER.test(password) ? $t('LOGIN.REGISTER_PASSWORD_WRONG') : '';
    });
    let _passwordError = $derived.by(() => {
        const { password, _password } = formState;
        if (!_password) return false;
        return password !== _password ? $t('TIPS.PLE_INPUT_SAME_PASSWORD') : '';
    });
    let btnDisabled = $derived.by(() => {
        const { password, _password } = formState;
        return !password || !_password || !!passwordError || !!_passwordError;
    });

    let loading = $state(false);
    const handleResetPassword = async () => {
        if (formState.password.length < 8 || !REG_EXP_PASSWORD_LETTER_AND_NUMBER.test(formState.password)) return;
        if (calculateUtf8ByteSize(formState.password) > PASSWORD_MAX_BYTE_SIZE) {
            sendMessage(EMessageType.ERROR, $t('TIPS.PASSWORD_BYTE_SIZE_EXCEEDED', { values: { SIZE: PASSWORD_MAX_BYTE_SIZE } }));
            return;
        }
        try {
            loading = true;
            await setPassword(formState.password);
        } catch (error) {
            console.log(error);
        } finally {
            loading = false;
        }
    };

    onMount(() => {
        setTopTips({ type: ETopTipsType.RESET_PASSWORD });
    });
    onDestroy(() => {
        if (topTips.type === ETopTipsType.RESET_PASSWORD) setTopTips({ type: void 0 });
    });
</script>

<div class="w-100 reset">
    <form autocomplete="off" onsubmit={(e) => e.preventDefault()}>
        <FormItem label={$t('common.newPassword')} required>
            <Input
                bind:value={formState.password}
                size="lg"
                type="password"
                name="password"
                placeholder={$t('LOGIN.PASSWORD_PLACEHOLDER')}
                error={passwordError}
                autocomplete="new-password"
                maxlength={PASSWORD_MAX_LENGTH}
                trim={true}
            />
        </FormItem>
        <FormItem label={$t('common.confirmPassword')} required>
            <Input
                bind:value={formState._password}
                size="lg"
                error={_passwordError}
                placeholder={$t('LOGIN.PASSWORD_PLACEHOLDER_AGAIN')}
                type="password"
                maxlength={PASSWORD_MAX_LENGTH}
                name="password"
                autocomplete="new-password"
                trim={true}
            />
        </FormItem>
    </form>
    <div class="btns">
        <div class="mx-auto btn-wrapper">
            <Button disabled={btnDisabled} type="primary" onclick={handleResetPassword} {loading}>{$t('common.save')}</Button>
        </div>
        <div class="mx-auto text-center btn-wrapper return-btn">
            <button class="btn btn-text" onclick={toCancelForgetPassword}>{$t('common.return')}</button>
        </div>
    </div>
    <div class="d-flex justify-content-center align-items-center tips">
        <SvgIcon name="iDeclining.svg" />
        {$t('LOGIN.REGISTER_PASSWORD_TIPS')}
    </div>
</div>

<style lang="scss">
    .reset {
        :global(.sf-input::placeholder) {
            font-size: 0.875rem;
        }

        .btns {
            margin-top: 1.5rem;
            .btn-wrapper {
                width: 19.69rem;
                &.return-btn {
                    margin-top: 0.5rem;
                    button {
                        color: var(--body-text-color-blue);
                        border-color: transparent;
                    }
                }
            }
        }
        .tips {
            margin-top: 0.5rem;
            font-size: 0.75rem;
            line-height: 1.06rem;
            color: var(--body-text-color-grey);
            text-align: center;
            :global(.sf-icon) {
                position: relative;
                top: -1px;
                margin-right: 0.31rem;
            }
        }
    }
</style>
