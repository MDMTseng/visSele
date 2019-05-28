<script>
	import { onMount } from 'svelte';
	//import axios from 'axios';
	
	import jsonp from 'jsonp';
	export let name;
	let text="Loading...";
	
	
	onMount(async () => {
        setTimeout(()=>{
        },3000)
	});

	let url = "http://hyv.decade.tw:8080/insp_time?";
	
	url+="tStart=2019/5/28/7:50:0&";
	if(true)
	{
		url+="projection="+JSON.stringify(
			{"_id":0,"InspectionData.time_ms":1,
			"InspectionData.judgeReports.id":1,
			"InspectionData.judgeReports.value":1}
			);
	}
	else{
		
		url+="projection="+JSON.stringify(
			{"_id":0,"InspectionData.time_ms":1,
			"InspectionData.judgeReports":1}
			);
	}
	url+="";
	
	jsonp(url,  (err,data)=>{
		console.log(err,data);
		text=JSON.stringify(
			data.map(data=>data.InspectionData[0].time_ms)
		);
	});
	
</script>

<style>
	h1 {
		color: purple;
	}
</style>

<h1>{text}</h1>