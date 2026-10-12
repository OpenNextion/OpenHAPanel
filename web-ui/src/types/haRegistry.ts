export interface IHassEntityRegistryDisplayEntry {
    ei?: string;
    entity_id?: string;
    di?: string | null;
    device_id?: string | null;
    ai?: string | null;
    area_id?: string | null;
    en?: string | null;
    ic?: string | null;
    ne?: string | null;
    or?: string | null;
    name?: string | null;
    original_name?: string | null;
    icon?: string | null;
    ec?: number | string | null;
    entity_category?: string | null;
    [key: string]: unknown;
}

export interface IHassEntityRegistryDisplayResult {
    entity_categories?: Record<string, string>;
    entities?: IHassEntityRegistryDisplayEntry[];
}

export type THassEntityRegistryDisplayResult = IHassEntityRegistryDisplayResult | IHassEntityRegistryDisplayEntry[];

export interface IHassDeviceRegistryEntry {
    id: string;
    area_id?: string | null;
    entry_type?: string | null;
    parent_device_id?: string | null;
    name?: string | null;
    name_by_user?: string | null;
    manufacturer?: string | null;
    model?: string | null;
    [key: string]: unknown;
}

export interface IHassStateEntry {
    entity_id: string;
    state: string;
    attributes?: {
        friendly_name?: string | null;
        icon?: string | null;
        [key: string]: unknown;
    };
    [key: string]: unknown;
}

export interface IHassAreaRegistryEntry {
    area_id: string;
    name: string;
    floor_id?: string | null;
    icon?: string | null;
    [key: string]: unknown;
}

export interface IHassFloorRegistryEntry {
    floor_id: string;
    name: string;
    level?: number | null;
    icon?: string | null;
    [key: string]: unknown;
}

export interface IHassRegistryTreeEntity {
    id: string;
    name: string;
    deviceId: string | null;
    areaId: string | null;
    raw: IHassEntityRegistryDisplayEntry;
}

export interface IHassRegistryTreeDevice {
    id: string;
    name: string;
    areaId: string | null;
    entities: IHassRegistryTreeEntity[];
    children: IHassRegistryTreeDevice[];
    raw: IHassDeviceRegistryEntry;
}

export interface IHassRegistryTreeDomain {
    domain: string;
    name: string;
    entities: IHassRegistryTreeEntity[];
}

export interface IHassRegistryTreeArea {
    id: string;
    name: string;
    floorId: string | null;
    devices: IHassRegistryTreeDevice[];
    entities: IHassRegistryTreeEntity[];
    raw: IHassAreaRegistryEntry;
}

export interface IHassRegistryTreeFloor {
    id: string;
    name: string;
    areas: IHassRegistryTreeArea[];
    raw: IHassFloorRegistryEntry;
}

export interface IHassRegistryTree {
    floors: IHassRegistryTreeFloor[];
    otherAreas: IHassRegistryTreeArea[];
    unassignedDevices: IHassRegistryTreeDevice[];
    unassignedServices: IHassRegistryTreeDevice[];
    unassignedEntityDomains: IHassRegistryTreeDomain[];
    unassignedHelperDomains: IHassRegistryTreeDomain[];
}
