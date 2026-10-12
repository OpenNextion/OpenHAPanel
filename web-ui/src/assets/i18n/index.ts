import { addMessages, init, locale } from 'svelte-i18n';
import en from './en.json';
import zhCN from './zh-cn.json';

export type AppLocale = 'zh-CN' | 'zh' | 'en';

addMessages('zh-CN', zhCN);
addMessages('zh', zhCN);
addMessages('en', en);

init({
  fallbackLocale: 'en',
  initialLocale: 'en',
});

locale.subscribe((value) => {
  if (!value) return;
  document.documentElement.lang = value;
});
