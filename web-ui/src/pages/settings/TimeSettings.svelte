<script lang="ts">
    import { onMount } from 'svelte';
    import { _ } from 'svelte-i18n';
    import { watch } from 'runed';
    import dayjs from 'dayjs';
    import { pop } from 'svelte-spa-router';

    import { deviceStore, updateDeviceField } from '@/stores/device';
    import { networkStore } from '@/stores/network';

    import { useNtpServersHooks } from '@/hooks/useNtpServer.svelte';

    import SettingPageHeader from '@/component/PageHeader/PageHeader.svelte';
    import Button from '@/component/Button/Button.svelte';

    import { ERROR_CODE, ROUTES_PATH } from '@/constant';
    import { EMessageType, sendMessage } from '@/stores/message';
    import sendForReply from '@/ws/sendForReply';
    import { EWsMethods } from '@/types/websocket';
    import SettingRow from '@/component/SettingRow/SettingRow.svelte';
    import SettingRowGroup from '@/component/SettingRow/SettingRowGroup.svelte';
    import { timerStore } from '@/stores/timer';
    import { get as lodashGet } from 'lodash-es';

    const { timeInfo, isEmptyHook, setNtpServers, setTzInfo } = useNtpServersHooks();

    let timeZone = $state('');
    let loading = $state(false);
    let utcOffset = $state(0);
    const currentTimeInMs = $derived(
        Number(lodashGet($timerStore, ['timers', 'currentTime', 'value'], $deviceStore.currentTime || dayjs().valueOf())),
    );

    const currentTime = $derived.by(() => {
        return dayjs.utc(currentTimeInMs).utcOffset(curOffset / 60);
    });
    const curOffset = $derived.by(() => {
        const { hasDst, nextDstShift, nextUtcOffset } = timeInfo.tzInfo;
        if (!hasDst) return utcOffset;
        return currentTimeInMs >= nextDstShift ? nextUtcOffset : utcOffset;
    });
    const hasInternetConnect = $derived.by(() => $networkStore.internetConnected);

    onMount(() => {
        if (isEmptyHook()) {
            setNtpServers($deviceStore.ntpServers);
            setTzInfo($deviceStore.tzInfo);
            const { timeZone, utcOffset } = $deviceStore.tzInfo;
            changeTimeZone(timeZone, utcOffset);
        } else {
            const { timeZone, utcOffset } = timeInfo.tzInfo;
            changeTimeZone(timeZone, utcOffset);
        }
    });

    watch(
        () => $deviceStore.tzInfo.timeZone,
        () => {
            const { timeZone, utcOffset } = $deviceStore.tzInfo;
            changeTimeZone(timeZone, utcOffset);
            setTzInfo($deviceStore.tzInfo);
        },
        {
            lazy: true,
        },
    );

    function changeTimeZone(tz: string, offset: number) {
        timeZone = tz;
        utcOffset = offset;
    }

    const handleDetect = async () => {
        loading = true;
        const timeInfoRsp = await sendForReply<{ timeZone: string }>({
            method: EWsMethods.GET_TIME_ZONE,
            data: { timeZone: '' },
        });
        loading = false;

        switch (timeInfoRsp.error) {
            case ERROR_CODE.SUCCESS:
                if (!timeInfoRsp.data) return;
                const { id, offset, dst, nextDst, nextOffset } = timeInfoRsp.data;
                const tzInfo = {
                    timeZone: id,
                    utcOffset: offset * 60,
                    isDst: dst === 1,
                    hasDst: dst !== 0,
                    nextDstShift: dayjs(nextDst).valueOf(),
                    nextUtcOffset: nextOffset * 60,
                };
                setTzInfo(tzInfo);
                changeTimeZone(id, offset * 60);
                break;
            case ERROR_CODE.TIMEZONE_DETECTING:
                loading = true;
                break;
            case ERROR_CODE.TIMEZONE_DETECTION_ERROR:
                sendMessage(EMessageType.ERROR, $_('TIPS.TIMEZONE_NETWORK_ABNORMAL'));
                break;
            default:
                sendMessage(EMessageType.ERROR, $_('TIPS.TIMEZONE_DETECT_FAILED'));
                break;
        }
    };

    let saveLoading = $state(false);
    const handleSave = async () => {
        if (!$networkStore.internetConnected || saveLoading) return;
        saveLoading = true;
        try {
            const setTimeZoneRes = await sendForReply({
                method: EWsMethods.SET_TIME_ZONE,
                data: { timeZone },
            });
            setTimeZoneRes.error === ERROR_CODE.SUCCESS && updateDeviceField('tzInfo', timeInfo.tzInfo);

            const setNtpServersRes = await sendForReply({
                method: EWsMethods.SET_NTP_SERVERS,
                data: timeInfo.ntpServers,
            });
            setNtpServersRes.error === ERROR_CODE.SUCCESS && updateDeviceField('ntpServers', timeInfo.ntpServers);

            if (setNtpServersRes.error === ERROR_CODE.SUCCESS && setTimeZoneRes.error === ERROR_CODE.SUCCESS) {
                sendMessage(EMessageType.SUCCESS, $_('TIPS.SAVED'));
                await pop();
            } else if (setNtpServersRes.error === ERROR_CODE.INPUT_BYTE_SIZE_EXCEEDED) {
                sendMessage(EMessageType.ERROR, $_('TIPS.INPUT_BYTE_SIZE_EXCEEDED'));
            }
        } finally {
            saveLoading = false;
        }
    };
</script>

<div class="page-container settings-detail">
    <SettingPageHeader title={$_('page.settings.time')} />
    <div class="time-settings-groups">
        <SettingRow title={$_('settingsDetail.dateTime')} content={currentTime.format('YYYY-MM-DD HH:mm')} margin={0} />
        <SettingRowGroup>
            <SettingRow title={$_('settingsDetail.timezone')} content={timeInfo.tzInfo.timeZone || '--'} route={ROUTES_PATH.SETTINGS_TIME_ZONE} />
            <SettingRow disable={loading || !hasInternetConnect} onclick={handleDetect}>
                {#snippet titleSnippet()}<span class="auto-detect-title">{$_('settingsDetail.autoDetect')}</span>{/snippet}
                {#snippet contentSnippet()}{#if loading}<span class="spinner-border spinner-border-sm" aria-hidden="true"></span>{/if}{/snippet}
            </SettingRow>
        </SettingRowGroup>
        <SettingRowGroup>
            <SettingRow title={$_('settingsDetail.ntp1')} content={timeInfo.ntpServers.server1 || '--'} route="/settings/time/ntp/server1" />
            <SettingRow title={$_('settingsDetail.ntp2')} content={timeInfo.ntpServers.server2 || '--'} route="/settings/time/ntp/server2" />
        </SettingRowGroup>
    </div>
    <Button type="primary" class="settings-save" style="width: min(70%, 560px)" onclick={handleSave} loading={saveLoading} disabled={!hasInternetConnect}>{$_('common.save')}</Button>
</div>

<style lang="scss">
    .time-settings-groups {
        display: flex;
        flex-direction: column;
        gap: 1.5rem;
    }

    .auto-detect-title {
        color: var(--body-text-color-blue, #0199df);
    }
</style>
