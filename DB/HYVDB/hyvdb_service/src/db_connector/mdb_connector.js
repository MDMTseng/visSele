const mongoose =require('mongoose');
const MDB_ATLAS ="mongodb+srv://admin:0922923392@clusterhy-zqbuj.mongodb.net/DB_HY?retryWrites=true";
// const MDB_LOCAL="mongodb://localhost:27017/db_hy";
// let localStorage = require('localStorage');
const {Inspection_With_TS_Schema,DefineFile_With_TS_Schema}=require('../schema/schema.js') ;
mongoose.Promise = global.Promise;
mongoose.connect(MDB_ATLAS, {useNewUrlParser: true});
mongoose.pluralize(null);
// let db =mongoose.connection.once('open', () => console.log(`Connected to mongo at ${url}`));
let db = mongoose.connection;
db.on('error', console.error.bind(console, 'MongoDB connection error:'));
db.on('error', console.error.bind(console, 'error：'));
db.on('open', function (ref) {
    console.log('[O]Connected to mongo server.');
    Object.keys(db.models).forEach((collection) => {
        console.info("Collection=>"+collection);
    });
})

let InspectionModel_A = db.model('Machine_A', Inspection_With_TS_Schema);
let DefineFileModel_A = db.model('DefineFile_A', DefineFile_With_TS_Schema);
//featureSet_sha1
function CRUD_upsertOne(which,insertWhat){//return promise
 
    /*if(which=='df'){
        new DefineFileModel_A({DF:insertWhat}).findOneAndUpdate({"DF.data.featureSet_sha1":insertWhat.data.featureSet_sha1 }, insertWhat, {upsert:true}, function(err, doc){
            if (err) {
                handleError(err,insertWhat);
            }
        });
    }*/
    return CRUD_insertOne(which,insertWhat);
}
function CRUD_insertOne(which,insertWhat){
    //Without callback it will return promise
    if(which==='df'){
        return new DefineFileModel_A({DefineFile:insertWhat}).save();
    }else if(which==='Inspection'){
        return new InspectionModel_A({InspectionData:insertWhat}).save();
    }
}

function CRUD_insertOne_directInsertByDBconnection(CollectionNameString,insertWhat){
    // InspectionModel.insertMany(insertWhat,function(err,records){
    // db.collection(CollectionNameString).insertOne(insertWhat,function(err,records){
    db.collection(CollectionNameString).insertOne(insertWhat,function(err,records){
        if (err)
            return handleError(err);
        else{
            // console.log("Record added as " , records);
            return handleOK(records );
        }
    });
}
function CRUD_InsertMany(model,insertWhat){
    model.insertMany(insertWhat,function(err){
        if (err) return handleError(err);
        else
            return handleOK(insertWhat);
    });
}
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
function CRUD_deleteMany(CollectionNameString,findWhere={}){
    db.collection(CollectionNameString).deleteMany(findWhere, function(err, obj) {
        if (err) throw err;
        console.log("1 Collection deleted");
    });
}

function CRUD_query(which,queryCMD){
    //Without callback it will return promise
    if(which=='df'){
        return DefineFileModel_A.find(queryCMD);
    }else if(which=='Inspection'){
        return InspectionModel_A.find(queryCMD);

    }


}
function CRUD_Find(model,findWhere){
    model.find(findWhere).exec(function (err, r) {
        if (err) return handleError(err);
        else return console.log("rrr",r);
    })
}
function CRUD_Drop(CollectionNameString){
    db.collection(CollectionNameString).drop(function(err, delOK) {
        if (err) console.log("Collection drop w/error.",err);
        if (delOK) console.log("Collection droped");
    });
}
function CRUD_CreateCollection(collectionName){
    db.createCollection(collectionName, function(err, res) {
        if (err) throw err;
        console.log("Collection created! "+collectionName);
    });
}


function CRUD_ModelSave(model){
    model.save(function (err, ok) {
        if (err)
            return handleError(err);
        else
            return handleOK(model,ok);
    });
}

function findResult(r,){
    console.log("rrr",r);
}
function handleError(e,insertWhat){
    console.log("[X]ExceptionHandler"+e);
    console.log("[X][?]InsertFail trying local storage..."+e);
    let len=handleLocalStorage(insertWhat);
    console.log("[X][O]save to local storage OK! Len="+len);

}
function handleOK(model){
    console.log("handleOK>",model.ops[0]._id);
}
function handleLocalStorage(insertWhat){
    if (localStorage) {
        console.log("Local Storage: Supported");
        localStorage.setItem("HYVision",JSON.stringify(insertWhat));
        console.log("[O]Local Storage saved and Len=",localStorage.length);
        return localStorage.length;
    } else {
        console.log("Local Storage: Unsupported");
    }

    return 0;
}
module.exports = {
    insertOne:CRUD_insertOne,
    insertMany:CRUD_InsertMany,
    upsertOne:CRUD_upsertOne,
    query:CRUD_query
};