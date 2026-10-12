import { cloneDeep, set } from 'lodash-es';
import { derived, get, writable } from 'svelte/store';
import type { IAuthParams, ILoginResponse, ILoginSession, IResumeSessionResponse } from '@/types/auth';
import { generateSharedKey, calculateSessionProof, calculateToken } from '@/utils/encryptContent';
import { replace } from 'svelte-spa-router';
import { ROUTES_PATH } from '@/constant';
import { encryptData } from '../utils/encryptContent';
import sendForReply from '@/ws/sendForReply';
import { EWsMethods } from '@/types/websocket';
//import { updateNotificationField } from '@/lib/stores/debugLog';

interface IAuth {
    username: string;
    passwordRequired: boolean;
    connectionLimitReached: boolean;
    passwordInitRequired: boolean;
    authParams: IAuthParams | null;
    loggedIn: boolean;
    sessionReady: boolean;
    initialSetup: boolean;
    handoffPending: boolean;
}

const INIT_VALUE: IAuth = {
    username: 'admin',
    passwordRequired: false,
    connectionLimitReached: false,
    passwordInitRequired: false,
    authParams: null,
    loggedIn: false,
    sessionReady: false,
    initialSetup: false,
    handoffPending: false,
};

const LOGIN_SESSION_STORAGE_KEY = `open-ha-panel:login-session:${window.location.origin}`;
const PROVISIONING_HANDOFF_HASH_PREFIX = '#handoff=';

function captureProvisioningHandoffTicket() {
    if (!window.location.hash.startsWith(PROVISIONING_HANDOFF_HASH_PREFIX)) return null;
    const rawTicket = window.location.hash.slice(PROVISIONING_HANDOFF_HASH_PREFIX.length);
    window.history.replaceState(window.history.state, '', `${window.location.pathname}${window.location.search}#/`);
    try {
        const ticket = decodeURIComponent(rawTicket);
        return /^[0-9a-f]{64}$/.test(ticket) ? ticket : null;
    } catch {
        return null;
    }
}

let provisioningHandoffTicket = captureProvisioningHandoffTicket();

export function getLoginSession(): ILoginSession | null {
    try {
        const value = localStorage.getItem(LOGIN_SESSION_STORAGE_KEY);
        if (!value) return null;
        const session = JSON.parse(value) as Partial<ILoginSession>;
        if (typeof session.sessionId !== 'string' || typeof session.sessionToken !== 'string' || typeof session.expiresAt !== 'number') {
            clearLoginSession();
            return null;
        }
        return session as ILoginSession;
    } catch {
        clearLoginSession();
        return null;
    }
}

export function saveLoginSession(session: ILoginSession) {
    try {
        localStorage.setItem(LOGIN_SESSION_STORAGE_KEY, JSON.stringify(session));
    } catch (error) {
        console.error('Unable to persist the login session', error);
    }
}

export function clearLoginSession() {
    try {
        localStorage.removeItem(LOGIN_SESSION_STORAGE_KEY);
    } catch (error) {
        console.error('Unable to clear the login session', error);
    }
}

export const authStore = writable<IAuth>({
    ...cloneDeep(INIT_VALUE),
    // Keep protected routes visible during a silent session resume only when
    // this browser has a complete persisted session for the current origin.
    loggedIn: Boolean(getLoginSession()?.sessionToken),
    handoffPending: Boolean(provisioningHandoffTicket),
});

export const none = derived(authStore, (store) => store.authParams?.nonce);

export const passwordInitRequired = derived(authStore, (store) => store.passwordInitRequired);

export const sharedKey = derived([authStore], ([$authStore]) => {
    return generateSharedKey($authStore.authParams?.nonce || '');
});

export function updateAuthField(field: keyof IAuth, value: any) {
    authStore.update((item) => {
        const cloneItem = cloneDeep(item);
        set(cloneItem, field, value);
        return cloneItem;
    });
}

export async function setPassword(password: string) {
    const data = encryptData(JSON.stringify({ password }));
    const response = await sendForReply({ method: EWsMethods.SET_PASSWORD, data });
    if (response.error === 0) clearLoginSession();
    return response;
}

export async function setLoginState(password: string, redirectPath: string = ROUTES_PATH.ROOT) {
    const params = get(authStore).authParams;
    if (!params) return;
    const { response, cnonce } = calculateToken(password);
    const { realm, nonce, qop, algorithm } = params;
    const username = get(authStore).username;
    const setLoginRes = await sendForReply<
        { username: string; realm: string; nonce: string; cnonce: string; qop: string; response: string; algorithm: string },
        ILoginResponse
    >({
        method: EWsMethods.SET_LOGIN_STATE,
        data: { username, realm, nonce, cnonce, qop, response, algorithm },
    });
    if (setLoginRes.error === 0) {
        clearLoginSession();
        const session = setLoginRes.data;
        if (session?.authenticated && session.nonce) {
            updateAuthField('authParams', { ...params, nonce: session.nonce });
        } else {
            console.error('Login response is missing the rotated nonce', setLoginRes);
            return { ...setLoginRes, error: -1 };
        }
        if (session?.sessionIssued && session.sessionId && session.sessionToken && typeof session.expiresAt === 'number') {
            saveLoginSession({ sessionId: session.sessionId, sessionToken: session.sessionToken, expiresAt: session.expiresAt });
        }
        updateAuthField('loggedIn', true);
        updateAuthField('sessionReady', true);
        replace(redirectPath);
        //updateNotificationField('checked', false);
    } else {
        console.error('login error', setLoginRes);
    }
    return setLoginRes;
}

export async function resumeLoginSession(serverNonce: string) {
    const session = getLoginSession();
    if (!session) return null;
    const { cnonce, proof } = calculateSessionProof(session.sessionId, session.sessionToken, serverNonce);
    return sendForReply<{ sessionId: string; cnonce: string; proof: string }, IResumeSessionResponse>({
        method: EWsMethods.RESUME_SESSION,
        data: { sessionId: session.sessionId, cnonce, proof },
    });
}

export async function exchangeProvisioningHandoff() {
    const ticket = provisioningHandoffTicket;
    provisioningHandoffTicket = null;
    updateAuthField('handoffPending', false);
    if (!ticket) return null;
    return sendForReply<{ ticket: string }, ILoginResponse>({
        method: EWsMethods.EXCHANGE_PROVISIONING_HANDOFF,
        data: { ticket },
    });
}

export async function logout() {
    const response = await sendForReply<Record<string, never>>({ method: EWsMethods.LOGOUT, data: {} });
    if (response.error === 0) {
        clearLoginSession();
        updateAuthField('sessionReady', false);
        updateAuthField('loggedIn', false);
        await replace(ROUTES_PATH.LOGIN);
    }
    return response;
}
