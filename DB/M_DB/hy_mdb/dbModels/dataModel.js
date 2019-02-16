
var mongoose = require('mongoose');
mongoose.Promise = global.Promise;
let old_connect_str="mongodb://clusterhy-zqbuj.mongodb.net:27017/DB_HY";
let mdb_atlas="mongodb+srv://admin:0922923392@clusterhy-zqbuj.mongodb.net/DB_HY?retryWrites=true";
mongoose.connect(mdb_atlas, {useNewUrlParser: true});
mongoose.Promise = global.Promise;
//Get the default connection
var db = mongoose.connection;
db.on('error', console.error.bind(console, 'MongoDB connection error:'));
db.on('error', console.error.bind(console, '连接错误：'))
db.once('open', (callback) => {
    console.log('MongoDB连接成功！！')
})
var SchemaX = mongoose.Schema;
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

var SomeModel = mongoose.model( '',schema3,'c2' );
// Create an instance of model SomeModel
// mongoose.deleteModel('tableName');
// SomeModel.remove([]);
let xxx=[];
for(let i=0;i<100;i++){
    xxx.push({id: i,id2: "z111"});
}
let xxx2=xxx.map((r)=> r+"a");
let xxx3=xxx.reduce((c,idx)=>c,xxx);
SomeModel.find({ id: '2' }).exec(function (err, r) {
    if (err) return handleError(err);
    else return console.log("rrr",r);
})
// SomeModel.insertMany(xxx,function(err){
//         if (err) return handleError(err);
//     else
//         return handleOK(xxx);
// });
// var awesome_instance = new SomeModel(xxx);

// SomeModel.create({ name: 'also_awesome' }, function (err, awesome_instance) {
//     if (err) return handleError(err);
//     else
//         return handleOK(awesome_instance);
// });

// awesome_instance.save(function (err, ok) {
//     if (err)
//         return handleError(err);
//     else
//         return handleOK(ok);
//     // saved!
// });
function findResult(r){
    console.log("rrr",r);
}
function handleError(e){
    console.log("create error"+e);
}
function handleOK(o){
    console.log("create OK");
    SomeModel.find({ id: '2' }).exec(function (err, r) {
        if (err) return handleError(err);
        else return console.log("rrr",r);
    })

}