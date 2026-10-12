<script lang="ts">
    import LoginForm from './LoginForm.svelte';
    import ResetPassword from './ResetPassword.svelte';
    import ConnectLimited from './ConnectLimited.svelte';
    import { useLogin, ELoginPageState } from '@/hooks/useLogin.svelte';
    import Modal from './Modal.svelte';

    const { loginPageState } = useLogin();
    let PageComponent = $derived.by(() => {
        switch (loginPageState.pageState) {
            case ELoginPageState.RESET_PASSWORD:
                return ResetPassword;
            case ELoginPageState.CONNECT_LIMITED:
                return ConnectLimited;
            default:
                return LoginForm;
        }
    });
</script>

<div class="flex-grow-1 login">
    <PageComponent />
    <Modal />
</div>

<style lang="scss">
    .login {
        display: flex;
        flex-direction: column;
        justify-content: center;
        max-width: 21.44rem;
    }
</style>
