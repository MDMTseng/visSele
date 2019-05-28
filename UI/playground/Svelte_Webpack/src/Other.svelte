<script>
	import { onMount } from 'svelte';
	export let name;
    let text="Loading...";
	onMount(async () => {
        setTimeout(()=>{
            text = "Hello "+name+" !"; 
        },3000)
	});
</script>

<style>
	h1 {
		color: purple;
	}
</style>

<h1>{text}</h1>