import { ROUTES_PATH } from '@/constant';
import {
    authStore,
    clearLoginSession,
    exchangeProvisioningHandoff,
    getLoginSession,
    resumeLoginSession,
    saveLoginSession,
    updateAuthField,
} from '@/stores/auth';
import type { IAuthNotify } from '@/types/auth';
import type { IResponse } from '@/types/websocket';
import { replace } from 'svelte-spa-router';
import { wsReconnectManager } from '@/ws/wsReconnect';
import { get } from 'svelte/store';

export async function handleAuthRequiredNotify(params: IResponse<IAuthNotify>) {
    console.log('auth required notify coming message', params);
    const { data } = params;
    // needs init password
    if ('passwordInitRequired' in data) {
        clearLoginSession();
        updateAuthField('sessionReady', false);
        updateAuthField('loggedIn', false);
        updateAuthField('passwordInitRequired', true);
        updateAuthField('authParams', { nonce: data.nonce });
        void replace(ROUTES_PATH.LOGIN);
        return;
    } else {
        updateAuthField('passwordInitRequired', false);
    }

    if ('passwordRequired' in data) {
        updateAuthField('passwordRequired', data.passwordRequired);
        if (data.passwordRequired) {
            const { passwordRequired, sessionResumeSupported, ...authParams } = data;
            updateAuthField('authParams', authParams);

            if (get(authStore).handoffPending) {
                try {
                    const handoffResponse = await exchangeProvisioningHandoff();
                    const session = handoffResponse?.data;
                    if (handoffResponse?.error === 0 && session?.authenticated && session.nonce) {
                        updateAuthField('authParams', { ...authParams, nonce: session.nonce });
                        if (session.sessionIssued && session.sessionId && session.sessionToken && typeof session.expiresAt === 'number') {
                            saveLoginSession({
                                sessionId: session.sessionId,
                                sessionToken: session.sessionToken,
                                expiresAt: session.expiresAt,
                            });
                        }
                        updateAuthField('sessionReady', true);
                        updateAuthField('loggedIn', true);
                        void replace(ROUTES_PATH.ROOT);
                        return;
                    }
                } catch (error) {
                    console.error('Provisioning handoff failed', error);
                }
                clearLoginSession();
            }

            if (sessionResumeSupported && getLoginSession()) {
                try {
                    const resumeResponse = await resumeLoginSession(data.nonce);
                    if (resumeResponse?.error === 0 &&
                        resumeResponse.data?.authenticated &&
                        resumeResponse.data.nonce) {
                        const currentAuthParams = get(authStore).authParams;
                        if (currentAuthParams) {
                            updateAuthField('authParams', {
                                ...currentAuthParams,
                                nonce: resumeResponse.data.nonce,
                            });
                        }
                        updateAuthField('sessionReady', true);
                        updateAuthField('loggedIn', true);
                        return;
                    }
                } catch {
                    // Fall through to a fresh password-authentication connection.
                }

                clearLoginSession();
                updateAuthField('sessionReady', false);
                updateAuthField('loggedIn', false);
                updateAuthField('authParams', null);
                // Every resume attempt rotates the server nonce. Reconnect so the
                // password form receives a fresh nonce before it is displayed.
                queueMicrotask(() => {
                    void import('@/ws/initWebsocket').then(({ initWebsocket }) => initWebsocket());
                });
                return;
            }

            updateAuthField('sessionReady', false);
            updateAuthField('loggedIn', false);
            if (!get(authStore).initialSetup) void replace(ROUTES_PATH.LOGIN);
        } else {
            // ap connect: no authentication required
            updateAuthField('sessionReady', true);
            updateAuthField('loggedIn', true);
            updateAuthField('authParams', { nonce: data.nonce });
            if (wsReconnectManager.status) return;
        }
        return;
    }

    // reach websocket connection limit
    if ('connectionLimitReached' in data) {
        updateAuthField('sessionReady', false);
        updateAuthField('loggedIn', false);
        updateAuthField('connectionLimitReached', true);
        return;
    }
}
