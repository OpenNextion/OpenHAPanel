import 'bootstrap/dist/css/bootstrap.min.css';
import './styles.css';
import './assets/i18n';
import { ENV_VCONSOLE } from './constant';
import dayjs from 'dayjs';
import utc from 'dayjs/plugin/utc';

import { mount } from 'svelte';
import App from './App.svelte';
import { initHomeAssistantWebSocket } from '@/wsForHA';

dayjs.extend(utc);


async function initVconsole() {
    const { default: Vconsole } = await import('vconsole');
    new Vconsole();
    console.log('vconsole init!');
}

function initApp() {
    return mount(App, {
        target: document.getElementById('app')!,
    });
}

async function main() {
    ENV_VCONSOLE === '1' && (await initVconsole());
    initApp();
    initHomeAssistantWebSocket();
}

main();
