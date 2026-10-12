import CryptoJS from 'crypto-js';
import { get } from 'svelte/store';
import { sharedKey } from '../stores/auth';
import { authStore } from '@/stores/auth';

type WordArray = CryptoJS.lib.WordArray;

interface IEncryptData {
    iv: string;
    content: string;
}

// generate dynamic key
export function generateSharedKey(nonce: string) {
    return CryptoJS.SHA256(nonce);
}

// encrypt data
function encrypt(data: string, sharedKey: WordArray) {
    const iv = CryptoJS.lib.WordArray.random(16);

    const encrypted = CryptoJS.AES.encrypt(data, sharedKey, {
        iv: iv,
        mode: CryptoJS.mode.CBC,
        padding: CryptoJS.pad.Pkcs7,
    });

    return {
        iv: CryptoJS.enc.Base64.stringify(iv),
        content: encrypted.toString(),
    };
}

// decrypt data
export function decrypt(encryptedData: { iv: string; content: string }, sharedKey: WordArray) {
    const iv = CryptoJS.enc.Base64.parse(encryptedData.iv);

    const decrypted = CryptoJS.AES.decrypt(encryptedData.content, sharedKey, {
        iv: iv,
        mode: CryptoJS.mode.CBC,
        padding: CryptoJS.pad.Pkcs7,
    });

    return decrypted.toString(CryptoJS.enc.Utf8);
}

export function encryptData(sensitiveData: string): IEncryptData {
    const curSharedKey = get(sharedKey);
    return encrypt(sensitiveData, curSharedKey);
}

export function decryptData(encryptedData: IEncryptData): string {
    const curSharedKey = get(sharedKey);
    return decrypt(encryptedData, curSharedKey);
}

/**
 * @description calculate token base on username and password
 * @export
 * @param {string} pwd
 * @returns {*}  {{ cnonce: string, response: string }}
 */
export function calculateToken(pwd: string): { cnonce: string; response: string } {
    const params = get(authStore).authParams;
    const username = get(authStore).username;
    if (!params) return { cnonce: '', response: '' };

    const { qop, nonce, realm } = params;
    const method = 'dummy_method';
    const uri = 'dummy_uri';
    const cnonce = createNonce();

    const HA1 = CryptoJS.SHA256(`${username}:${realm}:${pwd}`).toString(CryptoJS.enc.Hex);

    const HA2 = CryptoJS.SHA256(`${method}:${uri}`).toString(CryptoJS.enc.Hex);

    const response = CryptoJS.SHA256(`${HA1}:${nonce}:${cnonce}:${qop}:${HA2}`).toString(CryptoJS.enc.Hex);

    return {
        cnonce,
        response,
    };
}

export function createNonce(byteLength = 16) {
    return CryptoJS.lib.WordArray.random(byteLength).toString(CryptoJS.enc.Hex);
}

export function calculateSessionProof(sessionId: string, sessionToken: string, serverNonce: string) {
    const cnonce = createNonce();
    const verifier = CryptoJS.SHA256(CryptoJS.enc.Utf8.parse(sessionToken));
    const message = `resume-v1\n${sessionId}\n${serverNonce}\n${cnonce}`;
    const proof = CryptoJS.HmacSHA256(message, verifier).toString(CryptoJS.enc.Hex);
    return { cnonce, proof };
}
