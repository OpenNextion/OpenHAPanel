import { initWebsocket } from '@/ws/initWebsocket';
import { ETopTipsType, useShowTopTips } from '@/hooks/useShowTopTips.svelte';

class WsReconnectManager {
    static STEP = 5000;
    static MAX_MS = 60000;
    static TOP_TIPS_DISAPPEAR_MS = 5000;
    #timer: null | ReturnType<typeof setTimeout> = null;
    #nextMs = 0;
    #status = false;

    #getNextReconnectSecond() {
        const { MAX_MS, STEP } = WsReconnectManager;
        this.#nextMs = this.#nextMs >= MAX_MS ? MAX_MS : this.#nextMs + STEP;
        return this.#nextMs;
    }

    #clearTimer() {
        if (this.#timer) {
            clearTimeout(this.#timer);
            this.#timer = null;
        }
    }

    #registerTimer(cb?: () => void) {
        this.#clearTimer();
        const nextMs = this.#getNextReconnectSecond();
        this.#timer = setTimeout(async () => {
            const result = await initWebsocket();
            console.log(`RECONNECT: initWebsocket result ${JSON.stringify(result)}`);
            if (result?.error === 0) {
                console.log('RECONNECT: reconnect success');
                cb && cb();
            } else {
                console.warn('RECONNECT: reconnect failed');
                this.#registerTimer(cb);
            }
        }, nextMs);
        console.log(`RECONNECT: reconnect after ${nextMs}ms`);
    }

    /** Start ws reconnection */
    start() {
        if (this.status) {
            console.warn('RECONNECT: ws reconnect progressing, do not repeat the operation!');
            return;
        }
        console.log('RECONNECT: start!');
        const { topTips, setTopTips } = useShowTopTips();
        if (topTips.type !== ETopTipsType.RESET_PASSWORD) {
            setTopTips({ type: ETopTipsType.RECONNECT });
        }
        this.#status = true;
        this.#nextMs = 0;
        this.#registerTimer(() => {
            console.log('RECONNECT: cb exec');
            if (topTips.type === ETopTipsType.RESET_PASSWORD) {
                this.#status = false;
                return;
            }
            setTopTips({ type: ETopTipsType.RECONNECT_SUCCESS });
            setTimeout(() => {
                if (topTips.type === ETopTipsType.RECONNECT_SUCCESS) setTopTips({ type: void 0 });
            }, WsReconnectManager.TOP_TIPS_DISAPPEAR_MS);
            this.#status = false;
        });
    }

    /** Close ws reconnection */
    close() {
        console.log('RECONNECT: close!');
        this.#clearTimer();
        this.#status = false;
        this.#nextMs = 0;
        const { topTips, setTopTips } = useShowTopTips();
        if (topTips.type === ETopTipsType.RECONNECT || topTips.type === ETopTipsType.RECONNECT_SUCCESS) {
            setTopTips({ type: void 0 });
        }
    }

    /** Returns a Boolean value indicating the reconnection is in progress. */
    get status() {
        return this.#status;
    }
}

export const wsReconnectManager = new WsReconnectManager();
