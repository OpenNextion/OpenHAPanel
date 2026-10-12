import type { THassScreenAction } from '@/types/screenConfig';

export interface IHassEntitySupportConfig {
    deviceClasses?: readonly string[];
    previewSupported?: boolean;
    longPressSupported?: boolean;
    iconTapDefault?: THassScreenAction;
}

/** Entity capabilities currently supported by the screen renderer. */
export const HASS_ENTITY_SUPPORT_CONFIG: Readonly<Record<string, IHassEntitySupportConfig>> = Object.freeze({
    switch: {},
    light: {},
    cover: {},
    scene: {},
    button: { iconTapDefault: 'toggle' },
    sensor: { deviceClasses: ['temperature', 'humidity'] },
    binary_sensor: {},
    climate: { previewSupported: false, longPressSupported: false },
    event: { previewSupported: false, longPressSupported: false },
    siren: { previewSupported: false, longPressSupported: false },
    fan: { previewSupported: false, longPressSupported: false },
});

/** Entity categories that should not be offered by the dashboard entity picker. */
export const HASS_ENTITY_PICKER_EXCLUDED_CATEGORIES = Object.freeze(['config', 'diagnostic'] as const);
const excludedEntityCategories = new Set<string>(HASS_ENTITY_PICKER_EXCLUDED_CATEGORIES);

function matchesSupportConfig(entityId: string, deviceClass?: unknown) {
    const domain = entityId.split('.', 1)[0];
    const config = HASS_ENTITY_SUPPORT_CONFIG[domain];
    if (!config) return false;
    if (!config.deviceClasses?.length) return true;
    return typeof deviceClass === 'string' && config.deviceClasses.includes(deviceClass);
}

export function isHassEntityVisibleInPicker(
    entityId: string,
    deviceClass: unknown,
    entityCategory: unknown,
    entityCategories: Record<string, string> = {},
) {
    if (!matchesSupportConfig(entityId, deviceClass)) return false;
    const decodedCategory =
        typeof entityCategory === 'number' || (typeof entityCategory === 'string' && /^\d+$/.test(entityCategory))
            ? entityCategories[String(entityCategory)]
            : entityCategory;
    return typeof decodedCategory !== 'string' || !excludedEntityCategories.has(decodedCategory.toLowerCase());
}

export function isHassEntitySupported(entityId: string, deviceClass?: unknown) {
    const domain = entityId.split('.', 1)[0];
    const config = HASS_ENTITY_SUPPORT_CONFIG[domain];
    return Boolean(config && config.previewSupported !== false && matchesSupportConfig(entityId, deviceClass));
}

export function isHassEntityLongPressSupported(entityId: string, deviceClass?: unknown) {
    const domain = entityId.split('.', 1)[0];
    const config = HASS_ENTITY_SUPPORT_CONFIG[domain];
    return Boolean(config && config.longPressSupported !== false && matchesSupportConfig(entityId, deviceClass));
}
