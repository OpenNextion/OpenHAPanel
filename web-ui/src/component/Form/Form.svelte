<script lang="ts">
    import type { Snippet} from 'svelte';
    interface IForm {
        model: any|null;
        rules: any|null;
        onSubmit: any;
        validate: any | null;
        children?: Snippet | undefined
    }
    const { model = null, rules = null, onSubmit, validate = null, children }: IForm= $props();

    function reset(validate: any) {
        const keys = Object.keys(validate);
        const newErrorEntries = keys.map((key) => [key, '']);
        return Object.fromEntries(newErrorEntries);
    }

    function validateForm(event: any) {
        event.preventDefault();
        let validateStatus = true;
        if(rules === null){
            return onSubmit(validateStatus, validate);
        }
        const copyErrors = reset(validate);
        for (const field in rules) {
            const fieldRules = rules[field];
            for (const rule of fieldRules) {
                if (rule.required && (!model[field] || model[field].trim() === '')) {
                    copyErrors[field] = rule.message;
                    validateStatus = false;
                    break;
                }
                if (rule.max && model[field] && model[field].length > rule.max) {
                    copyErrors[field] = rule.message;
                    validateStatus = false;
                    break;
                }
            }
        }
        onSubmit(validateStatus, copyErrors);
    }

</script>

<form onsubmit={(event) => validateForm(event)}>
    <div>{@render children?.()}</div>
</form>

<!-- example
    // data
    const rules = {
        username: [
            {
                required: true,
                message: 'username is required',
            },
        ],
        password: [
            {
                required: true,
                message: 'password is required',
            },
            {
                max: 8,
                message: 'email长度不能超过8字符',
            },
        ],
    };

    let formState = $state({
        username: '',
        password: '',
    });

    let validate = $state({
        username: '',
        password: '',
    });

    function onSubmit(validateStatus: boolean, values: any) {
        validate = values;
        console.log(validate)
    }

    // script
    import FormItem from '@/lib/component/Form/FormItem.svelte';
    import Form from '@/lib/component/Form/Form.svelte';
    import Input from '@/lib/component/Form/Input.svelte';
    import Button from '@/lib/component/Button/Button.svelte';

    // html
    <Form model={formState} {rules} {onSubmit} {validate}>
        <FormItem label={$t('ROUTE.OVERVIEW')} name="username" validate={validate.username} style="margin-bottom:0">
            <Input disabled={false} allowClear={true} bind:value={formState.username} />
        </FormItem>
        <FormItem label="密码" name="password" validate={validate.password}>
            <Input disabled={false} allowClear={true} bind:value={formState.password} />
        </FormItem>
        <FormItem>
            <Button buttonType="submit"></Button>
        </FormItem>
    </Form>
-->
