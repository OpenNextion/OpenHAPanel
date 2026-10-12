import { writable } from 'svelte/store';
import { isHassEntitySupported, isHassEntityVisibleInPicker } from '@/constant/hassEntity';
import type {
    IHassAreaRegistryEntry,
    IHassDeviceRegistryEntry,
    IHassEntityRegistryDisplayEntry,
    IHassFloorRegistryEntry,
    IHassRegistryTree,
    IHassRegistryTreeArea,
    IHassRegistryTreeDevice,
    IHassRegistryTreeDomain,
    IHassRegistryTreeEntity,
    IHassStateEntry,
} from '@/types/haRegistry';

interface IHassRegistryStore {
    loading: boolean;
    error: string | null;
    states: IHassStateEntry[];
    entities: IHassEntityRegistryDisplayEntry[];
    devices: IHassDeviceRegistryEntry[];
    areas: IHassAreaRegistryEntry[];
    floors: IHassFloorRegistryEntry[];
    domainTranslations: Record<string, string>;
    entityCategories: Record<string, string>;
    tree: IHassRegistryTree;
}

const EMPTY_TREE: IHassRegistryTree = {
    floors: [],
    otherAreas: [],
    unassignedDevices: [],
    unassignedServices: [],
    unassignedEntityDomains: [],
    unassignedHelperDomains: [],
};

const INIT_VALUE: IHassRegistryStore = {
    loading: false,
    error: null,
    states: [],
    entities: [],
    devices: [],
    areas: [],
    floors: [],
    domainTranslations: {},
    entityCategories: {},
    tree: EMPTY_TREE,
};

const collator = new Intl.Collator(document.documentElement.lang || navigator.language || undefined, { numeric: true });

function compareNames<T extends { name: string }>(left: T, right: T) {
    return collator.compare(left.name, right.name);
}

const HELPER_DOMAINS = new Set(['counter', 'input_boolean', 'input_button', 'input_datetime', 'input_number', 'input_select', 'input_text', 'schedule', 'timer']);

function computeStateName(state: IHassStateEntry) {
    if (state.attributes?.friendly_name !== undefined) return String(state.attributes.friendly_name ?? '');
    return state.entity_id.slice(state.entity_id.indexOf('.') + 1).replace(/_/g, ' ');
}

function stripDeviceName(entityName: string, deviceName: string) {
    const lowerName = entityName.toLowerCase();
    const lowerDeviceName = deviceName.toLowerCase();
    for (const suffix of [' ', ': ', ' - ']) {
        const prefix = `${lowerDeviceName}${suffix}`;
        if (!lowerName.startsWith(prefix)) continue;
        const stripped = entityName.slice(prefix.length);
        if (!stripped) return undefined;
        const firstWord = stripped.slice(0, stripped.indexOf(' '));
        return firstWord.toLowerCase() !== firstWord ? stripped : stripped[0].toUpperCase() + stripped.slice(1);
    }
    return undefined;
}

function normalizeEntity(state: IHassStateEntry, entity: IHassEntityRegistryDisplayEntry | undefined, device?: IHassDeviceRegistryEntry): IHassRegistryTreeEntity {
    const id = state.entity_id;
    const registryName = entity?.en ?? entity?.name ?? entity?.ne ?? entity?.original_name ?? entity?.or;
    const deviceName = (device?.name_by_user ?? device?.name)?.trim();
    const entityName = registryName && deviceName ? (registryName === deviceName ? undefined : (stripDeviceName(registryName, deviceName) ?? registryName)) : registryName;
    return {
        id,
        name: entityName ?? computeStateName(state) ?? id,
        deviceId: entity?.di ?? entity?.device_id ?? null,
        areaId: entity?.ai ?? entity?.area_id ?? null,
        raw: entity ?? { entity_id: id },
    };
}

export function buildHassRegistryTree(
    states: IHassStateEntry[],
    entities: IHassEntityRegistryDisplayEntry[],
    devices: IHassDeviceRegistryEntry[],
    areas: IHassAreaRegistryEntry[],
    floors: IHassFloorRegistryEntry[],
    domainTranslations: Record<string, string>,
    entityCategories: Record<string, string> = {},
): IHassRegistryTree {
    const entityById = new Map(entities.map((entity) => [entity.ei ?? entity.entity_id, entity]));
    const deviceById = new Map(devices.map((device) => [device.id, device]));
    const stateById = new Map(states.map((state) => [state.entity_id, state]));
    const stateNameById = new Map(states.map((state) => [state.entity_id, computeStateName(state)]));
    const areasById = new Map<string, IHassRegistryTreeArea>();
    const deviceNodesByArea = new Map<string, Map<string, IHassRegistryTreeDevice>>();
    const unassignedDeviceNodes = new Map<string, IHassRegistryTreeDevice>();
    const unassignedServiceNodes = new Map<string, IHassRegistryTreeDevice>();
    const unassignedEntityByDomain = new Map<string, IHassRegistryTreeEntity[]>();
    const unassignedHelperByDomain = new Map<string, IHassRegistryTreeEntity[]>();

    areas.forEach((area) => {
        areasById.set(area.area_id, {
            id: area.area_id,
            name: area.name.trim(),
            floorId: area.floor_id ?? null,
            devices: [],
            entities: [],
            raw: area,
        });
    });

    const getDeviceNode = (device: IHassDeviceRegistryEntry, areaId: string | null, service = false) => {
        const targetMap = areaId
            ? (deviceNodesByArea.get(areaId) ?? new Map<string, IHassRegistryTreeDevice>())
            : service
              ? unassignedServiceNodes
              : unassignedDeviceNodes;
        if (areaId && !deviceNodesByArea.has(areaId)) deviceNodesByArea.set(areaId, targetMap);
        let node = targetMap.get(device.id);
        if (!node) {
            node = {
                id: device.id,
                name: (device.name_by_user ?? device.name)?.trim() || device.id,
                areaId,
                entities: [],
                children: [],
                raw: device,
            };
            targetMap.set(device.id, node);
        }
        return node;
    };

    const resolveDeviceAreaId = (device?: IHassDeviceRegistryEntry): string | null => {
        const visited = new Set<string>();
        let current = device;
        while (current && !visited.has(current.id)) {
            visited.add(current.id);
            if (current.area_id && areasById.has(current.area_id)) return current.area_id;
            current = current.parent_device_id ? deviceById.get(current.parent_device_id) : undefined;
        }
        return null;
    };

    const getEntitySortPriority = (entity: IHassRegistryTreeEntity) => {
        const state = stateById.get(entity.id);
        if (!isHassEntitySupported(entity.id, state?.attributes?.device_class)) return 2;
        if (state?.state === 'unavailable') return 1;
        return 0;
    };
    const compareEntities = (left: IHassRegistryTreeEntity, right: IHassRegistryTreeEntity) => {
        const priorityDifference = getEntitySortPriority(left) - getEntitySortPriority(right);
        if (priorityDifference !== 0) return priorityDifference;
        return collator.compare(stateNameById.get(left.id) ?? left.id, stateNameById.get(right.id) ?? right.id);
    };

    states.forEach((state) => {
        const entity = entityById.get(state.entity_id);
        if (entity?.hb) return;
        if (!isHassEntityVisibleInPicker(state.entity_id, state.attributes?.device_class, entity?.entity_category ?? entity?.ec, entityCategories)) return;
        const entityDeviceId = entity?.di ?? entity?.device_id;
        const node = normalizeEntity(state, entity, entityDeviceId ? deviceById.get(entityDeviceId) : undefined);
        const device = node.deviceId ? deviceById.get(node.deviceId) : undefined;
        const candidateAreaId = node.areaId ?? resolveDeviceAreaId(device);
        const areaId = candidateAreaId && areasById.has(candidateAreaId) ? candidateAreaId : null;
        node.areaId = areaId;

        if (areaId && device && !node.raw.ai && !node.raw.area_id) {
            getDeviceNode(device, areaId).entities.push(node);
        } else if (areaId) {
            areasById.get(areaId)?.entities.push(node);
        } else if (device) {
            getDeviceNode(device, null, device.entry_type === 'service').entities.push(node);
        } else {
            const domain = node.id.split('.')[0];
            const target = HELPER_DOMAINS.has(domain) ? unassignedHelperByDomain : unassignedEntityByDomain;
            target.set(domain, [...(target.get(domain) ?? []), node]);
        }
    });

    const buildDeviceHierarchy = (nodes: Map<string, IHassRegistryTreeDevice>) => {
        const roots: IHassRegistryTreeDevice[] = [];
        nodes.forEach((node) => {
            node.entities.sort(compareEntities);
            const parent = node.raw.parent_device_id ? nodes.get(node.raw.parent_device_id) : undefined;
            if (parent) parent.children.push(node);
            else roots.push(node);
        });
        const sortNodes = (items: IHassRegistryTreeDevice[]) => {
            items.sort(compareNames);
            items.forEach((item) => sortNodes(item.children));
        };
        sortNodes(roots);
        return roots;
    };

    const includeParentDevices = (nodes: Map<string, IHassRegistryTreeDevice>, areaId: string | null, service: boolean) => {
        [...nodes.values()].forEach((node) => {
            let child = node.raw;
            while (child.parent_device_id) {
                const parent = deviceById.get(child.parent_device_id);
                if (!parent || resolveDeviceAreaId(parent) !== areaId || (parent.entry_type === 'service') !== service) break;
                getDeviceNode(parent, areaId, service);
                child = parent;
            }
        });
    };

    deviceNodesByArea.forEach((nodes, areaId) => includeParentDevices(nodes, areaId, false));
    includeParentDevices(unassignedDeviceNodes, null, false);
    includeParentDevices(unassignedServiceNodes, null, true);

    areasById.forEach((area) => {
        const nodes = deviceNodesByArea.get(area.id);
        area.devices = nodes ? buildDeviceHierarchy(nodes) : [];
        area.entities.sort(compareEntities);
    });

    const buildDomainGroups = (source: Map<string, IHassRegistryTreeEntity[]>): IHassRegistryTreeDomain[] =>
        [...source.entries()]
            .map(([domain, domainEntities]) => ({
                domain,
                name: domainTranslations[`component.${domain}.title`] ?? domain,
                entities: domainEntities.sort(compareEntities),
            }))
            .sort(compareNames);

    const hasDeviceEntities = (device: IHassRegistryTreeDevice): boolean => Boolean(device.entities.length || device.children.some(hasDeviceEntities));
    const areasWithEntities = [...areasById.values()].filter((area) => area.entities.length || area.devices.some(hasDeviceEntities));

    const floorNodes = floors
        .map((floor) => ({
            id: floor.floor_id,
            name: floor.name,
            areas: areasWithEntities.filter((area) => area.floorId === floor.floor_id),
            raw: floor,
        }))
        .filter((floor) => floor.areas.length);

    return {
        floors: floorNodes,
        otherAreas: areasWithEntities.filter((area) => !area.floorId),
        unassignedDevices: buildDeviceHierarchy(unassignedDeviceNodes),
        unassignedServices: buildDeviceHierarchy(unassignedServiceNodes),
        unassignedEntityDomains: buildDomainGroups(unassignedEntityByDomain),
        unassignedHelperDomains: buildDomainGroups(unassignedHelperByDomain),
    };
}

export const haRegistryStore = writable<IHassRegistryStore>(INIT_VALUE);

export function setHassRegistryLoading(loading: boolean) {
    haRegistryStore.update((store) => ({ ...store, loading, error: loading ? null : store.error }));
}

export function setHassRegistryError(error: string) {
    haRegistryStore.update((store) => ({ ...store, loading: false, error }));
}

export function setHassRegistryData(
    data: Pick<IHassRegistryStore, 'states' | 'entities' | 'devices' | 'areas' | 'floors' | 'domainTranslations' | 'entityCategories'>,
) {
    haRegistryStore.set({
        ...data,
        loading: false,
        error: null,
        tree: buildHassRegistryTree(data.states, data.entities, data.devices, data.areas, data.floors, data.domainTranslations, data.entityCategories),
    });
}
