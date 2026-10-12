import { defineConfig, loadEnv } from 'vite';
import { svelte } from '@sveltejs/vite-plugin-svelte';
import { viteSingleFile } from 'vite-plugin-singlefile';
import viteCompression from 'vite-plugin-compression';
import { fileURLToPath, URL } from 'node:url';
import { createHtmlPlugin } from 'vite-plugin-html';
import terser from '@rollup/plugin-terser';

import fs from 'node:fs';
import path from 'node:path';

import { version } from './package.json';
import dayjs from 'dayjs';

// https://vite.dev/config/
export default defineConfig(({ mode }) => {
    const env = loadEnv(mode, process.cwd());
    const vconsole = env.VITE_VCONSOLE !== '1';
    // wiki: https://zh.wikipedia.org/wiki/Favicon
    const favicon = `data:image/x-icon;base64,${fs.readFileSync(path.join(__dirname, './src/assets/favicon.ico'), 'base64')}`;
    return {
        plugins: [
            svelte({
                compilerOptions: {
                    dev: false,
                },
            }),
            viteSingleFile({
                /** 删除vite携带的 module loader，节省包大小 */
                removeViteModuleLoader: true,
            }),
            viteCompression(),
            createHtmlPlugin({
                inject: {
                    data: {
                        favicon,
                        version,
                        buildTime: dayjs().format('YYYY/MM/DD HH:mm:ss'),
                    },
                },
            }),
        ],
        server: {
            host: true,
            port: Number(env.VITE_LOCAL_PORT) || 5173,
            open: true,
        },
        build: {
            sourcemap: false,
            /** 选择压缩方式 */
            minify: 'esbuild',
            target: 'esnext',
            /** 禁用静态文件合并限制 */
            assetsInlineLimit: 100000000,
            /** 禁用文件大小限制 */
            chunkSizeWarningLimit: 100000000,
            /** 禁用css代码分割，所有css代码会打包到一起 */
            cssCodeSplit: false,
            /** 禁用压缩大小报告，可以加快构建速度 */
            reportCompressedSize: false,
            rollupOptions: {
                external: [vconsole ? 'vconsole' : ''],
                output: {
                    inlineDynamicImports: true,
                },
                plugins: [
                    terser({
                        format: {
                            comments: false, // 去掉所有注释
                        },
                        compress: {
                            drop_console: vconsole, // 移除 console
                            drop_debugger: vconsole, // 移除 debugger
                        },
                    }),
                ],
            },
        },
        resolve: {
            alias: {
                '@': fileURLToPath(new URL('./src', import.meta.url)),
                '~bootstrap': path.resolve(__dirname, './node_modules/bootstrap'),
            },
        },
    };
});
