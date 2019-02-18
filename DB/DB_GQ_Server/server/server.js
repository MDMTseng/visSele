import express from 'express';
import bodyParser from 'body-parser';
import mongoose from 'mongoose';
import schema from '../schema/schema.js';
import { graphql } from 'graphql';
import { PubSub } from 'graphql-subscriptions';

export const pubsub = new PubSub();

const app = express();
const PORT = 4000;

const MDB_ATLAS ="mongodb+srv://admin:0922923392@clusterhy-zqbuj.mongodb.net/DB_HY?retryWrites=true";
const MDB_LOCAL="mongodb://localhost:27017/db_hy";
mongoose.Promise = global.Promise;
mongoose.connect(MDB_ATLAS, {useNewUrlParser: true});
let db = mongoose.connection;

//#########   basic gql test
let graphqlHTTP = require('express-graphql');
var { buildSchema } = require('graphql');
var schemaX = buildSchema(`
  type Query {
    hello: String
  }`);
var root = {
	hello: () => {
		return 'Hello world!';
	}
};
app.use(function(req, res, next) {
	res.header('Access-Control-Allow-Origin', 'http://localhost:8080');
	res.header('Access-Control-Allow-Headers', 'Content-Type, Authorization, Content-Length, X-Requested-With');
	if (req.method === 'OPTIONS') {
		res.sendStatus(200);
	} else {
		next();
	}
});
app.use('/gui', graphqlHTTP({
	schema: schemaX,
	rootValue: root,
	graphiql: true,
}));
//#########   basic gql test




db.on('error', console.error.bind(console, 'MongoDB connection error:'));
db.on('error', console.error.bind(console, 'error：'));
db.on('open', function (ref) {
	console.log('[O]Connected to mongo server.');
	Object.keys(db.models).forEach((collection) => {
		console.info("Collection=>"+collection);
	});
})
// db.once('open', (callback) => {
// 	console.log('MongoDB OK！！');
// 	collectionNames(function (err, names) {
// 		console.log(names); // [{ name: 'dbname.myCollection' }]
// 		module.exports.Collection = names;
// 	});
// });

app.use(bodyParser.text({
	type: 'application/graphql'
}));

app.post('/graphql', (req, res) => {
	graphql(schemaX, req.body).then((result) => {
		res.send(JSON.stringify(result, null, 2));
	});
});

const server = app.listen(PORT, () => {
	const host = server.address().address;
	const port = server.address().port;
	console.log(`Graphql is listening at https://${host}:${port}`);
});

function mdb_query(whichCollection,queryWhat){
	db.collection(whichCollection).find(queryWhat).toArray(function(err, result) {
		if (err) throw err;
		console.log(result);
		db.close();
	});
}

function dropDB(db){
	db.dropDatabase();
}
function basic_deleteMany(CollectionNameString,findWhere={}){
	db.collection(CollectionNameString).deleteMany(findWhere, function(err, obj) {
		if (err) throw err;
		console.log("1 Collection deleted");
	});
}
function basic_update(CollectionNameString,q,v){
	db.collection(CollectionNameString).updateOne(q, v, function(err, res) {
		if (err) throw err;
		console.log("1 document updated");
	});
}
function basic_query(CollectionNameString,q){
	db.collection(CollectionNameString).find(q).toArray(function(err, result) {
		if (err) throw err;
		console.log(result);
	});
}
function basic_Find(model,findWhere){
	model.find(findWhere).exec(function (err, r) {
		if (err) return handleError(err);
		else return console.log("rrr",r);
	})
}
function basic_Drop(CollectionNameString){
	db.collection(CollectionNameString).drop(function(err, delOK) {
		if (err) console.log("Collection drop w/error.",err);
		if (delOK) console.log("Collection droped");
	});
}
function basic_CreateCollection(collectionName){
	db.createCollection(collectionName, function(err, res) {
		if (err) throw err;
		console.log("Collection created! "+collectionName);
	});
}

function basic_InsertMany(model,insertWhat){
	model.insertMany(insertWhat,function(err){
		if (err) return handleError(err);
		else
			return handleOK(insertWhat);
	});
}
function basic_ModelSave(model){
	model.save(function (err, ok) {
		if (err)
			return handleError(err);
		else
			return handleOK(model,ok);
	});
}


function findResult(r){
	console.log("rrr",r);
}
function handleError(e){
	console.log("create error"+e);
}
function handleOK(model,o){
	console.log("handleOK>",model,o);


}