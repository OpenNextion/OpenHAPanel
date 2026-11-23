import type { IHassStateEntry } from '@/types/haRegistry';
import type { IHassScreenConfigFeature } from '@/types/screenConfig';
import type { THassTileFeatureType } from './tileFeatureRules';

type Attributes = NonNullable<IHassStateEntry['attributes']>;
type Option = { value: string; label: string; icon?: string };
type Favorite = { id: string; label: string; color?: string; position?: number };
type Action = { id: string; label: string; icon: string; disabled?: boolean };

function numeric(value: unknown): number | undefined {
    const number = typeof value === 'number' ? value : typeof value === 'string' && value.trim() ? Number(value) : NaN;
    return Number.isFinite(number) ? number : undefined;
}

function clamp(value: number, min: number, max: number) {
    return Math.min(max, Math.max(min, value));
}

function strings(value: unknown): string[] {
    return Array.isArray(value) ? value.filter((item): item is string => typeof item === 'string' && item.trim().length > 0) : [];
}

function attr(state: IHassStateEntry | undefined): Attributes {
    return state?.attributes ?? {};
}

export function getSliderPresentation(type: THassTileFeatureType, state?: IHassStateEntry) {
    const attributes = attr(state);
    if (type === 'light-brightness') {
        const value = Math.round((numeric(attributes.brightness) ?? 0) / 255 * 100);
        return { value: clamp(value, 0, 100), min: 0, max: 100, step: 1, valueText: `${clamp(value, 0, 100)}%` };
    }
    if (type === 'light-color-temp') {
        const minKelvin = numeric(attributes.min_color_temp_kelvin);
        const maxKelvin = numeric(attributes.max_color_temp_kelvin);
        if (minKelvin !== undefined && maxKelvin !== undefined && minKelvin < maxKelvin) {
            const value = clamp(numeric(attributes.color_temp_kelvin) ?? minKelvin, minKelvin, maxKelvin);
            return { value, min: minKelvin, max: maxKelvin, step: 1, valueText: `${Math.round(value)} K` };
        }
        const minMireds = numeric(attributes.min_mireds) ?? 153;
        const maxMireds = Math.max(minMireds + 1, numeric(attributes.max_mireds) ?? 500);
        // The visual track runs warm to cool, including for older mired-only lights.
        const min = 1_000_000 / maxMireds;
        const max = 1_000_000 / minMireds;
        const value = clamp(1_000_000 / (numeric(attributes.color_temp) ?? maxMireds), min, max);
        return { value, min, max, step: 1, valueText: `${Math.round(value)} K` };
    }
    const key = type === 'cover-position' ? 'current_position'
        : type === 'cover-tilt-position' ? 'current_tilt_position' : 'percentage';
    const inactiveCover = type === 'cover-position' && ['closed', 'off', 'unavailable', 'unknown'].includes(state?.state ?? '');
    const inactiveFan = type === 'fan-speed' && state?.state !== 'on';
    const value = clamp(inactiveCover || inactiveFan ? 0 : numeric(attributes[key]) ?? 0, 0, 100);
    const percentageStep = type === 'fan-speed' ? numeric(attributes.percentage_step) : undefined;
    return { value, min: 0, max: 100, step: percentageStep !== undefined && percentageStep > 0 ? percentageStep : 1, valueText: `${Math.round(value)}%` };
}

export function getFanSpeedChoices(state?: IHassStateEntry) {
    const attributes = attr(state);
    const percentageStep = numeric(attributes.percentage_step);
    if (percentageStep === undefined || percentageStep <= 18) return undefined;

    const speedCount = Math.round(100 / percentageStep);
    if (speedCount < 1 || speedCount > 6) return undefined;

    const namedSpeeds: Record<number, { label: string; icon: string }[]> = {
        1: [{ label: 'On', icon: 'mdi:fan' }],
        2: [{ label: 'Low', icon: 'mdi:fan-speed-1' }, { label: 'High', icon: 'mdi:fan-speed-2' }],
        3: [{ label: 'Low', icon: 'mdi:fan-speed-1' }, { label: 'Medium', icon: 'mdi:fan-speed-2' }, { label: 'High', icon: 'mdi:fan-speed-3' }],
    };

    const percentage = numeric(attributes.percentage);
    const selectedSpeed = state?.state === 'off' ? 0
        : percentage === undefined ? undefined : clamp(Math.round(percentage / percentageStep), 0, speedCount);
    return {
        value: selectedSpeed === undefined ? undefined : String(selectedSpeed),
        options: [
            { value: '0', label: 'Off', icon: 'mdi:fan-off' },
            ...Array.from({ length: speedCount }, (_, index) => ({
                value: String(index + 1),
                label: percentageStep >= 33 ? namedSpeeds[speedCount]?.[index].label ?? String(index + 1) : String(index + 1),
                icon: percentageStep >= 33 ? namedSpeeds[speedCount]?.[index].icon : undefined,
            })),
        ] satisfies Option[],
    };
}

export function getClimateTargetTemperature(state?: IHassStateEntry) {
    const attributes = attr(state);
    const min = numeric(attributes.min_temp) ?? 7;
    const max = Math.max(min, numeric(attributes.max_temp) ?? 35);
    const step = Math.max(0.1, numeric(attributes.target_temp_step) ?? 0.5);
    const unit = typeof attributes.temperature_unit === 'string' ? attributes.temperature_unit : '°C';
    const supportedFeatures = numeric(attributes.supported_features) ?? 0;
    const value = numeric(attributes.temperature);
    if ((supportedFeatures & 1) !== 0 && value !== undefined) return { targets: [{ key: 'temperature', value: clamp(value, min, max), min, max, step, unit }] };

    const low = numeric(attributes.target_temp_low);
    const high = numeric(attributes.target_temp_high);
    if (low !== undefined && high !== undefined && ((supportedFeatures & 2) !== 0 || value === undefined)) {
        return { targets: [
            { key: 'target_temp_low', value: clamp(low, min, max), min, max: Math.min(max, high), step, unit },
            { key: 'target_temp_high', value: clamp(high, min, max), min: Math.max(min, low), max, step, unit },
        ] };
    }
    // Keep the target control visible when the advertised feature has no usable value.
    return { targets: [{ key: 'temperature', value: undefined, min, max, step, unit }] };
}

export function getClimateTargetHumidity(state?: IHassStateEntry) {
    const attributes = attr(state);
    const min = numeric(attributes.min_humidity) ?? 30;
    const max = Math.max(min, numeric(attributes.max_humidity) ?? 99);
    const step = Math.max(1, numeric(attributes.target_humidity_step) ?? 1);
    const value = clamp(numeric(attributes.humidity) ?? min, min, max);
    return { value, min, max, step, valueText: `${Math.round(value)}%` };
}

const OPTION_ATTRIBUTES: Partial<Record<THassTileFeatureType, { list: string; current: string }>> = {
    'light-effect': { list: 'effect_list', current: 'effect' },
    'climate-hvac-modes': { list: 'hvac_modes', current: 'state' },
    'climate-preset-modes': { list: 'preset_modes', current: 'preset_mode' },
    'climate-fan-modes': { list: 'fan_modes', current: 'fan_mode' },
    'climate-swing-modes': { list: 'swing_modes', current: 'swing_mode' },
    'climate-swing-horizontal-modes': { list: 'swing_horizontal_modes', current: 'swing_horizontal_mode' },
    'fan-preset-modes': { list: 'preset_modes', current: 'preset_mode' },
};

const HVAC_MODE_ORDER = ['off', 'fan_only', 'dry', 'cool', 'heat', 'heat_cool', 'auto'];
const HVAC_MODE_ICONS: Record<string, string> = {
    off: 'mdi:power', fan_only: 'mdi:fan', dry: 'mdi:water-percent', cool: 'mdi:snowflake',
    heat: 'mdi:fire', heat_cool: 'mdi:sun-snowflake-variant', auto: 'mdi:thermostat-auto',
};
const CLIMATE_PRESET_ICONS: Record<string, string> = {
    none: 'mdi:circle-medium', activity: 'mdi:motion-sensor', away: 'mdi:account-arrow-right',
    boost: 'mdi:rocket-launch', comfort: 'mdi:sofa', eco: 'mdi:leaf', home: 'mdi:home', sleep: 'mdi:bed',
};
const CLIMATE_FAN_ICONS: Record<string, string> = {
    auto: 'mdi:fan-auto', diffuse: 'mdi:weather-windy', high: 'mdi:speedometer',
    low: 'mdi:speedometer-slow', medium: 'mdi:speedometer-medium', middle: 'mdi:speedometer-medium',
    off: 'mdi:fan-off', on: 'mdi:fan',
};
const CLIMATE_SWING_ICONS: Record<string, string> = {
    off: 'mdi:arrow-oscillating-off', on: 'mdi:arrow-oscillating', both: 'mdi:arrow-all',
    horizontal: 'mdi:arrow-left-right', vertical: 'mdi:arrow-up-down',
};
const CLIMATE_HORIZONTAL_SWING_ICONS: Record<string, string> = {
    off: 'mdi:arrow-oscillating-off', on: 'mdi:arrow-expand-horizontal',
};

function climateModeIcon(type: THassTileFeatureType, value: string): string | undefined {
    const icons = type === 'climate-hvac-modes' ? HVAC_MODE_ICONS
        : type === 'climate-preset-modes' ? CLIMATE_PRESET_ICONS
        : type === 'climate-fan-modes' ? CLIMATE_FAN_ICONS
        : type === 'climate-swing-modes' ? CLIMATE_SWING_ICONS
        : type === 'climate-swing-horizontal-modes' ? CLIMATE_HORIZONTAL_SWING_ICONS : undefined;
    return icons?.[value] ?? (icons ? 'mdi:circle-medium' : undefined);
}

export function getOptionPresentation(type: THassTileFeatureType, state?: IHassStateEntry): { value?: string; options: Option[]; variant: 'segmented' | 'select' | 'hvac' } {
    const attributes = attr(state);
    if (type === 'fan-direction') {
        return { value: typeof attributes.direction === 'string' ? attributes.direction : undefined,
            options: [{ value: 'forward', label: 'Forward', icon: 'mdi:rotate-right' }, { value: 'reverse', label: 'Reverse', icon: 'mdi:rotate-left' }], variant: 'segmented' };
    }
    const keys = OPTION_ATTRIBUTES[type];
    if (!keys) return { options: [], variant: 'segmented' };
    const isClimate = type.startsWith('climate-');
    const options = strings(attributes[keys.list]).map((value) => ({
        value,
        label: isClimate ? value.replace(/_/g, ' ').replace(/^./, (letter) => letter.toUpperCase()) : value.replace(/_/g, ' '),
        icon: climateModeIcon(type, value),
    }));
    if (type === 'climate-hvac-modes') options.sort((a, b) => HVAC_MODE_ORDER.indexOf(a.value) - HVAC_MODE_ORDER.indexOf(b.value));
    const current = keys.current === 'state' ? state?.state : attributes[keys.current];
    return { value: typeof current === 'string' ? current : undefined, options,
        variant: type === 'climate-hvac-modes' ? 'hvac'
            : isClimate || type === 'fan-preset-modes' || type === 'light-effect' || options.length > 4 ? 'select' : 'segmented' };
}

const COVER_ACTION_FLAGS = {
    'cover-open-close': [
        { id: 'open', label: 'Open', icon: 'mdi:arrow-up', bit: 1 },
        { id: 'stop', label: 'Stop', icon: 'mdi:stop', bit: 8 },
        { id: 'close', label: 'Close', icon: 'mdi:arrow-down', bit: 2 },
    ],
    'cover-tilt': [
        { id: 'open_tilt', label: 'Open tilt', icon: 'mdi:arrow-up', bit: 16 },
        { id: 'stop_tilt', label: 'Stop tilt', icon: 'mdi:stop', bit: 64 },
        { id: 'close_tilt', label: 'Close tilt', icon: 'mdi:arrow-down', bit: 32 },
    ],
} as const;

export function getCoverActions(type: 'cover-open-close' | 'cover-tilt', state?: IHassStateEntry): Action[] {
    const attributes = attr(state);
    const flags = numeric(attributes.supported_features) ?? 0;
    const horizontal = type === 'cover-open-close' && ['awning', 'door', 'gate', 'curtain'].includes(String(attributes.device_class ?? ''));
    const unavailable = state?.state === 'unavailable';
    const position = numeric(attributes[type === 'cover-tilt' ? 'current_tilt_position' : 'current_position']);
    const assumed = attributes.assumed_state === true;
    return COVER_ACTION_FLAGS[type].filter(({ bit }) => (flags & bit) !== 0).map(({ id, label, icon }) => {
        const isOpen = id === 'open' || id === 'open_tilt';
        const isClose = id === 'close' || id === 'close_tilt';
        const actionIcon = horizontal && isOpen ? 'mdi:arrow-expand-horizontal'
            : horizontal && isClose ? 'mdi:arrow-collapse-horizontal' : icon;
        const fullyOpen = position === 100;
        const fullyClosed = type === 'cover-tilt' ? position === 0 : position === undefined ? state?.state === 'closed' : position === 0;
        const disabled = unavailable || (!assumed && (
            (isOpen && (fullyOpen || (type === 'cover-open-close' && state?.state === 'opening'))) ||
            (isClose && (fullyClosed || (type === 'cover-open-close' && state?.state === 'closing')))
        ));
        return { id, label, icon: actionIcon, disabled };
    });
}

const DEFAULT_COVER_FAVORITE_POSITIONS = [0, 25, 75, 100] as const;

export function getHassCoverFavoriteOptions(saved: readonly number[] | undefined): Option[] {
    const unique = new Set<number>();
    for (const entry of saved ?? DEFAULT_COVER_FAVORITE_POSITIONS) {
        const value = numeric(entry);
        if (value !== undefined) unique.add(clamp(value, 0, 100));
    }
    return [...unique].map((value) => ({ value: String(value), label: `${value}%` }));
}

export function getCoverFavoriteCurrentPosition(type: 'cover-position-favorite' | 'cover-tilt-favorite', state?: IHassStateEntry): number | undefined {
    const key = type === 'cover-tilt-favorite' ? 'current_tilt_position' : 'current_position';
    return numeric(attr(state)[key]);
}

export function getLightColor(state?: IHassStateEntry): string {
    const rgb = attr(state).rgb_color;
    if (!Array.isArray(rgb) || rgb.length < 3) return '#ffffff';
    return `#${rgb.slice(0, 3).map((item) => Math.round(clamp(numeric(item) ?? 255, 0, 255)).toString(16).padStart(2, '0')).join('')}`;
}

type RgbColor = [number, number, number];

function rgbHex(rgb: RgbColor): string {
    return `#${rgb.map((channel) => Math.round(clamp(channel, 0, 255)).toString(16).padStart(2, '0')).join('')}`;
}

function rgbChannels(value: unknown, count: number): number[] | undefined {
    if (!Array.isArray(value) || value.length !== count) return undefined;
    const channels = value.map(numeric);
    return channels.every((channel): channel is number => channel !== undefined && channel >= 0 && channel <= 255) ? channels as number[] : undefined;
}

function kelvinRgb(kelvin: number): RgbColor {
    const temperature = clamp(kelvin, 1000, 40000) / 100;
    const red = temperature <= 66 ? 255 : 329.698727446 * (temperature - 60) ** -0.1332047592;
    const green = temperature <= 66
        ? 99.4708025861 * Math.log(temperature) - 161.1195681661
        : 288.1221695283 * (temperature - 60) ** -0.0755148492;
    const blue = temperature >= 66 ? 255 : temperature <= 19 ? 0 : 138.5177312231 * Math.log(temperature - 10) - 305.0447927307;
    return [clamp(red, 0, 255), clamp(green, 0, 255), clamp(blue, 0, 255)];
}

function hsRgb(hue: number, saturation: number): RgbColor {
    const normalizedHue = ((hue % 360) + 360) % 360 / 60;
    const chroma = clamp(saturation, 0, 100) / 100 * 255;
    const secondary = chroma * (1 - Math.abs(normalizedHue % 2 - 1));
    const offset = 255 - chroma;
    const segments: RgbColor[] = [
        [chroma, secondary, 0], [secondary, chroma, 0], [0, chroma, secondary],
        [0, secondary, chroma], [secondary, 0, chroma], [chroma, 0, secondary],
    ];
    const segment = segments[Math.floor(normalizedHue)];
    return [segment[0] + offset, segment[1] + offset, segment[2] + offset];
}

function matchMaxScale(input: number[], output: RgbColor): RgbColor {
    const maximum = Math.max(...output);
    const factor = maximum ? Math.max(...input) / maximum : 0;
    return output.map((channel) => Math.round(channel * factor)) as RgbColor;
}

function favoriteRgb(value: unknown, state?: IHassStateEntry): RgbColor | undefined {
    if (!value || typeof value !== 'object' || Array.isArray(value)) return undefined;
    const favorite = value as Record<string, unknown>;
    const rgb = rgbChannels(favorite.rgb_color, 3);
    if (rgb) return rgb as RgbColor;
    const kelvin = numeric(favorite.color_temp_kelvin);
    if (kelvin !== undefined && kelvin > 0) return kelvinRgb(kelvin);
    const hs = Array.isArray(favorite.hs_color) ? favorite.hs_color.map(numeric) : [];
    if (hs.length === 2 && hs[0] !== undefined && hs[1] !== undefined && hs[1] >= 0 && hs[1] <= 100) return hsRgb(hs[0], hs[1]);
    const rgbw = rgbChannels(favorite.rgbw_color, 4);
    if (rgbw) return matchMaxScale(rgbw, [rgbw[0] + rgbw[3], rgbw[1] + rgbw[3], rgbw[2] + rgbw[3]]);
    const rgbww = rgbChannels(favorite.rgbww_color, 5);
    if (rgbww) {
        const min = numeric(attr(state).min_color_temp_kelvin) ?? 2700;
        const max = numeric(attr(state).max_color_temp_kelvin) ?? 6500;
        const minMireds = Math.floor(1_000_000 / max);
        const maxMireds = Math.floor(1_000_000 / min);
        const whiteTotal = rgbww[3] + rgbww[4];
        const ratio = whiteTotal ? rgbww[4] / whiteTotal : 0.5;
        const mireds = minMireds + ratio * (maxMireds - minMireds);
        const whiteKelvin = mireds ? Math.floor(1_000_000 / mireds) : 0;
        const white = kelvinRgb(whiteKelvin);
        const whiteLevel = Math.max(rgbww[3], rgbww[4]) / 255;
        return matchMaxScale(rgbww, [rgbww[0] + white[0] * whiteLevel, rgbww[1] + white[1] * whiteLevel, rgbww[2] + white[2] * whiteLevel]);
    }
    return undefined;
}

function defaultLightFavorites(state?: IHassStateEntry): unknown[] {
    const attributes = attr(state);
    const modes = strings(attributes.supported_color_modes);
    const supportsTemperature = modes.some((mode) => mode === 'color_temp');
    const supportsColor = modes.some((mode) => ['hs', 'xy', 'rgb', 'rgbw', 'rgbww'].includes(mode));
    const favorites: unknown[] = [];
    if (supportsTemperature) {
        const min = numeric(attributes.min_color_temp_kelvin) ?? 2700;
        const max = numeric(attributes.max_color_temp_kelvin) ?? 6500;
        for (let index = 0; index < 4; index += 1) favorites.push({ color_temp_kelvin: Math.round(min + (max - min) * index / 3) });
    } else if (supportsColor) {
        for (let index = 0; index < 4; index += 1) favorites.push({ rgb_color: kelvinRgb(2000 + (6500 - 2000) * index / 3).map(Math.round) });
    }
    if (supportsColor) {
        favorites.push(
            { rgb_color: [127, 172, 255] },
            { rgb_color: [215, 150, 255] },
            { rgb_color: [255, 158, 243] },
            { rgb_color: [255, 110, 84] },
        );
    }
    return favorites;
}

export function getHassLightFavorites(saved: readonly unknown[] | undefined, state?: IHassStateEntry): Favorite[] {
    // An empty registry list is intentional; only a missing list uses HA's defaults.
    return (saved ?? defaultLightFavorites(state)).flatMap((entry, index): Favorite[] => {
        const rgb = favoriteRgb(entry, state);
        if (!rgb) return [];
        const color = rgbHex(rgb);
        return [{ id: String(index), label: color, color }];
    });
}

export function getTrendPoints(feature: IHassScreenConfigFeature): number[] {
    const raw = feature.data?.points;
    return Array.isArray(raw) ? raw.map(numeric).filter((value): value is number => value !== undefined).slice(-100) : [];
}
