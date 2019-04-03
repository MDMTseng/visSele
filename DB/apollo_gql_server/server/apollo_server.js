
const {typeDefs,resolvers}=require('../schema/schema.js') ;
const { ApolloServer } = require('apollo-server');

const server = new ApolloServer({ typeDefs, resolvers });

server.listen().then(({ url }) => {
    console.log(`🚀  Appplo Server ready at ${url}`);
});


var express = require('express');
var app = express();
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
        console.log("[WS][/]",msg);
    });
    console.log('socket /', req.testing);
});
app.ws('/query', function(ws, req) {
    ws.on('message', function(msg) {
        console.log("[WS][/query]",msg);
        ws.send(msg);
    });
    console.log('socket echo', req.testing);
});
app.ws('/insert', function(ws, req) {
    ws.on('message', function(msg) {
        console.log("[WS][/insert]",msg);
        // let MSG2JSON=JSON.parse(msg);
    });
    console.log('socket ws', req.testing);
});



app.listen(3333);


