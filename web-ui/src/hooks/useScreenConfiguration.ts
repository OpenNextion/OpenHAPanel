import { get } from 'svelte/store';
import {
    getHassEntityDisplayName,
    type THassActionSelection,
} from '@/hooks/useHassEntity';
import type { IHassEntityInteractionConfig } from '@/constant/hassEntity';
import { haRegistryStore, haRegistryTreeStore } from '@/stores/haRegistry';
import { deviceStore } from '@/stores/device';
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
import type { IHassScreenActionConfig, IHassScreenConfigFeature, THassScreenAction } from '@/types/screenConfig';
import type { TTileFeaturePosition } from '@/constant/tileFeaturePosition';

export type TEntityPickerMode = 'badge' | 'entity';
export type TActionSelection = THassActionSelection;

export interface IEntitySearchResult {
    entity: IHassRegistryTreeEntity;
    rank: number;
    name: string;
    displayName: string;
}

export const SCREEN_ACTION_OPTIONS: { value: THassScreenAction; labelKey: string }[] = [
    { value: 'more-info', labelKey: 'page.screen.actionMoreInfo' },
    { value: 'toggle', labelKey: 'page.screen.actionToggle' },
    { value: 'nothing', labelKey: 'page.screen.actionNothing' },
];

export function createDefaultScreenActionConfig(): IHassScreenActionConfig {
    return {};
}

export function getScreenActionOptions(interaction: IHassEntityInteractionConfig, translate: (key: string) => string) {
    const defaultKey = SCREEN_ACTION_OPTIONS.find((option) => option.value === interaction.default)?.labelKey;
    const defaultLabel = defaultKey ? translate(defaultKey) : String(interaction.default);
    const supportedOptions = SCREEN_ACTION_OPTIONS
        .filter((option) => interaction.options.includes(option.value))
        .map((option) => ({ value: option.value, label: translate(option.labelKey) }));
    return [{ value: 'default' as const, label: `${translate('page.screen.actionDefault')} (${defaultLabel})` }, ...supportedOptions];
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

export function addEntityToScreen(screenIndex: number, mode: TEntityPickerMode, entity: IHassRegistryTreeEntity, features: IHassScreenConfigFeature[] = []) {
    if (mode === 'badge') {
        addScreenBadge(screenIndex, {
            type: 'entity',
            data: { entity: entity.id, name: getHassEntityDisplayName(entity.id), actionConfig: createDefaultScreenActionConfig() },
        });
        return;
    }
    addScreenCard(screenIndex, {
        type: 'tile',
        data: {
            entity: entity.id,
            full_width: get(deviceStore).screenRotation.rotation !== 90,
            actionConfig: createDefaultScreenActionConfig(),
            ...(features.length ? { features } : {}),
        },
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
    fullWidth?: boolean,
    features?: IHassScreenConfigFeature[],
    featuresPosition?: TTileFeaturePosition,
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
    updateScreenCard(screenIndex, currentEntityId, entityId, actionConfig, fullWidth, features, featuresPosition);
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
    const tree = get(haRegistryTreeStore);
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
    tree.floors.forEach((floor) => floor.areas.forEach((area) => addArea(area, [floor.name])));
    tree.otherAreas.forEach((area) => addArea(area, []));
    tree.unassignedDevices.forEach((device) => addDevice(device, []));
    tree.unassignedServices.forEach((device) => addDevice(device, []));
    [...tree.unassignedEntityDomains, ...tree.unassignedHelperDomains].forEach((domain) => domain.entities.forEach((entity) => addEntity(entity, [domain.name])));
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
