export function prepareDraggedScreenCard(element?: HTMLElement) {
    if (!element) return;
    element.classList.add('screen-card-dragged-preview');
    element.querySelector<HTMLElement>('.screen-card')?.classList.add('screen-card-dragged-content-preview');

    // Drag previews are moved to document.body and lose the page's sizing variables.
    const page = document.querySelector<HTMLElement>('.screen-config-page');
    if (!page) return;
    const pageStyles = getComputedStyle(page);
    for (const variable of [
        '--screen-preview-width',
        '--screen-preview-height',
        '--screen-card-width',
        '--screen-card-height',
        '--screen-grid-columns',
    ]) {
        const value = pageStyles.getPropertyValue(variable).trim();
        if (value) element.style.setProperty(variable, value);
    }
}

export function prepareDraggedBadge(element?: HTMLElement) {
    if (!element) return;
    element.classList.add('screen-badge-dragged-preview');
    element.style.setProperty('width', 'max-content', 'important');
    const row = element.querySelector<HTMLElement>('.hass-entity-row');
    if (!row) return;
    row.classList.add('screen-item-dragged-content-preview');
    row.style.transform = 'scale(1.2)';
}

export function prepareDraggedEntity(element?: HTMLElement) {
    if (!element) return;
    element.classList.add('screen-entity-dragged-preview');
    element.style.width = `${element.getBoundingClientRect().width}px`;
    const row = element.querySelector<HTMLElement>('.hass-entity-row');
    if (!row) return;
    row.style.width = '100%';
    row.style.maxWidth = '100%';
    row.classList.add('screen-item-dragged-content-preview');
    row.style.transform = 'scale(1.1)';
}
