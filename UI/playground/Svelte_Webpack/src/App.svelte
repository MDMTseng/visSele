<script>
	export let name;

	import Other from './Other.svelte';
</script>

<style>
	h1 {
		color: purple;
	}
</style>

<Other name={name}/>