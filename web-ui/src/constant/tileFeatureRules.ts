import { isHassEntitySupported } from '@/constant/hassEntity';
import type { IHassStateEntry } from '@/types/haRegistry';
import type { IHassScreenConfigFeature } from '@/types/screenConfig';

export type THassTileFeatureType =
    | 'toggle'
    | 'button'
    | 'light-brightness'
    | 'light-color-temp'
    | 'light-color-favorites'
    | 'light-effect'
    | 'cover-open-close'
    | 'cover-position'
    | 'cover-position-favorite'
    | 'cover-tilt'
    | 'cover-tilt-position'
    | 'cover-tilt-favorite'
    | 'trend-graph'
    | 'bar-gauge'
    | 'climate-hvac-modes'
    | 'climate-preset-modes'
    | 'climate-fan-modes'
    | 'climate-swing-modes'
    | 'climate-swing-horizontal-modes'
    | 'target-temperature'
    | 'target-humidity'
    | 'fan-speed'
    | 'fan-preset-modes'
    | 'fan-direction'
    | 'fan-oscillate';

export interface ITileFeatureSuggestion {
    id: string;
    labelKey?: string;
    features: IHassScreenConfigFeature[];
}

export type TTileFeatureEditorDefinition = {
    type: THassTileFeatureType;
    labelKey: string;
    editor: 'checkbox';
};

// Only features with a Web renderer may be offered in the picker. Capability
// detection below can already recognize other HA features independently.
const RENDERABLE_TILE_FEATURE_TYPES = new Set<THassTileFeatureType>(['toggle', 'button']);
const FEATURE_EDITORS: readonly TTileFeatureEditorDefinition[] = [
    { type: 'toggle', labelKey: 'page.screen.actionToggle', editor: 'checkbox' },
    { type: 'button', labelKey: 'page.screen.actionButton', editor: 'checkbox' },
];

type TAttributes = IHassStateEntry['attributes'];

// HA supported_features flags are domain-specific; the same bit has different meanings.
const COVER = { OPEN: 1, CLOSE: 2, POSITION: 4, STOP: 8, OPEN_TILT: 16, CLOSE_TILT: 32, STOP_TILT: 64, TILT_POSITION: 128 } as const;
const FAN = { SPEED: 1, OSCILLATE: 2, DIRECTION: 4, PRESET: 8 } as const;
const CLIMATE = { TEMPERATURE: 1, TEMPERATURE_RANGE: 2, HUMIDITY: 4, FAN: 8, PRESET: 16, SWING: 32, HORIZONTAL_SWING: 512 } as const;
const LIGHT = { EFFECT: 4 } as const;
const LIGHT_COLOR_MODES = new Set(['hs', 'xy', 'rgb', 'rgbw', 'rgbww']);
const LIGHT_BRIGHTNESS_MODES = new Set(['brightness', 'color_temp', 'hs', 'xy', 'rgb', 'rgbw', 'rgbww', 'white']);
const NON_NUMERIC_SENSOR_CLASSES = new Set(['date', 'enum', 'timestamp']);
const NUMERIC_SENSOR_STATE_CLASSES = new Set(['measurement', 'total', 'total_increasing']);

function featureFlags(attributes: TAttributes) {
    const raw = attributes?.supported_features;
    const value = typeof raw === 'number' || (typeof raw === 'string' && raw.trim()) ? Number(raw) : NaN;
    return Number.isSafeInteger(value) && value >= 0 ? value : 0;
}

function hasFlag(flags: number, flag: number) {
    return (flags & flag) !== 0;
}

function hasStringList(attributes: TAttributes, key: string) {
    return Array.isArray(attributes?.[key]) && attributes[key].some((item) => typeof item === 'string' && item.trim());
}

function stringList(attributes: TAttributes, key: string) {
    const value = attributes?.[key];
    return Array.isArray(value) ? value.filter((item): item is string => typeof item === 'string') : [];
}

function isNumericSensor(state: IHassStateEntry) {
    const attributes = state.attributes;
    const deviceClass = attributes?.device_class;
    if (typeof deviceClass === 'string' && NON_NUMERIC_SENSOR_CLASSES.has(deviceClass)) return false;
    const raw = state.state.trim();
    if (raw && Number.isFinite(Number(raw))) return true;
    if (typeof attributes?.state_class === 'string' && NUMERIC_SENSOR_STATE_CLASSES.has(attributes.state_class)) return true;
    // Retain controls during a temporary unavailable state when the device class is numeric.
    return typeof deviceClass === 'string' && deviceClass !== '';
}

/** HA entity capabilities, independent of which feature controls this Web UI can render today. */
export function getAvailableTileFeatureTypes(entityId: string, state?: IHassStateEntry): THassTileFeatureType[] {
    if (!state || state.entity_id !== entityId) return [];
    const domain = entityId.split('.', 1)[0];
    const attributes = state?.attributes;
    if (!isHassEntitySupported(entityId, attributes?.device_class)) return [];

    const flags = featureFlags(attributes);
    switch (domain) {
        case 'switch':
        case 'siren':
            return ['toggle'];
        case 'scene':
        case 'button':
            return ['button'];
        case 'light': {
            const modes = stringList(attributes, 'supported_color_modes');
            const color = modes.some((mode) => LIGHT_COLOR_MODES.has(mode));
            const temperature = modes.includes('color_temp');
            const brightness = modes.some((mode) => LIGHT_BRIGHTNESS_MODES.has(mode))
                || (!modes.length && typeof attributes?.brightness === 'number');
            return [
                ...(brightness ? ['light-brightness' as const] : []),
                ...(temperature ? ['light-color-temp' as const] : []),
                // Favorite entries live outside the state object; this only checks the underlying color capability.
                ...(color || temperature ? ['light-color-favorites' as const] : []),
                ...(hasFlag(flags, LIGHT.EFFECT) && hasStringList(attributes, 'effect_list') ? ['light-effect' as const] : []),
            ];
        }
        case 'cover':
            return [
                ...(flags & (COVER.OPEN | COVER.CLOSE | COVER.STOP) ? ['cover-open-close' as const] : []),
                // As with lights, favorites still need their separate saved data before a UI can show them.
                ...(hasFlag(flags, COVER.POSITION) ? ['cover-position' as const, 'cover-position-favorite' as const] : []),
                ...(flags & (COVER.OPEN_TILT | COVER.CLOSE_TILT | COVER.STOP_TILT) ? ['cover-tilt' as const] : []),
                ...(hasFlag(flags, COVER.TILT_POSITION) ? ['cover-tilt-position' as const, 'cover-tilt-favorite' as const] : []),
            ];
        case 'sensor':
            if (!isNumericSensor(state)) return [];
            // A percentage unit affects HA's recommendation, not the gauge's availability.
            return ['trend-graph', 'bar-gauge'];
        case 'climate':
            return [
                ...(hasStringList(attributes, 'hvac_modes') ? ['climate-hvac-modes' as const] : []),
                ...(hasFlag(flags, CLIMATE.PRESET) && hasStringList(attributes, 'preset_modes') ? ['climate-preset-modes' as const] : []),
                ...(hasFlag(flags, CLIMATE.FAN) && hasStringList(attributes, 'fan_modes') ? ['climate-fan-modes' as const] : []),
                ...(hasFlag(flags, CLIMATE.SWING) && hasStringList(attributes, 'swing_modes') ? ['climate-swing-modes' as const] : []),
                ...(hasFlag(flags, CLIMATE.HORIZONTAL_SWING) && hasStringList(attributes, 'swing_horizontal_modes') ? ['climate-swing-horizontal-modes' as const] : []),
                ...(flags & (CLIMATE.TEMPERATURE | CLIMATE.TEMPERATURE_RANGE) ? ['target-temperature' as const] : []),
                ...(hasFlag(flags, CLIMATE.HUMIDITY) ? ['target-humidity' as const] : []),
            ];
        case 'fan':
            return [
                ...(hasFlag(flags, FAN.SPEED) ? ['fan-speed' as const] : []),
                ...(hasFlag(flags, FAN.PRESET) && hasStringList(attributes, 'preset_modes') ? ['fan-preset-modes' as const] : []),
                ...(hasFlag(flags, FAN.DIRECTION) ? ['fan-direction' as const] : []),
                ...(hasFlag(flags, FAN.OSCILLATE) ? ['fan-oscillate' as const] : []),
            ];
        default:
            // binary_sensor, event and siren currently have only the base Tile.
            return [];
    }
}

export function getSupportedTileFeatures(entityId: string, state: IHassStateEntry | undefined, features: IHassScreenConfigFeature[] = []) {
    const available = new Set(getAvailableTileFeatureTypes(entityId, state));
    return features.filter((feature) => available.has(feature.type as THassTileFeatureType));
}

/** Keep unsupported UI controls out of the preview until a renderer is added. */
export function getRenderableTileFeatures(entityId: string, state: IHassStateEntry | undefined, features: IHassScreenConfigFeature[] = []) {
    return getSupportedTileFeatures(entityId, state, features)
        .filter((feature) => RENDERABLE_TILE_FEATURE_TYPES.has(feature.type as THassTileFeatureType));
}

export function supportsTileFeature(entityId: string, state: IHassStateEntry | undefined, type: THassTileFeatureType) {
    return getAvailableTileFeatureTypes(entityId, state).includes(type);
}

export function getEditableTileFeatureDefinitions(entityId: string, state?: IHassStateEntry): TTileFeatureEditorDefinition[] {
    const available = new Set(getAvailableTileFeatureTypes(entityId, state));
    return FEATURE_EDITORS.filter(({ type }) => available.has(type) && RENDERABLE_TILE_FEATURE_TYPES.has(type));
}

export function getTileFeatureSuggestions(entityId: string, state?: IHassStateEntry): ITileFeatureSuggestion[] {
    return [
        { id: 'tile', features: [] },
        ...getEditableTileFeatureDefinitions(entityId, state)
            .map(({ type, labelKey }) => ({ id: type, labelKey, features: [{ type, data: {} }] })),
    ];
}

