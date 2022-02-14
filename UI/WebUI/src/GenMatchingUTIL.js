




export const Schema_str_version={
  "type": "string",
  "pattern": "^(\\d+\\.)?(\\d+\\.)?(\\*|\\d+)(\-RC\\d)?$"
}

export const Schema_str_MD5={
  "type": "string",
  "pattern": "^[A-Fa-f0-9]{32}$"
}

export const Schema_obj_CameraInfo={
  type: "object",
  properties: {
  	name:{"type": "string"},
  	id:{"type": "string"},
  	serial_number:{"type": "string"},
  	refrenceImage:{"type": "string"},
  },
  required: ["name","id","serial_number"],
  
}

export const Schema_obj_inspRules_pose_est={
  type: "object",
  properties: {
    type: {enum:['pose_est']},
    id:{"type": "number"},
    ROI:{
    	"type": "object",
      properties: {
        x:{"type": "number"},
        y:{"type": "number"},
        w:{"type": "number"},
        h:{"type": "number"},
      },
      required: ["x","y","w","h"],
    },
 	},
  required: ["type","id","ROI"],
}

export const Schema_obj_inspRules_line={
  type: "object",
  properties: {
    type: {enum: ['line']},
    id:{"type": "number"},
    AAA:{"type": "string"},
 	},
  required: ["type","id","AAA"],
}


export const Schema_array_inspRules={
  type: "array",
  items: [
    Schema_obj_inspRules_pose_est,
    Schema_obj_inspRules_line
  ]
}

export const Schema_obj_inspSet={
  type: "object",
  properties: {
  	cameraInfo:Schema_obj_CameraInfo,
    
    inspRules:Schema_array_inspRules
  },
  required: ["cameraInfo"],
  additionalProperties: true,
}

export const Schema_GenMatchingDefFile = {
  type: "object",
  properties: {
    version: Schema_str_version,
    MD5:Schema_str_MD5,
    inspectionSet:{
      type: "array",
      items:Schema_obj_inspSet,
      minItems: 1,
    }
  },
  required: ["version","inspectionSet"],
  additionalProperties: true,
}
// var json = {
//   version: "100.5-RC3",
//   inspectionSet:[
//   	{
//       cameraInfo:{
// 				name:"AAA",
//         id:"XCV4969585",
//         serial_number:"394082903483-234982304"
//       },
//       inspRules:[{
//       	type:"locating",id:19,
//         ROI:[20,20,100,100]

//       },{	
//       	type:"line",id:39,
//         AAA:"sdsds"
//       }]
//     },
//     {
//       cameraInfo:{
// 				name:"AAAV",
//         id:"DDD",
//         serial_number:"394082903483-234982304"
//       },
//       poseEstimation:{

//       }
//     }
//   ]
// };
// var ajv = new Ajv({
//   v5: true,
//   coerceTypes: true,
//   useDefaults: true,
//   removeAdditional: 'all'
// });
// var validate = ajv.compile(schema);
// var result = validate(json);
// console.log('Result: ', result);
// console.log('Errors: ', validate.errors);
// console.log('Final Json: ', json);
