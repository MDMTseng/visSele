const mongoose =require('mongoose');
const MDB_ATLAS ="mongodb+srv://admin:0922923392@clusterhy-zqbuj.mongodb.net/DB_HY?retryWrites=true";
const MDB_LOCAL="mongodb://localhost:27017/db_hy";
mongoose.Promise = global.Promise;
mongoose.connect(MDB_ATLAS, {useNewUrlParser: true});
let db = mongoose.connection;

db.on('error', console.error.bind(console, 'MongoDB connection error:'));
db.on('error', console.error.bind(console, 'error：'));
db.on('open', function (ref) {
    console.log('[O]Connected to mongo server.');
    Object.keys(db.models).forEach((collection) => {
        console.info("Collection=>"+collection);
    });
})


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