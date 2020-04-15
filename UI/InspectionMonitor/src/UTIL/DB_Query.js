import jsonp from 'jsonp';


let db_url = "http://hyv.decade.tw:8080/";

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
}

function defFileQuery(name,featureSet_sha1,projection)
{
    
    let url = db_url+"query/deffile?";
    
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


function inspectionQuery(subFeatureDefSha1,date_start,date_end,limit=100)
{	
    let TYPE="query/inspection";
    let url = db_url+TYPE+"?";

    url+="tStart="+date_start.toString()+"&tEnd="+date_end.toString()+"&";
    url+="limit="+limit+"&page=1&"
    url+="subFeatureDefSha1="+subFeatureDefSha1+"&"
    if(true)
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


export {defFileQuery,inspectionQuery};