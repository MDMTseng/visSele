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
app.ws('/', function(ws, req) {
    util.inspect(ws);
    ws.on('message', function(msg) {
        // CRUD_insertOne("Inspection",JSON.parse(jj));
        console.log("[WS][/]",msg);
    });
    console.log('socket /', req.testing);
});
app.ws('/query', function(ws, req) {
    ws.on('message', function(msg) {
        console.log("[WS][/query],RX_MSG=",msg);
        ws.send(msg);
    });
    console.log('socket echo,RX_MSG=', req.testing);
    mdb_connector.find({
        ts: {
            $gte: ISODate("2017-02-02 00:00:00").getTime(),
            $lt: ISODate("2017-02-05 00:00:00").getTime()
        }
    });
});

app.ws('/insert', function(ws, req) {
    ws.on('message', function(msg) {
        var RX_JSON=isJSON(msg);
        if(RX_JSON===false){
            console.log('[WS][RX][X_JSON],RX_MSG=', msg);
        }
        else{
            mdb_connector.insertOne("Inspection",RX_JSON);
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
app.listen(8010);


