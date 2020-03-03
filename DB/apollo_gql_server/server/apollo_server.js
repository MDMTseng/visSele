// import * as MDB from './server/mdb_connector.js';

// const {typeDefs,resolvers}=require('../schema/schema.js') ;
// const { ApolloServer ,qgl} = require('apollo-server');
// const { ApolloServer, gql } = require('apollo-server-express');//for use express server self define
// const server = new ApolloServer({ typeDefs, resolvers });
const mdb_connector = require('./mdb_connector.js');
const idb_connector = require('./idb_connector.js');

const express = require('express');
const app = express();
const useApolloEmbedExpress=true;



// if(useApolloEmbedExpress){
//     server.listen().then(({ url }) => {
//         console.log(`🚀  Appplo Server ready at ${url}`);
//     });
// }else{
//     server.applyMiddleware({ app }); // app is from an existing express app
//     app.listen({ port: 4000 }, () =>
//         console.log(`🚀 Server ready at http://localhost:4000${server.graphqlPath}`)
//     )
// }



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
{//http://hyv.decade.tw:8080/query/inspection?tStart=0&tEnd=2580451909781&repeatTime=400&projection={%22_id%22:0,%22InspectionData.repeatTime%22:1}
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
    qStr["InspectionData.subFeatureDefSha1"]={$regex:req.query.subFeatureDefSha1};
  }
  
  let repeatTime = parseInt(req.query.repeatTime);
  if(repeatTime==repeatTime)
  {
    qStr["InspectionData.repeatTime"]={$lt:repeatTime};
  }


  return qStr;
}


app.get('/DELETE', function(req, res){
    let qStr = queryParamParse(req);
    console.log(qStr);

    mdb_connector.deleteMany("Inspection",qStr).
    then((result)=>{
        
    }).
    catch((err)=>{
    });

})


app.get('/query/deffile', function(req, res) {
    //http://hyv.decade.tw:8080/query/deffile?name=Test1|FC.&limit=1000
    let projection=req.query.projection;

    try{
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
    if(queryPage===undefined || queryPage<1)queryPage=1;

    if(queryLimit===undefined)queryLimit=100;

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

    

    console.log(qStr,queryPage,queryLimit);
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
        console.log("[X]Q by get Q FAIL!!");
    });



    // res.sendFile(path.join(__dirname+'/index.html'));

    // {InspectionData.time_ms : {$gt:1556187710991}}

});



function inspection_result_query(req, res)
{
    //http://localhost:8080/insp_time?tStart=2019/5/15/0:0:0&tEnd=2019/5/15/0:0:1
    //{InspectionData.time_ms : {$gt:1556640000000}}
    //http://localhost:8080/insp_time?tStart=2019/5/15/9:59:0&subFeatureDefSha1=.42a5.
    //http://hyv.decade.tw:8080/insp_time?tStart=2019/5/27/9:59:0&projection={"_id":0,"InspectionData.time_ms":1} only return time
    //http://hyv.decade.tw:8080/insp_time?tStart=2019/5/27/9:59:0&projection={"_id":0,"InspectionData.time_ms":1,"InspectionData.judgeReports":1}
    //Return time and judgeReports

    //Find the count of certain subFeatureDefSha1/comb
    //http://hyv.decade.tw:8080/query/inspection?tStart=0&tEnd=2581663256894&subFeatureDefSha1=497298a734229971|012a3ef713a9124d6799ac&projection={"_id":0,"InspectionData.subFeatureDefSha1":1}&agg=[{"$group":{"_id":"$InspectionData.subFeatureDefSha1","tl": {"$sum":1}}}]


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


    if(queryPage===undefined || queryPage<1)queryPage=1;

    if(queryLimit===undefined || queryLimit!==queryLimit)queryLimit=1000;
    //console.log("qStr",qStr);
    let queryAggRules=[];
    if(querySample==querySample)
      queryAggRules.push({ "$sample" : {size:querySample} });
    if(req.query.agg!==undefined)
    {
      let agg = JSON.parse(req.query.agg);
      queryAggRules=queryAggRules.concat(agg);
    }

    
    mdb_connector.query("Inspection",qStr,projection,queryAggRules).limit(queryLimit).skip((queryPage-1)*queryLimit).
    then((result)=>{
       console.log(result);
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
        console.log("[X]Q by get Q FAIL!!");
    });



    // res.sendFile(path.join(__dirname+'/index.html'));

    // {InspectionData.time_ms : {$gt:1556187710991}}

}


app.get('/query/inspection',inspection_result_query);
app.get('/insp_time',inspection_result_query);





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
app.listen(8080);


