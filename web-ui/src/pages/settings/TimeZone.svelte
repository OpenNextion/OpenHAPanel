<script lang="ts">
    import SettingRow from '@/component/SettingRow/SettingRow.svelte';
    import SettingRowGroup from '@/component/SettingRow/SettingRowGroup.svelte';
    import { onMount } from 'svelte';
    import { _, t } from 'svelte-i18n';
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';
    import Button from '@/component/Button/Button.svelte';
    import { EWsMethods } from '@/types/websocket';
    import sendForReply from '@/ws/sendForReply';
    import { pop } from 'svelte-spa-router';
    import { ROUTES_PATH, TIMEZONE_INFO } from '@/constant';
    import dayjs from 'dayjs';
    import Input from '@/component/Input/Input.svelte';
    import SettingPageHeader from '@/component/PageHeader/PageHeader.svelte';
    import { useNtpServersHooks } from '@/hooks/useNtpServer.svelte';
    import { deviceStore } from '@/stores/device';

    const { timeInfo, isEmptyHook, setNtpServers, setTzInfo } = useNtpServersHooks();
    let timeZone = $state('');
    let inputValue = $state('');
    let loading = $state(false);

    onMount(() => {
        if (isEmptyHook()) {
            setNtpServers($deviceStore.ntpServers);
            setTzInfo($deviceStore.tzInfo);
        }
        timeZone = timeInfo.tzInfo.timeZone;
    });

    const filterTimeZone = $derived.by(() => Object.values(TIMEZONE_INFO).filter((item) => item.tzName && item.tzName.toLowerCase().indexOf(inputValue.toLowerCase()) > -1));

    const handleSave = async () => {
        if (!timeZone || loading) return;
        loading = true;
        try {
            const timeZoneRsp = await sendForReply<
                { timeZone: string },
                { offset: number; dst: number; nextDst: string | number; nextOffset: number }
            >({
                method: EWsMethods.GET_TIME_ZONE,
                data: { timeZone },
            });

            if (timeZoneRsp.error !== 0 || !timeZoneRsp.data) return;
            const { offset, dst, nextDst, nextOffset } = timeZoneRsp.data;
            const tzInfo = {
                timeZone,
                utcOffset: offset * 60,
                isDst: dst === 1,
                hasDst: dst !== 0,
                nextDstShift: dayjs(nextDst).valueOf(),
                nextUtcOffset: nextOffset * 60,
            };
            setTzInfo(tzInfo);
            await pop();
        } catch (error) {
            console.error('Failed to update timezone:', error);
        } finally {
            loading = false;
        }
    };
</script>

<div class="timezone-wrap">
    <SettingPageHeader title={$_('settingsDetail.timezone')} back={ROUTES_PATH.SETTINGS_TIME} />
    <div class="search-input">
        <Input size="lg" bind:value={inputValue} placeholder={timeZone} clear={true}>
            {#snippet suffixSnippet()}
                <span style="color: var(--body-text-color-highlight);">
                    <SvgIcon size="20px" name="search.svg" />
                </span>
            {/snippet}
        </Input>
    </div>
    <div class="content">
        <SettingRowGroup>
            {#each filterTimeZone as item}
                <div class="item d-flex align-items-center justify-content-center" role="none">
                    <SettingRow title={item.tzName} onclick={() => (timeZone = item.tzName)}>
                        {#snippet contentSnippet()}
                            {#if item.tzName === timeZone}
                                <span style="color: #498FFF">
                                    <SvgIcon name="check.svg" />
                                </span>
                            {/if}
                        {/snippet}
                    </SettingRow>
                </div>
            {/each}
        </SettingRowGroup>
    </div>
    <div class="footer">
        <Button style="width:19.69rem" type="primary" title={$t('common.save')} onclick={handleSave} {loading} disabled={!timeZone} />
    </div>
</div>

<style lang="scss">
    .timezone-wrap {
        .search-input {
            :global {
                .form-control {
                    background-color: var(--body-inner-level-2);
                }
            }
        }
        .content {
            height: calc(100dvh - 340px - var(--footer-height));
            min-height: 16rem;
            margin-top: 1.25rem;
            overflow: auto;
            border-radius: 0.63rem;
            background-color: var(--body-inner-level-2);

            @media (max-width: 768px) {
                height: calc(100dvh - 330px - var(--footer-height));
            }
            .item {
                font-size: 1rem;
                cursor: pointer;
            }
            &::-webkit-scrollbar {
                width: 0.5rem;
            }
            &::-webkit-scrollbar-track {
                border-radius: 0.625rem;
                background-color: transparent;
            }
            &::-webkit-scrollbar-thumb {
                border-radius: 0.625rem;
                box-shadow: inset 0 0 0 rgba(240, 240, 240, 0.5);
                background-color: #969696;
            }
        }
        .footer {
            display: flex;
            width: 100%;
            justify-content: center;
            margin-top: 2.5rem;
        }
    }
</style>
