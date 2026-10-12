import { EWsMethods } from '@/types/websocket';
import sendForReply from '@/ws/sendForReply';
import { derived } from 'svelte/store';
import { authStore, setLoginState, none, setPassword as setPasswordApi } from '@/stores/auth';
import { forgotPwdNotifyStore, forgotPwdStore, updatePwdField } from '@/stores/forgotPwd';
import { ERROR_CODE, PASSWORD_MAX_BYTE_SIZE, ROUTES_PATH } from '@/constant';
import { connectWithAp } from '@/stores/connectionStatus';
import { get, type Unsubscriber } from 'svelte/store';
import { replace, router } from 'svelte-spa-router';
import { initWebsocket, wsConnected } from '@/ws/initWebsocket';
import { tick } from 'svelte';
import { EMessageType, sendMessage } from '@/stores/message';
import { t } from 'svelte-i18n';
import { ETopTipsType, useShowTopTips } from '@/hooks/useShowTopTips.svelte';

export enum ELoginPageState {
    LOGIN,
    RESET_PASSWORD,
    CONNECT_LIMITED,
}

export enum EModalType {
    NULL,
    RESET_PASSWORD,
    OTHER_CLIENT_RESET_PASSWORD,
}

// unit: s
export const DEFAULT_FORGET_PASSWORD_COUNTDOWN = 10;
// unit: s
export const DEFAULT_RESET_PASSWORD_COUNTDOWN = 5 * 60;

// Clean up legacy storage if it exists from previous versions
if (typeof window !== 'undefined') {
    try {
        sessionStorage.removeItem('open-ha-panel:forgot-password-flow');
        localStorage.removeItem('open-ha-panel:forgot-password-flow');
    } catch {
        // ignore
    }
}

const loginPageState = $state<{
    pageState: ELoginPageState;
    modal: {
        visible: boolean;
        type: EModalType;
    };
    forgetPassword: {
        resetCountdown: number;
        resetInterval: ReturnType<typeof setInterval> | null;
        countdown: number;
        interval: ReturnType<typeof setInterval> | null;
    };
}>({
    pageState: ELoginPageState.LOGIN,
    modal: {
        visible: false,
        type: EModalType.NULL,
    },
    forgetPassword: {
        resetCountdown: DEFAULT_RESET_PASSWORD_COUNTDOWN,
        resetInterval: null,
        countdown: DEFAULT_FORGET_PASSWORD_COUNTDOWN,
        interval: null,
    },
});

const loginPageStateReadonly = $derived.by(() => loginPageState);

const setPageState = (pageState?: ELoginPageState) => {
    loginPageState.pageState = pageState ?? ELoginPageState.LOGIN;
};

const setModal = (visible?: boolean, modalType = EModalType.NULL) => {
    loginPageState.modal.type = modalType;
    tick().then(() => {
        loginPageState.modal.visible = !!visible;
    });
};

const getRemainingSeconds = (expiresAt: number) => Math.max(0, Math.ceil((expiresAt - Date.now()) / 1000));

const startForgetPasswordCountdown = (expiresAt = Date.now() + DEFAULT_FORGET_PASSWORD_COUNTDOWN * 1000) => {
    clearForgetPasswordInterval();
    const updateCountdown = () => {
        loginPageState.forgetPassword.countdown = getRemainingSeconds(expiresAt);
        if (loginPageState.forgetPassword.countdown === 0) clearForgetPasswordInterval();
    };
    updateCountdown();
    if (loginPageState.forgetPassword.countdown === 0) return;
    loginPageState.forgetPassword.interval = setInterval(() => {
        updateCountdown();
    }, 1000);
};
const clearForgetPasswordInterval = () => {
    const {
        forgetPassword: { interval },
    } = loginPageState;
    interval && clearInterval(interval);
    loginPageState.forgetPassword.interval = null;
};

const startResetPasswordCountdown = (expiresAt = Date.now() + DEFAULT_RESET_PASSWORD_COUNTDOWN * 1000) => {
    clearResetPasswordInterval();
    const updateCountdown = () => {
        loginPageState.forgetPassword.resetCountdown = getRemainingSeconds(expiresAt);
        if (loginPageState.forgetPassword.resetCountdown === 0) {
            clearResetPasswordInterval();
            setPageState(ELoginPageState.LOGIN);
        }
    };
    updateCountdown();
    if (loginPageState.forgetPassword.resetCountdown === 0) return;
    loginPageState.forgetPassword.resetInterval = setInterval(() => {
        updateCountdown();
    }, 1000);
};
const clearResetPasswordInterval = () => {
    const {
        forgetPassword: { resetInterval },
    } = loginPageState;
    resetInterval && clearInterval(resetInterval);
    loginPageState.forgetPassword.resetInterval = null;
};

export const completeForgotPasswordFlow = () => {
    clearForgetPasswordInterval();
    clearResetPasswordInterval();
    loginPageState.forgetPassword.countdown = DEFAULT_FORGET_PASSWORD_COUNTDOWN;
    loginPageState.forgetPassword.resetCountdown = DEFAULT_RESET_PASSWORD_COUNTDOWN;
    setModal(false);
    setPageState(ELoginPageState.LOGIN);

    const { topTips, setTopTips } = useShowTopTips();
    if (topTips.type === ETopTipsType.RESET_PASSWORD) setTopTips({ type: undefined });
    if (router.location !== ROUTES_PATH.LOGIN) void replace(ROUTES_PATH.LOGIN);
};

const toForgetPassword = async () => {
    if (!wsConnected()) {
        const ws = await initWebsocket();
        if (ws?.error !== 0) return;
    }
    const { error } = await sendForReply({
        method: EWsMethods.SET_FORGOT_PASSWORD_STATE,
        data: { state: 'waiting', duration: 300 },
    });
    if (error === ERROR_CODE.SUCCESS) {
        const expiresAt = Date.now() + DEFAULT_FORGET_PASSWORD_COUNTDOWN * 1000;
        startForgetPasswordCountdown(expiresAt);
        updatePwdField('state', 'waiting');
        setModal(true, EModalType.RESET_PASSWORD);
    } else if (error === ERROR_CODE.CHANGE_PASSWORD_IN_PROGRESS) {
        setModal(true, EModalType.OTHER_CLIENT_RESET_PASSWORD);
    }
};

const toCancelForgetPassword = async () => {
    const { error } = await sendForReply({
        method: EWsMethods.SET_FORGOT_PASSWORD_STATE,
        data: { state: 'done' },
    });
    if (error === 0) {
        updatePwdField('state', 'done');
        completeForgotPasswordFlow();
    } else {
        console.error('cancel forget password error: ', error);
    }
};

const setPassword = async (password: string) => {
    let autoLoginUnsubscriber: Unsubscriber | null = null;
    const clearAutoLoginSubscriber = () => {
        autoLoginUnsubscriber?.();
        autoLoginUnsubscriber = null;
    };
    const handleLocationChange = () => {
        // svelte-spa-router 5.x exposes the current route through `router`
        // instead of the removed subscribable `location` store.
        queueMicrotask(() => {
            if (router.location === ROUTES_PATH.LOGIN) return;
            clearAutoLoginSubscriber();
            window.removeEventListener('hashchange', handleLocationChange);
        });
    };
    window.addEventListener('hashchange', handleLocationChange);
    handleLocationChange();

    try {
        const { error } = await setPasswordApi(password);
        if (error === 0) {
            updatePwdField('state', 'done');
            const isAp = get(connectWithAp);
            if (isAp) {
                const ws = await initWebsocket();
                if (ws?.error === 0) replace(ROUTES_PATH.ROOT);
            } else {
                // Automatically log in after successfully resetting your password
                clearAutoLoginSubscriber();
                let prevNone = get(none);
                autoLoginUnsubscriber = none.subscribe((latestNone) => {
                    if (!latestNone || latestNone === prevNone) return;
                    prevNone = latestNone;
                    clearAutoLoginSubscriber();
                    setLoginState(password);
                });
                const ws = await initWebsocket();
                if (ws?.error !== 0) clearAutoLoginSubscriber();
            }
            setPageState(ELoginPageState.LOGIN);
        } else if (error === ERROR_CODE.INPUT_BYTE_SIZE_EXCEEDED) {
            sendMessage(EMessageType.ERROR, get(t)('TIPS.PASSWORD_BYTE_SIZE_EXCEEDED', { values: { SIZE: PASSWORD_MAX_BYTE_SIZE } }));
        }
    } catch (error) {
        console.error(error);
    }
};

authStore.subscribe((store) => {
    const { connectionLimitReached } = store || {};
    if (connectionLimitReached) setPageState(ELoginPageState.CONNECT_LIMITED);
});

const forgotPwdStatus = derived(forgotPwdStore, (store) => store.state);
let prevState = get(forgotPwdStatus);
forgotPwdStatus.subscribe((state) => {
    switch (state) {
        case 'ready': {
            setModal(false);
            clearForgetPasswordInterval();
            const expiresAt = Date.now() + DEFAULT_RESET_PASSWORD_COUNTDOWN * 1000;
            startResetPasswordCountdown(expiresAt);
            setPageState(ELoginPageState.RESET_PASSWORD);
            break;
        }
        case 'done': {
            if (prevState === 'ready') {
                completeForgotPasswordFlow();
            }
            break;
        }
    }
    prevState = state;
});

forgotPwdNotifyStore.subscribe((notify) => {
    if (notify?.state === 'done') completeForgotPasswordFlow();
});

export function useLogin() {
    return {
        loginPageState: loginPageStateReadonly,
        setPageState,
        setModal,
        startForgetPasswordCountdown,
        clearForgetPasswordInterval,
        startResetPasswordCountdown,
        clearResetPasswordInterval,
        toForgetPassword,
        toCancelForgetPassword,
        setPassword,
    };
}
