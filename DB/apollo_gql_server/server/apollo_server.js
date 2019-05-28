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
app.get('/insp_time', function(req, res) {
    //http://localhost:8080/insp_time?tStart=2019/5/15/0:0:0&tEnd=2019/5/15/0:0:1
    //{InspectionData.time_ms : {$gt:1556640000000}}
    //http://localhost:8080/insp_time?tStart=2019/5/15/9:59:0&subFeatureDefSha1=.42a5.
    //http://hyv.decade.tw:8080/insp_time?tStart=2019/5/27/9:59:0&projection={"_id":0,"InspectionData.time_ms":1} only return time
    //http://hyv.decade.tw:8080/insp_time?tStart=2019/5/27/9:59:0&projection={"_id":0,"InspectionData.time_ms":1,"InspectionData.judgeReports":1}
    //Return time and judgeReports
    console.log(req.query.tStart);
    console.log(req.query.tEnd);

    let projection=req.query.projection;
    console.log(req.query.projection);
    try{
        projection=JSON.parse(projection);
    }
    catch (e) 
    {
        projection={"_id":0,"InspectionData.time_ms":1};
    }
    let start_MS = (new Date(req.query.tStart)).getTime();
    let endd=new Date(req.query.tEnd).getTime();
    let end_MS = (endd==endd)?endd:new Date().getTime();
    // let qStr={"InspectionData.time_ms" : {$gt:start_MS, $lt:end_MS},"InspectionData.subFeatureDefSha1"};
    let qStr={"InspectionData.time_ms" : {$gt:start_MS, $lt:end_MS}};
    if(req.query.subFeatureDefSha1!==undefined)
    {
        qStr["InspectionData.subFeatureDefSha1"]={$regex:req.query.subFeatureDefSha1};
    }

    if(req.query.subFeatureDefSha1!==undefined)
    {
        qStr["InspectionData.subFeatureDefSha1"]={$regex:req.query.subFeatureDefSha1};
    }
    console.log(start_MS);
    console.log(end_MS);
    console.log(qStr);
    mdb_connector.query("Inspection",qStr,projection).
    then((result)=>{
        // console.log(result);

        res.send(result);
        console.log("[O]Q by get Q OK!! len=" + result.length);
    }).
    catch((err)=>{
        res.send("[X]Q by get Q FAIL!!");
        console.log("[X]Q by get Q FAIL!!");
    });



    // res.sendFile(path.join(__dirname+'/index.html'));

    // {InspectionData.time_ms : {$gt:1556187710991}}

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


