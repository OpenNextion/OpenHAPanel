/** auth status notify */
export type IAuthNotify = IAuthNotifyInitRequired | IAuthNotifyConnectionLimit | IAuthNotifyPwdStatus;

interface IAuthNotifyPwdNotRequired {
    passwordRequired: false;
    nonce: string;
    sessionResumeSupported?: boolean;
}

interface IAuthNotifyPwdRequired extends IAuthParams {
    passwordRequired: true;
    sessionResumeSupported?: boolean;
}

export type IAuthNotifyPwdStatus = IAuthNotifyPwdNotRequired | IAuthNotifyPwdRequired;

interface IAuthNotifyInitRequired {
    passwordInitRequired: true;
    nonce: string;
}

interface IAuthNotifyConnectionLimit {
    connectionLimitReached: true;
}

export interface IAuthParams {
    username: string;
    realm: string;
    nonce: string;
    algorithm: string;
    qop: string;
}

export interface IForgotPwdNotify {
    state: 'ready' | 'done' | 'waiting';
}

export interface ILoginSession {
    sessionId: string;
    sessionToken: string;
    expiresAt: number;
}

export interface ILoginResponse {
    authenticated: boolean;
    nonce?: string;
    sessionIssued: boolean;
    sessionId?: string;
    sessionToken?: string;
    expiresAt?: number;
    sessionIssueReason?: string;
}

export interface IResumeSessionResponse {
    authenticated: boolean;
    nonce?: string;
    expiresAt?: number;
    reason?: 'sessionInvalid' | 'sessionExpired' | 'timeUnavailable' | 'tooManyAttempts';
}
