import { get } from 'svelte/store';
import { getHassEntityMdiIcon, HASS_ENTITY_SUPPORT_CONFIG } from '@/constant';
import { haRegistryStore } from '@/stores/haRegistry';
import { addScreenBadge, addScreenCard, removeScreenCard, screenConfigStore, updateScreenCard, updateScreenCardActionConfig } from '@/stores/screenConfig';
import type { IHassRegistryTreeArea, IHassRegistryTreeDevice, IHassRegistryTreeEntity } from '@/types/haRegistry';
import type { IHassScreenActionConfig, THassScreenAction } from '@/types/screenConfig';

export type TEntityPickerMode = 'badge' | 'entity';
export type TActionSelection = 'default' | THassScreenAction;

export interface IEntitySearchResult {
    entity: IHassRegistryTreeEntity;
    rank: number;
    name: string;
}

export const SCREEN_ACTION_OPTIONS: { value: TActionSelection; label: string }[] = [
    { value: 'more-info', label: 'More info' },
    { value: 'toggle', label: 'Toggle' },
    { value: 'nothing', label: 'Nothing' },
];

export function getScreenEntityDefaultActions(entityId: string): {
    tap: THassScreenAction;
    hold: THassScreenAction;
    iconTap: THassScreenAction;
} {
    const domain = entityId.split('.')[0] ?? '';
    const iconTapDefault = HASS_ENTITY_SUPPORT_CONFIG[domain]?.iconTapDefault ?? (['light', 'switch', 'scene'].includes(domain) ? 'toggle' : 'nothing');
    return {
        tap: 'more-info',
        hold: 'nothing',
        iconTap: iconTapDefault,
    };
}

export function createDefaultScreenActionConfig(): IHassScreenActionConfig {
    return {};
}

export function getScreenActionOptions(defaultAction: THassScreenAction) {
    const defaultLabel = SCREEN_ACTION_OPTIONS.find((option) => option.value === defaultAction)?.label ?? defaultAction;
    return [{ value: 'default' as const, label: `Default (${defaultLabel})` }, ...SCREEN_ACTION_OPTIONS];
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

export function getScreenEntityName(entityId: string) {
    const entity = get(haRegistryStore).entities.find((item) => (item.ei ?? item.entity_id) === entityId);
    return entity?.en ?? entity?.name ?? entity?.ne ?? entity?.original_name ?? entity?.or ?? entityId;
}

export function getScreenEntityIcon(entityId: string) {
    const registry = get(haRegistryStore);
    const entity = registry.entities.find((item) => (item.ei ?? item.entity_id) === entityId);
    const state = registry.states.find((item) => item.entity_id === entityId);
    return getHassEntityMdiIcon(entityId, entity?.ic ?? entity?.icon, state?.attributes?.device_class, state?.state);
}

export function addEntityToScreen(screenIndex: number, mode: TEntityPickerMode, entity: IHassRegistryTreeEntity) {
    if (mode === 'badge') {
        addScreenBadge(screenIndex, {
            type: 'entity',
            data: { entity: entity.id, name: entity.name, actionConfig: createDefaultScreenActionConfig() },
        });
        return;
    }
    addScreenCard(screenIndex, {
        type: 'tile',
        data: { entity: entity.id, full_width: true, actionConfig: createDefaultScreenActionConfig() },
    });
}

export function saveScreenCardActions(screenIndex: number, entityId: string, tap: TActionSelection, hold: TActionSelection, iconTap: TActionSelection) {
    updateScreenCardActionConfig(screenIndex, entityId, {
        tap_action: toScreenActionItem(tap),
        hold_action: toScreenActionItem(hold),
        icon_tap_action: toScreenActionItem(iconTap),
    });
}

export function saveScreenCardSettings(
    screenIndex: number,
    currentEntityId: string,
    entityId: string,
    tap: TActionSelection,
    hold: TActionSelection,
    iconTap: TActionSelection,
) {
    updateScreenCard(screenIndex, currentEntityId, entityId, {
        tap_action: toScreenActionItem(tap),
        hold_action: toScreenActionItem(hold),
        icon_tap_action: toScreenActionItem(iconTap),
    });
}

export function deleteScreenCard(screenIndex: number, entityId: string) {
    removeScreenCard(screenIndex, entityId);
}

export function getScreenCardActionSelections(screenIndex: number, entityId: string) {
    const card = get(screenConfigStore)
        .screens.find((screen) => screen.index === screenIndex)
        ?.cards?.find((item) => item.data.entity === entityId);
    return {
        tap: toScreenActionSelection(card?.data.actionConfig?.tap_action?.action),
        hold: toScreenActionSelection(card?.data.actionConfig?.hold_action?.action),
        iconTap: toScreenActionSelection(card?.data.actionConfig?.icon_tap_action?.action),
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
    const stateNames = new Map(registry.states.map((state) => [state.entity_id, state.attributes?.friendly_name ?? '']));
    const entities = new Map<string, { entity: IHassRegistryTreeEntity; searchText: string; name: string; id: string }>();
    const addEntity = (entity: IHassRegistryTreeEntity, context: string[] = []) => {
        if (entities.has(entity.id)) return;
        const raw = entity.raw;
        entities.set(entity.id, {
            entity,
            name: normalizeSearchValue(entity.name),
            id: normalizeSearchValue(entity.id),
            searchText: normalizeSearchValue(
                [entity.name, entity.id, stateNames.get(entity.id), raw.en, raw.name, raw.ne, raw.original_name, raw.or, ...context].filter(Boolean).join(' '),
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
            rank: !query ? 0 : item.id === query ? 0 : item.name === query ? 1 : item.name.startsWith(query) ? 2 : item.id.startsWith(query) ? 3 : 4,
        }))
        .sort((left, right) => left.rank - right.rank || left.name.localeCompare(right.name, document.documentElement.lang || navigator.language, { numeric: true }));
}
