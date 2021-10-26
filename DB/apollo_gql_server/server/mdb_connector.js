const mongoose =require('mongoose');
// const MDB_ATLAS ="mongodb+srv://admin:0922923392@clusterhy-zqbuj.mongodb.net/DB_HY?retryWrites=true";
// const MDB_ATLAS ="mongodb+srv://xceptionadmin:0982133498@xceptiondata.eqcea.mongodb.net/DB_HY?retryWrites=true&w=majority";

// const MDB_LOCAL="mongodb://localhost:27017/db_hy";
// let localStorage = require('localStorage');

const {Inspection_With_TS_Schema,DefineFile_With_TS_Schema,CustomDisplay_With_TS_Schema}=require('../schema/schema.js') ;




let db=undefined;
let InspectionModel_A=undefined;
let DefineFileModel_A=undefined;
let CustomDisplay_A=undefined;



function INIT_DB(dbUrl)
{
  if(db!==undefined)
  {
    return false;
  }
  mongoose.Promise = global.Promise;
  
  console.log(">>>>");
  return mongoose.connect(dbUrl, {
    keepAlive: 1,useNewUrlParser: true
  }).then(() =>
  {
    console.log('mongoDB connected...')
    console.log("1>>>>");
    mongoose.pluralize(null);
  
    let db = mongoose.connection;
    db.on('error', console.error.bind(console, 'MongoDB connection error:'));
    db.on('error', console.error.bind(console, 'error：'));
    db.on('open', function (ref) {
        console.log('[O]Connected to mongo server.');
        Object.keys(db.models).forEach((collection) => {
            console.info("Collection=>"+collection);
        });
    })
    
  
    InspectionModel_A = db.model(      'Machine_A', Inspection_With_TS_Schema);
    DefineFileModel_A = db.model(   'DefineFile_A', DefineFile_With_TS_Schema);
    CustomDisplay_A   = db.model('CustomDisplay_A', CustomDisplay_With_TS_Schema);
    return db;
  })
}



function CRUD_insertOne(which,insertWhat){
    //Without callback it will return promise
    if(which=='df'){
      return new DefineFileModel_A({DefineFile:insertWhat}).save();
    }else if(which=='Inspection'){
      return new InspectionModel_A({InspectionData:insertWhat}).save();
    }else if(which=='CustomDisplay'){
      //
      if(insertWhat._id!==undefined)
      {
        return CustomDisplay_A.updateOne(
          { _id: insertWhat._id },
          { $set: insertWhat}
        )
      }
      else
      {
        return new CustomDisplay_A(insertWhat).save();
      }
    }
}

function CRUD_deleteMany(which,queryCMD){
  if(which=='df'){
      return DefineFileModel_A.deleteMany(queryCMD);
  }else if(which=='Inspection'){
    return InspectionModel_A.deleteMany(queryCMD);
  }else if(which=='CustomDisplay'){
    return CustomDisplay_A.deleteMany(queryCMD);
  }
}

function CRUD_query(which,queryCMD,projection,etc=[]){
    //Without callback it will return promise

    let cmd=[];
    cmd.push({ "$match" : queryCMD});
    if(projection!==undefined)
      cmd.push({ "$project" :projection});
    cmd=cmd.concat(etc);
    if(which=='df'){
        //return DefineFileModel_A.find(queryCMD,projection);
      return DefineFileModel_A.aggregate(cmd);
    }else if(which=='Inspection'){
      //return InspectionModel_A.find(queryCMD,projection);
      return InspectionModel_A.aggregate(cmd)

    }else if(which=='CustomDisplay'){
        //return InspectionModel_A.find(queryCMD,projection);
      return CustomDisplay_A.aggregate(cmd)
    }


}
module.exports = {
    INIT:INIT_DB,
    deleteMany:CRUD_deleteMany,
    insertOne:CRUD_insertOne,
    query:CRUD_query
};