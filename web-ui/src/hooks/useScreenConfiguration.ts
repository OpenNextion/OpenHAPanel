import { get } from 'svelte/store';
import {
    getHassEntityDisplayName,
    type THassActionSelection,
} from '@/hooks/useHassEntity';
import type { IHassEntityInteractionConfig } from '@/constant/hassEntity';
import { haRegistryStore } from '@/stores/haRegistry';
import {
    addScreenBadge,
    addScreenCard,
    removeScreenBadge,
    removeScreenCard,
    screenConfigStore,
    updateScreenBadge,
    updateScreenCard,
} from '@/stores/screenConfig';
import type { IHassRegistryTreeArea, IHassRegistryTreeDevice, IHassRegistryTreeEntity } from '@/types/haRegistry';
import type { IHassScreenActionConfig, THassScreenAction } from '@/types/screenConfig';

export type TEntityPickerMode = 'badge' | 'entity';
export type TActionSelection = THassActionSelection;

export interface IEntitySearchResult {
    entity: IHassRegistryTreeEntity;
    rank: number;
    name: string;
    displayName: string;
}

export const SCREEN_ACTION_OPTIONS: { value: THassScreenAction; label: string }[] = [
    { value: 'more-info', label: 'More info' },
    { value: 'toggle', label: 'Toggle' },
    { value: 'nothing', label: 'Nothing' },
];

export function createDefaultScreenActionConfig(): IHassScreenActionConfig {
    return {};
}

export function getScreenActionOptions(interaction: IHassEntityInteractionConfig) {
    const defaultLabel = SCREEN_ACTION_OPTIONS.find((option) => option.value === interaction.default)?.label ?? String(interaction.default);
    const supportedOptions = SCREEN_ACTION_OPTIONS.filter((option) => interaction.options.includes(option.value));
    return [{ value: 'default' as const, label: `Default (${defaultLabel})` }, ...supportedOptions];
}

export function toScreenActionSelection(action: THassScreenAction | undefined): TActionSelection {
    return action ?? 'default';
}

export function resolveScreenAction(action: TActionSelection): THassScreenAction | undefined {
    return action === 'default' ? undefined : action;
}

function toScreenActionItem(action: TActionSelection) {
    const resolvedAction = resolveScreenAction(action);
    return resolvedAction ? { action: resolvedAction } : undefined;
}

export function addEntityToScreen(screenIndex: number, mode: TEntityPickerMode, entity: IHassRegistryTreeEntity) {
    if (mode === 'badge') {
        addScreenBadge(screenIndex, {
            type: 'entity',
            data: { entity: entity.id, name: getHassEntityDisplayName(entity.id), actionConfig: createDefaultScreenActionConfig() },
        });
        return;
    }
    addScreenCard(screenIndex, {
        type: 'tile',
        data: { entity: entity.id, full_width: true, actionConfig: createDefaultScreenActionConfig() },
    });
}

export function saveScreenEntitySettings(
    mode: TEntityPickerMode,
    screenIndex: number,
    currentEntityId: string,
    entityId: string,
    name: string,
    tap: TActionSelection,
    hold: TActionSelection,
    iconTap: TActionSelection,
) {
    const actionConfig = {
        tap_action: toScreenActionItem(tap),
        hold_action: toScreenActionItem(hold),
        icon_tap_action: toScreenActionItem(iconTap),
    };
    if (mode === 'badge') {
        updateScreenBadge(screenIndex, currentEntityId, entityId, name, actionConfig);
        return;
    }
    updateScreenCard(screenIndex, currentEntityId, entityId, actionConfig);
}

export function deleteScreenEntity(mode: TEntityPickerMode, screenIndex: number, entityId: string) {
    if (mode === 'badge') removeScreenBadge(screenIndex, entityId);
    else removeScreenCard(screenIndex, entityId);
}

export function getScreenEntityActionSelections(mode: TEntityPickerMode, screenIndex: number, entityId: string) {
    const screen = get(screenConfigStore).screens.find((item) => item.index === screenIndex);
    const actionConfig =
        mode === 'badge'
            ? screen?.badges?.find((item) => item.data.entity === entityId)?.data.actionConfig
            : screen?.cards?.find((item) => item.data.entity === entityId)?.data.actionConfig;
    return {
        tap: toScreenActionSelection(actionConfig?.tap_action?.action),
        hold: toScreenActionSelection(actionConfig?.hold_action?.action),
        iconTap: toScreenActionSelection(actionConfig?.icon_tap_action?.action),
    };
}

function normalizeSearchValue(value: unknown) {
    return String(value ?? '')
        .normalize('NFKD')
        .replace(/\p{Diacritic}/gu, '')
        .toLocaleLowerCase();
}

export function searchScreenEntities(queryValue: string): IEntitySearchResult[] {
    const query = normalizeSearchValue(queryValue).trim();
    const registry = get(haRegistryStore);
    const entities = new Map<string, { entity: IHassRegistryTreeEntity; searchText: string; name: string; displayName: string; id: string }>();
    const addEntity = (entity: IHassRegistryTreeEntity, context: string[] = []) => {
        if (entities.has(entity.id)) return;
        const raw = entity.raw;
        const displayName = getHassEntityDisplayName(entity.id, registry);
        entities.set(entity.id, {
            entity,
            displayName,
            name: normalizeSearchValue(displayName),
            id: normalizeSearchValue(entity.id),
            searchText: normalizeSearchValue(
                [displayName, entity.id, raw.en, raw.name, raw.ne, raw.original_name, raw.or, ...context].filter(Boolean).join(' '),
            ),
        });
    };
    const addDevice = (device: IHassRegistryTreeDevice, context: string[]) => {
        const nextContext = [...context, device.name];
        device.entities.forEach((entity) => addEntity(entity, nextContext));
        device.children.forEach((child) => addDevice(child, nextContext));
    };
    const addArea = (area: IHassRegistryTreeArea, context: string[]) => {
        const nextContext = [...context, area.name];
        area.entities.forEach((entity) => addEntity(entity, nextContext));
        area.devices.forEach((device) => addDevice(device, nextContext));
    };
    registry.tree.floors.forEach((floor) => floor.areas.forEach((area) => addArea(area, [floor.name])));
    registry.tree.otherAreas.forEach((area) => addArea(area, []));
    registry.tree.unassignedDevices.forEach((device) => addDevice(device, []));
    registry.tree.unassignedServices.forEach((device) => addDevice(device, []));
    [...registry.tree.unassignedEntityDomains, ...registry.tree.unassignedHelperDomains].forEach((domain) => domain.entities.forEach((entity) => addEntity(entity, [domain.name])));
    const terms = query ? query.split(/\s+/) : [];
    return [...entities.values()]
        .filter((item) => terms.every((term) => item.searchText.includes(term)))
        .map((item) => ({
            entity: item.entity,
            name: item.name,
            displayName: item.displayName,
            rank: !query ? 0 : item.id === query ? 0 : item.name === query ? 1 : item.name.startsWith(query) ? 2 : item.id.startsWith(query) ? 3 : 4,
        }))
        .sort((left, right) => left.rank - right.rank || left.name.localeCompare(right.name, document.documentElement.lang || navigator.language, { numeric: true }));
}
