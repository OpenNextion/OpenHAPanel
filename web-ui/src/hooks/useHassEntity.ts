import { get } from 'svelte/store';
import {
    getHassEntityMdiIcon,
    HASS_ENTITY_SUPPORT_CONFIG,
    isHassEntitySupported,
    type IHassEntityPreviewConfig,
    type IHassEntitySupportConfig,
} from '@/constant';
import { haRegistryStore } from '@/stores/haRegistry';
import type {
    IHassAreaRegistryEntry,
    IHassDeviceRegistryEntry,
    IHassEntityRegistryDisplayEntry,
    IHassStateEntry,
} from '@/types/haRegistry';
import type { THassScreenAction } from '@/types/screenConfig';

export type THassActionSelection = 'default' | THassScreenAction;

interface IHassEntityRegistryData {
    states: IHassStateEntry[];
    entities: IHassEntityRegistryDisplayEntry[];
    devices: IHassDeviceRegistryEntry[];
    areas: IHassAreaRegistryEntry[];
}

interface IHassEntityPresentationOptions {
    state?: string;
    icon?: string | null;
}

const FALLBACK_PREVIEW: IHassEntityPreviewConfig = { label: 'Unknown', state: 'unknown', tone: 'neutral' };

function getRegistryData(registry?: IHassEntityRegistryData) {
    return registry ?? get(haRegistryStore);
}

export function getHassEntityDomain(entityId: string) {
    return entityId.split('.', 1)[0] ?? '';
}

export function getHassEntitySupportConfig(entityId: string): IHassEntitySupportConfig | undefined {
    return HASS_ENTITY_SUPPORT_CONFIG[getHassEntityDomain(entityId)];
}

export function getHassEntityDisplayName(entityId: string, registry?: IHassEntityRegistryData) {
    const friendlyName = getRegistryData(registry).states.find((item) => item.entity_id === entityId)?.attributes?.friendly_name;
    return typeof friendlyName === 'string' && friendlyName.trim() ? friendlyName.trim() : entityId;
}

export function getHassEntityContext(entityId: string, registry?: IHassEntityRegistryData) {
    const data = getRegistryData(registry);
    const entity = data.entities.find((item) => (item.ei ?? item.entity_id) === entityId);
    const deviceId = entity?.di ?? entity?.device_id;
    const device = deviceId ? data.devices.find((item) => item.id === deviceId) : undefined;
    const areaId = entity?.ai ?? entity?.area_id ?? device?.area_id;
    const area = areaId ? data.areas.find((item) => item.area_id === areaId) : undefined;
    return [area?.name, device?.name_by_user ?? device?.name].filter(Boolean).join(' › ') || entityId;
}

export function getHassEntityCapabilities(entityId: string, registry?: IHassEntityRegistryData) {
    const data = getRegistryData(registry);
    const stateObject = data.states.find((item) => item.entity_id === entityId);
    const deviceClass = typeof stateObject?.attributes?.device_class === 'string' ? stateObject.attributes.device_class : '';
    const config = getHassEntitySupportConfig(entityId);
    const interactions = config?.interactions ?? {
        tap: { supported: true, options: ['more-info', 'toggle', 'nothing'] as const, default: 'more-info' as const },
        hold: { supported: true, options: ['more-info', 'toggle', 'nothing'] as const, default: 'nothing' as const },
        iconTap: { supported: true, options: ['more-info', 'toggle', 'nothing'] as const, default: 'nothing' as const },
    };
    return {
        config,
        deviceClass,
        supported: isHassEntitySupported(entityId, deviceClass),
        interactions,
        defaultActions: {
            tap: interactions.tap.default,
            hold: interactions.hold.default,
            iconTap: interactions.iconTap.default,
        },
    };
}

export function normalizeHassEntityAction(
    action: THassActionSelection,
    entityId: string,
    registry?: IHassEntityRegistryData,
    interaction: 'tap' | 'hold' | 'iconTap' = 'iconTap',
    mode: 'entity' | 'badge' = 'entity',
): THassActionSelection {
    if (action === 'default') return action;
    const capabilities = getHassEntityCapabilities(entityId, registry);
    const interactionConfig = mode === 'badge' && interaction !== 'iconTap'
        ? capabilities.config?.badgeInteractions?.[interaction] ?? capabilities.interactions[interaction]
        : capabilities.interactions[interaction];
    return interactionConfig.options.includes(action) ? action : 'default';
}

export function getHassEntityPresentation(
    entityId: string,
    registry?: IHassEntityRegistryData,
    options: IHassEntityPresentationOptions = {},
) {
    const data = getRegistryData(registry);
    const stateObject = data.states.find((item) => item.entity_id === entityId);
    const registryEntity = data.entities.find((item) => (item.ei ?? item.entity_id) === entityId);
    const capabilities = getHassEntityCapabilities(entityId, data);
    const preview = capabilities.config?.previewByDeviceClass?.[capabilities.deviceClass] ?? capabilities.config?.preview ?? FALLBACK_PREVIEW;
    const isUnavailable = options.state?.toLowerCase() === 'unavailable' || stateObject?.state.toLowerCase() === 'unavailable';
    const iconState = isUnavailable ? 'unavailable' : (stateObject?.state ?? options.state ?? preview.state);
    const configuredIcon = options.icon ?? registryEntity?.ic ?? registryEntity?.icon ?? stateObject?.attributes?.icon;
    let iconStyle: string | undefined;
    if (capabilities.config?.useRgbColor && iconState === 'on' && !isUnavailable) {
        const rgbColor = stateObject?.attributes?.rgb_color;
        if (Array.isArray(rgbColor) && rgbColor.length >= 3) {
            const values = rgbColor.slice(0, 3).map(Number);
            if (values.every((value) => Number.isFinite(value) && value >= 0 && value <= 255)) {
                iconStyle = `--entity-icon-color: rgb(${values.join(', ')}); --entity-icon-background: rgba(${values.join(', ')}, 0.2);`;
            }
        }
    }
    return {
        ...capabilities,
        displayName: getHassEntityDisplayName(entityId, data),
        context: getHassEntityContext(entityId, data),
        displayState: isUnavailable ? 'Unavailable' : (options.state ?? preview.label),
        icon: getHassEntityMdiIcon(entityId, configuredIcon, capabilities.deviceClass, iconState),
        iconState,
        iconStyle,
        isUnavailable,
        preview,
        stateObject,
    };
}
