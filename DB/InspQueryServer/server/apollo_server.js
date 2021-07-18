
const mdb_connector = require('./mdb_connector.js');

const express = require('express');
const app = express();
var expressWs = require('express-ws')(app);

app.use(function (req, res, next) {
    console.log('middleware');
    req.testing = Date.now();
    return next();
});
app.get('/', function(req, res, next){
    console.log('get route', req.testing);
    res.end();
});

function queryParamParse(req)
{//http://db.xception.tech:8080/query/inspection?tStart=0&tEnd=2580451909781&repeatTime=400&projection={%22_id%22:0,%22InspectionData.repeatTime%22:1}
  let tStart = parseInt(req.query.tStart);
  if(tStart!==tStart)tStart = req.query.tStart
  let tEnd = parseInt(req.query.tEnd);
  if(tEnd!==tEnd)tEnd = req.query.tEnd
  
  let start_MS = (new Date(tStart)).getTime();
  let endd=new Date(tEnd).getTime();
  let end_MS = (endd==endd)?endd:new Date().getTime();
  // let qStr={"InspectionData.time_ms" : {$gt:start_MS, $lt:end_MS},"InspectionData.subFeatureDefSha1"};
  let qStr={"InspectionData.time_ms" : {$gt:start_MS, $lt:end_MS}};
  if(req.query.subFeatureDefSha1!==undefined)
  {
    qStr["InspectionData.subFeatureDefSha1"]={$in:req.query.subFeatureDefSha1.split("|")};
  }
  if(req.query.subFeatureDefSha1_regex!==undefined)
  {
    qStr["InspectionData.subFeatureDefSha1"]={$regex:req.query.subFeatureDefSha1_regex};
  }
  
  let repeatTime = parseInt(req.query.repeatTime);
  if(repeatTime==repeatTime)
  {
    qStr["InspectionData.repeatTime"]={$lt:repeatTime};
  }

  //http://hyv.decade.tw:8080/query/inspection?tStart=Fri%20Jun%2004%202021%2003:54:42%20GMT+0800%20(%E5%8F%B0%E5%8C%97%E6%A8%99%E6%BA%96%E6%99%82%E9%96%93)&tEnd=Sun%20Jun%2006%202021%2003:54:42%20GMT+0800%20(%E5%8F%B0%E5%8C%97%E6%A8%99%E6%BA%96%E6%99%82%E9%96%93)&limit=10000000&page=1&subFeatureDefSha1=174e9024490bb113384d00454fdf702f671c7c03&projection={%22_id%22:0,%22InspectionData.time_ms%22:1,%22InspectionData.judgeReports.id%22:1,%22InspectionData.judgeReports.value%22:1,%22InspectionData.judgeReports.status%22:1,%22createdAt%22:1,%22updatedAt%22:1,%22InspectionData.tag%22:1}&callback=__jp2&time_ms_mod=2&time_ms_rem=0
  {
    let mod = parseInt(req.query.time_ms_mod);
    let remainder = parseInt(req.query.time_ms_rem);
    if(mod==mod && remainder==remainder)
    {
    //   console.log(mod , remainder)
    //   return { $and: [ qStr,{"InspectionData.time_ms":{ $mod: [ mod, remainder]}}] };

        return { $and: [ 
            qStr,
            {
                "InspectionData.time_ms":
                { 
                    $mod: [ 
                        mod, 
                        remainder
                    ]
                },
                
                // $expr: {$gt:[{$floor: {$divide: [{$arrayElemAt:["$InspectionData",0,"time_ms"]}, 100] } },8] }
                // $expr: {$gt:["$InspectionData.time_ms",8] }
                
                // $expr: { $lt:[ {$mod: ["$InspectionData.time_ms",mod]}, remainder] }
                // $expr: { $function: {
                //     body: (time_ms)=> true,
                //     args: [ "$InspectionData.time_ms" ],
                //     lang: "js"
                // } },
            }
        ] };
    }

  }
  
//   return { $and: [ qStr,{"InspectionData.time_ms":{ $mod: [ 1000, 0 ]}}] };
  return qStr;
}


app.get('/DELETE', function(req, res){
    let qStr = queryParamParse(req);
    console.log(qStr);

    mdb_connector.deleteMany("Inspection",qStr).
    then((result)=>{
      
      res.send("DELETE OK");
    }).
    catch((err)=>{
      
      res.send("DELETE FAILED");
    });

})


app.get('/query/deffile', function(req, res) {
    //http://db.xception.tech:8080/query/deffile?name=Test1|FC.&limit=1000
    let projection=req.query.projection;

    try{
      if(projection==="{}")
      {
        projection=undefined
      }
      else
        projection=JSON.parse(projection);
    }
    catch (e) 
    {
        projection={"_id":0,"DefineFile.name":1,"DefineFile.featureSet_sha1":1,"createdAt":1};
    }


    let qStr ={};
    if(req.query.name!==undefined)
    {
        qStr["DefineFile.name"]={$regex:req.query.name};
    }
    if(req.query.featureSet_sha1!==undefined)
    {
        qStr["DefineFile.featureSet_sha1"]={$regex:req.query.featureSet_sha1
        };
    }
    
    let queryPage=parseInt(req.query.page);
    let queryLimit=parseInt(req.query.limit);
    if(queryPage===undefined || queryPage!=queryPage || queryPage<1)queryPage=1;

    if(queryLimit===undefined || queryLimit!=queryLimit)queryLimit=100;

    if(qStr["DefineFile.name"]===undefined && qStr["DefineFile.featureSet_sha1"]===undefined )
    {
        if(req.query.callback===undefined)//normal ajax
        {
            res.send([]);
        }
        else
        {
            let cbName = req.query.callback;
            res.send(cbName+"("+JSON.stringify([])+")");
        }
        return;
    }

    let queryAggRules=[];
    if(req.query.agg!==undefined)
    {
      let agg = JSON.parse(req.query.agg);
      queryAggRules=queryAggRules.concat(agg);
    }

    

    //console.log(qStr,queryPage,queryLimit,(queryPage-1)*queryLimit,queryLimit);
    mdb_connector.query("df",qStr,projection,queryAggRules).skip((queryPage-1)*queryLimit).limit(queryLimit).
    then((result)=>{
        // console.log(result);
        if(req.query.callback===undefined)//normal ajax
        {
            res.send(result);
        }
        else
        {
            let cbName = req.query.callback;

            res.send(cbName+"("+JSON.stringify(result)+")");
        }
        console.log(req.query.tEnd);
        console.log("[O]Q by get Q OK!! len=" + result.length);
    }).
    catch((err)=>{
        res.send("[X]Q by get Q FAIL!!");
        console.log("[X]Q by get Q FAIL!!",err);
    });



    // res.sendFile(path.join(__dirname+'/index.html'));

    // {InspectionData.time_ms : {$gt:1556187710991}}

});



function inspection_result_query(req, res)
{
    //http://localhost:8080/insp_time?tStart=2019/5/15/0:0:0&tEnd=2019/5/15/0:0:1
    //{InspectionData.time_ms : {$gt:1556640000000}}
    //http://localhost:8080/insp_time?tStart=2019/5/15/9:59:0&subFeatureDefSha1=.42a5.
    //http://db.xception.tech:8080/insp_time?tStart=2019/5/27/9:59:0&projection={"_id":0,"InspectionData.time_ms":1} only return time
    //http://db.xception.tech:8080/insp_time?tStart=2019/5/27/9:59:0&projection={"_id":0,"InspectionData.time_ms":1,"InspectionData.judgeReports":1}
    //Return time and judgeReports

    //Find the count of certain subFeatureDefSha1/comb
    //http://db.xception.tech:8080/query/inspection?tStart=0&tEnd=2581663256894&subFeatureDefSha1=497298a734229971|012a3ef713a9124d6799ac&projection={"_id":0,"InspectionData.subFeatureDefSha1":1}&agg=[{"$group":{"_id":"$InspectionData.subFeatureDefSha1","tl": {"$sum":1}}}]


    //param list
    //tStart=2019/5/15/9:59:0
    //tEnd=2019/5/15/9:59:0
    //subFeatureDefSha1=[REGX matched sha1]
    //projection={js OBJ} object which in mongodb projection format
    //page={int} page number starts from 1
    //limit={int} maximum query result in this page, 1000 by default

    let projection=req.query.projection;

    try{
        projection=JSON.parse(projection);
    }
    catch (e) 
    {
        projection={"_id":0,"InspectionData.time_ms":1};
    }

    let qStr = queryParamParse(req);
    let queryPage=parseInt(req.query.page);
    let queryLimit=parseInt(req.query.limit);
    let querySample=parseInt(req.query.sample);


    if(queryPage===undefined || queryPage!=queryPage || queryPage<1)queryPage=1;

    if(queryLimit===undefined || queryLimit!=queryLimit)queryLimit=1000;
    //console.log("qStr",qStr);
    let queryAggRules=[];
    if(querySample==querySample)
      queryAggRules.push({ "$sample" : {size:querySample} });
    if(req.query.agg!==undefined)
    {
      let agg = JSON.parse(req.query.agg);
      queryAggRules=queryAggRules.concat(agg);
    }

    // console.log(qStr);
    // console.log(projection);
    // console.log(queryAggRules);
    mdb_connector.query("Inspection",qStr,projection,queryAggRules).limit(queryLimit).skip((queryPage-1)*queryLimit).
    then((result)=>{
      //  console.log(result);
        if(req.query.callback===undefined)//normal ajax
        {
            res.send(result);
        }
        else
        {
            let cbName = req.query.callback;

            res.send(cbName+"("+JSON.stringify(result)+")");
        }
        console.log(req.query.tEnd);
        console.log("[O]Q by get Q OK!! len=" + result.length);
    }).
    catch((err)=>{
        res.send("[X]Q by get Q FAIL!!");
        console.log("[X]Q by get Q FAIL!!",err);
    });



    // res.sendFile(path.join(__dirname+'/index.html'));

    // {InspectionData.time_ms : {$gt:1556187710991}}

}


app.get('/query/inspection',inspection_result_query);
app.get('/insp_time',inspection_result_query);

function jsonStrAddPadding(jsonp_preFix,jsonStr)
{

  if(jsonp_preFix===undefined)//normal ajax
  {
    return jsonStr;
  }
  else
  {
    return jsonp_preFix+"("+jsonStr+")";
  }

}

function customdisplay_QueryParamParse(queryObj)
{
  let recObj={};


  if(queryObj._id!==undefined)
  {
    recObj._id=queryObj._id;
  }
  if(queryObj.name!==undefined)
    recObj.name ={$regex:queryObj.name};
  
  if(queryObj.targetDefHash!==undefined)
    recObj["targetDeffiles.hash"]=queryObj.targetDefHash;

  
  return recObj;
}

app.get('/insert/customdisplay',(req, res)=>{
  let recObj=customdisplay_QueryParamParse(req.query);


  try{
    //recObj.name = req.query.name;
    //recObj.targetDeffiles = JSON.parse(req.query.targetDeffiles);
    recObj=JSON.parse(req.query.full);
  }
  catch(err)
  {
    let rspStr=JSON.stringify({
        type:"NAK",
        err:err,
        misc:"targetDeffiles is not parsible"
      });
    
    res.send(jsonStrAddPadding(req.query.callback,rspStr));

    return;
  }
  mdb_connector.insertOne("CustomDisplay",recObj).
    then((prod)=>{
      let rspStr=JSON.stringify({
        type:"ACK"
      })
  
      res.send(jsonStrAddPadding(req.query.callback,rspStr));
    }).
    catch((err)=>{
        console.log("[X]INSP InsertFailed!!",err);
      
      let rspStr=JSON.stringify({
        type:"NAK",
        err:err
      });
  
      res.send(jsonStrAddPadding(req.query.callback,rspStr));
    });
});


app.get('/delete/customdisplay',(req, res)=>{

  let recObj=customdisplay_QueryParamParse(req.query);
  if(recObj._id===undefined)
  {
    let rspStr=JSON.stringify({
      type:"NAK"
    });
    res.send(jsonStrAddPadding(req.query.callback,rspStr));
  }
  mdb_connector.deleteMany("CustomDisplay",recObj).
    then((prod)=>{

      let delCount = prod.n;
      let rspStr=JSON.stringify({
        type:"ACK",
        count:delCount
      })
  
      res.send(jsonStrAddPadding(req.query.callback,rspStr));
    }).
    catch((err)=>{
        console.log("[X]INSP Delete Failed!!",err);
      
      let rspStr=JSON.stringify({
        type:"NAK",
        err:err
      });
  
      res.send(jsonStrAddPadding(req.query.callback,rspStr));
    });

})
app.get('/query/customdisplay',(req, res)=>{
  let recObj=customdisplay_QueryParamParse(req.query);

  let projection=req.query.projection;

  try{
      projection=JSON.parse(projection);
  }
  catch (e) 
  {
      projection={"name":1,"targetDeffiles.hash":1};;
  }

  mdb_connector.query("CustomDisplay",recObj,projection).
    then((prod)=>{
      res.send(jsonStrAddPadding(req.query.callback,JSON.stringify({
        type:"ACK",
        prod
      })));
    }).
    catch((err)=>{

      res.send(jsonStrAddPadding(req.query.callback,JSON.stringify({
        type:"NAK",
        err:err
      })));

      console.log("[X]INSP InsertFailed!!",err);
    });

});



app.ws('/', function(ws, req) {
    util.inspect(ws);
    ws.on('message', function(msg) {
        // CRUD_insertOne("Inspection",JSON.parse(jj));
        console.log("[WS][/]",msg);
    });
    console.log('socket /', req.testing);
});
app.ws('/query/insp', function(ws, req) {

    ws.on('message', function(msg) {
        var RX_JSON=isJSON(msg);
        if(RX_JSON===false){
            console.log('[WS][RX][X_JSON],RX_MSG=', msg);
        }
        else{
            let req_id = RX_JSON.req_id;
            //console.log(RX_JSON.dbcmd.cmd);
            switch(RX_JSON.dbcmd.db_action)
            {
                case "query":
                    mdb_connector.query("inspection",RX_JSON.dbcmd.cmd).
                        then((res)=>{

                            //console.log(res);
                            ws.send(JSON.stringify({
                                type:"ACK",
                                req_id:req_id,
                                dbcmd:RX_JSON.dbcmd,
                                result:res
                            }));
                            console.log("[O]INSP InsertOK!!");
                        }).
                        catch((err)=>{
                            ws.send(JSON.stringify({
                                type:"NAK",
                                req_id:req_id,
                                error:err,
                                dbcmd:RX_JSON.dbcmd
                            }));
                            console.log("[X]DEF QueryFailed!!",err);
                        });
                break;
                default:
                    ws.send(JSON.stringify({
                        type:"NAK",
                        req_id:req_id,
                    }));
                break
            }
        }
    });
});

app.ws('/insert/insp', function(ws, req) {
    ws.on('message', function(msg) {
        var RX_JSON=isJSON(msg);
        if(RX_JSON===false){
            console.log('[WS][RX][X_JSON],RX_MSG=', msg);
        }
        else{
            let req_id = RX_JSON.req_id;

            switch(RX_JSON.dbcmd.db_action)
            {
                case "insert":
                    mdb_connector.insertOne("Inspection",RX_JSON.data).
                        then((prod)=>{
                            ws.send(JSON.stringify({
                                type:"ACK",
                                req_id:req_id,
                                _id:prod._id,
                                dbcmd:RX_JSON.dbcmd
                            }));
                            console.log("[O]INSP InsertOK!!");
                        }).
                        catch((err)=>{
                            ws.send(JSON.stringify({
                                type:"NAK",
                                req_id:req_id,
                                error:err,
                                dbcmd:RX_JSON.dbcmd
                            }));
                            console.log("[X]INSP InsertFailed!!",err);
                        });
                break;
                default:
                    ws.send(JSON.stringify({
                        type:"NAK",
                        req_id:req_id,
                    }));
                break
            }
        }
    });

});





app.ws('/query/def', function(ws, req) {
    ws.on('message', function(msg) {
        var RX_JSON=isJSON(msg);
        if(RX_JSON===false){
            console.log('[WS][RX][X_JSON],RX_MSG=', msg);
        }
        else{
            let req_id = RX_JSON.req_id;
            //console.log(RX_JSON.dbcmd.cmd);
            switch(RX_JSON.dbcmd.db_action)
            {
                case "query":
                    mdb_connector.query("df",RX_JSON.dbcmd.cmd).
                        then((res)=>{

                            //console.log(res);
                            ws.send(JSON.stringify({
                                type:"ACK",
                                req_id:req_id,
                                dbcmd:RX_JSON.dbcmd,
                                result:res
                            }));
                            console.log("[O]INSP InsertOK!!");
                        }).
                        catch((err)=>{
                            ws.send(JSON.stringify({
                                type:"NAK",
                                req_id:req_id,
                                error:err,
                                dbcmd:RX_JSON.dbcmd
                            }));
                            console.log("[X]DEF QueryFailed!!",err);
                        });
                break;
                default:
                    ws.send(JSON.stringify({
                        type:"NAK",
                        req_id:req_id,
                    }));
                break
            }
        }
    });
});
app.ws('/insert/def', function(ws, req) {
    ws.on('message', function(msg) {
        var RX_JSON=isJSON(msg);


        if(RX_JSON===false){
            console.log('[WS][RX][X_JSON],RX_MSG=', msg);
        }
        else{
            let req_id = RX_JSON.req_id;

            switch(RX_JSON.dbcmd.db_action)
            {
                case "insert":
                    mdb_connector.upsertOne("df",RX_JSON.data).
                        then((prod)=>{
                            ws.send(JSON.stringify({
                                type:"ACK",
                                req_id:req_id,
                                dbcmd:RX_JSON.dbcmd
                            }));
                            console.log("[O]DF InsertOK!!");
                        }).
                        catch((err)=>{
                            ws.send(JSON.stringify({
                                type:"NAK",
                                req_id:req_id,
                                dbcmd:RX_JSON.dbcmd,
                                error:err
                            }));
                            console.log("[X]DF InsertFailed!!",err);
                        });

                break;
                default:
                    ws.send(JSON.stringify({
                        type:"NAK",
                        dbcmd:RX_JSON.dbcmd,
                        error:"Error: db_action("+RX_JSON.dbcmd.db_action+") is not defind.",
                        req_id:req_id,
                    }));
                break
            }
            // idb_connector.insertOne("Inspection",RX_JSON);
            // console.log('[WS][RX][O_JSON],RX_MSG=', msg);
            // var result = Object.assign({},msg, {"TS":new BSNO.Timestamp()});
            // var RX_JSON_TS=Object.assign({TS: new Timestamp()}, RX_JSON);
            // var RX_JSON_TS=Object.assign({ timestamps: true }, RX_JSON);
        }
    });

});
var objectIdFromDate = function (date) {
    return Math.floor(date.getTime() / 1000).toString(16) + "0000000000000000";
};

var dateFromObjectId = function (objectId) {
    return new Date(parseInt(objectId.substring(0, 8), 16) * 1000);
};
function insert_InspectionCollection(text) {

}
function isJSON(text) {
    try {
        if (typeof text !== "string") {
            return false;
        } else {
            return JSON.parse(text);
        }
    } catch (error) {
        return false;
    }
}
function JSONTryParse(input) {
    try {
        if (input) {
            var o = JSON.parse(input);
           if (o && o.constructor === Object) {
                return o;
            }
        }
    }
    catch (e) {
    }

    return false;
};
app.listen(8081);


