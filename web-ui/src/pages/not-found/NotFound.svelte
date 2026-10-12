<script lang="ts">import { _ } from 'svelte-i18n'; import { link } from 'svelte-spa-router'</script>
<div class="page-container text-center py-5"><div class="display-1 fw-bold text-body-tertiary">404</div><h1 class="h3 mb-4">{$_('page.notFound.title')}</h1><a class="btn btn-primary" href="/" use:link>{$_('page.notFound.back')}</a></div>
