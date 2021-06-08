import jsonp from 'jsonp';


export let db_url="http://db.xception.tech:8080";

function defFileQueryStr(name,featureSet_sha1,projection)
{
  let url ="";
  
  if(name!==undefined)
  url+="&name="+name;

  if(featureSet_sha1!==undefined)
    url+="&featureSet_sha1="+featureSet_sha1;

  if(projection!==undefined)
    url+="&projection="+JSON.stringify(projection);
  else
    url+="&projection={}";

  return url;
}

function defFileQuery(name,featureSet_sha1,projection)
{
    
    let url = db_url+"/query/deffile?";
    
    url+="limit=1000";
    url+=defFileQueryStr(name,featureSet_sha1,projection);
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


function inspectionQuery(subFeatureDefSha1,date_start,date_end,limit=100,sample=undefined,prjection=undefined,agg=undefined)
{	
    let TYPE="/query/inspection";
    let url = db_url+TYPE+"?";

    url+="subFeatureDefSha1="+subFeatureDefSha1+"&"
    url+="tStart="+date_start+"&tEnd="+date_end+"&";
    if(limit!==undefined)
        url+="limit="+limit+"&page=1&"
    if(sample!==undefined)
        url+="sample="+sample+"&"
    if(agg!=undefined)
        url+="agg="+JSON.stringify(agg)+"&"

    if(prjection===undefined)
    {
        url+="projection="+JSON.stringify(
            {"_id":0,"InspectionData.time_ms":1,
            "InspectionData.judgeReports.id":1,
            "InspectionData.judgeReports.value":1,
            "InspectionData.judgeReports.status":1,
            
            "createdAt":1,
            "updatedAt":1,
            "InspectionData.tag":1
            });
    }
    else{
        
        url+="projection="+JSON.stringify(prjection);
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


export {defFileQuery,inspectionQuery};