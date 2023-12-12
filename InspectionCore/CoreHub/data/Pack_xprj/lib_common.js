
async function INIT()
{

}


function dec2binStr(num, digit = 32) { let retStr = ""; let rh = 1 << (digit - 1); for (let i = 0; i < digit; i++) { retStr += (num & rh) == 0 ? '0' : '1'; num *= 2; } return retStr; }

function rangeGen(from, to, count) {
  let seq = [];
  for (let i = 0; i < count; i++) {
    seq.push(from + (to - from) * (i / (count - 1)));
  }
  return seq
}

function RotMat(theda) {
  let s = Math.sin(theda);
  let c = Math.cos(theda);
  let m = 1;
  return [
    [m * c, m * -s],
    [m * s, m * c]
  ]
}

function MatxVec(M2x2, V2x1) {
  return [M2x2[0][0] * V2x1[0] + M2x2[0][1] * V2x1[1], M2x2[1][0] * V2x1[0] + M2x2[1][1] * V2x1[1]]
}
function VecAdd(V1_2x1, V2_2x1) {
  return [V1_2x1[0] + V2_2x1[0], V1_2x1[1] + V2_2x1[1]]
}

function VecSub(V1_2x1, V2_2x1) {
  return [V1_2x1[0] - V2_2x1[0], V1_2x1[1] - V2_2x1[1]]
}
function VecMult(V1_2x1, scalar) {
  return [V1_2x1[0] * scalar, V1_2x1[1] * scalar]
}



/*

[00  01  02]
[10  11  12]
[20  21  22]


*/

function MatxVec3x3(M3x3, V3x1) {
  return [
    M3x3[0][0] * V3x1[0] + M3x3[0][1] * V3x1[1] + M3x3[0][2] * V3x1[2],
    M3x3[1][0] * V3x1[0] + M3x3[1][1] * V3x1[1] + M3x3[1][2] * V3x1[2],
    M3x3[2][0] * V3x1[0] + M3x3[2][1] * V3x1[1] + M3x3[2][2] * V3x1[2]]
}



function FixedStringify(obj, fixedDig = 3) {
  return JSON.stringify(obj, function (key, val) {
    if (typeof val !== "number") return val;
    return val.toFixed ? Number(val.toFixed(fixedDig)) : val;
  })

}


function cbPromise(cb, param) {
  return new Promise((res, rej) => {
    cb((info) => {
      res(info)
    })

  })
}


function pt_pair_2_pt_pair_transform(pt_pair1, pt_pair2) {

  // console.log("click");


  let pt0_cam = pt_pair1[0];
  let pt0_head = pt_pair2[0];
  let pt1_cam = pt_pair1[1];
  let pt1_head = pt_pair2[1];




  let pre_offset = [-pt0_cam[0], -pt0_cam[1]];
  let post_offset = pt0_head;

  let m = Math.hypot(pt0_head[0] - pt1_head[0], pt0_head[1] - pt1_head[1]) /
    Math.hypot(pt0_cam[0] - pt1_cam[0], pt0_cam[1] - pt1_cam[1]);

  let rot1 =
    Math.atan2(pt1_head[1] - pt0_head[1], pt1_head[0] - pt0_head[0])

  let rot2 =//in camera corrd sys, Y in inverted
    Math.atan2(-(pt1_cam[1] - pt0_cam[1]), pt1_cam[0] - pt0_cam[0]);

  let rot = rot1 - rot2;

  let s = Math.sin(rot);
  let c = Math.cos(rot);

  //pre_offset
  let mat2x2 = [
    [m * c, m * -s * -1],
    [m * s, m * c * -1]
  ]
  //post_offset



  // {
  //   let vec=VecAdd(pt1_cam,pre_offset);
  //   vec=MatxVec(mat2x2,vec);
  //   vec=VecAdd(vec,post_offset);

  //   console.log(pre_offset,post_offset,m,rot,mat2x2);

  //   console.log(pt1_cam,vec,pt1_head);

  // }

  return {
    pre_offset,
    mat2x2,
    post_offset
  }
  //
  // let vec=[info.x,info.y]
  // vec=VecAdd(vec,pre_offset);
  // vec=MatxVec(mat2x2,vec);
  // vec=VecAdd(vec,post_offset);

}

async function CNCSend(data) {
    if (data.id === undefined) {
        if(v.ddddIdx==undefined)v.ddddIdx=0;
        data.id =v.ddddIdx;
        v.ddddIdx++;
    }

    let retPkgs=await InspTargetExchange("CNCPeripheral", { type: "MESSAGE", msg: data });
    let retMsg = retPkgs.find((p) => p.type == "PD");
    return retMsg?.data?.msg;
}



async function G(code)
{
  return CNCSend({"type":"GCODE","code":code})
}

let SYS_OUT_PIN_DEF = {

    Feeder_L_Light1: 30,
    Feeder_C_CAM1: 31,
    Feeder_A_Run1: 11,
    Feeder_A_Run2: 12,
    Feeder_A_Run3: 13,
    Feeder_A_Run4: 14,
  
  
    Insp_L_Light1: 25,
    Insp_L_Light2: 26,
    Insp_C_CAM1: 27,
  
  
    Reel_L_Light1: 16,
    Reel_C_CAM1: 15,
    Reel_A_Run1: 29,
  
  
    Arm_A_Pick1: 0,
    Arm_A_Pick2: 1,
    Arm_A_Pick3: 2,
    Arm_A_Pick4: 3,
  
  
  }
  
  

let DBG_TAG_IMG_SV = "X";//"DBG_IM_SV"

let DBG_TAG = DBG_TAG_IMG_SV;

let SYS_CAM_INFO = {
  Feeder_CAM1: {
    pin: SYS_OUT_PIN_DEF.Feeder_C_CAM1,
    id: "Hikvision-2BDF73541011-00E73541011",
    brif_id: "00E73541011",

  },
  Reel_CAM1: {
    pin: SYS_OUT_PIN_DEF.Reel_C_CAM1,
    id: "Hikrobot-2BDF79315117-00K79315117",
    brif_id: "00K79315117"
  },
  Insp_CAM1: {
    pin: SYS_OUT_PIN_DEF.Insp_C_CAM1,
    id: "Hikrobot-2BDF71598890-00F71598890",
    brif_id: "00F71598890"
  },


}

  // await G(`G01 Y250 X380 ${WF}`)
  // await ackG("M400");
  // await G(`G01 X100 ${WF}`)
  // await G(`G01 X350 ${WF}`)
  

let SYS_CONFIG = {
  SAFE_Z: 70,
  SAFE_Z_SensorLock: 70.5,

  OBJ_W:2,
  OBJ_L:3,
  OBJ_H:7,

  FeederReadyPickZ:50,
  FeederNearPickZ:50,//15,
  FeederPickZ:13.5-7,



  ReelNearPickZ:19,
  ReelPlaceZ:9,

  SAFE_RX: 350,
  SAFE_LX: 150,

  INSP_Y: 260 - 2,
  INSP_X: 281,
  INSP_Z: 54,

  INSP_Z1234_Rough_Dist: 39,

  INSP_X_R: 380,
  INSP_X_L: 100,

  //sensor in bit field
  ES_SENSOR_ALL: 0xFF,
  ES_SENSOR_Z: 0xF,
  ES_SENSOR_Y: 0x80,

  MAXF: 1000,
  MAXACC: 3000,



  Reel_Tar_Y: 8,

}



let END_STOP_PINS=SYS_CONFIG.ES_SENSOR_ALL;

async function Enter_Z2SafeZone_n_Check(F = `F${SYS_CONFIG.MAXF} ACC${SYS_CONFIG.MAXACC}`, doS1Check = true) {
    let ENDSTOPFULL = SYS_CONFIG.ES_SENSOR_ALL;
    let ENDSTOPZ = SYS_CONFIG.ES_SENSOR_Z;
    if (doS1Check) {
  
      END_STOP_PINS&=~ENDSTOPZ;
      await G(`M120.1  PINS${END_STOP_PINS} PNS${ENDSTOPFULL}`);
  
      let ZpreSafe = SYS_CONFIG.SAFE_Z;
      await G(`G01 Z1_${ZpreSafe} Z2_${ZpreSafe} Z3_${ZpreSafe} Z4_${ZpreSafe} ${F}`)
  
  
      // await G(`M400`)
      END_STOP_PINS|=ENDSTOPZ;
      await G(`M120.1 PINS${END_STOP_PINS} PNS${ENDSTOPFULL}`);//enable
    }
  
    {

      END_STOP_PINS&=~ENDSTOPZ;
      await G(`M120.1  PINS${END_STOP_PINS} PNS${ENDSTOPFULL - ENDSTOPZ}`);
  
      let ZSafe = SYS_CONFIG.SAFE_Z_SensorLock;
      await G(`G01 Z1_${ZSafe} Z2_${ZSafe} Z3_${ZSafe} Z4_${ZSafe} ${F}`)
  
  
      // await G(`M400`)
      END_STOP_PINS|=ENDSTOPZ;
      await G(`M120.1  PINS${END_STOP_PINS} PNS${ENDSTOPFULL - ENDSTOPZ}`);
    }
  
    // await G(`G04 P10`);//HACK somehow PORT isn't really update until there is a breath time in ESP32....
  }
  


  
async function Exit_Z2SafeZone_n_Check() {
    let ENDSTOPFULL = SYS_CONFIG.ES_SENSOR_ALL;
    let ENDSTOPZ = SYS_CONFIG.ES_SENSOR_Z;
  
    END_STOP_PINS&=~ENDSTOPZ;
    await G(`G04 P4`);
    await G(`M120.1  PINS${END_STOP_PINS} PNS${ENDSTOPFULL - ENDSTOPZ}`);
  }
  

  async function test3() {

    let initM114={X:0,Y:0,Z1_:0,Z2_:0,Z3_:0,Z4_:0}
    initM114.X = Math.round(initM114.X * 100) / 100;
    initM114.Y = Math.round(initM114.Y * 100) / 100;
    initM114.Z1_ = Math.round(initM114.Z1_ * 100) / 100;
  
    let advUnit = 1;
  
    let ACC = "3000";
    let F = "500";
  
    let InspTarUI_Feeder_API_Set = {}
    let InspTarUI_Reel_API_Set = {}
    let ZR_Idx = 1;
  
  
  
    let _UIStack=[];
  
  
  
    function UIStack_Current()
    {
      return _UIStack[_UIStack.length-1];
    }
  
    function UIStackBack(updateCB,resultInfo)
    {
      if(_UIStack.length==1)return;
      
      let UIInfo = _UIStack.pop(); 
      if(UIInfo.result!==undefined)
      {
        UIInfo.result(resultInfo) 
      }
  
      updateCB(UIStack_Current().UI)
    }
  
    function UIStackGo(updateCB,UI,resultCB)
    {
      _UIStack.push({
        UI,
        result:resultCB
      })
      if(updateCB!==undefined)
        updateCB(UIStack_Current().UI)
    }
  
  
    let JOG_UI=    {
      text: () => "X:" + initM114.X.toFixed(3) + " Y:" + initM114.Y.toFixed(3) +" Z" + ZR_Idx + ":" + initM114[`Z${ZR_Idx}_`].toFixed(2),
      opts: ["$t:X:", "<←", "←", "→", "→>", "$\n", "$t:Y:", "<☉>",  "☉", "⊗","<⊗>", "$\n", "$t:Z:","<↑>", "↑", "↓", "<↓>", "SAFE_ZONE", "UNSAFE_ZONE"],
      callback: async (idx, value, updateCB) => {
  
  
  
  
        initM114 = (await G("M114")).M114;
        let FA = `F${F} ACC${ACC}`
        switch (value) {
          case "→>":
            initM114.X += advUnit;
            break;
          case "→":
            initM114.X += advUnit / 10;
            break;
          case "<←":
            initM114.X -= advUnit;
            break;
          case "←":
            initM114.X -= advUnit / 10;
            break;
  
          case "<⊗>":
            initM114.Y += advUnit;
            break;
          case "⊗":
            initM114.Y += advUnit / 10;
            break;
          case "<☉>":
            initM114.Y -= advUnit;
            break;
          case "☉":
            initM114.Y -= advUnit / 10;
            break;
          case "<↑>":
            initM114[`Z${ZR_Idx}_`] += advUnit;
            await G(`G1 Z${ZR_Idx}_${initM114[`Z${ZR_Idx}_`]} ${FA}`)
            break;
  
          case "↑":
            initM114[`Z${ZR_Idx}_`] += advUnit / 10;
            await G(`G1 Z${ZR_Idx}_${initM114[`Z${ZR_Idx}_`]} ${FA}`)
            break;
          case "↓":
            initM114[`Z${ZR_Idx}_`] -= advUnit / 10;
            await G(`G1 Z${ZR_Idx}_${initM114[`Z${ZR_Idx}_`]} ${FA}`)
            break;
          case "<↓>":
            initM114[`Z${ZR_Idx}_`] -= advUnit;
            await G(`G1 Z${ZR_Idx}_${initM114[`Z${ZR_Idx}_`]} ${FA}`)
            break;
          case "SAFE_ZONE":
            await Enter_Z2SafeZone_n_Check();
            await Exit_Z2SafeZone_n_Check();
            await ackG("M400");
            let M114 = (await G("M114")).M114;
            initM114[`Z${ZR_Idx}_`] = M114[`Z${ZR_Idx}_`];
            // cbInfo[idx].text =  "Z1:" +M114.Z1_.toFixed(2)
            updateCB(UIStack_Current().UI)
            return;
  
  
          case "UNSAFE_ZONE":
  
            await Exit_Z2SafeZone_n_Check();
        }
        await G(`G1 X${initM114.X} Y${initM114.Y} ${FA}`)
        await ackG("M400");
        // let M114=(await G("M114")).M114;
        // cbInfo[idx].text = "X:" +M114.X.toFixed(3)+" Y:"+M114.Y.toFixed(3)
        updateCB(UIStack_Current().UI)
      }, default: 0
    }
  
  
    let Calib_UI={
      text: () => undefined,
      opts: [ 
      {
        type: "button", key: "Btn_ShowCalib",
        text: () => "Show calibInfo",
        onClick: async (updateCB) => {
          console.log(v.CalibInfo)
        }
      }]
    }
    let Feeder_loc_UIInfo={
        "pt1": {
          "headXY": [NaN,NaN,NaN
          ],
          "fcamXY": [NaN,NaN
          ]
        },
        "pt2": {
          "headXY": [NaN,NaN,NaN
          ],
          "fcamXY": [NaN,NaN
          ]
        }
    }
    let Feeder_loc_UI= {
      text: () => ">>",
      opts: [ 
        
        {
          type: "button",
          text: "SHOT_Feeder",
          onClick: async (updateCB) => {
  
            let FA = `F${F} ACC${ACC}`
            await Enter_Z2SafeZone_n_Check();
            await G(`G1 Y${150} ${FA}`)
  
            await G(`M400`)
            console.log(await FeederCheck(false))
          }
        },
        {
          type: "button",
          text: "FeederManCalib",
          onClick: async (updateCB) => {
  
            let FA = `F${F} ACC${ACC}`
            await Enter_Z2SafeZone_n_Check();
            await G(`G1 Y${150} ${FA}`)
  
            await G(`M400`)
            console.log(await FeederCheck())
  
  
  
            let pt0_cam = v.Calib_Feeder_XY_Info["pt1"].fcamXY;
            let pt0_head = v.Calib_Feeder_XY_Info["pt1"].headXY;
            let pt1_cam = v.Calib_Feeder_XY_Info["pt2"].fcamXY;
            let pt1_head = v.Calib_Feeder_XY_Info["pt2"].headXY;
  
  
  
            lv.convPack = pt_pair_2_pt_pair_transform(
              [pt0_cam, pt1_cam],
              [pt0_head, pt1_head])
  
  
            let pre_offset = lv.convPack.pre_offset
            let mat2x2 = lv.convPack.mat2x2
            let post_offset = lv.convPack.post_offset
  
            console.log("1")
            lv.testGo = "Wait input 1/4";updateCB(UIStack_Current().UI)
  
            let info1 = await cbPromise(InspTarUI_Feeder_API_Set.onMouseClick)
  
  
  
            console.log(info1)
  
  
            let infos = [info1];
            for (let i = 0; i < infos.length; i++) {
              let info = infos[i]
              let headIdx = 1;//ZR_Idx;//1~4
  
  
              let zcalib = v.CalibInfo.Z_mm2pix.offsets;
              let headOffset = v.CalibInfo.headOffset;
  
  
  
              let XOffset = -(headIdx - 1) * (118.2 / 3);
  
  
              let vec = [info.x, info.y]
              vec = VecAdd(vec, pre_offset);
              vec = MatxVec(mat2x2, vec);
              vec = VecAdd(vec, post_offset);
              lv.testGo = vec;
              // updateCB(cbInfo)
  
              updateCB(UIStack_Current().UI)
  
  
  
              let FA = `F${F} ACC${ACC}`
              let ZFA = `F${F} ACC${4500}`
              await Enter_Z2SafeZone_n_Check();
              await Exit_Z2SafeZone_n_Check();
  
              let locPos = VecAdd(vec, headOffset[headIdx].xyOffset);
              await G(`G1 X${locPos[0]} Y${locPos[1]} R${headIdx}_${0}  ${FA}`)
              // await G(`G1 Z${headIdx}_${initM114.Z1_} ${FA}`)
  
              let Zh = zcalib[headIdx].mm_offset + 18//initM114.Z1_+;
              await G(`G1 Z${headIdx}_${Zh} ${ZFA}`)
  
  
            }
  
          }
        },"$\n",
        ...['pt1', 'pt2'].map(ptn => {
          return [
            "$t:" + ptn + ":",
            {
              type: "button", key: "HeadXY_Set" + ptn,
              text: () => ">>>",
              onClick: async (updateCB) => {
  
                // console.log("click");
                // await ackG("M400");
                // let M114 = (await G("M114")).M114;
                // v.Calib_Feeder_XY_Info[ptn].headXY = [M114.X, M114.Y, M114.Z1_]
  
                // updateCB(cbInfo)
  
              }
            }, "$t:       ",
            {
              type: "button", key: "HeadXY_Set" + ptn + "_go",
              text: "GoXY",
              onClick: async (updateCB) => {
                let FA = `F${F} ACC${ACC}`
  
                // for(let kk=0;kk<5;kk++)
                // for( let ptn of ['pt1','pt2'])
                // {
                await Enter_Z2SafeZone_n_Check();
                await Exit_Z2SafeZone_n_Check();
                let TarXY = v.Calib_Feeder_XY_Info[ptn].headXY;
  
                await G(`G1 X${TarXY[0]} Y${TarXY[1]} ${FA}`)
                await G(`G1 Z1_${initM114.Z1_} ${FA}`)
  
                await G(`M400`)
                // let encv=await CNC_api.send_P({"type":"AUX_ENC_V"})
  
                // progressUpdate(">>>"+encv.enc_v)
                // NOT_ABORT();
                // }
  
  
              }
            },
            {
              type: "button", key: "fCAMXY_Set1" + ptn,
              text: () => "KKK",
              onClick: async (updateCB) => {
                console.log("click");
                InspTarUI_Feeder_API_Set.onMouseClick(async (info) => {
  
  
                  // console.log(info);
                  // v.Calib_Feeder_XY_Info[ptn].fcamXY = [info.x, info.y]
                  // updateCB(cbInfo)
  
                })
              }
            },
            "$\n",
          ]
        }).flat(),
  
        ...['pt1', 'pt2'].map(ptn => {
          return [
            "$t:>>>" + ptn + ":",
            {
              type: "button", key: "xxHeadXY_Set" + ptn,
              text: () => FixedStringify(Feeder_loc_UIInfo[ptn].headXY),
              onClick: async (updateCB) => {
  
  
                // updateCB(cbInfo)
  
              }
            }, "$t:       ",
            {
              type: "button", key: "xxHeadXY_Set" + ptn + "_go",
              text: "GoXY",
              onClick: async (updateCB) => {
                let FA = `F${F} ACC${ACC}`
  
                // for(let kk=0;kk<5;kk++)
                // for( let ptn of ['pt1','pt2'])
                // {
                await Enter_Z2SafeZone_n_Check();
                await Exit_Z2SafeZone_n_Check();
                let TarXY = Feeder_loc_UIInfo[ptn].headXY;
  
                await G(`G1 X${TarXY[0]} Y${TarXY[1]} ${FA}`)
                await G(`G1 Z1_${initM114.Z1_} ${FA}`)
  
                await G(`M400`)
                // let encv=await CNC_api.send_P({"type":"AUX_ENC_V"})
  
                // progressUpdate(">>>"+encv.enc_v)
                // NOT_ABORT();
                // }
  
  
              }
            },
            {
              type: "button", key: "xxfCAMXY_Set1" + ptn,
              text: () => FixedStringify(Feeder_loc_UIInfo[ptn].fcamXY),
              onClick: async (updateCB) => {
                console.log("click");
                InspTarUI_Feeder_API_Set.onMouseClick(async (info) => {
  
  
                  console.log(info);
                  Feeder_loc_UIInfo[ptn].fcamXY = [info.x, info.y]
  
                  let headOffset = v.CalibInfo.headOffset;
  
  
  
                  let pt0_cam = v.Calib_Feeder_XY_Info["pt1"].fcamXY;
                  let pt0_head = v.Calib_Feeder_XY_Info["pt1"].headXY;
                  let pt1_cam = v.Calib_Feeder_XY_Info["pt2"].fcamXY;
                  let pt1_head = v.Calib_Feeder_XY_Info["pt2"].headXY;
  
  
  
                  let convPack = pt_pair_2_pt_pair_transform(
                    [pt0_cam, pt1_cam],
                    [pt0_head, pt1_head])
  
  
                    
  
  
                  let pre_offset = convPack.pre_offset
                  let mat2x2 = convPack.mat2x2
                  let post_offset = convPack.post_offset
  
  
  
                  let vec = [info.x, info.y]
                  vec = VecAdd(vec, pre_offset);
                  vec = MatxVec(mat2x2, vec);
                  vec = VecAdd(vec, post_offset);
                  lv.testGo = vec;
                  // updateCB(cbInfo)
      
                  updateCB(UIStack_Current().UI)
      
      
      
                  let FA = `F${F} ACC${ACC}`
                  let ZFA = `F${F} ACC${4500}`
                  await Enter_Z2SafeZone_n_Check();
                  await Exit_Z2SafeZone_n_Check();
  
                  let headIdx=1;
      
                  let locPos = VecAdd(vec, headOffset[headIdx].xyOffset);
                  await G(`G1 X${locPos[0]} Y${locPos[1]} R${headIdx}_${0}  ${FA}`)
  
                  let zcalib = v.CalibInfo.Z_mm2pix.offsets;
  
                  let Zh = zcalib[headIdx].mm_offset + SYS_CONFIG.FeederNearPickZ//initM114.Z1_+;
                  await G(`G1 Z${headIdx}_${Zh} ${ZFA}`)
                  
                  await G(`M400`)
                  let sinfo=[
  
                    {
                      text: () => "UI3...",
                      opts: [
                        {
                          type: "button", key: "back",
                          text: () => "<=",
                          onClick: async (updateCB) => {
  
                            console.log("click");
                            await ackG("M400");
                            let M114 = (await G("M114")).M114;
              
              
                            
                            UIStackBack(updateCB,[M114.X, M114.Y, M114.Z1_]);
  
  
  
  
                          }
                        },],
                      callback: async (idx, key, updateCB) => {
                        // updateCB(cInfo)
                      }, default: 0
                    },
                    JOG_UI
                    // Reel_loc_UI,
                  ]
                  UIStackGo(updateCB,sinfo,(result)=>{
                    console.log(result)
  
                    Feeder_loc_UIInfo[ptn].headXY = result
                  })
      
                })
              }
            },
            "$\n",
          ]
        }).flat(),
  
  
        {
          type: "button", key: "Update",
          text: () => "Update Mapping Info" ,
          onClick: async (updateCB) => {
            v.Calib_Feeder_XY_Info=Feeder_loc_UIInfo;
  
            updateCB(UIStack_Current().UI)
          }
        },
  
        {
          type: "button", key: "CamFeederGo_Auto",
          text: () => "CamAutoPick",
          onClick: async (updateCB) => {
            console.log("click");
  
            await FeederPickAuto();
            // await G(`G4 P2000`)
          }
        }, "$t:       ",
        {
          type: "button",
          text: "Release",
          onClick: async (updateCB) => {
  
            await G(`G1 R1_0  R2_0  R3_0  R4_0`)
            await G(`M42 P0 S0`)
            await G(`M42 P1 S0`)
            await G(`M42 P2 S0`)
            await G(`M42 P3 S0`)
          }
        },
  
        {
          type: "button",
          text: "GoInsp",
          onClick: async (updateCB) => {
  
            console.log(v.CalibInfo);
  
            let FA = `F${1500} ACC${4500}`
            let X = 350;
  
            await Exit_Z2SafeZone_n_Check();
  
            if (0) {
              let t;
  
              let p1 = 10;
              let p2 = 70;
              p1 = 300;
              p2 = 70;
              t = Date.now();
              await G(`G1 X${p1} ${FA}`)
              await G(`G1 X${0} ${FA}`)
              await G(`M400`)
              console.log(Date.now() - t);
  
  
  
              t = Date.now();
              await G(`G1 Y${p1} ${FA}`)
              await G(`G1 Y${p2} ${FA}`)
              await G(`M400`)
              console.log(Date.now() - t);
  
  
              p1 = 10;
              p2 = 70;
              t = Date.now();
              await G(`G1 Z1_${p1} ${FA}`)
              await G(`G1 Z1_${p2} ${FA}`)
              await G(`M400`)
              console.log(Date.now() - t);
            }
  
            if (0) {
              await G(`G1 X${50} ${FA}`)
              await G(`M400`)
              FA = `F${1200} ACC${4500}`
              let xAdv = 39;
              let ydiff = 0.3;
              let zdiff = 0;
              let gX = 50;
              let gY = 40;
              let gZ = 70;
  
              await G(`G4 P100`);
  
              for (let i = 0; i < 2; i++) {
                gX += xAdv; gY += ydiff; gZ += zdiff;
                await G(`G1 X${gX} Y${gY} Z1_${gZ} ${FA}`);
  
                await G(`M42 P${25} S1 `)
  
                await G(`G1 X${gX + xAdv / 2} ${FA}`);
                await G(`M42 P${25} S0 `)
  
                gX += xAdv; gY -= ydiff; gZ -= zdiff;
                await G(`G1 X${gX} Y${gY} Z1_${gZ} ${FA}`);
  
  
                await G(`M42 P${25} S1 `)
  
                await G(`G1 X${gX + xAdv / 2} ${FA}`);
                await G(`M42 P${25} S0 `)
              }
              await Enter_Z2SafeZone_n_Check();
              await Exit_Z2SafeZone_n_Check();
            }
  
            await OBJInspStationRun();
          }
        },
  
  
        {
          type: "button",
          text: "Image try load",
          onClick: async (updateCB) => {
            if (await ImageLoadFromInspTar("ImDataSave", ["00F71598890"], ["ObjInsp"], 651, 2, (cands) => cands[0]) === undefined) {
              return;
            }
          }
        },
        "$\n",
        {
          type: "InspTar_UI",
          id: "shapeMat_Feeder",
          params:{style:{float:"left",width:"90%",height:"600px"}},
          APIExport: (api_set) => {
            InspTarUI_Feeder_API_Set = api_set;
            console.log("APIExport")
          }
        }
      ],
  
      callback: async (idx, key, updateCB) => {
        switch(key)
        {
  
        }
      }
    }
  
    let Reel_loc_UIInfo={
      "pt1": {
        "headXY": [NaN,NaN,NaN
        ],
        "fcamXY": [NaN,NaN
        ]
      },
      "pt2": {
        "headXY": [NaN,NaN,NaN
        ],
        "fcamXY": [NaN,NaN
        ]
      }
    }
    let Reel_loc_UI=    {
      text: () => ">>",
      opts: [  "Go_Reel", "ReelCheck", "ReelCheckTrack", "ReelLocReset", "$\n",
      ...['pt1', 'pt2'].map(ptn => {
  
        return [
          "$t:" + ptn + ":",
          {
            type: "button", key: "ReelXY_Set" + ptn,
            text: () => FixedStringify(v.Calib_Reel_XY_Info[ptn].headXY),
            onClick: async (updateCB) => {
  
              // console.log("click");
              // await ackG("M400");
              // let M114 = (await G("M114")).M114;
              // v.Calib_Reel_XY_Info[ptn].headXY = [M114.X, M114.Y, M114.Z1_]
  
              // updateCB(cbInfo)
  
            }
          }, "$t:       ",
          {
            type: "button", key: "HeadXY_Set" + ptn + "_go",
            text: "GoXY",
            onClick: async (updateCB) => {
              let FA = `F${F} ACC${ACC}`
  
              // for(let kk=0;kk<5;kk++)
              // for( let ptn of ['pt1','pt2'])
              // {
              console.log(v.Calib_Reel_XY_Info);
              await Enter_Z2SafeZone_n_Check();
              await Exit_Z2SafeZone_n_Check();
              let TarXY = v.Calib_Reel_XY_Info[ptn].headXY;
  
              await G(`G1 X${TarXY[0]} Y${TarXY[1]} ${FA}`)
              await G(`G1 Z1_${SYS_CONFIG.Reel_Tar_Y} ${FA}`)
  
              await G(`M400`)
  
              // let encv=await CNC_api.send_P({"type":"AUX_ENC_V"})
  
              // progressUpdate(">>>"+encv.enc_v)
              // NOT_ABORT();
              // }
  
  
            }
          },
          {
            type: "button", key: "fCAMXY_Set1" + ptn,
            text: () => FixedStringify(v.Calib_Reel_XY_Info[ptn].fcamXY),
            onClick: async (updateCB) => {
              console.log("click");
              InspTarUI_Reel_API_Set.onMouseClick(async (info) => {
  
  
                console.log(info);
                // v.Calib_Reel_XY_Info[ptn].fcamXY = [info.x, info.y]
                // updateCB(cbInfo)
  
              })
            }
          },
          "$\n",
        ]
      }).flat(),
  
  
      ...['pt1', 'pt2'].map(ptn => {
        return [
          "$t:>>>" + ptn + ":",
          {
            type: "button", key: "xxHeadXY_Set" + ptn,
            text: () => FixedStringify(Reel_loc_UIInfo[ptn].headXY),
            onClick: async (updateCB) => {
  
  
              // updateCB(cbInfo)
  
            }
          }, "$t:       ",
          {
            type: "button", key: "xxHeadXY_Set" + ptn + "_go",
            text: "GoXY",
            onClick: async (updateCB) => {
              let FA = `F${F} ACC${ACC}`
  
              // for(let kk=0;kk<5;kk++)
              // for( let ptn of ['pt1','pt2'])
              // {
              await Enter_Z2SafeZone_n_Check();
              await Exit_Z2SafeZone_n_Check();
              let TarXY = Reel_loc_UIInfo[ptn].headXY;
  
              await G(`G1 X${TarXY[0]} Y${TarXY[1]} ${FA}`)
              await G(`G1 Z1_${initM114.Z1_} ${FA}`)
  
              await G(`M400`)
              // let encv=await CNC_api.send_P({"type":"AUX_ENC_V"})
  
              // progressUpdate(">>>"+encv.enc_v)
              // NOT_ABORT();
              // }
  
  
            }
          },
          {
            type: "button", key: "xxfCAMXY_Set1" + ptn,
            text: () => FixedStringify(Reel_loc_UIInfo[ptn].fcamXY),
            onClick: async (updateCB) => {
              console.log("click");
              InspTarUI_Reel_API_Set.onMouseClick(async (info) => {
  
  
                console.log(info);
                Reel_loc_UIInfo[ptn].fcamXY = [info.x, info.y]
  
                let headOffset = v.CalibInfo.headOffset;
  
  
  
                let pt0_cam = v.Calib_Reel_XY_Info["pt1"].fcamXY;
                let pt0_head = v.Calib_Reel_XY_Info["pt1"].headXY;
                let pt1_cam = v.Calib_Reel_XY_Info["pt2"].fcamXY;
                let pt1_head = v.Calib_Reel_XY_Info["pt2"].headXY;
  
  
  
                let convPack = pt_pair_2_pt_pair_transform(
                  [pt0_cam, pt1_cam],
                  [pt0_head, pt1_head])
  
  
                  
  
  
                let pre_offset = convPack.pre_offset
                let mat2x2 = convPack.mat2x2
                let post_offset = convPack.post_offset
  
  
  
                let vec = [info.x, info.y]
                vec = VecAdd(vec, pre_offset);
                vec = MatxVec(mat2x2, vec);
                vec = VecAdd(vec, post_offset);
                lv.testGo = vec;
                // updateCB(cbInfo)
    
                updateCB(UIStack_Current().UI)
    
    
    
                let FA = `F${F} ACC${ACC}`
                let ZFA = `F${F} ACC${4500}`
                await Enter_Z2SafeZone_n_Check();
                await Exit_Z2SafeZone_n_Check();
  
                let headIdx=1;
    
                let locPos = VecAdd(vec, headOffset[headIdx].xyOffset);
                await G(`G1 X${locPos[0]} Y${locPos[1]} R${headIdx}_${0}  ${FA}`)
  
                let zcalib = v.CalibInfo.Z_mm2pix.offsets;
  
                let Zh = zcalib[headIdx].mm_offset + SYS_CONFIG.ReelNearPickZ//initM114.Z1_+;
                await G(`G1 Z${headIdx}_${Zh} ${ZFA}`)
                
                await G(`M400`)
                let sinfo=[
  
                  {
                    text: () => "UI3...",
                    opts: [
                      {
                        type: "button", key: "back",
                        text: () => "<=",
                        onClick: async (updateCB) => {
  
                          console.log("click");
                          await ackG("M400");
                          let M114 = (await G("M114")).M114;
            
            
                          
                          UIStackBack(updateCB,[M114.X, M114.Y, M114.Z1_]);
  
  
  
  
                        }
                      },],
                    callback: async (idx, key, updateCB) => {
                      // updateCB(cInfo)
                    }, default: 0
                  },
                  JOG_UI
                  // Reel_loc_UI,
                ]
                UIStackGo(updateCB,sinfo,(result)=>{
                  console.log(result)
  
                  Reel_loc_UIInfo[ptn].headXY = result
                })
    
              })
            }
          },
          "$\n",
        ]
      }).flat(),
  
  
      {
        type: "button", key: "Update",
        text: () => "Update Mapping Info" ,
        onClick: async (updateCB) => {
          v.Calib_Reel_XY_Info=Reel_loc_UIInfo;
  
          updateCB(UIStack_Current().UI)
        }
      },
  
      {
        type: "button", key: "CamReelGo_Auto",
        text: () => "ReelPlaceAuto",
        onClick: async (updateCB) => {
  
          console.log("click");
  
  
          await ReelPlacingAuto();
          // lv.testGo=vec;
          updateCB(cbInfo)
          // await G(`G4 P2000`)
        }
      },
  
  
      "$t:       ",
      {
        type: "button",
        text: "NozzelSuck",
        onClick: async (updateCB) => {
  
          await G(`M42 P0 S1`)
          await G(`M42 P1 S1`)
          await G(`M42 P2 S1`)
          await G(`M42 P3 S1`)
        }
      },
  
      "$\n",
      {
        type: "InspTar_UI",
        id: "SBM_ReelLocating",

        APIExport: (api_set) => {
          InspTarUI_Reel_API_Set = api_set;
          console.log("SBM_ReelLocating APIExport")
        }
      },],
      callback: async (idx, key, updateCB) => {
  
        let FA = `F${F} ACC${ACC}`
        switch(key)
        {
  
          case "Go_Reel":
            {
    
              let ReelGoPin = 29;
              let ReelGoDist = 20;
              let ReelGo_Compensat = 3 + 4;
    
              let encv = await CNC_api.send_P({ "type": "AUX_ENC_V", aid: 1 })
              let initEnvV = encv.enc_v;
              (async () => {
                for (let i = 0; i < 5; i++) {
    
                  await CNC_api.send_P({ "type": "AUX_IO_CTRL", "pin": ReelGoPin, "state": 1, aid: 1 })
    
                  initEnvV += ReelGoDist;
                  await CNC_api.send_P({ "type": "AUX_WAIT_FOR_ENC", "value": initEnvV - ReelGo_Compensat, aid: 1 })
    
                  // CID_00E73541011 TTAG_FlexBowlCheck TID_4459
    
    
                  await CNC_api.send_P({ "type": "AUX_IO_CTRL", "pin": ReelGoPin, "state": 0, aid: 1 })
                  await CNC_api.send_P({ "type": "AUX_WAIT_FOR_FINISH", aid: 1 })
    
                  // progressUpdate(">>"+i)
                  // await CNC_api.send_P({"type":"AUX_DELAY","P":500})
                  NOT_ABORT();
                }
              })()
              return;
            }
  
  
          case "ReelCheck":
            {
  
              await Enter_Z2SafeZone_n_Check();
              await G(`G1 X${329} ${FA}`)
              await G(`G1 Y${380} ${FA}`)
              await G(`M400`)
              console.log(await ReelCheck("X", 1))
              return;
            }
  
          case "ReelCheckTrack":
            {
              await ReelCheckTrack("ReelLocReset", 1)
              return;
            }
          case "ReelLocReset":
            {
  
              await CNC_api.send_P({ "type": "AUX_SET_ENC", value: 0, aid: 1 })
  
              await ReelCheck("ReelLocReset", 1)
              return;
            }
        }
        updateCB(UIStack_Current().UI)
      }, default: 0
    }
  
    let lv = {}
    let cbInfo = [
  
      {
        text: () => " <<<距離:" + advUnit + " 速度:" + F,
        opts: [
          "$t:距離", "D0.01", "D0.1", "D1.0", "D10", "D100", "$\n",
          "$t:速度", "F10", "F100", "F500", "F1000", "$\n"],
        callback: async (idx, key, updateCB) => {}, default: 0
      },
      // {
      //   text: () => "Z>:" + ZR_Idx,
      //   opts: ["1", "2", "3", "4"],
      //   callback: async (idx, value, updateCB) => {
      //     switch (value) {
      //       case "1": ZR_Idx = 1; break;
      //       case "2": ZR_Idx = 2; break;
      //       case "3": ZR_Idx = 3; break;
      //       case "4": ZR_Idx = 4; break;
      //     }
  
      //     updateCB(cbInfo)
      //   }
      // },
      ,
      {
        text: () => "UI... ",
        opts: [    
                
            {
                type: "InspTar_UI",
                id: "SBM_FBLOC",
                
                params:{
                    style:{float:"left",width:"50%",height:"600px"},
            
                    APIExport: (api_set) => {
                        InspTarUI_Reel_API_Set = api_set;
                        console.log("SBM_ReelLocating APIExport",api_set)
                        api_set.setDrawHook({
                          preDraw:()=>{},
                          postDraw:(ctrl_or_draw, g, canvas_obj)=>{
                            if(ctrl_or_draw==true)return;
                            console.log(ctrl_or_draw)

                            let ctx = g.ctx;


                            ctx.beginPath();
                            ctx.moveTo(0,0);
                            ctx.lineTo(100,100);
                            //ctx.closePath();
                            ctx.stroke();
                          }
                        })
                        }
                },
                
            }, 

            {
                type: "button", key: "sssff",
                text: () => "sss",
                onClick: async () => {

                    console.log("dddd");
                    InspTarUI_Reel_API_Set.onMouseClick(async (info) => {
                        console.log(">>>>>>>",info);
                    })        
                }        
            },
          {
            type: "button", key: "CalibCenter",
            text: () => "CalibCenter",
            onClick: async (updateCB) => {
              
              let cInfo=[
  
                {
                  text: () => "CalibCenter",
                  opts: [
                    {
                      type: "button", key: "back",
                      text: () => "<=",
                      onClick: async (updateCB) => {
                        UIStackBack(updateCB,{ok:"OK"});
                      }
                    },
                    "$t:           ",
                    {
                      type: "button", key: "DO_CALIB",
                      text: () => "DO_CALIB",
                      onClick: async (updateCB) => {
  
                        await CompCalib();
                      }
                    }
                  ],
                  callback: async (idx, key, updateCB) => {
                   
                    // cbInfo[idx].text =  " <<<距離:"+advUnit+" 速度:"+F,
                    
                    updateCB(cInfo)
                  }, default: 0
                },
                JOG_UI,
                Calib_UI
              ]//
              UIStackGo(updateCB,cInfo,(result)=>{
                console.log(result)
              })
            }
          },
          {
            type: "button", key: "FeederLocMapping",
            text: () => "FeederLocMapping",
            onClick: async (updateCB) => {
              
              let cInfo=[
  
                {
                  text: () => "Feeder Loc Mapping",
                  opts: [
                    {
                      type: "button", key: "back",
                      text: () => "<=",
                      onClick: async (updateCB) => {
                        UIStackBack(updateCB,{ok:"OK"});
                      }
                    },],
                  callback: async (idx, key, updateCB) => {
                   
                    // cbInfo[idx].text =  " <<<距離:"+advUnit+" 速度:"+F,
                    
                    updateCB(cInfo)
                  }, default: 0
                },
                JOG_UI,
                Feeder_loc_UI,
                // Reel_loc_UI,
              ]
              UIStackGo(updateCB,cInfo,(result)=>{
                console.log(result)
              })
            }
          },
          {
            type: "button", key: "ReelLocMapping",
            text: () => "ReelLocMapping",
            onClick: async (updateCB) => {
              
              let cInfo=[
  
                {
                  text: null,
                  opts: [
                    {
                      type: "button", key: "back",
                      text: "<",
                      onClick: async (updateCB) => {
                        UIStackBack(updateCB,{ok:"OK"});
                      }
                    },"$t:ReelLocMapping",
                    {
                      type: "button", key: "XXX",
                      text: "XXXX",
                      onClick: async (updateCB) => {
                        v.Calib_Reel_XY_Info=undefined;
                      }
                    }],
                  callback: async (idx, key, updateCB) => {
                   
                    // cbInfo[idx].text =  " <<<距離:"+advUnit+" 速度:"+F,
                    
                    updateCB(cInfo)
                  }, default: 0
                },
                JOG_UI,
                Reel_loc_UI,
                // Reel_loc_UI,
              ]
              UIStackGo(updateCB,cInfo,(result)=>{
                console.log(result)
              })
            }
          }
        ],
  
        callback: async (idx, key, updateCB) => {
        }
      },
      {
        text: () => " <<<距離:" + advUnit + " 速度:" + F,
        opts: [ "HeadCalib",        
         
          {
            type: "button", key: "Btn_ShowCalib",
            text: () => "Show calibInfo",
            onClick: async (updateCB) => {
              console.log(v.CalibInfo)
            }
          }, 
          {
            type: "button", key: "Btn_ShowFeederReelMap",
            text: () => "Show FeederReelMap",
            onClick: async (updateCB) => {
              console.log("Feeder map",v.Calib_Feeder_XY_Info )
              console.log("Reel map",v.Calib_Reel_XY_Info )
            }
          }, 
          "Validation", " .  H .  ", "$\n",
  
        ],
        callback: async (idx, key, updateCB) => {
          let FA = `F${F} ACC${ACC}`
          switch (key) {
            case "HeadCalib":
              try {
                await CompCalib();
              } catch (e) {
                console.log(e)
              }
              return;
  
            case "Validation":
              try {
                await CalibValidation();
              } catch (e) {
                console.log(e)
              }
              return;
  
  
  
            case " .  H .  ":
              await SimpHoming(1);
  
              await ackG("M400");
              initM114 = (await G("M114")).M114;
  
              await CompCalib();
              updateCB(cbInfo)
              return;
  
          }
        }, default: 0
      },
  
      // Feeder_loc_UI,
    ]
  
    UIStackGo(undefined,cbInfo)
    await waitUserCallBack(null, cbInfo)
}
  



async function CameraSNameSWTrigger(side_name,ttags,trigger_id,doTriggerInfoMocking=true)
{
  let CameraInfo=v.CameraInfo;
  let cam=CameraInfo.find(CameraInfo=>{
    return CameraInfo.side_name==side_name;
  });

  if(cam===undefined)
  {
    return await api.CameraSWTrigger(side_name,ttags,trigger_id,doTriggerInfoMocking);
  }

  return await api.CameraSWTrigger(cam.id,ttags,trigger_id,doTriggerInfoMocking);
}



; ({
  INIT,
  rangeGen,
  G,
  SYS_CONFIG,
  SYS_CAM_INFO,
  CNCSend,
  cbPromise,
  FixedStringify,
  pt_pair_2_pt_pair_transform,
  MatxVec,
  VecAdd,
  VecSub,
  VecMult,
  RotMat,
  MatxVec3x3,
  dec2binStr,
  
  CameraSNameSWTrigger
  })
  
  