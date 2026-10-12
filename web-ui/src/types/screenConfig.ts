export interface IHassScreenConfigNotify {
    screens: IHassScreenConfigItem[];
}
export interface IHassScreenConfigItem {
    index: number;
    title: string;
    icon: string;
    type: string;
    badges?: IHassScreenConfigBadge[];
    cards?: IHassScreenConfigCard[];
    features?: IHassScreenConfigFeature[];
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
    };
}

export interface IHassScreenConfigFeature {
    type: string;
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
