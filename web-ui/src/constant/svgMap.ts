const dataURI2Html = (svgDataURL: string) => decodeURIComponent(svgDataURL.replace('data:image/svg+xml,', ''));
const svgModules = import.meta.glob('@/assets/svg/*.svg', { eager: true });
const createDom = (html: string) => {
    const div = document.createElement('div');
    div.innerHTML = html;
    return div.children[0];
};

export interface SvgConfig {
    svgReferenceName: string;
    html: string;
    width: string | number;
    height: string | number;
    viewBox: string;
}

/** svg Map */
export const svgMap: Map<string, SvgConfig> = new Map(
    Object.entries(svgModules).map(([filepath, module]) => {
        const svgReferenceName = filepath.substring(filepath.lastIndexOf('\/') + 1, filepath.length);
        const { default: svgDataURL } = module as { default: string };
        const svgHtml = dataURI2Html(svgDataURL);
        const svgDom = createDom(svgHtml);
        const svgWidth = svgDom.getAttribute('width');
        const svgHeight = svgDom.getAttribute('height');
        const svgConfig = {
            svgReferenceName,
            html: svgHtml.replace('<svg', `<svg id=${svgReferenceName}`),
            width: svgWidth ? svgWidth : 16,
            height: svgHeight ? svgHeight : 16,
            viewBox: svgDom.getAttribute('viewBox') ?? `0 0 ${svgWidth} ${svgHeight}`,
        };
        return [svgReferenceName, svgConfig];
    }),
);
