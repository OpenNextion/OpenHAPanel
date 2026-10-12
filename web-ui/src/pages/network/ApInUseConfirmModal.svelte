<script lang="ts">
    import { _ } from 'svelte-i18n';
    import Modal from '@/component/Modal/Modal.svelte';

    interface Props {
        visible: boolean;
        loading?: boolean;
        confirm: () => void | Promise<void>;
        cancel: () => void;
    }

    let { visible, loading = false, confirm, cancel }: Props = $props();
</script>

<Modal
    modalVisible={visible}
    confirmLoading={loading}
    title={$_('networkPage.apInUseTitle')}
    subTitle={$_('networkPage.apInUseConfirm')}
    cancelBtn={true}
    {confirm}
    {cancel}
    setVisible={(nextVisible) => !nextVisible && cancel()}
/>
