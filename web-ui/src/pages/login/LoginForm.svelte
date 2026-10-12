<script lang="ts">
    import Button from '@/component/Button/Button.svelte';
    import FormItem from '@/component/Form/FormItem.svelte';
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';
    import Input from '@/component/Input/Input.svelte';
    import { t } from 'svelte-i18n';
    import { authStore, none, passwordInitRequired as _passwordInitRequired, setLoginState, setPassword, updateAuthField } from '@/stores/auth';
    import { type Unsubscriber } from 'svelte/store';
    import { onDestroy } from 'svelte';
    import { on } from 'svelte/events';
    import { debounce } from 'lodash-es';
    import { initWebsocket, wsConnected } from '@/ws/initWebsocket';
    import { useLogin } from '@/hooks/useLogin.svelte';
    import { wsReconnectManager } from '@/ws/wsReconnect';
    import Modal from '@/component/Modal/Modal.svelte';
    import { connectWithAp } from '@/stores/connectionStatus';
    //import { useProgress } from '@/hook/useProgress.svelte';
    import { calculateUtf8ByteSize, sleep } from '@/utils/commonUtils';
    import { SEND_MSG_TIMEOUT } from '@/ws';
    import { EMessageType, sendMessage } from '@/stores/message';
    import { ERROR_CODE, PASSWORD_MAX_BYTE_SIZE, PASSWORD_MAX_LENGTH, REG_EXP_PASSWORD_LETTER_AND_NUMBER } from '@/constant';
    import { ROUTES_PATH } from '@/constant';
    import { replace } from 'svelte-spa-router';

    //const { initProgressModal } = useProgress();
    const { toForgetPassword } = useLogin();
    const REMEMBERED_PASSWORD_STORAGE_KEY = 'open-ha-panel:remembered-password';

    function getRememberedPassword() {
        try {
            return localStorage.getItem(REMEMBERED_PASSWORD_STORAGE_KEY) ?? '';
        } catch {
            return '';
        }
    }

    function clearRememberedPassword() {
        try {
            localStorage.removeItem(REMEMBERED_PASSWORD_STORAGE_KEY);
        } catch (error) {
            console.error('Unable to clear remembered password', error);
        }
    }

    const rememberedPassword = getRememberedPassword();
    let loading = $state(false);
    let password = $state(rememberedPassword);
    let rememberPassword = $state(Boolean(rememberedPassword));
    let passwordCache = '';
    let passwordEmptyError = $state(false);
    let passwordErrorKey = $state('');
    let passwordError = $derived(passwordEmptyError || (passwordErrorKey ? $t(passwordErrorKey) : ''));
    let passwordAgain = $state('');
    let passwordAgainErrorKey = $state('');
    let passwordAgainError = $derived(passwordAgainErrorKey ? $t(passwordAgainErrorKey) : '');
    let passwordInitRequired = $state(false);
    let disabled = $derived.by(() => {
        const passwordDisabled = !password || !!passwordError;
        const passwordAgainDisabled = !passwordAgain || !!passwordAgainError;
        return passwordInitRequired ? passwordDisabled || passwordAgainDisabled : passwordDisabled;
    });
    let modalVisible = $state(false);

    const validatePassword = () => {
        if (!password) {
            passwordEmptyError = true;
            passwordErrorKey = '';
        } else if (passwordInitRequired && (password.length < 8 || !REG_EXP_PASSWORD_LETTER_AND_NUMBER.test(password))) {
            passwordEmptyError = false;
            passwordErrorKey = 'LOGIN.REGISTER_PASSWORD_WRONG';
        } else {
            passwordEmptyError = false;
            passwordErrorKey = '';
        }
        passwordInitRequired && validatePasswordAgain();
    };
    const validatePasswordAgain = () => {
        if (passwordAgain && passwordAgain !== password) {
            passwordAgainErrorKey = 'TIPS.PLE_INPUT_SAME_PASSWORD';
        } else {
            passwordAgainErrorKey = '';
        }
    };

    async function toLoginOrRegister() {
        if (loading || disabled) return;
        if (calculateUtf8ByteSize(password) > PASSWORD_MAX_BYTE_SIZE) {
            sendMessage(EMessageType.ERROR, $t('TIPS.PASSWORD_BYTE_SIZE_EXCEEDED', { values: { SIZE: PASSWORD_MAX_BYTE_SIZE } }));
            return;
        }
        loading = true;
        passwordCache = password;
        //initProgressModal();
        if (wsReconnectManager.status) {
            wsReconnectManager.close();
        }
        if (wsConnected() && $none) {
            passwordInitRequired ? toRegister() : toLogin();
        } else {
            let prevNone = $none;
            const unSubscribe = none.subscribe((latestNone) => {
                if (!latestNone || prevNone === latestNone) return;
                prevNone = latestNone;
                passwordInitRequired ? toRegister() : toLogin();
                unSubscribe();
            });
            const initResp = await initWebsocket();
            if (initResp?.error === 0) {
                wsReconnectManager.close();
            } else {
                loading = false;
                unSubscribe();
            }
        }
    }
    async function toRegister() {
        if (passwordCache.length < 8 || !REG_EXP_PASSWORD_LETTER_AND_NUMBER.test(passwordCache)) {
            passwordEmptyError = false;
            passwordErrorKey = 'LOGIN.REGISTER_PASSWORD_WRONG';
            loading = false;
            return;
        }
        let unSubscribe: Unsubscriber | null = null;
        const clearSubscribe = () => {
            if (unSubscribe) {
                unSubscribe();
                unSubscribe = null;
            }
        };
        const afterRegister = async (redirectPath: string = ROUTES_PATH.ROOT, loginAfterReconnect = true) => {
            clearSubscribe();
            if (loginAfterReconnect) {
                let prevNone = $none;
                unSubscribe = none.subscribe((latestNone) => {
                    if (!latestNone || latestNone === prevNone) return;
                    prevNone = latestNone;
                    clearSubscribe();
                    toLogin(redirectPath);
                });
            }
            // ws will disconnect after set password, ws reconnects and waits for the latest none to log in automatically
            const initResp = await initWebsocket();
            if (initResp?.error !== 0) {
                loading = false;
                wsReconnectManager.start();
            }
        };
        try {
            console.log('%c setPwd start', 'background: green');
            const loadingTimeout = setTimeout(() => (loading = false), SEND_MSG_TIMEOUT);
            const setPwdRes = await setPassword(passwordCache);
            console.log(`%c setPwdRes: ${JSON.stringify(setPwdRes)}`, 'background: green');
            if (setPwdRes.error === 0) {
                const redirectPath = $connectWithAp ? ROUTES_PATH.WIFI_SETUP : ROUTES_PATH.ROOT;
                if ($connectWithAp) {
                    updateAuthField('initialSetup', true);
                    updateAuthField('sessionReady', false);
                    await replace(ROUTES_PATH.WIFI_SETUP);
                }
                await sleep(500);
                clearTimeout(loadingTimeout);
                afterRegister(redirectPath, !$connectWithAp);
            } else {
                if (setPwdRes.error === ERROR_CODE.INPUT_BYTE_SIZE_EXCEEDED) {
                    sendMessage(EMessageType.ERROR, $t('TIPS.PASSWORD_BYTE_SIZE_EXCEEDED', { values: { SIZE: PASSWORD_MAX_BYTE_SIZE } }));
                }
                loading = false;
            }
        } catch (error) {
            console.error('toRegister error:', error);
            if ($connectWithAp) {
                updateAuthField('initialSetup', true);
                updateAuthField('sessionReady', false);
                await replace(ROUTES_PATH.WIFI_SETUP);
            }
            afterRegister($connectWithAp ? ROUTES_PATH.WIFI_SETUP : ROUTES_PATH.ROOT, !$connectWithAp);
        }
    }
    async function toLogin(redirectPath: string = ROUTES_PATH.ROOT) {
        loading = true;
        const loadingTimeout = setTimeout(() => (loading = false), SEND_MSG_TIMEOUT);
        const setLoginRes = await setLoginState(passwordCache, redirectPath);
        if (setLoginRes?.error === 0) {
            try {
                if (rememberPassword) {
                    localStorage.setItem(REMEMBERED_PASSWORD_STORAGE_KEY, passwordCache);
                } else {
                    clearRememberedPassword();
                }
            } catch (error) {
                console.error('Unable to remember password', error);
            }
        } else {
            clearTimeout(loadingTimeout);
            console.error('login error', setLoginRes);
            passwordEmptyError = false;
            passwordErrorKey = 'TIPS.PASSWORD_ERROR';
        }
        loading = false;
    }

    const cancelKeydown = on(
        window.document,
        'keydown',
        debounce((e: KeyboardEvent) => {
            if (e.key === 'Enter' || e.keyCode === 13) toLoginOrRegister();
            e.stopPropagation();
        }, 250),
    );

    const unSubscriber = _passwordInitRequired.subscribe((value) => {
        if (value) {
            passwordInitRequired = value;
            password = '';
            passwordEmptyError = false;
            passwordErrorKey = '';
            passwordAgainErrorKey = '';
            rememberPassword = false;
            clearRememberedPassword();
        }
    });

    onDestroy(() => {
        unSubscriber();
        cancelKeydown();
    });
</script>

<div class="w-100 login-form">
    <form autocomplete="off" onsubmit={(e) => e.preventDefault()}>
        <label for="username">
            <input type="text" id="username" name="username" value={$authStore.username} hidden />
        </label>
        <FormItem label={$t('LOGIN.LOGIN_PASSWORD')} required style={`margin-bottom: ${passwordInitRequired ? '2rem' : '1rem'}`}>
            <Input
                bind:value={password}
                size="lg"
                type="password"
                oninput={validatePassword}
                name="password"
                placeholder={$t('LOGIN.PASSWORD_PLACEHOLDER')}
                error={passwordError}
                autocomplete={passwordInitRequired ? 'new-password' : 'current-password'}
                maxlength={PASSWORD_MAX_LENGTH}
                trim={true}
            />
        </FormItem>
        {#if passwordInitRequired}
            <FormItem label={$t('common.confirmPassword')} required>
                <Input
                    bind:value={passwordAgain}
                    size="lg"
                    type="password"
                    oninput={validatePasswordAgain}
                    name="password"
                    placeholder={$t('LOGIN.PASSWORD_PLACEHOLDER_AGAIN')}
                    error={passwordAgainError}
                    autocomplete="new-password"
                    maxlength={PASSWORD_MAX_LENGTH}
                    trim={true}
                />
            </FormItem>
        {/if}
    </form>
    <div class="reset-password">
        {#if !passwordInitRequired}
            <label class="remember-password">
                <input
                    class="form-check-input"
                    type="checkbox"
                    bind:checked={rememberPassword}
                    onchange={() => {
                        if (!rememberPassword) clearRememberedPassword();
                    }}
                />
                <span>{$t('LOGIN.REMEMBER_PASSWORD')}</span>
            </label>
            <button class="btn btn-link text-decoration-none p-0" onclick={toForgetPassword}>{$t('LOGIN.FORGET_PASSWORD')}</button>
        {/if}
    </div>
    <div class="btns">
        <div class="btn-wrapper">
            <Button type="primary" onclick={toLoginOrRegister} {loading} {disabled}>{$t('common.login')}</Button>
            {#if passwordInitRequired}
                <div class="tips">
                    <SvgIcon name="iDeclining.svg" />
                    <p>{$t('LOGIN.REGISTER_PASSWORD_TIPS')}</p>
                </div>
            {/if}
        </div>
    </div>
</div>

<Modal {modalVisible} footer={true} confirm={() => (modalVisible = false)} confirmTitle={$t('common.confirm')} title={$t('LOGIN.SAFETY')} />

<style lang="scss">
    .btn-wrapper {
        max-width: 19.69rem;
        margin: 0 auto;
    }

    .login-form {
        :global {
            .sf-input::placeholder {
                font-size: 0.875rem;
            }
            .form-item {
                .form-item-footer {
                    display: none;
                }
            }
        }
        .btns {
            margin-top: 1.5rem;
            .tips {
                position: relative;
                color: var(--body-text-color-grey);
                padding-left: 1.06rem;
                margin-top: 0.75rem;
                :global(.sf-icon) {
                    position: absolute;
                    top: 0;
                    left: 0;
                }
                p {
                    width: 100%;
                    margin-bottom: 0.2rem;
                    font-size: 0.75rem;
                    line-height: 1.2rem;
                    color: var(--body-text-color-grey);
                }
            }
        }
        .reset-password {
            display: flex;
            width: 100%;
            min-height: 1.5rem;
            padding-top: 0.75rem;
            align-items: center;
            justify-content: space-between;
            gap: 1rem;
            .remember-password {
                display: inline-flex;
                min-width: 0;
                margin: 0;
                align-items: center;
                gap: 0.4rem;
                color: var(--bs-body-color);
                font-size: 0.75rem;
                white-space: nowrap;
                cursor: pointer;
                //span {
                //
                //}
                .form-check-input {
                    flex: 0 0 auto;
                    width: 1rem;
                    height: 1rem;
                    margin: 0;
                    border-radius: 50%;
                    cursor: pointer;
                }
            }
            .btn-link {
                color: var(--primary-button-bg);
            }
            button {
                color: var(--body-text-color-blue);
                font-size: 0.75rem;
                white-space: nowrap;
            }
        }
    }
</style>
