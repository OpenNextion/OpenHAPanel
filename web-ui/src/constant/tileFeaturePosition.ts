import type { IHassScreenConfigCard } from '@/types/screenConfig';

export type TTileFeaturePosition = 'bottom' | 'inline';

/** An omitted card-level position uses the default bottom layout. */
export function getTileFeaturePosition(position?: unknown): TTileFeaturePosition {
    return position === 'inline' ? 'inline' : 'bottom';
}

/** Move positions saved on individual features by older versions to the card. */
export function normalizeTileFeaturePosition(data: IHassScreenConfigCard['data']): IHassScreenConfigCard['data'] {
    const legacyPosition = data.features?.map((feature) => feature.data?.features_position)
        .find((value) => value === 'inline' || value === 'bottom');
    const position = data.features_position ?? legacyPosition;
    const features = data.features?.map((feature) => {
        const { features_position: _legacyPosition, ...featureData } = feature.data ?? {};
        return { ...feature, data: featureData };
    });
    const { features_position: _previousPosition, features: _previousFeatures, ...cardData } = data;
    return {
        ...cardData,
        ...(features ? { features } : {}),
        ...(position === 'inline' ? { features_position: 'inline' as const } : {}),
    };
}
