
let mongoose = require('mongoose');
// mongoose.Promise = global.Promise;//Mongoose 5.0 will use native promises by default
let old_connect_str="mongodb://clusterhy-zqbuj.mongodb.net:27017/DB_HY";
let mdb_atlas="mongodb+srv://admin:0922923392@clusterhy-zqbuj.mongodb.net/DB_HY?retryWrites=true";
let mdb_atlas_local="mongodb://localhost:27017/db_hy";



mongoose.connect(mdb_atlas, {useNewUrlParser: true});
mongoose.Promise = global.Promise;
//Get the default connection
let db = mongoose.connection;
db.on('error', console.error.bind(console, 'MongoDB connection error:'));
db.on('error', console.error.bind(console, '连接错误：'))
db.once('open', (callback) => {
    console.log('MongoDB连接成功！！');
    Object.keys(db.models).forEach((collection) => {
        // You can get the string name.
        console.info(collection);
        // Or you can do something else with the model.
        db.models[collection].remove({});
    });
})
let SchemaX = mongoose.Schema;
let schema1 = new SchemaX({
    id: String,
    id2: String
},{timestamps: { createdAt: 'createTime', updatedAt: 'updateTime' }});
let schema2 = new SchemaX({
    id: String,
    id2: String
}, {versionKey: false}, { autoCreate: true, capped: 1024 }, {timestamps: true});
let schema3 = new SchemaX({
    id: String,
    id2: String
},{timestamps: true});

let Schema_FeatureSets = new SchemaX({
    FeatureID: String,
    Filename: String,
    FeatureContents: String
},{timestamps: true});
let Schema_InspectionResult = new SchemaX({
    FeatureID: String,
    InspectionResult: String
},{timestamps: true});



let Model_FeatureSets = mongoose.model( 'Collection_FeatureSets',Schema_FeatureSets,'Collection_FeatureSets' );
let Model_InspectionResult = mongoose.model( 'Collection_InspectionResult',Schema_InspectionResult,'Collection_InspectionResult' );


let tempArr=[];
// basic_InsertMany(Model_FeatureSets,getTestArray(tempArr));
// basic_InsertMany(Model_InspectionResult,getTestArray(tempArr));
// Model_FeatureSets.deleteMany({});
let delQuery={};
// basic_Drop("Collection_FeatureSets");
// basic_Drop('Collection_InspectionResult');

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

function getTestArray(xxx){
    // let xxx=[];
    for(let i=0;i<100;i++){
        xxx.push({id: i,id2: "z111"});
    }
    let xxx2=xxx.map((r)=> r+"a");
    let xxx3=xxx.reduce((c,idx)=>c,xxx);
    return xxx;
}