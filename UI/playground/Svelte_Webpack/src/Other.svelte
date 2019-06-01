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

	let db_url = "http://hyv.decade.tw:8080/";
	function defFileQuery(name,featureSet_sha1)
	{
		
		let url = db_url+"query/deffile?";
		url+="limit=1000";
		
		if(name!==undefined)
			url+="&name="+name;
		if(featureSet_sha1!==undefined)
			url+="&featureSet_sha1="+featureSet_sha1;
		//url+="&projection={}"
		return new Promise((res,rej)=>{

			jsonp(url,  (err,data)=>{
				if(err === null)
					res(data);
				else
					rej(err)
				console.log(err,data);
			});
		});
	}
	

	function inspectionQuery(subFeatureDefSha1,limit=100)
	{	
		let TYPE="query/inspection";
		let url = db_url+TYPE+"?";
	
		url+="tStart=2018/5/19/7:50:0&tEnd=2019/4/10/18:50:0&";
		url+="limit="+limit+"&page=1&"
		url+="subFeatureDefSha1="+subFeatureDefSha1+"&"
		if(true)
		{
			url+="projection="+JSON.stringify(
				{"_id":0,"InspectionData.time_ms":1,
				"InspectionData.subFeatureDefSha1":1,
				"InspectionData.judgeReports.id":1,
				"InspectionData.judgeReports.value":1,
				"InspectionData.judgeReports.status":1,
				});
		}
		else{
			
			url+="projection="+JSON.stringify(
				{"_id":0,"InspectionData.time_ms":1,
				"InspectionData.judgeReports":1}
				);
		}
		url+="";
		

		
		return new Promise((res,rej)=>{

			jsonp(url,  (err,data)=>{
				
				if(err === null)
					res(data);
				else
					rej(err)
				console.log(err,data);
				/*console.log(err,data);
				text=JSON.stringify(
					data.
					reduce((arr,data)=>
					{
						data.InspectionData.forEach(ele =>arr.push(ele));
						return arr;
					},[]).
					map(data=>data.judgeReports)
				);*/
			});
		});

	}

	

	async function f1() {
		var defFs = await defFileQuery(undefined,"0fad94ed984b2905a315cb4e2d4ac31a8d627e33");
		var hashM=defFs.map(defF=>defF.DefineFile.featureSet_sha1).reduce((sumHash,hash)=>sumHash+"|"+hash);
		console.log(hashM); // 10
		//var q = await inspectionQuery("^.{0}$");
		//console.log(q); // 10
	}
	f1();
	
</script>

<style>
	h1 {
		color: purple;
	}
</style>

<h1>{text}</h1>