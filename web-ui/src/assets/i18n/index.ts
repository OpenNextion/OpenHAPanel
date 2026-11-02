import { addMessages, init, locale } from 'svelte-i18n';
import type ELanguage from '@/types/language';
import en from './en.json';

export type AppLocale = ELanguage;

addMessages('en', en);

init({
  fallbackLocale: 'en',
  initialLocale: 'en',
});

locale.subscribe((value) => {
  if (!value) return;
  document.documentElement.lang = value;
});
