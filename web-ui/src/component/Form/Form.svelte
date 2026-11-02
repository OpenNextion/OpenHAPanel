<script lang="ts">
    import type { Snippet } from 'svelte';
    interface IForm {
        model: any | null;
        rules: any | null;
        onSubmit: any;
        validate: any | null;
        children?: Snippet | undefined;
    }
    const { model = null, rules = null, onSubmit, validate = null, children }: IForm = $props();

    function reset(validate: any) {
        const keys = Object.keys(validate);
        const newErrorEntries = keys.map((key) => [key, '']);
        return Object.fromEntries(newErrorEntries);
    }

    function validateForm(event: any) {
        event.preventDefault();
        let validateStatus = true;
        if (rules === null) {
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
