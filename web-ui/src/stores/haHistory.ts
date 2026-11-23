import { writable } from 'svelte/store';

export interface IHassHistoryPoint {
    timestamp: number;
    value: number;
}

interface IHassHistoryEntry {
    points: IHassHistoryPoint[];
    loading: boolean;
}

interface IHassHistoryStore {
    entities: Record<string, IHassHistoryEntry>;
}

export const haHistoryStore = writable<IHassHistoryStore>({ entities: {} });

export function beginHassEntityHistory(entityId: string) {
    haHistoryStore.update((store) => ({
        ...store,
        entities: {
            ...store.entities,
            [entityId]: { points: store.entities[entityId]?.points ?? [], loading: true },
        },
    }));
}

export function appendHassHistoryStream(entityId: string, points: IHassHistoryPoint[], reset = false) {
    haHistoryStore.update((store) => {
        const entry = store.entities[entityId];
        const combined = [...(reset ? [] : entry?.points ?? []), ...points]
            .sort((left, right) => left.timestamp - right.timestamp)
            .filter((point, index, all) => index === 0 || point.timestamp !== all[index - 1].timestamp || point.value !== all[index - 1].value);
        const cutoff = Date.now() - 24 * 60 * 60 * 1000;
        let firstVisible = combined.findIndex((point) => point.timestamp >= cutoff);
        if (firstVisible < 0) firstVisible = combined.length;
        if (firstVisible > 0) {
            combined[firstVisible - 1] = { ...combined[firstVisible - 1], timestamp: cutoff };
            combined.splice(0, firstVisible - 1);
        }
        return {
            ...store,
            entities: { ...store.entities, [entityId]: { points: combined, loading: false } },
        };
    });
}

export function failHassEntityHistory(entityId: string) {
    haHistoryStore.update((store) => {
        const entry = store.entities[entityId];
        if (!entry) return store;
        return { ...store, entities: { ...store.entities, [entityId]: { ...entry, loading: false } } };
    });
}

export function clearHassHistory() {
    haHistoryStore.set({ entities: {} });
}
