import { unwrapFunctionStore, format } from 'svelte-i18n';
import { EIPAcquisition } from '@/types/network';
import { t } from 'svelte-i18n';
import { get } from 'svelte/store';
import { REG_EXP_IPV4, ROUTES_PATH } from '@/constant';
//import { ERouterStatus } from '../types/enum/EWirelessController';

// use document.execCommand('copy') to copy text;
function fallbackCopyText(text: string) {
    return new Promise((resolve, reject) => {
        try {
            const textarea = document.createElement('textarea');
            textarea.value = text;
            textarea.style.position = 'fixed';
            textarea.style.top = '0px';
            textarea.style.left = '-9999px';
            textarea.style.opacity = '0px';
            textarea.style.pointerEvents = 'none';
            textarea.setAttribute('readonly', '');
            document.body.appendChild(textarea);
            if (navigator.userAgent.match(/iphone|ipad|ipod/i)) {
                textarea.contentEditable = 'true';
                textarea.readOnly = true;
                const range = document.createRange();
                range.selectNodeContents(textarea);
                const selection = window.getSelection();
                selection?.removeAllRanges();
                selection?.addRange(range);
                textarea.setSelectionRange(0, 999999);
            } else {
                textarea.select();
            }
            const successful = document.execCommand('copy');
            document.body.removeChild(textarea);
            successful ? resolve(void 0) : reject(new Error('execCommand copy failed!'));
        } catch (err) {
            reject(err);
        }
    });
}

export function copyText(text: string) {
    return new Promise((resolve, reject) => {
        if (navigator.clipboard) {
            navigator.clipboard
                .writeText(text)
                .then(() => resolve(void 0))
                .catch((err) => {
                    console.error('clipboard api failed!', err);
                    fallbackCopyText(text).then(resolve).catch(reject);
                });
        } else {
            fallbackCopyText(text).then(resolve).catch(reject);
        }
    });
}

export function formatDuration(seconds: number): string {
    // Calculate days, hours, and minutes
    const days = Math.floor(seconds / (24 * 3600));
    const hours = Math.floor((seconds % (24 * 3600)) / 3600);
    const minutes = Math.floor((seconds % 3600) / 60);

    // Format hours and minutes to always be two digits
    const formattedHours = hours.toString().padStart(2, '0');
    const formattedMinutes = minutes.toString().padStart(2, '0');

    // Construct the result string
    const $format = unwrapFunctionStore(format);
    if (days > 0) {
        return `${days}${$format('common.day')} ${formattedHours}${$format('common.hour')} ${formattedMinutes}${$format('common.minute')}`;
    } else {
        return `${formattedHours}${$format('common.hour')} ${formattedMinutes}${$format('common.minute')}`;
    }
}

export function getIpAcquisitionText(ipAcquisition?: EIPAcquisition) {
    const _t = get(t);
    switch (ipAcquisition) {
        case EIPAcquisition.DHCP:
            return 'DHCP';
        case EIPAcquisition.STATIC:
            return _t('NETWORK_MODE.IP_WAY_MANUAL');
        default:
            return '';
    }
}


export function sleep(ms: number) {
    return new Promise((resolve) => setTimeout(resolve, ms));
}

/** Check whether it complies with the ipv4 address format */
export function validIPv4(ip: string) {
    return REG_EXP_IPV4.test(ip);
}

/** Check whether it complies with the subnet mask address format */
export function validateSubnetMask(mask: string) {
    const isIpv4 = validIPv4(mask);
    if (!isIpv4) return false;
    const parts = mask.split('.');
    const binaryStr = parts.map((p) => parseInt(p, 10).toString(2).padStart(8, '0')).join('');
    if (binaryStr.indexOf('01') > -1) return false;
    return !(binaryStr === '00000000000000000000000000000000' || binaryStr === '11111111111111111111111111111111');
}

/** html escape */
export function htmlEscape(htmlStr: string) {
    return htmlStr.length === 0
        ? ''
        : htmlStr.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/\s/g, '&nbsp;').replace(/\'/g, '&#39;').replace(/\"/g, '&quot;');
}

/** HTML Unescape */
export function htmlUnescape(encodeHtmlStr: string) {
    return encodeHtmlStr.length === 0
        ? ''
        : encodeHtmlStr
              .replace(/&amp;/g, '&')
              .replace(/&lt;/g, '<')
              .replace(/&gt;/g, '>')
              .replace(/&nbsp;/g, ' ')
              .replace(/&#39;/g, "'")
              .replace(/&quot;/g, '\"');
}

export function upperCaseFirstLetter(str: string) {
    return `${str.slice(0, 1).toLocaleUpperCase()}${str.slice(1)}`;
}

/** get filename ext */
export function getFileExtension(path: string) {
    const filename = path.split(/[\\/]/).pop()!;
    const lastDotIndex = filename.lastIndexOf('.');
    return lastDotIndex <= 0 || lastDotIndex === filename.length - 1 ? '' : filename.slice(lastDotIndex);
}

/** calculate the byte length of the string */
export function calculateUtf8ByteSize(str: string): number {
    let size = 0;
    for (let i = 0; i < str.length; i++) {
        const code = str.charCodeAt(i);
        if (code >= 0xd800 && code <= 0xdbff) {
            if (i + 1 < str.length) {
                const code2 = str.charCodeAt(i + 1);
                if (code2 >= 0xdc00 && code2 <= 0xdfff) {
                    size += 4;
                    i++;
                    continue;
                }
            }
        }
        if (code <= 0x7f) {
            size += 1;
        } else if (code <= 0x7ff) {
            size += 2;
        } else {
            size += 3;
        }
    }
    return size;
}

/**
 * Determine whether child is a child element of parent
 * @param {HTMLElement} parent parent DOM
 * @param {HTMLElement} child child DOM
 * @returns
 */
export function childOf(parent: HTMLElement, child: HTMLElement) {
    let current: HTMLElement | null = child;
    while (current) {
        if (current === parent) return true;
        current = current.parentElement;
    }
    return false;
}

/**
 * Compare versions, return true if version 1 is greater than version 2
 * @param {string} version1 version1
 * @param {string} version2 version2
 * @returns {boolean}
 */
export function compareVersions(version1: string, version2: string): boolean {
    const formatterVersion = (version: string) => version.toLowerCase().replace('v', '');
    const formatterVersion1 = formatterVersion(version1);
    const formatterVersion2 = formatterVersion(version2);
    const v1Parts = formatterVersion1.split('.').map(Number);
    const v2Parts = formatterVersion2.split('.').map(Number);
    const maxLength = Math.max(v1Parts.length, v2Parts.length);
    for (let i = 0; i < maxLength; i++) {
        const v1Part = i < v1Parts.length ? v1Parts[i] : 0;
        const v2Part = i < v2Parts.length ? v2Parts[i] : 0;
        if (v1Part > v2Part) return true;
        if (v1Part < v2Part) return false;
    }
    return false;
}
