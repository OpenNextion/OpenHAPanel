export interface IHassScreenConfigNotify {
    screens: IHassScreenConfigItem[];
}
export interface IHassScreenConfigItem {
    index: number;
    title: string;
    icon: string;
    type: string;
    columns: number;
    badges?: IHassScreenConfigBadge[];
    cards?: IHassScreenConfigCard[];
}
``;

export interface IHassScreenConfigBadge {
    type: string;
    data: {
        entity: string;
        name: string;
        actionConfig: IHassScreenActionConfig;
    };
}

export interface IHassScreenConfigCard {
    type: string;
    data: {
        entity: string;
        full_width: boolean;
        actionConfig: IHassScreenActionConfig;
        features?: IHassScreenConfigFeature[];
        features_position?: 'bottom' | 'inline';
    };
}

export interface IHassScreenConfigFeature {
    type: string;
    data: {
        [key: string]: unknown;
    };
    /** Feature-specific options (for example, a slider's range or style). */
    [key: string]: unknown;
}

export type THassScreenAction = 'more-info' | 'toggle' | 'nothing';

export interface IHassScreenActionItem {
    action: THassScreenAction;
    [key: string]: unknown;
}

export interface IHassScreenActionConfig {
    tap_action?: IHassScreenActionItem;
    hold_action?: IHassScreenActionItem;
    icon_tap_action?: IHassScreenActionItem;
}
