
const mongoose =require('mongoose');
let Inspection_With_TS_Schema = new mongoose.Schema(
    {InspectionData:{}},
    { timestamps: true }
    );
let DefineFile_With_TS_Schema = new mongoose.Schema(
    {DefineFile:{}},
    { timestamps: true }
);

var targetDeffiles_Schema  = new mongoose.Schema({ 
  featureSet_sha1_root: String,
  featureSet_sha1_pre: String,
  featureSet_sha1: String, trackTree: Boolean,
  path:String,
  name: String,
  tags:String
});

let CustomDisplay_With_TS_Schema = new mongoose.Schema(
    {
      name:  String,
      cat:  String,  
      targetDeffiles:[targetDeffiles_Schema],

    },
    { timestamps: true }
);

module.exports = {DefineFile_With_TS_Schema,Inspection_With_TS_Schema,CustomDisplay_With_TS_Schema};