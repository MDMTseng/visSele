let Lib;
let T_Data;
async function INIT() {

  let path = v.$DEFPATH
  console.log(">>>", path)
  Lib = await INCLUDE(path + "/lib_common.js")
  // T_Data = await INCLUDE(path + "/lib_test1_data.js")
  console.log(Lib)

}

// async function Lib.CNCSend(data) {
//     if (data.id === undefined) {
//         if(v.ddddIdx==undefined)v.ddddIdx=0;
//         data.id =v.ddddIdx;
//         v.ddddIdx++;
//     }

//     let retPkgs=await InspTargetExchange("CNCPeripheral", { type: "MESSAGE", msg: data });
//     let retMsg = retPkgs.find((p) => p.type == "PD");
//     return retMsg?.data?.msg;
// }



async function G(code) {
  return Lib.CNCSend({ "type": "G", "code": code })
}

let SYS_OUT_PIN_DEF = {

  Feeder_L_Light1: 30,
  Feeder_L_Light2: 27,
  Feeder_C_CAM1: 31,
  Feeder_A_Run1: 11,
  Feeder_A_Run2: 12,
  Feeder_A_Run3: 13,
  Feeder_A_Run4: 14,

  Feeder_F_Run1: 17,


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

let SYS_CONFIG = {
  SAFE_Z_ZERO: 70,
  SAFE_Z: 70 - 0.5,
  SAFE_Z_SensorLock: 70 + 0.5,

  OBJ_W: 2,
  OBJ_L: 3,
  OBJ_H: 7,

  FeederReadyPickZ: 50,
  FeederNearPickZ: 50,//15,
  FeederPickZ: 13.5 - 7,



  ReelNearPickZ: 19,
  ReelPlaceZ: 9,

  SAFE_RX: 350,
  SAFE_LX: 150,

  INSP_Y: 260 - 2,
  INSP_X: 281,
  INSP_Z: 54,


  //sensor in bit field
  ES_SENSOR_ALL: 0x4FF,//0x400 is the emergency stop btn
  ES_SENSOR_Z: 0xF,
  ES_SENSOR_Y: 0x80,

  MAXF: 1000,
  MAXACC: 2000,


  Head_Base_Dist: 40,
  Pick_Head_Count: 4,


  // A_Axis_Units_to_mm:20.885826771653544,//21.22(  200steps*5(reduction)/(60mm{roller D}*PI) ),
  A_Axis_Units_to_mm:20.5,
  A_Axis_RunHitEnc_BackUnitSpace:20,


  CalibLoc:[327, 440 - 5, 57],



  Holes_per_Slot: 1,
  Reel_Hole_Distance: 4,
  HeadReleaseLoc:[171,279,60],

  PickReadyLoc:[300,140],//[300,100],
  Feeder_prePick_lift:2,
  Feeder_postPick_lift:7,



  SideCam_mmpp:0.07,
  SideCam_InspLoc:[298,156,30],
  SideCam_RAngOffset:[20+25,15+25,0+25,-10+25],
  // SideCam_RAngOffset_Side:[17,6,-10,-10],

  SideCam_RAngOffset_Side:[17,6,-10,-10],




  Reel_prePlace_lift:3,
  Reel_Place_lift:0,
  Reel_postPlace_lift:0,

  Reel_prePick_lift:1,
  Reel_Pick_lift:-0.2,
  Reel_postPick_lift:5,

  Reel_postPlace_Z:50,
  Reel_postPick_Z:50,



  Calib_Target_Info:
    {
        "nloc_btm": {
            "x": 616.1170654296875,
            "y": 694.7062377929688
        },
        "nloc_side": {
            "x": 1637.7774658203125,
            "y": 665.2476196289062
        }
    }
  
}


let E_headState = {
  empty: 0,
  mayHaveObj: 1,
  OK: 2,
  NG: -1
}

let END_STOP_PINS = SYS_CONFIG.ES_SENSOR_ALL;

async function Enter_Z2SafeZone_n_Check(F = `F${SYS_CONFIG.MAXF} ACC${SYS_CONFIG.MAXACC}`, doS1Check = true) {
  let ENDSTOPFULL = SYS_CONFIG.ES_SENSOR_ALL;
  let ENDSTOPZ = SYS_CONFIG.ES_SENSOR_Z;
  if (doS1Check) {

    END_STOP_PINS &= ~ENDSTOPZ;
    await G(`M120.1  PINS${END_STOP_PINS} PNS${ENDSTOPFULL}`);

    let ZpreSafe = SYS_CONFIG.SAFE_Z;
    await G(`G01 Z1_${ZpreSafe} Z2_${ZpreSafe} Z3_${ZpreSafe} Z4_${ZpreSafe} ${F}`)


    // await G(`M400`)
    END_STOP_PINS |= ENDSTOPZ;
    await G(`M120.1 PINS${END_STOP_PINS} PNS${ENDSTOPFULL}`);//enable
  }

  {

    END_STOP_PINS &= ~ENDSTOPZ;
    await G(`M120.1  PINS${END_STOP_PINS} PNS${ENDSTOPFULL - ENDSTOPZ}`);

    let ZSafe = SYS_CONFIG.SAFE_Z_SensorLock;
    await G(`G01 Z1_${ZSafe} Z2_${ZSafe} Z3_${ZSafe} Z4_${ZSafe} ${F}`)


    // await G(`M400`)
    END_STOP_PINS |= ENDSTOPZ;
    await G(`M120.1  PINS${END_STOP_PINS} PNS${ENDSTOPFULL - ENDSTOPZ}`);
  }

  // await G(`G04 P10`);//HACK somehow PORT isn't really update until there is a breath time in ESP32....
}




async function Exit_Z2SafeZone_n_Check() {
  let ENDSTOPFULL = SYS_CONFIG.ES_SENSOR_ALL;
  let ENDSTOPZ = SYS_CONFIG.ES_SENSOR_Z;

  END_STOP_PINS &= ~ENDSTOPZ;
  await G(`G04 P4`);
  await G(`M120.1  PINS${END_STOP_PINS} PNS${ENDSTOPFULL - ENDSTOPZ}`);
}


async function SimpHoming(loopCount = 1) {
  let SafeF = "F200 ACC200"
  for (let hcount = 0; hcount < loopCount; hcount++) {

    // progressUpdate("count:" + hcount)
    hcount++;
    let ret = await G(`M119`)
    console.log(ret.M119);

    await G(`M121`)//dis


    for (let i = 0; i < 1; i++) {
      await G("G28 ")
    }
    let XYoffset = 10;
    let Z0 = SYS_CONFIG.SAFE_Z_ZERO;
    await G(`M400 `)
    await G(`G92 X${-XYoffset} Y${-XYoffset} Z1_${Z0} Z2_${Z0} Z3_${Z0} Z4_${Z0} ${SafeF}`)

    // await G(`G1 X10 Y10  ${SafeF}`)

    let Zprep = SYS_CONFIG.SAFE_Z;
    await G(`G1 R1_${360} R2_${360} R3_${360} R4_${360}  ${SafeF}`)
    await G(`G1 X0 Y0 Z1_${Zprep} Z2_${Zprep} Z3_${Zprep} Z4_${Zprep}  R1_${0} R2_${0} R3_${0} R4_${0}  ${SafeF}`)

    //await G(`M120`)//en
    NOT_ABORT();
    //break;
  }

  await Enter_Z2SafeZone_n_Check(SafeF);
}



async function Enable_YFar_Check(enable) {
  let ENDSTOPFULL = SYS_CONFIG.ES_SENSOR_ALL;
  let ENDSTOPY = SYS_CONFIG.ES_SENSOR_Y;
  let ENDSTOPZ = SYS_CONFIG.ES_SENSOR_Z;

  if (enable)
    END_STOP_PINS |= ENDSTOPY;
  else
    END_STOP_PINS &= ~ENDSTOPY;
  console.log(END_STOP_PINS);
  await G(`M120.1  PINS${END_STOP_PINS}`);
}


async function CNCTestRun() {

  // await SimpHoming(1);
  // return;

  // await G(`M121`)
  // // await G(`G1 Z1_0  Z2_0  Z3_0  Z4_00 F800 ACC1200`)
  // // await G(`G1 Z1_30  Z2_40  Z3_50  Z4_60 F800 ACC1200`)

  // // console.log(await G("G92 A0"));
  // await G(`G1 Z1_30  Z2_30  Z3_30  Z4_30  F800 ACC1200`)
  // await G(`G1 Z1_60  Z2_60  Z3_60  Z4_60 F800 ACC1200`)
  // // Enter_Z2SafeZone_n_Check();
  // // Exit_Z2SafeZone_n_Check();
  // return;
  await G(`M121`)//dis
  let AUX_THREAD_ID = 1;

  let advStepPerEnc = 21;
  let distUnit = 4;
  let setCount = 10;
  let encStep = distUnit;
  let FACC = 5000;
  let ACC = 4500;

  // let FF=1500;

  let n_enc = 0;
  console.log("AUX_SET_ENC>>>>>", await Lib.CNCSend({ "type": "AUX_SET_ENC", value: 0, aid: AUX_THREAD_ID }));

  for (let k = 0; k < 150; k++) {

    encStep = ((k % setCount) < (setCount - 1)) ? (distUnit) : (distUnit * (setCount * 3));
    n_enc += encStep;
    let A_Tar = advStepPerEnc * encStep;


    for (let i = 0; i < 1; i++) {

      let xBase = 300;
      console.log(await G("G92 A0"));

      // console.log(await G(`G01 Y50 F2000    ACC${ACC}`));

      console.log(await G(`G01 X${xBase + 100} Y20 A${A_Tar - 10}  F2000 ACC${ACC}`));
      console.log(await G(`G01 X${xBase + 50} Y50 F2000 ACC${FACC}`));
      console.log(await G(`G01 X${xBase + 100} Y40 F2000 ACC${FACC}`));
      console.log(await G(`G01 X${xBase + 50} Y60 F2000 ACC${FACC}`));


      console.log(await G(`G01 X${xBase + 100} Y380 F1500  A${A_Tar - 8} ACC${ACC}`));

      console.log(await G(`G01.ENC A${A_Tar} ENC${n_enc} SMULT5000 F8 ACC1000`));
      let dZ = 15;

      await G(`G1 Z1_${dZ}  Z2_${dZ}  Z3_${dZ}  Z4_${dZ}  F800 ACC1000`)
      await G(`G1 Z1_60  Z2_60  Z3_60  Z4_60 F800 ACC1000`)
      // console.log(await G(`G01 A${A_Tar-1} F1000 ACC${1000}`));
    }
    await G(`M400`)


    NOT_ABORT();

  }
  console.log(await G("M114"), "M114");


}

async function CNCTestRun2() {




  let initT = Date.now();

  function dT() { return Date.now() - initT }
  let AUX_THREAD_ID = 1;
  let waitKey = "ddd"
  SigWait_Reg(waitKey);
  (async () => {//ENC set thread
    console.log(dT(), "ENC set thread")
    console.log("AUX_SET_ENC>>>>>", await Lib.CNCSend({ "type": "AUX_SET_ENC", value: 9, aid: AUX_THREAD_ID }));

    await Lib.CNCSend({ "type": "AUX_DELAY", "P": 300, aid: AUX_THREAD_ID })

    await Lib.CNCSend({ "type": "AUX_WAIT_FOR_FINISH", aid: AUX_THREAD_ID })



    console.log("AUX_ENC_V>>>>>", await Lib.CNCSend({ "type": "AUX_ENC_V", aid: AUX_THREAD_ID }));

    console.log("AUX_SET_ENC>>>>>", await Lib.CNCSend({ "type": "AUX_SET_ENC", value: 10, aid: AUX_THREAD_ID }));
    console.log("AUX_ENC_V>>>>>", await Lib.CNCSend({ "type": "AUX_ENC_V", aid: AUX_THREAD_ID }));

    SigWait_Trig(waitKey, { report: 111 });
  })().catch((e) => {
  })


  console.log(">>>>>");
  console.log(await Lib.CNCSend({ type: "get_setup" }));

  await G(`M120`)//dis
  console.log("GO>>>>>");
  console.log(await G("G01 X0 Y0 F2000 ACC3000"));
  console.log(await G("G01 X99 Y0 F2000 ACC3000"));
  console.log(await G("G01.ENC X100 ENC10 SMULT10 F1000"), "G01.ENC");
  await G(`M400`)
  console.log(await G("M114"), "M114");
  console.log(await G("G01 X0 Y0 F2000 ACC3000"));


  await G(`M400`)
  console.log("GO>>>>> DONE")

  await SigWait(waitKey)
  console.log("DONE....")
}


function rand(min = 0, max = 1) {
  return Math.floor(Math.random() * max) + min;
};



function calibInfoSafeCheck(calibInfo = undefined) {
  if (calibInfo === undefined) return false;
  {

    let xMaxAbsOffset = 0;
    let yMaxAbsOffset = 0;
    let zMaxAbsOffset = 0;
    for (let i = 0; i < 4; i++) {

      let xoffset = (calibInfo === undefined ? 0 : calibInfo.XY.offset[i].x);
      let yoffset = (calibInfo === undefined ? 0 : calibInfo.XY.offset[i].y);
      let zoffset = (calibInfo === undefined ? 0 : calibInfo.Z.offset[i]);
      xMaxAbsOffset = Math.max(xMaxAbsOffset, Math.abs(xoffset));
      yMaxAbsOffset = Math.max(yMaxAbsOffset, Math.abs(yoffset));
      zMaxAbsOffset = Math.max(zMaxAbsOffset, Math.abs(zoffset));
    }

    let maxOffsetThres = 2.3;
    let maxOffsetThres_Z = 3;
    if (xMaxAbsOffset > maxOffsetThres || xMaxAbsOffset < -maxOffsetThres ||
      yMaxAbsOffset > maxOffsetThres || yMaxAbsOffset < -maxOffsetThres ||
      zMaxAbsOffset > maxOffsetThres_Z || zMaxAbsOffset < -maxOffsetThres_Z) {
      throw "the offset is too large:" + xMaxAbsOffset + "," + yMaxAbsOffset + "," + zMaxAbsOffset;
      return false;
    }
  }
  return true;
}

async function HeadCalibVerificationAct(ActPts = [], calibInfo = undefined,returnLocGCode=`G01 X${20} Y${200} F1000 ACC2000`,checkIndexes=[0,1,2,3],FA="F1000 ACC2000") {
  // await G(`M120`)

  await Enter_Z2SafeZone_n_Check();

  if (calibInfo !== undefined) {

    if (calibInfoSafeCheck(calibInfo) == false) {
      throw "the offset is too large";
      return undefined;
    }
  }

  await G(`M121`)//dis
  let calibN1Pos = SYS_CONFIG.CalibLoc;
  let pY = calibN1Pos[1];
  await Enable_YFar_Check(false);
  await G(`G01  X${calibN1Pos[0]} Y${pY} `+FA);

  let InspLightPinS = (2 ** SYS_OUT_PIN_DEF.Insp_L_Light1);
  let inspRecord = [];
  let isAllTargetDetected = true;


  let repWaitKey = "sdsa";


  for (let j = 0; j <checkIndexes.length; j++) {

    let i=checkIndexes[j]
    let xoffset = (calibInfo === undefined ? 0 : calibInfo.XY.offset[i].x);
    let yoffset = (calibInfo === undefined ? 0 : calibInfo.XY.offset[i].y);
    let zoffset = (calibInfo === undefined ? 0 : calibInfo.Z.offset[i]);

    let headRecord = [];
    inspRecord.push(headRecord);
    await Exit_Z2SafeZone_n_Check();
    await G(`M400`);

    let cX = calibN1Pos[0] - i * 40 + xoffset;
    let cY = pY + yoffset;
    let cZ = calibN1Pos[2] + zoffset;

    await G(`G01 X${cX.toFixed(3)} F1000 ACC2000`);
    await G(`G01 Z${i + 1}_${cZ} F500 ACC1000`);
    await G(`M400`);
    await G(`M42 PORT${InspLightPinS}    S${InspLightPinS}`);


    let InspInfo=[];
    let step = 0;
    for (let loc of ActPts) {

      let trigID = 10000 + i * 100 + step;
      let curRWKey=repWaitKey+trigID;
      reportWait_reg(curRWKey + "_side", "SBM_NozzleCalibLoc", trigID);

      reportWait_reg(curRWKey + "_btm", "SBM_NozzleCalibBtmLoc", trigID);
      
      InspInfo.push({
        rkey:curRWKey,
        trigID:trigID,
        loc,
      });

      await G(`G01 X${cX + loc[0] - 1}  Y${cY + loc[1] - 1} Z${i + 1}_${cZ + loc[2] + 0.5} F500 ACC1000`);
      await G(`G01 X${cX + loc[0]}  Y${cY + loc[1]} Z${i + 1}_${cZ + loc[2]} R${i + 1}_${loc[3]} F100 ACC100`);

      await G(`M400`);

      await Lib.CameraSNameSWTrigger("HeadCalibCam", "CAM_Calib", trigID, true);

      await delay(30);
      step++;


    }

    for(let iinfo of InspInfo){
      let report_side = await reportWait(iinfo.rkey + "_side")
      let nloc_side = objEleGet(report_side, ["report", 0, "center"]);

      let report_btm = await reportWait(iinfo.rkey + "_btm")
      let nloc_btm = objEleGet(report_btm, ["report", 0, "center"]);
      console.log("trigID:" + iinfo.trigID, " nloc_btm:", nloc_btm, " nloc_side:", nloc_side);
      console.log("report_side:" + report_side, " report_btm:", report_btm);
      // if (nloc_btm === undefined || nloc_side === undefined) {
      //   isAllTargetDetected = false;
      //   break;
      // }


      headRecord.push({
        loc: iinfo.loc,
        nloc_btm,
        nloc_side,

        confidence_btm: objEleGet(report_btm, ["report", 0, "confidence"]),
        confidence_side: objEleGet(report_side, ["report", 0, "confidence"]),

      });
    }





    console.log(inspRecord);
    await G(`M42 PORT${InspLightPinS}    S${0}`);

    await G(`M400`);
    await Enter_Z2SafeZone_n_Check();
    if (isAllTargetDetected == false) break;
  }


  await G(`M400`);




  await Enter_Z2SafeZone_n_Check();
  await G(returnLocGCode);
  await G(`M400`);
  await Enable_YFar_Check(true);

  if (isAllTargetDetected == false) return undefined;
  return inspRecord;
}

async function HeadCalibCalc2(actRec) {


  let CalibInfo;
  let XYCalibInfo;
  {
    let tv3 = THREE.Vector3

    let n1X1 = actRec[0][1];
    let n1X2 = actRec[0][2];
    let nLocDiff = new tv3((n1X1.nloc_btm.x - n1X2.nloc_btm.x), (n1X1.nloc_btm.y - n1X2.nloc_btm.y), 0)
    nLocDiff.divideScalar(n1X1.loc[0] - n1X2.loc[0]);

    let n1Y1 = actRec[0][3];
    let n1Y2 = actRec[0][4];
    let nLocDiffY = new tv3((n1Y1.nloc_btm.x - n1Y2.nloc_btm.x), (n1Y1.nloc_btm.y - n1Y2.nloc_btm.y), 0)
    nLocDiffY.divideScalar(n1Y1.loc[1] - n1Y2.loc[1]);
    console.log(nLocDiffY);



    const M_XY2Pix = new THREE.Matrix3(
      nLocDiff.x, nLocDiffY.x, 0,
      nLocDiff.y, nLocDiffY.y, 0,
      0, 0, 1);

    const M_Pix2XY = M_XY2Pix.clone().invert()


    console.log("M_XY2Pix", M_XY2Pix);
    console.log("M_Pix2XYM", M_Pix2XY);

    const vectorX1 = new tv3(1, 0, 0).applyMatrix3(M_XY2Pix);
    const vectorY1 = new tv3(0, 1, 0).applyMatrix3(M_XY2Pix);
    console.log("vectorX1:", vectorX1);
    console.log("vectorY1:", vectorY1);


    let nozzleCenter = actRec.map(rec => {
      let xsum = 0;
      let ysum = 0;
      let idxStart = 0;
      let idxEnd = rec.length - 1;
      for (let i = idxStart; i <= idxEnd; i++) {
        xsum += rec[i].nloc_btm.x;
        ysum += rec[i].nloc_btm.y;
      }
      xsum /= (idxEnd - idxStart + 1);
      ysum /= (idxEnd - idxStart + 1);
      return new tv3(xsum, ysum, 0);
    });

    let nozzleCenter0 = nozzleCenter[0].clone();
    let nozzleXYOffset = nozzleCenter.map(nc => nc.sub(nozzleCenter0).applyMatrix3(M_Pix2XY).multiplyScalar(-1))
    XYCalibInfo = {
      conv: {
        M_XY2Pix,
        M_Pix2XY
      },
      offset: nozzleXYOffset
    }


  }



  let ZCalibInfo;
  {


    //Z axis PIXEL/UNIT
    let n1Z1 = actRec[0][5];
    let n1Z2 = actRec[0][6];
    let Zdiff = n1Z1.loc[2] - n1Z2.loc[2];
    let nLocDiffZ = n1Z1.nloc_side.x - n1Z2.nloc_side.x;
    console.log(Zdiff, nLocDiffZ, nLocDiffZ / Zdiff);
    let zUpP = Zdiff / nLocDiffZ;


    let zuOffset = actRec.map(rec => {

      return -(rec[6].nloc_side.x - actRec[0][6].nloc_side.x) * zUpP;
    });
    console.log("Z zUpP:", zUpP, " offset:", zuOffset);
    ZCalibInfo = {
      offset: zuOffset,
    };
  }

  CalibInfo = {
    XY: XYCalibInfo,
    Z: ZCalibInfo
  }
  return CalibInfo;
}

async function HeadCalibCalc(actRec) {


  let CalibInfo;
  let XYCalibInfo;
  {
    let tv3 = THREE.Vector3

    let n1X1 = actRec[0][1];
    let n1X2 = actRec[0][2];
    let nLocDiff = new tv3((n1X1.nloc_btm.x - n1X2.nloc_btm.x), (n1X1.nloc_btm.y - n1X2.nloc_btm.y), 0)
    nLocDiff.divideScalar(n1X1.loc[0] - n1X2.loc[0]);

    let n1Y1 = actRec[0][3];
    let n1Y2 = actRec[0][4];
    let nLocDiffY = new tv3((n1Y1.nloc_btm.x - n1Y2.nloc_btm.x), (n1Y1.nloc_btm.y - n1Y2.nloc_btm.y), 0)
    nLocDiffY.divideScalar(n1Y1.loc[1] - n1Y2.loc[1]);
    console.log(nLocDiffY);



    const M_XY2Pix = new THREE.Matrix3(
      nLocDiff.x, nLocDiffY.x, 0,
      nLocDiff.y, nLocDiffY.y, 0,
      0, 0, 1);

    const M_Pix2XY = M_XY2Pix.clone().invert()


    console.log("M_XY2Pix", M_XY2Pix);
    console.log("M_Pix2XYM", M_Pix2XY);

    const vectorX1 = new tv3(1, 0, 0).applyMatrix3(M_XY2Pix);
    const vectorY1 = new tv3(0, 1, 0).applyMatrix3(M_XY2Pix);
    console.log("vectorX1:", vectorX1);
    console.log("vectorY1:", vectorY1);


    let nozzleCenter = actRec.map(rec => {
      let xsum = 0;
      let ysum = 0;
      let idxes = [0];
      for (let i of idxes) {
        xsum += rec[i].nloc_btm.x;
        ysum += rec[i].nloc_btm.y;
      }
      xsum /= idxes.length;
      ysum /= idxes.length;
      return new tv3(xsum, ysum, 0);
    });

    let nozzleCenter0 = SYS_CONFIG.Calib_Target_Info.nloc_btm;
    nozzleCenter0=new tv3(nozzleCenter0.x,nozzleCenter0.y,0);
    
    let nozzleXYOffset = nozzleCenter.map(nc => nc.sub(nozzleCenter0).applyMatrix3(M_Pix2XY).multiplyScalar(-1))
    XYCalibInfo = {
      conv: {
        M_XY2Pix,
        M_Pix2XY
      },
      offset: nozzleXYOffset
    }


  }



  let ZCalibInfo;
  {


    //Z axis PIXEL/UNIT
    let n1Z1 = actRec[0][5];
    let n1Z2 = actRec[0][6];
    let Zdiff = n1Z1.loc[2] - n1Z2.loc[2];
    let nLocDiffZ = n1Z1.nloc_side.x - n1Z2.nloc_side.x;
    console.log(Zdiff, nLocDiffZ, nLocDiffZ / Zdiff);
    let zUpP = Zdiff / nLocDiffZ;

  
    
    console.log(actRec);
    let zuOffset = actRec.map(rec => {

      return -(rec[0].nloc_side.x - SYS_CONFIG.Calib_Target_Info.nloc_side.x) * zUpP;
    });
    console.log("Z zUpP:", zUpP, " offset:", zuOffset);
    ZCalibInfo = {
      conv: {

        M_Z2Pix:1/zUpP,
        M_Pix2Z:zUpP
      },
      offset: zuOffset,
    };
  }

  CalibInfo = {
    XY: XYCalibInfo,
    Z: ZCalibInfo
  }
  return CalibInfo;
}


async function HeadVerification(vpts = [[0, 0, 0, 0], [0, 0, 0, 90], [0, 0, 0, 180], [0, 0, 0, 270]],checkIndexes=[0,1,2,3]) {
  if (v.CalibInfo !== undefined) {
    console.log(v.CalibInfo);
    let actRec3 = await HeadCalibVerificationAct(vpts, v.CalibInfo,`G01 X${20} Y${200} F1000 ACC2000`,checkIndexes);
    // let CalibInfo3=await HeadCalibCalc(actRec3);
    console.log(actRec3);
    return actRec3;
  }
  return;

}


async function HeadVerificationSimpleStat()
{
  let headLocReps=await HeadVerification(
    [0].map((v) => [0, 0, 0, v])
  );

  let tv3 = THREE.Vector3
  let ref_nloc_btm=SYS_CONFIG.Calib_Target_Info.nloc_btm;
  let ref_nloc_side=SYS_CONFIG.Calib_Target_Info.nloc_side;

  // 
  // let locDiff_pix=new tv3(cur_nloc_btm.x-ref_nloc_btm.x,cur_nloc_btm.y-ref_nloc_btm.y,0)

  // console.log(">running Calib>>> cur:",cur_nloc_btm," ref:",ref_nloc_btm)

  // console.log(">running Calib>>>locDiff_pix", locDiff_pix, " M_Pix2MM ",v.CalibInfo.XY.conv.M_Pix2XY)
  // let locDiff_mm=locDiff_pix.clone().applyMatrix3(v.CalibInfo.XY.conv.M_Pix2XY);


  console.log(headLocReps);

  
  let diffXYZ_mm=headLocReps.map(rep=>{



    let cur_nloc_btm=rep[0].nloc_btm;
    let cur_nloc_side=rep[0].nloc_side;


    let locDiff_pix=new tv3(cur_nloc_btm.x-ref_nloc_btm.x,cur_nloc_btm.y-ref_nloc_btm.y,0)
    let locDiff_mm=locDiff_pix.clone().applyMatrix3(v.CalibInfo.XY.conv.M_Pix2XY);

    let zDiff_pix=cur_nloc_side.x-ref_nloc_side.x;
    return {
      x:locDiff_mm.x,
      y:locDiff_mm.y,
      z:zDiff_pix*v.CalibInfo.Z.conv.M_Pix2Z
    }
  });


  let diffXYZ_mm_stat=diffXYZ_mm.reduce((acc,cur)=>{
    acc.max_xyz.x=(acc.max_xyz.x<cur.x)?cur.x:acc.max_xyz.x;
    acc.max_xyz.y=(acc.max_xyz.y<cur.y)?cur.y:acc.max_xyz.y;
    acc.max_xyz.z=(acc.max_xyz.z<cur.z)?cur.z:acc.max_xyz.z;

    acc.absAvg_xyz.x+=Math.abs(cur.x)/diffXYZ_mm.length;
    acc.absAvg_xyz.y+=Math.abs(cur.y)/diffXYZ_mm.length;
    acc.absAvg_xyz.z+=Math.abs(cur.z)/diffXYZ_mm.length;


    acc.max=Math.max(acc.max,acc.max_xyz.x,acc.max_xyz.y,acc.max_xyz.z);
    return acc;
  },{

    max:0,
    max_xyz:{
      x:0,y:0,z:0
    },

    absAvg_xyz:{
      x:0,y:0,z:0
    },

    

  })


  console.log(diffXYZ_mm,diffXYZ_mm_stat);
  return diffXYZ_mm_stat;
}


async function HeadCalib() {
  let pts = [[0, 0, 0, 0],
  [-5, 0, 0, 0], [5, 0, 0, 0],
  [0, 3, 0, 0], [0, -5, 0, 0],
  [0, 0, 1, 0], [0, 0, -2, 0]];

  let calibRecord = undefined;//||Lib.calibRecord;


  let calibReturnLoc={x:20,y:200};

  let actRec = calibRecord !== undefined ? calibRecord : await HeadCalibVerificationAct(pts, v.CalibInfo,`G01 X${calibReturnLoc.x} Y${calibReturnLoc.y} F1000 ACC2000`)
  // console.log(comlib,actRec);

  if (actRec === undefined) return;
  let CalibInfo = v.CalibInfo;
  let CalibInfo2 = await HeadCalibCalc(actRec);

  if (CalibInfo !== undefined) {//merge
    let alpha = 0.8;
    CalibInfo.actInfo = {bkXY:CalibInfo.XY,bkZ:CalibInfo.Z,tarPts:pts,actRec};




    // CalibInfo2.XY.conv.M_XY2Pix[0] *= alpha;
    // CalibInfo2.XY.conv.M_XY2Pix[1] *= alpha;
    // CalibInfo2.XY.conv.M_XY2Pix[3] *= alpha;
    // CalibInfo2.XY.conv.M_XY2Pix[4] *= alpha;

    // CalibInfo2.XY.conv.M_Pix2XY[0] *= alpha;
    // CalibInfo2.XY.conv.M_Pix2XY[1] *= alpha;
    // CalibInfo2.XY.conv.M_Pix2XY[3] *= alpha;
    // CalibInfo2.XY.conv.M_Pix2XY[4] *= alpha;

    // CalibInfo.XY.conv.M_XY2Pix = CalibInfo.XY.conv.M_XY2Pix.clone().multiply(CalibInfo2.XY.conv.M_XY2Pix);
    // CalibInfo.XY.conv.M_Pix2XY = CalibInfo.XY.conv.M_Pix2XY.clone().multiply(CalibInfo2.XY.conv.M_Pix2XY);

    CalibInfo.XY.conv.M_XY2Pix=CalibInfo2.XY.conv.M_XY2Pix;
    CalibInfo.XY.conv.M_Pix2XY=CalibInfo2.XY.conv.M_Pix2XY;

    CalibInfo.Z.conv=CalibInfo2.Z.conv;

    CalibInfo.XY.offset = CalibInfo.XY.offset.map((v, i) => v.clone().add(CalibInfo2.XY.offset[i].clone().multiplyScalar(alpha)));
    CalibInfo.Z.offset = CalibInfo.Z.offset.map((v, i) => v + CalibInfo2.Z.offset[i] * (alpha));

  }
  else {
    CalibInfo = CalibInfo2;
  }



  if(1)//offset corrdinate according to the first head
  {
    let coffset=CalibInfo.XY.offset[0].clone();



    await G(`M400`);
    await G(`G92 X${calibReturnLoc.x-coffset.x} Y${calibReturnLoc.y-coffset.y}`)//


    
    CalibInfo.XY.offset = CalibInfo.XY.offset.map((v, i) => v.clone().sub(coffset));


  }


  v.CalibInfo = CalibInfo;

  console.log("CalibInfo:",CalibInfo);

}

async function HeadGo2(nIdx = 0, pos = [0, 0, 50]) {
  if (calibInfoSafeCheck(v.CalibInfo) == false) return;

  let xoffset = v.CalibInfo.XY.offset[nIdx].x;
  let yoffset = v.CalibInfo.XY.offset[nIdx].y;
  let zoffset = v.CalibInfo.Z.offset[nIdx];
  await Enter_Z2SafeZone_n_Check();
  await G(`G01 X${(nIdx * -40) + pos[0] + xoffset} Y${pos[1] + yoffset} F1000 ACC4000`);

  await Exit_Z2SafeZone_n_Check();
  await G(`G01 Z${nIdx + 1}_${pos[2] + zoffset} F1000 ACC4000`);

  await G(`G04 P100`);



  await Enter_Z2SafeZone_n_Check();
}


async function HeadGo3(nIdx = 0, pos = [0, 0, 50], stanbyZ = 50) {
  if (calibInfoSafeCheck(v.CalibInfo) == false) return;
  let NF = `F3000 ACC4000`
  let ZF = `F3000 ACC4000`
  let xoffset = v.CalibInfo.XY.offset[nIdx].x;
  let yoffset = v.CalibInfo.XY.offset[nIdx].y;
  let zoffset = v.CalibInfo.Z.offset[nIdx];
  await G(`G01 Z${nIdx}_${stanbyZ} X${(nIdx * -40) + pos[0] + xoffset} Y${pos[1] + yoffset} Z${nIdx + 1}_${pos[2] + zoffset + 1} ` + NF);

  await G(`G01 Z${nIdx + 1}_${pos[2] + zoffset} ` + ZF);

  await G(`G04 P5`);
  await G(`M42 P${nIdx} S1`);
  await G(`G04 P5`);

  await G(`G01 Z${nIdx + 1}_${pos[2] + zoffset + 5} ` + ZF);



}


let cur_enc = 0;

async function HeadGo_cycle(encStep) {
  let NF = `F3000 ACC4000`
  let ZF = `F3000 ACC4000`

  let advStepPerEnc = 21;
  let A_Tar = advStepPerEnc * encStep;


  await Exit_Z2SafeZone_n_Check();
  let stanbyZ = 50;
  let pickZ = 13;
  let dropZ = 8;
  await G("G92 A0");
  await G(`G01 Z1_${stanbyZ} Z2_${stanbyZ} Z3_${stanbyZ} Z4_${stanbyZ} R1_0 R2_0 R3_0 R4_0` + NF);
  await G(`G01 X${363.5} Y${79} A${A_Tar / 2}  ` + NF);
  for (let i = 0; i < 4; i++)
    await HeadGo3(i, [363.5, 79, pickZ], stanbyZ)

  await G(`G01 Z1_${stanbyZ} Z2_${stanbyZ} Z3_${stanbyZ} Z4_${stanbyZ} `);

  await G(`G01 X${309 - 1.7} Y${372 - 0.2} A${A_Tar - 10}  R1_90 R2_90 R3_90 R4_90 ` + NF);


  cur_enc += encStep;
  await G(`G01.ENC A${A_Tar} ENC${cur_enc} SMULT5000 F8 ACC1000`);

  await G(`G04 P2000`);
  let zoffset = v.CalibInfo.Z.offset;
  await G(`G01 Z1_${dropZ + zoffset[0]} Z2_${dropZ + zoffset[1]} Z3_${dropZ + zoffset[2]} Z4_${dropZ + zoffset[3]} ` + ZF);

  for (let i = 0; i < 4; i++)
    await G(`M42 P${i} S0`);
  // await G(`G04 P200`);
  // await G(`G01 Z1_${dropZ+zoffset[0]+2} Z2_${dropZ+zoffset[1]+2} Z3_${dropZ+zoffset[2]+2} Z4_${dropZ+zoffset[3]+2} `+ZF);
  // await Enter_Z2SafeZone_n_Check();
  // await G(`G01 Z1_${stanbyZ} Z2_${stanbyZ} Z3_${stanbyZ} Z4_${stanbyZ} `+NF);



}

async function HeadGo(cycles = 1) {
  let AUX_THREAD_ID = 1;
  console.log("AUX_SET_ENC>>>>>", await Lib.CNCSend({ "type": "AUX_SET_ENC", value: 0, aid: AUX_THREAD_ID }));
  cur_enc = 0;
  var startDate = new Date();

  let setCount = 10;
  for (let i = 0; i < cycles; i++) {
    let distUnit = 4;
    let encStep = ((i % setCount) < (setCount - 1)) ? (distUnit) : (distUnit * (setCount * 3));
    // await Enter_Z2SafeZone_n_Check();


    await HeadGo_cycle(encStep)

    await G(`M400`);
    var endDate = new Date();
    var seconds = (endDate.getTime() - startDate.getTime()) / 1000;
    progressUpdate("time:" + (seconds / (i + 1)).toFixed(2) + " c:" + i)
    NOT_ABORT();
  }

  await delay(2000)
}

async function test2() {

  await G(`M121`)//dis

  if (false) {


    let zd = 20;
    let zs = 22;
    let zb = 50;
    let xyRandRange = 2;


    let pX = 20;
    let pY = 200;
    await G(`G01 X${pX} Y${pY} F1000 ACC2000`);

    var startDate = new Date();
    for (let i = 0; i < 5; i++) {

      await G(`G01 Z${4}_${zs} Z${1}_${zs} Z${2}_${zs} Z${3}_${zs} F1000 ACC4000`);


      if (false) {
        await G(`G04 P0`);
        await G(`G01 Z${4}_${zd} Z${1}_${zd} Z${2}_${zd} Z${3}_${zd} F1000 ACC4000`);
        await G(`G04 P10`);
      }
      else {
        for (let j = 1; j <= 4; j++) {
          let ZRet = "";
          if (j >= 2)
            ZRet = `Z${j - 1}_${zs} R${j}_${0}`
          await G(`G01 ${ZRet} X${pX + rand(-xyRandRange, xyRandRange)} Y${pY + rand(-xyRandRange, xyRandRange)} F1000 ACC5000`);
          await G(`G01 Z${j}_${zd} R${j}_${0} F1000 ACC5000`);
        }

      }

      await G(`G01 Z${4}_${zb} Z${1}_${zb} Z${2}_${zb} Z${3}_${zb} F1000 ACC4000`);
    }

    await G(`M400`)
    var endDate = new Date();
    var seconds = (endDate.getTime() - startDate.getTime()) / 1000;
    progressUpdate("elapse:" + seconds)
    await delay(2000)
  }
  // return;
  if (1) {

    let FB_Vib_pin1 = (2 ** SYS_OUT_PIN_DEF.Feeder_A_Run1);
    let FB_Vib_pin2 = (2 ** SYS_OUT_PIN_DEF.Feeder_A_Run2);
    let FB_Vib_pin3 = (2 ** SYS_OUT_PIN_DEF.Feeder_A_Run3);
    let FB_Vib_pin4 = (2 ** SYS_OUT_PIN_DEF.Feeder_A_Run4);



    // let pin=FB_Vib_pin1;

    for (let pin of [false ? undefined : [FB_Vib_pin1], false ? undefined : [FB_Vib_pin2, 500], [FB_Vib_pin3, 500]]) {
      if (pin === undefined) continue;
      await G(`M42 PORT${pin[0]}    S${pin[0]}`)

      await G(`G04 P${pin[1] ?? 500}`);

      await G(`M42 PORT${pin[0]}    S${0}`)
    }

    // for(let i=0;i<1;i++)
    // {
    //   let PIN=FB_Vib_pin2;

    //   await G(`M42 PORT${PIN} S${PIN}`)

    //   await G(`G04 P400`);

    //   await G(`M42 PORT${PIN} S${0}`)
    //   // await G(`G04 P100`);
    // }


    //   await G(`M42 PORT${FB_Vib_pin3} S${FB_Vib_pin3}`)

    //   await G(`G04 P500`);

    // await G(`M42 PORT${FB_Vib_pin3} S${0}`)



    await G(`G04 P1000`);

    await G(`M400`);
  }
  if (1) {

    let cam_led_pins = 0
      | 1 * (2 ** SYS_OUT_PIN_DEF.Feeder_L_Light1)
      | 1 * (2 ** SYS_OUT_PIN_DEF.Feeder_A_Run4)
      | 1 * (2 ** SYS_OUT_PIN_DEF.Insp_L_Light1)
      | 1 * (2 ** SYS_OUT_PIN_DEF.Insp_L_Light2);



    await G(`M42 PORT${cam_led_pins}    S${cam_led_pins}`)
    await delay(40);
    // await G(`G04 P2000`);

    await Lib.CameraSNameSWTrigger("FeederCam", "CAM_FB", 0, true);
    await delay(20);

    await G(`M42 PORT${cam_led_pins}    S${0}`)
  }


}



function FixedStringify(obj, fixedDig = 3) {
  return JSON.stringify(obj, function (key, val) {
    if (typeof val !== "number") return val;
    return val.toFixed ? Number(val.toFixed(fixedDig)) : val;
  },2)

}




async function ReelCheck(trigID = -990, AUX_THREAD_ID = 0) {

  await Lib.CNCSend({ "type": "AUX_IO_CTRL", "pin": SYS_OUT_PIN_DEF.Reel_L_Light1, "state": 1, aid: AUX_THREAD_ID })

  await Lib.CNCSend({ "type": "AUX_WAIT_FOR_FINISH", aid: AUX_THREAD_ID })
  await Lib.CameraSNameSWTrigger("ReelCheckCam", "CAM_Reel", trigID, true);

  await Lib.CNCSend({ "type": "AUX_DELAY", "P": 50, aid: 0, aid: AUX_THREAD_ID })
  await Lib.CNCSend({ "type": "AUX_IO_CTRL", "pin": SYS_OUT_PIN_DEF.Reel_L_Light1, "state": 0, aid: AUX_THREAD_ID })


}




let dff = 10;
async function FeederCheck(doFeederAct = true, AUX_THREAD_ID = 0) {

  let repKey = "FeederCheck2";
  reportWait_reg(repKey, "SBM_FBLOC", 4459);
  reportWait_reg(repKey + "_SF", "SurfaceCheck_FB", 4459);


  await Lib.CNCSend({ "type": "AUX_DELAY", "P": dff, aid: 0, aid: AUX_THREAD_ID })
  // dff+=5;
  await Lib.CNCSend({ "type": "AUX_IO_CTRL", "pin": SYS_OUT_PIN_DEF.Feeder_L_Light1, "state": 1, aid: AUX_THREAD_ID })

  // await Lib.CNCSend({ "type": "AUX_WAIT_FOR_FINISH", aid: AUX_THREAD_ID })

  await Lib.CameraSNameSWTrigger("FeederCam", "CAM_FB", 4459, true);
  await Lib.CNCSend({ "type": "AUX_DELAY", "P": 2, aid: 0, aid: AUX_THREAD_ID })
  await Lib.CNCSend({ "type": "AUX_IO_CTRL", "pin": SYS_OUT_PIN_DEF.Feeder_L_Light1, "state": 0, aid: AUX_THREAD_ID })

  // await delay(5)

  let rep = await reportWait(repKey, 10000);
  let rep_SF = await reportWait(repKey + "_SF", 10000);

  console.log(rep, rep_SF);
  return {
    locating_rep: rep,
    surfaceCheck_rep: rep_SF
  }
}

async function ShakeFeeder(vibTime = 400, AUX_THREAD_ID = 2) {
  let FB_Vib_pin1 = (2 ** SYS_OUT_PIN_DEF.Feeder_A_Run1);
  let FB_Vib_pin2 = (2 ** SYS_OUT_PIN_DEF.Feeder_A_Run2);
  let FB_Vib_pin3 = (2 ** SYS_OUT_PIN_DEF.Feeder_A_Run3);
  let FB_Vib_pin4 = (2 ** SYS_OUT_PIN_DEF.Feeder_A_Run4);



  // let pin=FB_Vib_pin1;

  // for(let pin of [false?undefined:[FB_Vib_pin1],false?undefined:[FB_Vib_pin2,500],[FB_Vib_pin3,500]])
  for (let pin of [false ? undefined : [FB_Vib_pin1]]) {
    if (pin === undefined) continue;

    await Lib.CNCSend({ "type": "AUX_IO_CTRL", "pin": SYS_OUT_PIN_DEF.Feeder_A_Run1, "state": 1, aid: AUX_THREAD_ID })

    await Lib.CNCSend({ "type": "AUX_DELAY", "P": vibTime, aid: AUX_THREAD_ID })

    await Lib.CNCSend({ "type": "AUX_IO_CTRL", "pin": SYS_OUT_PIN_DEF.Feeder_A_Run1, "state": 0, aid: AUX_THREAD_ID })

  }

  await Lib.CNCSend({ "type": "AUX_WAIT_FOR_FINISH", aid: AUX_THREAD_ID })
}

async function FeederCheckPickXY() {
  let lv = {};



  await ShakeFeeder();
  await delay(700);
  let feederCheck = await FeederCheck();

  let loc = feederCheck.locating_rep.report.filter((lpt, idx, lrep) => {
    let screp = feederCheck.surfaceCheck_rep.report.sub_reports;
    return (screp[idx].category > 0)
  })

  // console.log(loc)
  return loc;

}

const INIT_headPickIndexs=Array(SYS_CONFIG.Pick_Head_Count).fill(0).map((v,i)=>i);
const INIT_headPickXAdj=Array(SYS_CONFIG.Pick_Head_Count).fill(0);

async function FeederPickAct(availItems, Feeder_loc_Convert, F = 500, ACC = 1000,
  headPickIndexs = INIT_headPickIndexs,
  headPickXAdj = INIT_headPickXAdj,
  doLiftLastHead = true,
  EMCheckStopPointCB = async (info) => { return true }) {
  console.log(availItems);

  let availItems_pick = [...availItems];

  availItems_pick = availItems_pick.sort((a, b) => a.center.x - b.center.x);
  if (availItems_pick.length > SYS_CONFIG.Pick_Head_Count) {
    let newInfos = [];//spread X location
    for (let i = 0; i < SYS_CONFIG.Pick_Head_Count; i++) {
      newInfos.push(availItems_pick[Math.floor((availItems_pick.length - 1) * i / (SYS_CONFIG.Pick_Head_Count-1))]);
    }
    availItems_pick = newInfos;
  }
  console.log(availItems_pick);

  let FA = `F${F} ACC${ACC} DEA${ACC}`
  // await G(`G1 X${345} Y${100} ${FA}`)
  await Exit_Z2SafeZone_n_Check();
  let i = 0;
  let objIndex = 0;
  let STEPC = 0;
  let headPickedIndexs = [];
  let restZ = SYS_CONFIG.SAFE_Z;
  let preZIdx = -1;
  for (i = 0; i < availItems_pick.length; i++) {
    //if i is found in headPickIndexs
    if (headPickIndexs.indexOf(i) < 0) continue;
    let tv3 = THREE.Vector3
    let vpt0_cam = new tv3(availItems_pick[objIndex].center.x, availItems_pick[objIndex].center.y, 1);
    let vpt0_head_comp = vpt0_cam.clone().applyMatrix3(Feeder_loc_Convert.M_CamVecs);
    // console.log(vpt0_cam,vpt0_head_comp);

    let XYZ_Offset = {
      x: -i * SYS_CONFIG.Head_Base_Dist + v.CalibInfo.XY.offset[i].x,
      y: +v.CalibInfo.XY.offset[i].y,
      z: +v.CalibInfo.Z.offset[i],
    }

    let Zprelift = SYS_CONFIG.Feeder_prePick_lift;
    let Zpostlift = SYS_CONFIG.Feeder_postPick_lift;

    let tarZ = Feeder_loc_Convert.pickZ + XYZ_Offset.z;
    let Xtar = vpt0_head_comp.x + XYZ_Offset.x + headPickXAdj[i];
    let Ytar = vpt0_head_comp.y + XYZ_Offset.y;//+0.2;

    let xyPreOffset = 0.2;

    let pickAngle = -availItems_pick[objIndex].angle * 180 / Math.PI;
    pickAngle %= 180;
    await G(`G1  X${(Xtar + xyPreOffset).toFixed(4)} Y${(Ytar + xyPreOffset).toFixed(4)}` +
      ` Z${preZIdx + 1}_${restZ} Z${i + 1}_${(tarZ + Zprelift).toFixed(4)} ` +
      ` R${preZIdx + 1}_${0} R${i + 1}_${(pickAngle).toFixed(4)} ${FA}`)

    if (await EMCheckStopPointCB({ type: "FeederPicking", state: "prep", headIdx: i }) == false) break;
    // await G(`G04 P10`);
    await G(`G1  X${(Xtar).toFixed(4)} Y${(Ytar).toFixed(4)} Z${i + 1}_${(tarZ+1).toFixed(4)} ${FA}`)

    if (await EMCheckStopPointCB({ type: "FeederPicking", state: "pick", headIdx: i }) == false) break;

    await G(`M42 P${SYS_OUT_PIN_DEF["Arm_A_Pick" + (i + 1)]} S1`)
    await G(`G1  X${(Xtar).toFixed(4)} Y${(Ytar).toFixed(4)} Z${i + 1}_${(tarZ).toFixed(4)} ${FA}`)

    await G(`G04 P5`);

    await G(`G1 Z${i + 1}_${tarZ + 1} ${FA}`)
    
    await G(`G1 Z${i + 1}_${tarZ + Zpostlift} ${FA}`)
    if (await EMCheckStopPointCB({ type: "FeederPicking", state: "lift", headIdx: i }) == false) break;
    headPickedIndexs.push(i);
    objIndex++;
    preZIdx = i;
  }

  if(doLiftLastHead)
  {
    await G(`G1  ` +
      ` Z${preZIdx + 1}_${restZ} ` +
      ` R${preZIdx + 1}_${0}  ${FA}`)
  
    await Enter_Z2SafeZone_n_Check();

  }
  return headPickedIndexs;
}


const abortController = new AbortController();



let ReelENC = 0;
async function ReelGoAdv2(adv_mm = 1, RunSTR = ` F20 ACC${50}`) {


  // console.log("AUX_SET_ENC>>>>>",await Lib.CNCSend({ "type": "AUX_SET_ENC",value:0, aid: 0 }));
  // ReelENC=0


  let encStep = adv_mm;
  ReelENC += encStep;
  let A_Tar = Math.round(SYS_CONFIG.A_Axis_Units_to_mm * encStep);

  await G("G92 A0");

  let DRunSpace = SYS_CONFIG.A_Axis_RunHitEnc_BackUnitSpace;
  if (A_Tar >= DRunSpace) {
    await G(`G01 A${A_Tar - DRunSpace} ${RunSTR}`);
  }
  else {
    await G(`G01 ${RunSTR}`);
  }

  // await G("G04 P0");
  await G(`G01.ENC A${A_Tar} ENC${ReelENC} SMULT500 F50 ACC100`);

}

async function ReelGoAdv(adv_mm = 1, RunSTR = ` F20 ACC${50}`) {


  // console.log("AUX_SET_ENC>>>>>",await Lib.CNCSend({ "type": "AUX_SET_ENC",value:0, aid: 0 }));
  // ReelENC=0


  let encStep = adv_mm;
  ReelENC += encStep;
  let A_Tar = Math.round((SYS_CONFIG.A_Axis_Units_to_mm*1.0) * encStep);

  await G("G92 A0");

  let DRunSpace = 0;
  if (adv_mm>0) {
    //ENC_ES will cutoff the motion when the encoder is hit
    await G(`G01.ENC_ES A${A_Tar + DRunSpace} AX_A ENC${ReelENC} ${RunSTR}`);
    
    
    //G01.ENC will run extended steps until the encoder is hit
    await G(`G01.ENC A${A_Tar + DRunSpace+SYS_CONFIG.A_Axis_Units_to_mm}  ENC${ReelENC} SMULT100 F500`);
  }
  else {
    await G(`G01 ${RunSTR}`);
  }


}

async function ReelGoAdv_s(adv_step, RunSTR = ` F50 ACC${100}`) {
  await G("G92 A0");
  await G(`G01 A${adv_step} ${RunSTR}`);
}


async function ReelPositioning_test(Feeder_loc_Convert, Reel_loc_UIInfo, abortSig = undefined) {


  while (true) {


    await ReelGoAdv(4 * 6);
    await G(`M400`);

    let trigID = 44;
    let repWaitKey = ">>><"
    reportWait_reg(repWaitKey + "_RO", "SurfaceCheck_ReelObj", trigID);
    reportWait_reg(repWaitKey + "_RS", "SurfaceCheck_ReelSlot", trigID);

    await ReelCheck(trigID, 1);


    let report_RO = await reportWait(repWaitKey + "_RO")
    let report_RS = await reportWait(repWaitKey + "_RS")

    let Obj_srep = report_RO.report.sub_reports;
    let Slot_srep = report_RS.report.sub_reports;
    console.log({ Obj_srep, Slot_srep })


    if (abortSig && abortSig.aborted) {
      break;
    }

    // await delay(500);

  }


}

async function ReelLocating()
{

  let ACC = "4000";
  let F = "1500";
  let FA = `F${F} ACC${ACC}`

  await Enter_Z2SafeZone_n_Check();
  await G(`G1 X${300} Y${150} ${FA}`)
  await G(`M400`)


  let ReelLocInfo = {
    center: undefined,
    mmpp: NaN
  }
  {


    let reelLocRepKey = "ReelLocRepKey_";
    {

      let reelLocRepTID = 556;
      await Lib.CNCSend({ "type": "AUX_IO_CTRL", "pin": SYS_OUT_PIN_DEF.Reel_L_Light1, "state": 1, aid: 0 })

      reportWait_reg(reelLocRepKey, "SBM_ReelLoc", reelLocRepTID);

      await Lib.CNCSend({ "type": "AUX_DELAY", "P": 500, aid: 0 })

      await Lib.CameraSNameSWTrigger("ReelCheckCam", "CAM_ReelLoc", reelLocRepTID, true);

      await Lib.CNCSend({ "type": "AUX_IO_CTRL", "pin": SYS_OUT_PIN_DEF.Reel_L_Light1, "state": 0, aid: 0 })



    }

    {
      let reelLocRep = (await reportWait(reelLocRepKey)).report;
      console.log(reelLocRep);

      ReelLocInfo.center = reelLocRep[0].center;

      let isOK = reelLocRep.reduce((res, rep) => res && (rep.confidence != 0), true);
      if (isOK == false) {
        GoRun = false;
      }

      let holeDist_pix = Math.hypot(reelLocRep[0].center.x - reelLocRep[1].center.x, reelLocRep[0].center.y - reelLocRep[1].center.y);

      ReelLocInfo.mmpp = SYS_CONFIG.Reel_Hole_Distance / holeDist_pix;
    }


  }
  console.log("ReelLocInfo:", ReelLocInfo);
  return ReelLocInfo;
}

async function CYCLERun_test(Feeder_loc_Convert, Reel_loc_UIInfo, abortSig = undefined, EMCheckStopPointCB = async (info) => { return true }) {

  let Mark = "_" + Math.random().toString(36).substring(7);


  let ReelReady_key = "ReelReady" + Mark;
  let ReelClear2Check_key = "ReelClear2Check" + Mark;



  let FeederReady_key = "FeederReady" + Mark;
  let FeederClear2Act_key = "FeederClear2Act" + Mark;
  let FeederClear2Check_key = "FeederClear2Check" + Mark;




  console.log("AUX_GET_ENC>>>>>", await Lib.CNCSend({ "type": "AUX_GET_ENC", aid: 0 }));

  let placeTarCountSeq=[...v.ReelPackingInfo.packingSeq]//[20];//[1,-49,500,-20,2,-49,500,-20,1 ];
  if(placeTarCountSeq.length==0)return;

  if(placeTarCountSeq[0]<0)
  {
    await ReelGoAdv((-placeTarCountSeq[0]) * SYS_CONFIG.Reel_Hole_Distance*SYS_CONFIG.Holes_per_Slot);
    placeTarCountSeq.shift();

    v.ReelPackingInfo.packingSeq=[...placeTarCountSeq];
  }
  if(placeTarCountSeq.length==0)return;



  let timeMult = 1;
  SigWait_Reg(FeederReady_key);
  SigWait_Reg(FeederClear2Act_key);
  SigWait_Reg(FeederClear2Check_key);






  //FeederCheck....
  let FStr = "F    "
  console.log(v.sigListener);
  (async () => {//run thread, KIND OF
    let vibExtendCD = 0;


    let avaCands = [];
    while (true) {
      let step = 0;
      console.log(step++, FStr + ":Wait Clear2Check ...");
      await SigWait(FeederClear2Act_key, 30000);
      console.log(step++, FStr + ":Feeder is Clear to act ...");
      SigWait_Reg(FeederClear2Act_key);

      if (avaCands.length < SYS_CONFIG.Pick_Head_Count) {

        console.log(step++, FStr + ":Feeder vib ...");

        await ShakeFeeder(vibExtendCD > 0 ? 1200 : 400);

        vibExtendCD--;
        await SigWait(FeederClear2Check_key, 30000);
        SigWait_Reg(FeederClear2Check_key);
        // await delay(200*timeMult);
        console.log(step++, FStr + ":Feeder cam check ...");
        await delay(300);

        let feederCheck = await FeederCheck();
        let objCounts = feederCheck.locating_rep.report.length;
        avaCands = feederCheck.locating_rep.report.filter((lpt, idx, lrep) => {
          let screp = feederCheck.surfaceCheck_rep.report.sub_reports;
          return (screp[idx].category > 0)
        })

        if (
          (objCounts < 70 && avaCands.length < SYS_CONFIG.Pick_Head_Count)||
          (objCounts < 60 && avaCands.length < SYS_CONFIG.Pick_Head_Count*2)) {//Time to add more in feeder

          await Lib.CNCSend({ "type": "AUX_IO_CTRL", "pin": SYS_OUT_PIN_DEF.Feeder_F_Run1, "state": 1, aid: 4 })

          await Lib.CNCSend({ "type": "AUX_DELAY", "P": 700, aid: 4 })
          await Lib.CNCSend({ "type": "AUX_IO_CTRL", "pin": SYS_OUT_PIN_DEF.Feeder_F_Run1, "state": 0, aid: 4 })
          vibExtendCD = 3;
        }


      }
      else {
        await SigWait(FeederClear2Check_key, 30000);
        SigWait_Reg(FeederClear2Check_key);

      }

      let pickItems = []
      //split first 4 items from avaCands to availItems
      pickItems = avaCands.splice(0, SYS_CONFIG.Pick_Head_Count);
      // pickItems=avaCands.splice(0,avaCands.length);

      console.log(step++, FStr + ":Feeder send check info ...", pickItems);
      SigWait_Trig(FeederReady_key, pickItems);
    }

  })().catch((e) => {
    console.log("Feeder Thread exit....", e)
  })






  SigWait_Reg(ReelReady_key);
  SigWait_Reg(ReelClear2Check_key);

  //Reel....
  let RStr = "R  "
  console.log(v.sigListener);
  (async () => {//run thread, KIND OF

    while (true) {
      let step = 0;
      console.log(step++, RStr + ":Wait Clear2Act ...");
      await SigWait(ReelClear2Check_key, 30000);
      SigWait_Reg(ReelClear2Check_key);
      console.log(step++, RStr + ":Reel cam check top side ...");

      let slotNeedClean = [];
      let slotCanPlace = [];
      let slotPlaced = [];


      let trigID = 44;
      let repWaitKey = ">>><"
      reportWait_reg(repWaitKey + "_RO", "SurfaceCheck_ReelObj", trigID);
      reportWait_reg(repWaitKey + "_RS", "SurfaceCheck_ReelSlot", trigID);

      await ReelCheck(trigID, 1);


      let report_RO = await reportWait(repWaitKey + "_RO")
      let report_RS = await reportWait(repWaitKey + "_RS")

      let Obj_srep = report_RO.report.sub_reports;
      let Slot_srep = report_RS.report.sub_reports;
      console.log({ Obj_srep, Slot_srep })

      let advCount = 0;
      for (let i = 0; i < Obj_srep.length; i++) {
        if (Obj_srep[i].category != 1) break;
        advCount++;
      }



      for (let i = 0; i < Slot_srep.length; i++) {
        let isAObj = (i < Obj_srep.length) && (Obj_srep[i].category == 1);
        let objInPlace = (Slot_srep[i].category != 1) && isAObj;
        let eptSlot = Slot_srep[i].category == 1;
        slotPlaced.push(objInPlace ? i : -1)
        slotCanPlace.push(eptSlot ? i : -1);
        slotNeedClean.push((!objInPlace && !eptSlot) ? i : -1);
      }

      // console.log("advCount:",advCount);

      // console.log("slotPlaced:",slotPlaced);
      // console.log("slotCanPlace:",slotCanPlace);
      // console.log("slotNeedClean:",slotNeedClean);






      SigWait_Trig(ReelReady_key, {objInspResult:Obj_srep ,advCount, slotPlaced, slotCanPlace, slotNeedClean });
    }

  })().catch((e) => {
    console.log("Reel Thread exit....", e)
  })


  let GoRun = true;

  let ACC = 4800;
  let F = 1700;
  let FA = `F${F} ACC${ACC}`
  let FA_SZ = `F${2000} ACC${5500}`
  await Enter_Z2SafeZone_n_Check();
  // await G(`G1 X${300} Y${250} ${FA}`)

  // await G(`M400`)

  SigWait_Trig(FeederClear2Act_key);

  SigWait_Trig(FeederClear2Check_key);


  let FeederReadyInfo = await SigWait(FeederReady_key, 30000);
  SigWait_Reg(FeederReady_key);


  await G(`G1 X${300} Y${150} ${FA}`)
  await G(`M400`)


  let ReelLocInfo = v.CalibInfo.ReelLoc;//await ReelLocating()
  console.log("ReelLocInfo:", ReelLocInfo);


  SigWait_Trig(ReelClear2Check_key);

  let ReelReadyInfo = undefined;



  async function nozzleRelease(releaseIdexs = [0, 1, 2, 3]) {

    await Enter_Z2SafeZone_n_Check();
    await G(`G1 X${SYS_CONFIG.HeadReleaseLoc[0]} Y${SYS_CONFIG.HeadReleaseLoc[1]} ${FA}`)
    // await G(`G1 R1_20  R2_20  R3_20  R4_20  ${FA}`)
    // await G(`G1 R1_0  R2_0  R3_0  R4_0  ${FA}`)
    for (let idx of releaseIdexs) {
      await G(`M42 P${idx} S0`)
    }

    // await G(`G1 R1_-20  R2_-20  R3_-20  R4_-20  ${FA}`)
    // await G(`G1 R1_0  R2_0  R3_0  R4_0  ${FA}`)
    await G(`M400`)

  }
  await nozzleRelease();

  let Reel_A_Run1_pin = (2 ** SYS_OUT_PIN_DEF.Reel_A_Run1);

  await G(`M42 PORT${Reel_A_Run1_pin}    S${Reel_A_Run1_pin}`);


  let startTime = Date.now();
  let startReelENC = ReelENC;

  let feederFetchAdj = Array(SYS_CONFIG.Pick_Head_Count).fill(-0);


  let headState = Array(SYS_CONFIG.Pick_Head_Count).fill( E_headState.empty);

  let placedCount = 0;  
  //0:empty 1:may have obj 2:obj OK,-1:obj NG
  try {
    let XYRelocCD=0;


    let prePassCount=0;
    let LPSpeed=0;

    let preTime = Date.now();
    for (let i = 0; GoRun; i++) {
      //1. pick from feeder, and check the Reel
      let STEPC = 0;


      await G(`G1 X${SYS_CONFIG.PickReadyLoc[0]} Y${SYS_CONFIG.PickReadyLoc[1]} ${FA}`)


      let curTime = Date.now();

      let ReelAdv = (ReelENC - startReelENC);
      let PassCount = ReelAdv / SYS_CONFIG.Reel_Hole_Distance/SYS_CONFIG.Holes_per_Slot;
      let elapsedTime = curTime - startTime;
      let speed = PassCount / (elapsedTime+0.1) * 60 * 1000

      let curSpeed=(PassCount-prePassCount)/(curTime-preTime+0.1)*60*1000;
      LPSpeed+=(curSpeed-LPSpeed)*0.1;

      console.log("Cycle:", i, " PassCount:", PassCount, "pcs", " CurCycleTime:", (curTime - preTime) / 1000, "s",
        "Speed:", (speed).toFixed(1),"("+LPSpeed.toFixed(1)+")", "pcs/min", " TimeElapse:", ((elapsedTime) / 1000 / 60).toFixed(3), "min");
      preTime = curTime;
      prePassCount=PassCount;
      if (await EMCheckStopPointCB({ type:"STATISTIC", count: PassCount,overall_speed: speed, current_speed:LPSpeed }) == false) break;
      

      if (await EMCheckStopPointCB({ type:"CYCLE_STATE", state: "begin" }) == false) break;
      
      // console.log("____FeederReadyInfo:",FeederReadyInfo);



      // await delay(500);
      // await G(`M400`)
      //1.1 picking

      // console.log("____Picking");
      // for(let j=0;j<2;j++)
      // {
      //   await G(`G1 X${300} Y${100} ${FA}`)
      //   await G(`G1 X${250} Y${100} ${FA}`)
      // }
      let pickHeadIndexs = headState.map((st, i) => st == E_headState.empty ? i : -1).filter(i => i != -1);
      console.log("$$pickHeadIndexs:", pickHeadIndexs);
      let headPickedIndexs = await FeederPickAct(FeederReadyInfo, Feeder_loc_Convert, F, ACC, pickHeadIndexs, feederFetchAdj,false, EMCheckStopPointCB);
      
      headPickedIndexs.forEach(i => headState[i] = E_headState.mayHaveObj);
      console.log("$$headState:", headState);

      await G(`M400`)
      SigWait_Trig(ReelClear2Check_key);
      if (await EMCheckStopPointCB({ type: "PickDone" }) == false) break;



      //1.2 picked, ok to vib feeder
      SigWait_Trig(FeederClear2Act_key);
      ///////////////////////////////////////////SIDE check///////////////////////////////////////////////

      let repSideCheckWaitKey = "repWK";
      let SideCheckResultPromise=undefined;
      {
        await Exit_Z2SafeZone_n_Check();
        let LocFA=FA;

        await G(`G01 X${SYS_CONFIG.SideCam_InspLoc[0]} Y${SYS_CONFIG.SideCam_InspLoc[1]} `+
                `Z1_${SYS_CONFIG.SideCam_InspLoc[2]} Z2_${SYS_CONFIG.SideCam_InspLoc[2]} Z3_${SYS_CONFIG.SideCam_InspLoc[2]} Z4_${SYS_CONFIG.SideCam_InspLoc[2]} `+
                `R1_${SYS_CONFIG.SideCam_RAngOffset[0] } R2_${SYS_CONFIG.SideCam_RAngOffset[1]} R3_${SYS_CONFIG.SideCam_RAngOffset[2]} R4_${SYS_CONFIG.SideCam_RAngOffset[3]} ${LocFA}`)


        await G(`M400`)

        if (abortSig && abortSig.aborted) {
          break;
        }
  
        if (await EMCheckStopPointCB({ type: "SideCheck" }) == false) break;
        await Lib.CNCSend({ "type": "AUX_IO_CTRL", "pin": SYS_OUT_PIN_DEF.Feeder_L_Light2, "state": 1, aid: 0 })

        {
          
          let trigID = 4564;

          reportWait_reg(repSideCheckWaitKey + "_loc", "SBM_CAM_A", trigID);


          {//set flag to SurfaceCheck_CAM_A to use extParam of SurfaceCheck_CAM_A
            await InspTargetExchange("SurfaceCheck_CAM_A", { type: "useExtParam" })
            reportWait_reg(repSideCheckWaitKey + "_check", "SurfaceCheck_CAM_A", trigID);
          }

          await Lib.CameraSNameSWTrigger("SideCheckCam", ["CAM_A","s_SIDE_FLAT"], trigID, true);

          // await Lib.CameraSNameSWTrigger("", ["CAM_A","s_SIDE_FLAT"], trigID, true);

          if (await EMCheckStopPointCB({ type: "SideCheck",state:"side_flat" }) == false) break;

          let sideAng=50;
          await G(`G01 R1_${sideAng+SYS_CONFIG.SideCam_RAngOffset_Side[0] } R2_${sideAng+SYS_CONFIG.SideCam_RAngOffset_Side[1]} R3_${sideAng+SYS_CONFIG.SideCam_RAngOffset_Side[2]} R4_${sideAng+SYS_CONFIG.SideCam_RAngOffset_Side[3]} ${LocFA}`)


          await G(`M400`)

          reportWait_reg(repSideCheckWaitKey + "_side", "SurfaceCheck_CAM_SIDE", trigID+2);
          await Lib.CameraSNameSWTrigger("SideCheckCam", ["CAM_A","s_SIDE_SIDE"], trigID+2, true);


          if (await EMCheckStopPointCB({ type: "SideCheck",state:"side_side" }) == false) break;



          await Enter_Z2SafeZone_n_Check();
          Lib.CNCSend({ "type": "AUX_IO_CTRL", "pin": SYS_OUT_PIN_DEF.Feeder_L_Light2, "state": 0, aid: 0 })






          SideCheckResultPromise=new Promise(async (resolve,reject)=>{

            let hRs = [];
            let objXErr = [];
            {

              let report_loc = await reportWait(repSideCheckWaitKey + "_loc")



              let report_check = undefined;
              {//modify the locating report and feed to extParam of SurfaceCheck_CAM_A
                let new_report_loc = report_loc;

                new_report_loc.report[0].angle = (-4-1) * Math.PI / 180;
                new_report_loc.report[1].angle = (-2+-1) * Math.PI / 180;
                new_report_loc.report[2].angle = ( 2-1) * Math.PI / 180;
                new_report_loc.report[3].angle = ( 3.5) * Math.PI / 180;
                await InspTargetExchange("SurfaceCheck_CAM_A", { type: "extParam", orientation: new_report_loc.report })
                report_check = await reportWait(repSideCheckWaitKey + "_check")
                //and disable the extParam of SurfaceCheck_CAM_A at last
                await InspTargetExchange("SurfaceCheck_CAM_A", { type: "useExtParam", enable: false });

              }
              // console.log(report_check);


              let distDiff = report_check.report.sub_reports.map(rep => {
                if (rep.sub_regions.length == 0) return NaN;
                return rep.sub_regions[9].score - rep.sub_regions[10].score
              })

              objXErr = distDiff.map(diff => diff * SYS_CONFIG.SideCam_mmpp);

              let report_check_side = await reportWait(repSideCheckWaitKey + "_side")
              console.log(report_check_side);

              let failInfo=[];
              let sideWidthList=[];
              let dir = report_check.report.sub_reports.map((rep, idx) => {

                let failVec= [];


                if (rep.sub_regions.length == 0)
                {
                  failVec.push({r:"ShapeCheck data.lenth:"+rep.sub_regions.length});
                  failInfo.push(failVec)
                  return undefined;
                }


                let sideCheck=report_check_side.report.sub_reports[idx]

                if (sideCheck.sub_regions.length == 0)
                {
                  failVec.push({r:"SideCheck data.lenth:"+sideCheck.sub_regions.length});
                  failInfo.push(failVec)
                  return undefined;
                }





                {
                  let prhibitZone=sideCheck.sub_regions[4].category;
                  if (prhibitZone!= 1) 
                  {
                    failVec.push({r:"prhibitZone"})
                  };

                  let sideLB=sideCheck.sub_regions[0].score;
                  let sideRB=sideCheck.sub_regions[1].score;
                  let sideWidth=sideRB-sideLB;
                  let sideNozzleX=sideCheck.sub_regions[2];

                  let LBV=15;
                  let UBV=50;
                  sideWidthList.push(sideWidth);
                  if(sideWidth<LBV)
                    failVec.push({r:"sideWidth <"+LBV,sideWidth,sideLB,sideRB,sideNozzleX});

                  if(sideWidth>UBV)
                    failVec.push({r:"sideWidth >"+UBV,sideWidth,sideLB,sideRB,sideNozzleX});

                }




                if (rep.sub_regions[2].category != 1)failVec.push({r:"BtmCheck failed",rep:rep.sub_regions[2]});
                if (rep.sub_regions[11].category != 1)failVec.push({r:"LBCheck failed",rep:rep.sub_regions[11]});
                if (rep.sub_regions[12].category != 1)failVec.push({r:"RBCheck failed",rep:rep.sub_regions[12]});
                if (rep.sub_regions[14].category != 1)failVec.push({r:"SurroundCheck failed",rep:rep.sub_regions[14]});


                // if(Math.abs(distDiff[idx])>7)return undefined;
                if (rep.sub_regions[1].category != rep.sub_regions[4].category)
                  failVec.push({r:"OrientationCheck failed",rep:[rep.sub_regions[1],rep.sub_regions[4]]});

                failInfo.push(failVec)
                if(failVec.length>0)return undefined;
                return rep.sub_regions[1].category;
              })

              console.log("dir:", dir,"failInfo:",failInfo);

              {
                let adjAddAlpha = 0.1;
                feederFetchAdj = feederFetchAdj.map((adj, idx) => {
                  if (dir[idx] === undefined) return adj;
                  // adj*=0.9;
                  return adj - objXErr[idx] * adjAddAlpha;
                })
                console.log("feederFetchAdj:",feederFetchAdj);
              }

              // await delay(3000)
              // if(dir.findIndex(d=>d===undefined)!=-1)
              // {
              //   await nozzleRelease();

              //   continue;
              // }

              dir.forEach((d, i) => {
                if (d === undefined) {
                  if (headState[i] != E_headState.empty) {
                    headState[i] = E_headState.NG;
                  }
                }
                else {
                  headState[i] = E_headState.OK;
                }
              })


              hRs = [0, 0, 0, 0];
              for (let i = 0; i < dir.length; i++) {
                if (dir[i] === undefined) continue;
                let sideWidthDiff2AngCoeff=1;
                let angOffset = (sideWidthList[i]-43)*sideWidthDiff2AngCoeff;

                if (dir[i] == 1) {

                  hRs[i] = 90+angOffset;
                }
                else if (dir[i] == -1) {
                  objXErr[i] *= -1;
                  hRs[i] = -90+angOffset;
                }
              }

              // console.log("distDiff:",distDiff,"objXErr:",objXErr,"hRs:",hRs);
              // await Lib.CameraSNameSWTrigger("Hikrobot-2BDF50664114-00F50664114","CAM_A",trigID+1,true);
              // console.log(report_loc,report_check,dir);
              // await delay(500);
              // await Enter_Z2SafeZone_n_Check();


              // PackAutoTermFlag=true;

            
            }
            resolve({
              hRs,
              objXErr,
            });

          });
    



        }


        // await delay(100)
      }



      //2. get reel check info
      ReelReadyInfo = await SigWait(ReelReady_key, 30000);
      SigWait_Reg(ReelReady_key);



      // console.log("____ReelReadyInfo:",ReelReadyInfo);
      // console.log("____hRs:",hRs,"objXErr:",objXErr);

      // console.log("____Placing");
      //2.1 place object to reel

      // await G(`G1 X${100} Y${300} ${FA}`)
      // await G(`M400`)
      // for(let j=0;j<3;j++)
      // {
      //   await G(`G1 X${100} Y${300} ${FA}`)
      //   await G(`G1 X${150} Y${300} ${FA}`)
      // }

      console.log("placeTarCountSeq:",placeTarCountSeq);

      let RestPlaceNumber=0;


      let pickNGCount=0;
      ///////////////////////////////////////////Reel loc check///////////////////////////////////////////////
      {
        let ReelSlotAdvCount = ReelReadyInfo.advCount;
        let slotNeedClean = ReelReadyInfo.slotNeedClean;
        let slotCanPlace = ReelReadyInfo.slotCanPlace;
        let slotPlaced = ReelReadyInfo.slotPlaced;
        let objInspResult=ReelReadyInfo.objInspResult;

        placedCount+=ReelSlotAdvCount;
        RestPlaceNumber=placeTarCountSeq[0]-placedCount;
        let avaSlotList = slotCanPlace
          .filter(slotIdx => slotIdx !== -1)
          .map(idx => idx - ReelSlotAdvCount)

          console.log("$$avaSlotList:",avaSlotList," RestPlaceNumber:",RestPlaceNumber);

        avaSlotList=avaSlotList.filter(slotIdx=>slotIdx<RestPlaceNumber);  
        // console.log(avaSlotList);

        let cleanSlotList = slotNeedClean
          .filter(slotIdx => slotIdx !== -1)
          .map(idx => idx - ReelSlotAdvCount)
        // .filter(slotIdx=>slotIdx>0);


        let placedSlotList = slotPlaced
          .filter(slotIdx => slotIdx !== -1)
          .map(idx => idx - ReelSlotAdvCount)
        // .filter(slotIdx=>slotIdx>0);

        console.log("$$avaSlotList:",avaSlotList);
        console.log("$$cleanSlotList:",cleanSlotList);
        console.log("$$placedSlotList:",placedSlotList);

        if(ReelSlotAdvCount==0&&avaSlotList.length==0&&cleanSlotList.length!=0&&RestPlaceNumber!=0)
        {

          let releaseIdxes=cleanSlotList.filter((v,i)=>i<SYS_CONFIG.Pick_Head_Count).map((v,i)=>i)
          console.log("$$RELEASE IDXES:",releaseIdxes);
          await nozzleRelease(releaseIdxes);
          releaseIdxes.forEach(idx=>headState[idx]=E_headState.empty);
        }



        let baseLoc = Reel_loc_UIInfo["pt1"].headXY;


        // let Rget = (offset = 0,hRsFactor=1) => hRs.reduce((str, rs, idx) => str + `R${idx + 1}_${rs*hRsFactor + offset} `, "");
        // let Zget = (offset = 0) => idxzt.reduce((str, zs, idx) => str + `Z${idx + 1}_${zs + offset} `, "");



        if (await EMCheckStopPointCB({ type: "ReelAdvancing", unit: ReelSlotAdvCount }) == false) break;

        let reelLocRepKey = "ReelLocRepKey";
        {

          let reelLocRepTID = 556;

          reportWait_reg(reelLocRepKey, "SBM_ReelLoc", reelLocRepTID);
          let xyGCode = `X${baseLoc[0]} Y${baseLoc[1]} ${ReelSlotAdvCount > 6 ? `F${F} ACC${(ACC*3/4).toFixed(1)}` : ""} ${FA}`;
          if (ReelSlotAdvCount > 0) {

            await ReelGoAdv(ReelSlotAdvCount * SYS_CONFIG.Reel_Hole_Distance*SYS_CONFIG.Holes_per_Slot, xyGCode);

            // await G(`M400`)
          }
          else {
            await G(`G1 ${xyGCode}`);
          }


          if (await EMCheckStopPointCB({ type: "ReelLocating" }) == false) break;

          await G(`M400`)

          if(1){
            let CurENCReading=(await Lib.CNCSend({ "type": "AUX_ENC_V", aid: 0 })).enc_v
            console.log("ENC_CHECK:",CurENCReading,ReelENC);

            if(Math.abs(CurENCReading-ReelENC)>2)
            {
              await Enter_Z2SafeZone_n_Check();
              // break;
              throw new Error(`ENC_CHECK_FAIL  abs(CurENCReading:${CurENCReading}-tarENC:${ReelENC})>2`);
            }
            ReelENC=CurENCReading;
          }
          else
          {

            await delay(4);
          }
          


          await Lib.CNCSend({ "type": "AUX_IO_CTRL", "pin": SYS_OUT_PIN_DEF.Reel_L_Light1, "state": 1, aid: 0 })

          await Lib.CameraSNameSWTrigger("ReelCheckCam", "CAM_ReelLoc", reelLocRepTID, true);
          await Lib.CNCSend({ "type": "AUX_DELAY", "P": 5, aid: 0, aid: 0 })

          await Lib.CNCSend({ "type": "AUX_IO_CTRL", "pin": SYS_OUT_PIN_DEF.Reel_L_Light1, "state": 0, aid: 0 })

          await Exit_Z2SafeZone_n_Check();

        }

        let { 
          hRs,
          objXErr
        }=await SideCheckResultPromise;


        let tarZ = baseLoc[2];

        // console.log("tarZ>>>",tarZ);

        // await G(`G1 Z1_${tarZ+5}  Z2_${tarZ+5}  Z3_${tarZ+5}  Z4_${tarZ+5} ${FA}`)
        // await G(`G04 P3000`);

        let idxzt = v.CalibInfo.Z.offset.map(offset => tarZ + offset);


        SigWait_Trig(FeederClear2Check_key);


        // let FA = `F${F} ACC${ACC}`
        if (true) {



          let slotCount = 0;
          let preHeadIdx = 0;
          let placeCount = 0;
          let pickCount = 0;


          let reelOffset_ = undefined;

          async function reelOffsetGet()
          {

            if(reelOffset_!==undefined)return reelOffset_;
            let reelLocRep = (await reportWait(reelLocRepKey)).report;



            if ((reelLocRep?.[0]?.confidence > 0)) {
              let reelLocPt = reelLocRep?.[0]?.center;
              let offset_pix = {
                x: reelLocPt.x - ReelLocInfo.center.x,
                y: -(reelLocPt.y - ReelLocInfo.center.y),
              }
              reelOffset_ = {
                x: offset_pix.x * ReelLocInfo.mmpp - 0.0,
                y: offset_pix.y * ReelLocInfo.mmpp,
              }
              // reelLocRep?.report?[0]?.center;
              // console.log("reelLocPt:",reelLocPt,ReelLocInfo,offset_pix,reelOffset);
              return reelOffset_;
            }
            else {

              console.log("ReelLoc Fail!");
              throw new Error("Reel Locating Failed! (locating circle/slot is not able to be found)");
              // break;
            }
            return undefined;
          }



          let reelOffset =undefined;

          console.log("$$headState:", headState);
          if (await EMCheckStopPointCB({ type: "ReelPlacing", state: "begin" }) == false) break;

          for (let j = 0; j < SYS_CONFIG.Pick_Head_Count; j++) {
            let headIdx = j;
            if (hRs[headIdx] == 0) continue;
            let xoffset = (v.CalibInfo === undefined ? 0 : v.CalibInfo.XY.offset[headIdx].x);
            let yoffset = (v.CalibInfo === undefined ? 0 : v.CalibInfo.XY.offset[headIdx].y);
            let zoffset = (v.CalibInfo === undefined ? 0 : v.CalibInfo.Z.offset[headIdx]);

            if (j >= avaSlotList.length) break;
            let slotIdx = avaSlotList[slotCount];
            if(slotIdx>=RestPlaceNumber)break;

            slotCount++;
            let cZ = tarZ + zoffset;
            cZ = Math.round(cZ * 1000) / 1000;

            //HACK.......Ugly speed up
            //Initial movement is to move without reelOffset(with higher Z), then move to the correct position(with target Z)
            if(reelOffset===undefined)
            {
              let def_reelOffset={x:0,y:0}
              let Xpos = def_reelOffset.x + baseLoc[0] + xoffset
                        + slotIdx * (SYS_CONFIG.Holes_per_Slot*SYS_CONFIG.Reel_Hole_Distance) 
                        - j * SYS_CONFIG.Head_Base_Dist;
              let Ypos = def_reelOffset.y + baseLoc[1] + yoffset + objXErr[headIdx];
              // await G(`M400`)
              let GCode = `X${Xpos.toFixed(3)} Y${Ypos.toFixed(3)} Z${preHeadIdx}_${SYS_CONFIG.Reel_postPlace_Z} Z${headIdx + 1}_${cZ + SYS_CONFIG.Reel_prePlace_lift+5}   R${j + 1}_${hRs[j]} ${FA_SZ}`
              
              await G(`G01 ${GCode}`);
            }

            reelOffset = await reelOffsetGet();
            if(reelOffset===undefined)break;
            let Xpos = reelOffset.x + baseLoc[0] + xoffset
                      + slotIdx * (SYS_CONFIG.Holes_per_Slot*SYS_CONFIG.Reel_Hole_Distance) 
                      - j * SYS_CONFIG.Head_Base_Dist;
            let Ypos = reelOffset.y + baseLoc[1] + yoffset + objXErr[headIdx];
            // await G(`M400`)
            if(headIdx==4-1 &&preHeadIdx ==1)
            {
              let GCode = `Z${preHeadIdx}_${SYS_CONFIG.Reel_postPlace_Z}  R${j + 1}_${hRs[j]} ${FA_SZ}`
              
              await G(`G01 ${GCode}`);

            }
            let GCode = `X${Xpos.toFixed(3)} Y${Ypos.toFixed(3)} Z${preHeadIdx}_${SYS_CONFIG.Reel_postPlace_Z} Z${headIdx + 1}_${cZ + SYS_CONFIG.Reel_prePlace_lift}   R${j + 1}_${hRs[j]} ${FA_SZ}`
            
            await G(`G01 ${GCode}`);

            //HACK.......Ugly speed up


            preHeadIdx = headIdx + 1;
            if (await EMCheckStopPointCB({ type: "ReelPlacing", state: "prep", headIdx: headIdx }) == false) break;
            // await G(`G04 P800`);
            await G(`G01 Z${j + 1}_${cZ} ${FA_SZ}`);
            if (await EMCheckStopPointCB({ type: "ReelPlacing", state: "place", headIdx: headIdx }) == false) break;
            // await G(`G01 Z${j+1}_${tarZ+zoffset+testLiftZ} ${FA_SZ}`);

            await G(`M42 P${j} S0`)
            // await G(`G04 P400`);
            await G(`G04 P5`);
            await G(`G01 Z${j + 1}_${cZ+SYS_CONFIG.Reel_postPlace_lift} R${j + 1}_${hRs[j] * 3 / 5} ${FA_SZ}`);


            headState[headIdx] = E_headState.empty;
            if (await EMCheckStopPointCB({ type: "ReelPlacing", state: "lift", headIdx: headIdx }) == false) break;

            placeCount++;
            // await G(`G04 P5`);
            // await G(`G01 X${Xpos-1} ${FA}`);
            // await G(`G01 R${j+1}_${hRs[j]*4/5} ${FA_SZ}`);
          }
          reelOffset = await reelOffsetGet();
          if(reelOffset===undefined)
          {
            await Enter_Z2SafeZone_n_Check();
            break;
          }

          console.log(`ReelOffset:${reelOffset.x.toFixed(3)},${reelOffset.y.toFixed(3)}`);
          await G(`G01 Z${preHeadIdx}_${SYS_CONFIG.Reel_postPlace_Z} ${FA_SZ}`);
          if (await EMCheckStopPointCB({ type: "ReelPlacing", state: "end" }) == false) break;
          // if(avaHeadIdx>=0)
          //   await G(`G04 P2000`);
          // console.log(">avaHeadIdx>",avaHeadIdx);
          // console.log(">avaSlotList>",avaSlotList,"ReelSlotAdvCount:",ReelSlotAdvCount,"slotNeedClean:",slotPlaced);
          preHeadIdx = 0;
          let slotFillCount = 0;
          if (await EMCheckStopPointCB({ type: "ReelPicking", state: "begin" }) == false) break;
          for (let j = 0; j < SYS_CONFIG.Pick_Head_Count; j++) {



            let headIdx = j;
            if (headState[headIdx] != E_headState.empty) continue;
            if (slotFillCount >= cleanSlotList.length) break;
            let slotIdx = cleanSlotList[slotFillCount++];
            if (slotIdx < 0) break;
            if (slotIdx >= objInspResult.length - ReelSlotAdvCount) break;//do not pick slots that are not quality inspected yet
            pickNGCount++;
            headState[headIdx] = E_headState.NG;

            let xoffset = (v.CalibInfo === undefined ? 0 : v.CalibInfo.XY.offset[headIdx].x);
            let yoffset = (v.CalibInfo === undefined ? 0 : v.CalibInfo.XY.offset[headIdx].y);
            let zoffset = (v.CalibInfo === undefined ? 0 : v.CalibInfo.Z.offset[headIdx]);

            let Xpos = reelOffset.x + baseLoc[0] + xoffset
                      + slotIdx * (SYS_CONFIG.Holes_per_Slot*SYS_CONFIG.Reel_Hole_Distance) 
                      - j * SYS_CONFIG.Head_Base_Dist;

            let Ypos = reelOffset.y + baseLoc[1] + yoffset;




            let cZ = tarZ + zoffset;
            cZ = Math.round(cZ * 1000) / 1000;
            // await G(`M400`)
            let GCode = `X${Xpos} Y${Ypos} Z${preHeadIdx}_${SYS_CONFIG.Reel_postPick_Z} Z${headIdx + 1}_${cZ + SYS_CONFIG.Reel_prePick_lift} ${FA}`
            preHeadIdx = headIdx + 1;


            // console.log(">TG>>>");
            await G(`G01 ${GCode}`);

            if (await EMCheckStopPointCB({ type: "ReelPicking", state: "prep", headIdx: headIdx }) == false) break;
            // await G(`G04 P1000`);
            await G(`G01 Z${headIdx + 1}_${cZ +SYS_CONFIG.Reel_Pick_lift} ${FA}`);

            if (await EMCheckStopPointCB({ type: "ReelPicking", state: "pick", headIdx: headIdx }) == false) break;
            // await G(`G01 Z${j+1}_${tarZ+zoffset+testLiftZ} ${FA}`);

            await G(`M42 P${headIdx} S1`)
            await G(`G04 P30`);
            await G(`G01 Z${headIdx + 1}_${cZ + SYS_CONFIG.Reel_postPick_lift} ${FA}`);
            if (await EMCheckStopPointCB({ type: "ReelPicking", state: "lift", headIdx: headIdx }) == false) break;
            pickCount++;
          }
          await reelOffsetGet();
          console.log("$$headState:", headState);
          console.log("$$END:", headState);
          if (await EMCheckStopPointCB({ type: "ReelPicking", state: "end", headState }) == false) break;

          await Enter_Z2SafeZone_n_Check(`F${SYS_CONFIG.MAXF} ACC${SYS_CONFIG.MAXACC}`, false);


          if(v.ReelPackingInfo.packingSeq.length>0)
          {
            v.ReelPackingInfo.packingSeq[0]=RestPlaceNumber;

          }

          if(RestPlaceNumber<=0)
          {
            placedCount=0;
            placeTarCountSeq.shift();
            v.ReelPackingInfo.packingSeq=[...placeTarCountSeq];
            if(placeTarCountSeq.length==0)
            {
              console.log("DONE!!!!!");
              break; 
            }
            while(placeTarCountSeq[0]<0)
            {
              await ReelGoAdv((-placeTarCountSeq[0]) * SYS_CONFIG.Reel_Hole_Distance*SYS_CONFIG.Holes_per_Slot);
              placeTarCountSeq.shift();
              v.ReelPackingInfo.packingSeq=[...placeTarCountSeq];
              if(placeTarCountSeq.length==0)
              {
                break; 
              }
            }

            if(placeTarCountSeq.length==0)
            {
              console.log("DONE!!!!!");
              break; 
            }
          }




          // await G(`G04 P2000`);

          // await delay(1000);
          // await G(`G1 X${300} Y${100} ${FA}`)
          // await G(`G1 R1_0  R2_0  R3_0  R4_0`)
          // await Enter_Z2SafeZone_n_Check();
          // await Enter_Z2SafeZone_n_Check();
          // await G(`M400`)

        }


        console.log(headState);
        {
          let releaseIndexes = headState.map((st, i) => (st == E_headState.NG) ? i : -1).filter(i => i >= 0);
          if (releaseIndexes.length > 0) {
            await nozzleRelease(releaseIndexes);
            releaseIndexes.forEach(i => headState[i] = E_headState.empty);
          }
        }
      }






      // if(term()==true)break;


      if (abortSig && abortSig.aborted) {
        break;
      }

      //HACK fix, somehow the XY robot would drift after a while, so adjust it back here
      //Also, do adjust only if head0 is empty.
      if(XYRelocCD<=0 &&pickNGCount==0 &&headState[0]==E_headState.empty)
      {

        let X=SYS_CONFIG.PickReadyLoc[0];
        let Y=SYS_CONFIG.PickReadyLoc[1];

        let actRec3 = await HeadCalibVerificationAct(
          [[0,0,0,0]], 
          v.CalibInfo,
          `G1 X${X} Y${Y} ${FA}`,
          [0],FA);

        if(actRec3[0][0].confidence_btm<85 && actRec3[0][0].confidence_side<85)
        {
          console.log("actRec3",actRec3, "confidence too low");
          break;
        }
        let cur_nloc_btm=actRec3[0][0].nloc_btm;
        let ref_nloc_btm=SYS_CONFIG.Calib_Target_Info.nloc_btm;

        let tv3 = THREE.Vector3
        let locDiff_pix=new tv3(cur_nloc_btm.x-ref_nloc_btm.x,cur_nloc_btm.y-ref_nloc_btm.y,0)

        console.log(">running Calib>>> cur:",cur_nloc_btm," ref:",ref_nloc_btm)

        console.log(">running Calib>>>locDiff_pix", locDiff_pix, " M_Pix2MM ",v.CalibInfo.XY.conv.M_Pix2XY)
        let locDiff_mm=locDiff_pix.clone().applyMatrix3(v.CalibInfo.XY.conv.M_Pix2XY);
      
        console.log("actRec3",actRec3,"locDiff_mm",locDiff_mm);

        if(Math.abs(locDiff_mm.x)>1 || Math.abs(locDiff_mm.y)>1)
        {
          console.log("Offset too large, aborting");
          break;
        }
        await G(`M400`);
        await G(`G92 X${X+locDiff_mm.x} Y${Y+locDiff_mm.y}`)//HACK fix, somehow the XY robot would drift after a while, so adjust it back here

        feederFetchAdj=feederFetchAdj.map(x=>x-locDiff_mm.x);
        console.log(">running Calib>>>locDiff_mm",locDiff_mm)
        XYRelocCD=70;
      }
      else
      {
        await G(`G1 X${SYS_CONFIG.PickReadyLoc[0]} Y${SYS_CONFIG.PickReadyLoc[1]} ${FA}`)
        XYRelocCD--;
      }



      FeederReadyInfo = await SigWait(FeederReady_key,30000);
      SigWait_Reg(FeederReady_key);


      if (await EMCheckStopPointCB({ type: "CYCLE_STATE", state: "end" }) == false) break;

    }
  }
  catch (e) {

    function stacktrace() {
      var err = new Error();
      return err.stack;
    }

    (await EMCheckStopPointCB({ type: "ERROR", e,trace:stacktrace()}) == false)
    console.log(">>>", e);
  }


  // await G(`M42 PORT${Reel_A_Run1_pin}    S${0}`);
  console.log(`EXIT`);
  SigWait_Trig_Reject(FeederClear2Act_key, "EXIT");
  SigWait_Trig_Reject(FeederClear2Check_key, "EXIT");
  SigWait_Trig_Reject(FeederReady_key, "EXIT");


  SigWait_Trig_Reject(ReelClear2Check_key, "EXIT");
  SigWait_Trig_Reject(ReelReady_key, "EXIT");

  await nozzleRelease();

  if (await EMCheckStopPointCB({ type: "CYCLE_STATE", state: "finish" }) == false);

}
function vof(v_f) {
  return (typeof v_f === 'function')?v_f():v_f;
}
async function PackingCtrlPanelUI() {

  // let initM114 = (await G("M114")).M114;
  await G("M121")
  let initM114={X:0,Y:0,Z1_:0,Z2_:0,Z3_:0,Z4_:0}

  initM114.X = Math.round(initM114.X * 100) / 100;
  initM114.Y = Math.round(initM114.Y * 100) / 100;
  initM114.Z1_ = Math.round(initM114.Z1_ * 100) / 100;

  let advUnit = 1;

  let ACC = "4000";
  let F = "1500";

  let InspTarUI_Feeder_API_Set = {}
  let InspTarUI_Reel_API_Set = {}
  let ZR_Idx = 1;



  let _UIStack = [];



  function UIStack_Current() {
    return _UIStack[_UIStack.length - 1];
  }

  function UIStackBack(updateCB, resultInfo) {
    if (_UIStack.length == 1) return;

    let UIInfo = _UIStack.pop();
    if (UIInfo.result !== undefined) {
      UIInfo.result(resultInfo)
    }

    updateCB(UIStack_Current().UI)
  }

  function UIStackGo(updateCB, UI, resultCB) {
    _UIStack.push({
      UI,
      result: resultCB
    })
    if (updateCB !== undefined)
      updateCB(UIStack_Current().UI)
  }


  let JOG_UI = {
    text: () => "X:" + initM114.X.toFixed(3) + " Y:" + initM114.Y.toFixed(3) + " Z" + ZR_Idx + ":" + initM114[`Z${ZR_Idx}_`].toFixed(2),
    opts: ["$t:X:", "<←", "←", "→", "→>", "$\n", "$t:Y:", "<☉>", "☉", "⊗", "<⊗>", "$\n", "$t:Z:", "<↑>", "↑", "↓", "<↓>", "SAFE_ZONE", "UNSAFE_ZONE"],
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
          await G("M400");
          let M114 = (await G("M114")).M114;
          initM114[`Z${ZR_Idx}_`] = M114[`Z${ZR_Idx}_`];
          // cbInfo[idx].text =  "Z1:" +M114.Z1_.toFixed(2)
          updateCB(UIStack_Current().UI)
          return;


        case "UNSAFE_ZONE":

          await Exit_Z2SafeZone_n_Check();
      }
      await G(`G1 X${initM114.X} Y${initM114.Y} ${FA}`)
      await G("M400");
      // let M114=(await G("M114")).M114;
      // cbInfo[idx].text = "X:" +M114.X.toFixed(3)+" Y:"+M114.Y.toFixed(3)
      updateCB(UIStack_Current().UI)
    }, default: 0
  }

  let FACC_setup_UI =
  {
    text: () => " <<<距離:" + advUnit + " 速度:" + F + " ACC:" + ACC,
    opts: [
      "$t:距離", "D0.01", "D0.1", "D1.0", "D10", "D30", "D200", "$\n",
      "$t:速度", "F10", "F100", "F500", "F1000", "$\n"],
    callback: async (idx, key, updateCB) => {
      switch (key) {

        case "D0.01":
          advUnit = 0.01
          break;
        case "D0.1":
          advUnit = 0.1
          break;
        case "D1.0":
          advUnit = 1
          break;
        case "D10":
          advUnit = 10
          break;
        case "D30":
          advUnit = 30
          break;
        case "D200":
          advUnit = 200
          break;
        case "F10":
          F = 10
          ACC = "3"
          break;
        case "F100":
          F = 100
          ACC = 400
          break;
        case "F500":
          F = 500

          ACC = 700
          break;
        case "F1000":
          F = 1000
          ACC = 1000
          break;
        case "XX":

          return;
      }

      // cbInfo[idx].text =  " <<<距離:"+advUnit+" 速度:"+F,

      updateCB(UIStack_Current().UI)
    }, default: 0
  }


  let Calib_UI = {
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

  let Feeder_loc_conv_offset = undefined;
  let Feeder_loc_UIInfo = {
    "pt1": {
      "headXY": [NaN, NaN, NaN
      ],
      "fcamXY": [NaN, NaN
      ]
    },
    "pt2": {
      "headXY": [NaN, NaN, NaN
      ],
      "fcamXY": [NaN, NaN
      ]
    },
  }

  let Reel_loc_UIInfo = {
    "pt1": {
      "headXY": [NaN, NaN, NaN
      ]
    }
  }


  {
    Feeder_loc_UIInfo =
    {
      "pt1": {
        "headXY": [
          341.3468628,
          87.51874542,
          12.89999962
        ],
        "fcamXY": [
          1552.9196136774435,
          1717.7562972561368
        ]
      },
      "pt2": {
        "headXY": [
          382.6124878,
          62.90781021,
          12.89999962
        ],
        "fcamXY": [
          3707.8940678424087,
          2981.38714389835
        ]
      }
    }

    Reel_loc_UIInfo =
    {
      "pt1": {
        "headXY": [
          305.132,
          371.194,
          7.5
        ]
      },
    }

    {

      const storedData = localStorage.getItem('Feeder_loc_UIInfo');
      if (storedData !== null) {
        const parsedData = JSON.parse(storedData);
        Feeder_loc_UIInfo = parsedData;
      }

      const storedData2 = localStorage.getItem('Reel_loc_UIInfo');
      if (storedData2 !== null) {
        const parsedData2 = JSON.parse(storedData2);
        Reel_loc_UIInfo = parsedData2;
      }
    }
  }


  function Feeder_loc_Convert_Calc(pt0_cam, pt1_cam, pt0_head, pt1_head, adj_offset = [0, 0, 0]) {
    let Feeder_loc_Convert;
    let tv3 = THREE.Vector3

    pt0_head = pt0_head.map((v, idx) => v + adj_offset[idx]);
    pt1_head = pt1_head.map((v, idx) => v + adj_offset[idx]);

    let convPack = Lib.pt_pair_2_pt_pair_transform(
      [pt0_cam, pt1_cam],
      [pt0_head, pt1_head])
    console.log(convPack);


    const M_CamVecs = new THREE.Matrix3(
      convPack.mat2x2[0][0], convPack.mat2x2[0][1], 0,
      convPack.mat2x2[1][0], convPack.mat2x2[1][1], 0,
      0, 0, 1);


    let vpt0_cam = new tv3(pt0_cam[0], pt0_cam[1], 1);
    let vpt0_head = new tv3(pt0_head[0], pt0_head[1], 1);
    let vpt0_head_comp = vpt0_cam.clone().applyMatrix3(M_CamVecs);
    vpt0_head_comp.sub(vpt0_head);
    M_CamVecs.elements[6] = -vpt0_head_comp.x;
    M_CamVecs.elements[7] = -vpt0_head_comp.y;

    Feeder_loc_Convert = {
      M_CamVecs,
      pickZ: pt0_head[2]
    }
    // console.log(M_CamVecs);
    // vpt0_head_comp=vpt0_cam.clone().applyMatrix3(M_CamVecs);
    // console.log(vpt0_cam,vpt0_head_comp,pt0_head);



    // vpt0_head_comp=vpt0_cam.clone().applyMatrix3(M_CamVecs);
    // console.log(vpt0_cam,vpt0_head_comp,pt0_head);

    return Feeder_loc_Convert;
  }



  if (v.Feeder_loc_UIInfo !== undefined) {
    Feeder_loc_UIInfo = v.Feeder_loc_UIInfo;
  }


  if (v.Reel_loc_UIInfo !== undefined) {
    Reel_loc_UIInfo = v.Reel_loc_UIInfo;
  }

  let Feeder_loc_Convert = Feeder_loc_Convert_Calc(
    Feeder_loc_UIInfo["pt1"].fcamXY,
    Feeder_loc_UIInfo["pt2"].fcamXY,
    Feeder_loc_UIInfo["pt1"].headXY,
    Feeder_loc_UIInfo["pt2"].headXY);

  let PackAutoTermFlag = false;
  let PackAutoStopped = true;


  let tmpObj = {};
  async function HeadGo(hidx = 0, pos = { x: NaN, y: NaN, z: NaN }) {



    let FA = `F${F} ACC${ACC}`
    await Enter_Z2SafeZone_n_Check();
    await Exit_Z2SafeZone_n_Check();


    let XYZ_Offset = {
      x: -hidx * 40 + v.CalibInfo.XY.offset[hidx].x,
      y: +v.CalibInfo.XY.offset[hidx].y,
      z: +v.CalibInfo.Z.offset[hidx],
    }


    // let tarX=vpt0_head_comp.x+XYZ_Offset.x;
    // let tarY=vpt0_head_comp.y+XYZ_Offset.y;
    // let tarZ=Feeder_loc_Convert.pickZ+XYZ_Offset.z;

    let tarX = pos.x + XYZ_Offset.x;
    let tarY = pos.y + XYZ_Offset.y;
    let tarZ = pos.z + XYZ_Offset.z;
    await G(`G1 X${tarX - 2} Y${tarY - 2} ${FA} `)
    await G(`G1  X${tarX} Y${tarY} Z${hidx + 1}_${tarZ} ${FA} `)

    await G(`M400`)


  }
  async function CamFeederHeadGo(headXYCb = async (imgLoc, convHeadLoc) => { }, detectionStickR = 0) {


    console.log("click");

    let drawHookKey = "xxfCAMXY_Set";
    let selDetectedLoc = undefined;
    // console.log("dddd");
    InspTarUI_Reel_API_Set.onMouseClick(async (info) => {
      console.log(selDetectedLoc, ">>>>>>>", info);
      if (selDetectedLoc !== undefined) {
        info = selDetectedLoc;
      }
      let tv3 = THREE.Vector3
      let vpt0_cam = new tv3(info.x, info.y, 1);
      let vpt0_head_comp = vpt0_cam.clone().applyMatrix3(Feeder_loc_Convert.M_CamVecs);
      console.log(vpt0_cam, vpt0_head_comp);

      await headXYCb(vpt0_cam, vpt0_head_comp);
      delete InspTarUI_Reel_API_Set.extInfo.drawHookSet[drawHookKey];
      updateCB(UIStack_Current().UI)
    })
    InspTarUI_Reel_API_Set.extInfo.drawHookSet[drawHookKey] =
    {
      preDraw: () => { },
      postDraw: (ctrl_or_draw, g, canvas_obj) => {
        if (ctrl_or_draw == true) {
          // console.log(InspTarUI_Reel_API_Set.latest_RP);
          return;
        }

        let mouseOnCanvas = canvas_obj.VecX2DMat(g.mouseStatus, g.worldTransform_inv);
        let ctx = g.ctx;

        // console.log(InspTarUI_Reel_API_Set);

        let camMag = canvas_obj.camera.GetCameraScale();
        let reportList = InspTarUI_Reel_API_Set.latest_RP?.report;

        if (reportList !== undefined) {
          let closestReport = undefined;
          let closestDist = 100;
          reportList.forEach(report => {

            let dist = Math.hypot(mouseOnCanvas.x - report.center.x, mouseOnCanvas.y - report.center.y);

            if (dist < closestDist) {
              closestDist = dist;
              closestReport = report;
            }

          })

          selDetectedLoc = undefined;
          if (closestReport !== undefined) {
            if (closestDist < detectionStickR) {
              selDetectedLoc = closestReport.center;
              ctx.lineWidth = 14 / camMag;
              ctx.strokeStyle = `HSLA(90, 100%, 50%,1)`;
              canvas_obj.rUtil.drawCross(ctx, { x: closestReport.center.x, y: closestReport.center.y }, 22 / camMag);
            }
          }
        }

        ctx.save();
        ctx.resetTransform();
        ctx.font = "20px Arial";
        ctx.fillStyle = "rgba(150,50, 50,0.5)";
        ctx.fillText("請點擊畫面上的目標點XX>[]>" + Lib.FixedStringify(mouseOnCanvas, 1), 0, 300);
        ctx.restore();

      }
    }



  }


  console.log("AUX_SET_ENC>>>>>", await Lib.CNCSend({ "type": "AUX_SET_ENC", value: 0, aid: 0 }));
  ReelENC = 0;

  let localAbortObj = {};




  let Feeder_loc_UI = {
    text: () => ">>",
    opts: [



      {
        type: "InspTar_UI",
        id: "SBM_FBLOC",

        params: {
          style: { float: "left", width: "70%", height: "600px" },

          APIExport: (api_set) => {
            InspTarUI_Reel_API_Set = {
              ...api_set, extInfo: {

                ...InspTarUI_Reel_API_Set.extInfo
              }
            };

            if (InspTarUI_Reel_API_Set.extInfo?.drawHookSet === undefined) {
              InspTarUI_Reel_API_Set.extInfo.drawHookSet = {};
            }



            console.log("InspTarUI_Reel_API_Set", InspTarUI_Reel_API_Set)
            api_set.setDrawHook({
              preDraw: () => { },
              postDraw: (ctrl_or_draw, g, canvas_obj) => {

                let drawHookSet = InspTarUI_Reel_API_Set?.extInfo?.drawHookSet;
                if (drawHookSet === undefined) return;

                Object.keys(drawHookSet).forEach(key => {
                  let drawHook = drawHookSet[key];
                  if (drawHook.postDraw === undefined) return;
                  drawHook.postDraw(ctrl_or_draw, g, canvas_obj);
                })

              }
            })
          }
        },
      },
      {
        type: "button",
        text: "柔震照相",
        onClick: async (updateCB) => {

          let FA = `F${F} ACC${ACC}`
          await Enter_Z2SafeZone_n_Check();
          await G(`G1 Y${150} ${FA}`)

          await G(`G04 P1000`)
          await G(`M400`)

          await FeederCheck(true, 1);
        }
      },


      {
        type: "button",
        text: "側照照相",
        onClick: async (updateCB) => {

          let hRs = [];
          let objXErr = [];
          let objCheckPix2mm = 1 / 10;

          {
            let trigID = 4564;
            let repWaitKey = "repWK";

            reportWait_reg(repWaitKey + "_loc", "SBM_CAM_A", trigID);


            {//set flag to SurfaceCheck_CAM_A to use extParam of SurfaceCheck_CAM_A
              await InspTargetExchange("SurfaceCheck_CAM_A", { type: "useExtParam" })
              reportWait_reg(repWaitKey + "_check", "SurfaceCheck_CAM_A", trigID);
            }

            await Lib.CameraSNameSWTrigger("SideCheckCam", "CAM_A", trigID, true);


            let report_loc = await reportWait(repWaitKey + "_loc")

            let report_check = undefined;
            {//modify the locating report and feed to extParam of SurfaceCheck_CAM_A
              console.log(report_loc);
              let new_report_loc = report_loc;

              new_report_loc.report[0].angle = -4 * Math.PI / 180;
              new_report_loc.report[1].angle = -2 * Math.PI / 180;
              new_report_loc.report[2].angle = 2 * Math.PI / 180;
              new_report_loc.report[3].angle = 4 * Math.PI / 180;
              await InspTargetExchange("SurfaceCheck_CAM_A", { type: "extParam", orientation: new_report_loc.report })
              report_check = await reportWait(repWaitKey + "_check")
              //and disable the extParam of SurfaceCheck_CAM_A at last
              await InspTargetExchange("RegionInsp_Reel", { type: "useExtParam", enable: false });

            }
            console.log(report_check);


            let distDiff = report_check.report.sub_reports.map(rep => {
              if (rep.sub_regions.length == 0) return NaN;
              return rep.sub_regions[9].score - rep.sub_regions[10].score
            })

            objXErr = distDiff.map(diff => diff * objCheckPix2mm);
            let dir = report_check.report.sub_reports.map((rep, idx) => {
              if (rep.sub_regions.length == 0) return undefined;
              if (rep.sub_regions[2].category != 1) return undefined;
              // if(Math.abs(distDiff[idx])>7)return undefined;
              if (rep.sub_regions[1].category != rep.sub_regions[4].category) return undefined;


              return rep.sub_regions[1].category;
            })

            console.log("distDiff:", distDiff);

            // await delay(3000)
            // if(dir.findIndex(d=>d===undefined)!=-1)
            // {
            //   await nozzleRelease();

            //   continue;
            // }



            let R_str = "";
            hRs = [0, 0, 0, 0];
            for (let i = 0; i < dir.length; i++) {
              if (dir[i] === undefined) continue;
              if (dir[i] == 1) {

                hRs[i] = 90;
                R_str += `R${i + 1}_90 `
              }
              else if (dir[i] == -1) {
                objXErr[i] *= -1;
                hRs[i] = -90;
                R_str += `R${i + 1}_-90 `
              }
            }

            // await Lib.CameraSNameSWTrigger("Hikrobot-2BDF50664114-00F50664114","CAM_A",trigID+1,true);
            // console.log(report_loc,report_check,dir);
            // await delay(500);
            // await Enter_Z2SafeZone_n_Check();


            // PackAutoTermFlag=true;

          }

        }
      },










      ...['pt1', 'pt2'].map(ptn => {
        return [
          "$t:Feeder_" + ptn + ":",
          {
            type: "button", key: "xxHeadXY_Set" + ptn,
            text: () => FixedStringify(Feeder_loc_UIInfo[ptn].headXY, 3),
            onClick: async (updateCB) => {


              console.log("click");
              await G("M400")
              let M114 = (await G("M114")).M114;
              Feeder_loc_UIInfo[ptn].headXY = [M114.X, M114.Y, M114.Z1_]

              updateCB(UIStack_Current().UI)
              // updateCB(cbInfo)

            }
          }, "$t:       ",
          {
            type: "button", key: "xxHeadXY_Set" + ptn + "_go",
            text: "GoXY",
            onClick: async (updateCB) => {
              let FA = `F${F} ACC${ACC}`
              await Enter_Z2SafeZone_n_Check();
              await Exit_Z2SafeZone_n_Check();
              let TarXY = Feeder_loc_UIInfo[ptn].headXY;

              await G(`G1 X${TarXY[0]} Y${TarXY[1]} ${FA}`)
              await G(`G1 Z1_${TarXY[2]} ${FA}`)

              await G(`M400`)

              console.log(Feeder_loc_UIInfo);

            }
          },
          {
            type: "button", key: "xxfCAMXY_Set1" + ptn,
            text: () => FixedStringify(Feeder_loc_UIInfo[ptn].fcamXY, 1),
            onClick: async (updateCB) => {
              console.log("click");

              let drawHookKey = "xxfCAMXY_Set";

              // console.log("dddd");
              InspTarUI_Reel_API_Set.onMouseClick(async (info) => {
                console.log(">>>>>>>", info);
                Feeder_loc_UIInfo[ptn].fcamXY = [info.x, info.y];


                updateCB(UIStack_Current().UI)

                delete InspTarUI_Reel_API_Set.extInfo.drawHookSet[drawHookKey];
              })
              InspTarUI_Reel_API_Set.extInfo.drawHookSet[drawHookKey] =
              {
                preDraw: () => { },
                postDraw: (ctrl_or_draw, g, canvas_obj) => {
                  if (ctrl_or_draw == true) {
                    // console.log(InspTarUI_Reel_API_Set.latest_RP);
                    return;
                  }
                  // console.log(ctrl_or_draw)

                  let mouseOnCanvas = canvas_obj.VecX2DMat(g.mouseStatus, g.worldTransform_inv);
                  let ctx = g.ctx;




                  ctx.save();
                  ctx.resetTransform();
                  ctx.font = "20px Arial";
                  ctx.fillStyle = "rgba(150,50, 50,0.5)";
                  ctx.fillText("請點擊畫面上的目標點>[" + ptn + "]>" + Lib.FixedStringify(mouseOnCanvas, 1), 0, 100);
                  ctx.restore();

                }
              }

            }
          },
          "$\n",
        ]
      }).flat(),


      // ...(()=>{

      //   return [



      //   ]
      // })()
      // ,

      {
        type: "button", key: "Update",
        text: () => ">>" + JSON.stringify(Feeder_loc_conv_offset,(key, val)=>val===undefined?val:
          (val.toFixed ? Number(val.toFixed(4)) : val)
          ),
        onClick: async (updateCB) => {


          console.log(Feeder_loc_Convert, Feeder_loc_UIInfo)
          let M114 = (await G("M114")).M114;
          let curLoc = [M114.X, M114.Y, M114.Z1_];
          if (Feeder_loc_conv_offset === undefined || Feeder_loc_conv_offset.curLoc === undefined) {

            Feeder_loc_conv_offset = {
              curLoc: curLoc
            };
          }
          else {
            let pre_cur = Feeder_loc_conv_offset.curLoc
            Feeder_loc_conv_offset = {
              offset: [curLoc[0] - pre_cur[0], curLoc[1] - pre_cur[1], curLoc[2] - pre_cur[2]]
            };
          }
          updateCB(UIStack_Current().UI)
        }
      },



      {
        type: "button", key: "Update",
        text: () => "Update Mapping Info",
        onClick: async (updateCB) => {


          if (Feeder_loc_conv_offset !== undefined && Feeder_loc_conv_offset.offset !== undefined) {
            let adj_offset = [0, 0, 0]
            adj_offset = Feeder_loc_conv_offset.offset;
            Feeder_loc_UIInfo["pt1"].headXY = adj_offset.map((v, i) => v + Feeder_loc_UIInfo["pt1"].headXY[i]);
            Feeder_loc_UIInfo["pt2"].headXY = adj_offset.map((v, i) => v + Feeder_loc_UIInfo["pt2"].headXY[i]);
          }

          Feeder_loc_Convert = Feeder_loc_Convert_Calc(
            Feeder_loc_UIInfo["pt1"].fcamXY,
            Feeder_loc_UIInfo["pt2"].fcamXY,
            Feeder_loc_UIInfo["pt1"].headXY,
            Feeder_loc_UIInfo["pt2"].headXY);

          Feeder_loc_conv_offset = undefined;
          v.Feeder_loc_UIInfo = Feeder_loc_UIInfo;

          localStorage.setItem('Feeder_loc_UIInfo', JSON.stringify(Feeder_loc_UIInfo));

          updateCB(UIStack_Current().UI)
        }
      },
      "$\n",
      ,
      {
        type: "button", key: "CamFeederGo_Img",
        text: () => "HeadGoF_H1",
        onClick: async (updateCB) => {


          await CamFeederHeadGo(async (imgLoc, convHeadLoc) => {
            tmpObj.convHeadLoc = { ...convHeadLoc, z: Feeder_loc_Convert.pickZ };
            await HeadGo(0, { ...convHeadLoc, z: Feeder_loc_Convert.pickZ + 2 });
          }, 100)


        }
      },
      {
        type: "button", key: "CamFeederGo_Img1",
        text: () => "1",
        onClick: async (updateCB) => {


          await HeadGo(0, tmpObj.convHeadLoc);


        }
      },
      {
        type: "button", key: "CamFeederGo_Img1",
        text: () => "2",
        onClick: async (updateCB) => {


          await HeadGo(1, tmpObj.convHeadLoc);

        }
      },
      {
        type: "button", key: "CamFeederGo_Img2",
        text: () => "3",
        onClick: async (updateCB) => {

          await HeadGo(2, tmpObj.convHeadLoc);

        }
      },
      {
        type: "button", key: "CamFeederGo_Img3",
        text: () => "4",
        onClick: async (updateCB) => {


          await HeadGo(3, tmpObj.convHeadLoc);
        }
      },
    ]
  }



  let Reel_loc_UI = {
    text: () => ">>",
    opts: [

      {
        type: "button",
        text: "SHOT_Reel",
        onClick: async (updateCB) => {

          let FA = `F${F} ACC${ACC}`
          await Enter_Z2SafeZone_n_Check();


          let trigID=44;
          let repWaitKey=">>><"
          reportWait_reg(repWaitKey+"_RO", "SurfaceCheck_ReelObj", trigID);
          reportWait_reg(repWaitKey+"_RS", "SurfaceCheck_ReelSlot", trigID);

          
          // await G(`G1 X${340} Y${150} ${FA}`)

          await G(`G04 P1000`)
          await G(`M400`)

          await ReelCheck(trigID);


          let report_RO = await reportWait(repWaitKey+"_RO")
          let report_RS = await reportWait(repWaitKey+"_RS")

          let Obj_srep = report_RO.report.sub_reports;
          let Slot_srep = report_RS.report.sub_reports;
          console.log({Obj_srep,Slot_srep})

          let advCount=0;
          for(let i=0;i<Obj_srep.length;i++)
          {
            if(Obj_srep[i].category!=1)break;
            advCount++;
          }
          


          let slotNeedClean=[];
          let slotCanPlace=[];
          let slotPlaced=[];

          for(let i=0;i<Slot_srep.length;i++)
          {
            let isAObj=(i<Obj_srep.length) && (Obj_srep[i].category==1);
            let objInPlace=(Slot_srep[i].category!=1) && isAObj;
            let eptSlot=Slot_srep[i].category==1;
            slotPlaced.push(objInPlace?i:"_")
            slotCanPlace.push(eptSlot?i:"_");
            slotNeedClean.push((!objInPlace && !eptSlot)?i:"_");
          }
          
          
          console.log("advCount:",advCount);
          
          console.log("slotPlaced:",slotPlaced);
          console.log("slotCanPlace:",slotCanPlace);
          console.log("slotNeedClean:",slotNeedClean);


          //Check the all the Slot from report of SurfaceCheck_ReelSlot can be found

        }
      },

      {
        type: "button",
        text: "Reg_Reel_Loc",
        onClick: async (updateCB) => {
          let RLInfo=await ReelLocating(); 
          console.log(RLInfo);

          v.CalibInfo.ReelLoc=RLInfo;
        }
      },
      "$\n",



      ...['pt1'].map(ptn => {

        async function HeadGoReel(headIdx = 0, slotIdx = 0) {
          console.log(v.CalibInfo);
          let xoffset = (v.CalibInfo === undefined ? 0 : v.CalibInfo.XY.offset[headIdx].x) + slotIdx * 4 - (headIdx * 40);
          let yoffset = (v.CalibInfo === undefined ? 0 : v.CalibInfo.XY.offset[headIdx].y);
          let zoffset = (v.CalibInfo === undefined ? 0 : v.CalibInfo.Z.offset[headIdx]);


          let FA = `F${F} ACC${ACC}`
          await Enter_Z2SafeZone_n_Check();
          await Exit_Z2SafeZone_n_Check();
          let TarXY = Reel_loc_UIInfo[ptn].headXY;

          await G(`G1 X${TarXY[0] + xoffset} Y${TarXY[1] + yoffset} ${FA}`)

          // await G(`G1 Z1_${TarXY[2]+zoffset[0]} Z2_${TarXY[2]+zoffset[1]} Z3_${TarXY[2]+zoffset[2]} Z4_${TarXY[2]+zoffset[3]}  ${FA}`)
          await G(`G1 Z${headIdx + 1}_${TarXY[2] + zoffset} ${FA}`)

          await G(`M400`)

          console.log(Reel_loc_UIInfo);
        }


        return [
          "$t:Reel_" + ptn + ":",
          {
            type: "button", key: "xxHeadXY_Set" + ptn,
            text: () => FixedStringify(Reel_loc_UIInfo[ptn].headXY, 3),
            onClick: async (updateCB) => {


              console.log("click");
              await G("M400")

              let headIdx=0;
              console.log(v.CalibInfo);
              let xoffset = (v.CalibInfo === undefined ? 0 : v.CalibInfo.XY.offset[headIdx].x);
              let yoffset = (v.CalibInfo === undefined ? 0 : v.CalibInfo.XY.offset[headIdx].y);
              let zoffset = (v.CalibInfo === undefined ? 0 : v.CalibInfo.Z.offset[headIdx]);

              let M114 = (await G("M114")).M114;
              Reel_loc_UIInfo[ptn].headXY = [M114.X-xoffset, M114.Y-yoffset, M114.Z1_-zoffset]

              updateCB(UIStack_Current().UI)
              // updateCB(cbInfo)

            }
          }, "$t:       ",
          {
            type: "button", key: "xxHeadXY_Set" + ptn + "_go",
            text: "GoXY",
            onClick: async (updateCB) => {
              await HeadGoReel(0, 0);

            }
          },
          {
            type: "button", key: "xxHeadXY_Set" + ptn + "_go",
            text: "2",
            onClick: async (updateCB) => {
              await HeadGoReel(1, 0);
            }
          },
          {
            type: "button", key: "xxHeadXY_Set" + ptn + "_go",
            text: "3",
            onClick: async (updateCB) => {
              await HeadGoReel(2, 0);

            }
          },
          {
            type: "button", key: "xxHeadXY_Set" + ptn + "_go",
            text: "4",
            onClick: async (updateCB) => {
              await HeadGoReel(3, 0);

            }
          },
          "$\n",
        ]
      }).flat(),


      {
        type: "button", key: "Update",
        text: () => "Update Mapping Info",
        onClick: async (updateCB) => {

          v.Reel_loc_UIInfo = Reel_loc_UIInfo;

          localStorage.setItem('Reel_loc_UIInfo', JSON.stringify(Reel_loc_UIInfo));

          updateCB(UIStack_Current().UI)
        }
      },

      "$\n",
      "$\n",


      {
        type: "button",
        text: "Reel0",
        onClick: async (updateCB) => {

          console.log("AUX_SET_ENC>>>>>", await Lib.CNCSend({ "type": "AUX_SET_ENC", value: 0, aid: 0 }));
          ReelENC = 0;
          // await ReelGoAdv(1);

        }
      },
      {
        type: "button",
        text: "Adv.1",
        onClick: async (updateCB) => {
          await ReelGoAdv_s(1);

        }
      },
      {
        type: "button",
        text: "Adv1",
        onClick: async (updateCB) => {
          await ReelGoAdv(1);
          await G("M400")
          
          let CurENCReading=(await Lib.CNCSend({ "type": "AUX_ENC_V", aid: 0 })).enc_v


          console.log("ENC_CHECK:",CurENCReading,ReelENC);


        }
      },
      {
        type: "button",
        text: "+4",
        onClick: async (updateCB) => {
          await ReelGoAdv(4);

        }
      },
      {
        type: "button",
        text: "+16",
        onClick: async (updateCB) => {

          await ReelGoAdv(4 * 4);
        }
      },
      {
        type: "button",
        text: "+4*16",
        onClick: async (updateCB) => {

          await ReelGoAdv(4 * 16);
        }
      },
    ]
  }


  let Simple_IO_Ctrl_UI = {
    text: () => "Simple_IO_Ctrl_UI",
    opts: [



      "$t:柔震:",
      {
        type: "button",
        text: "震",
        onClick: async (updateCB) => {

          await Lib.CNCSend({ "type": "AUX_IO_CTRL", "pin": SYS_OUT_PIN_DEF.Feeder_A_Run1, "state": 1, aid: 0 })

        }
      },
      {
        type: "button",
        text: "X", id: "FV1X",
        onClick: async (updateCB) => {

          await Lib.CNCSend({ "type": "AUX_IO_CTRL", "pin": SYS_OUT_PIN_DEF.Feeder_A_Run1, "state": 0, aid: 0 })

        }
      },


      {
        type: "button",
        text: "補料",
        onClick: async (updateCB) => {

          await Lib.CNCSend({ "type": "AUX_IO_CTRL", "pin": SYS_OUT_PIN_DEF.Feeder_F_Run1, "state": 1, aid: 0 })

        }
      },
      {
        type: "button",
        text: "X", id: "FV2X",
        onClick: async (updateCB) => {

          await Lib.CNCSend({ "type": "AUX_IO_CTRL", "pin": SYS_OUT_PIN_DEF.Feeder_F_Run1, "state": 0, aid: 0 })

        }
      },



      {
        type: "button",
        text: "柔震打光",
        onClick: async (updateCB) => {

          await Lib.CNCSend({ "type": "AUX_IO_CTRL", "pin": SYS_OUT_PIN_DEF.Feeder_L_Light1, "state": 1, aid: 0 })

        }
      },
      {
        type: "button",
        text: "X", id: "FL1X",
        onClick: async (updateCB) => {

          await Lib.CNCSend({ "type": "AUX_IO_CTRL", "pin": SYS_OUT_PIN_DEF.Feeder_L_Light1, "state": 0, aid: 0 })

        }
      },
      {
        type: "button",
        text: "側照光",
        onClick: async (updateCB) => {

          await Lib.CNCSend({ "type": "AUX_IO_CTRL", "pin": SYS_OUT_PIN_DEF.Feeder_L_Light2, "state": 1, aid: 0 })

        }
      },
      {
        type: "button",
        text: "X", id: "FL2X",
        onClick: async (updateCB) => {

          await Lib.CNCSend({ "type": "AUX_IO_CTRL", "pin": SYS_OUT_PIN_DEF.Feeder_L_Light2, "state": 0, aid: 0 })

        }
      },


      "$\n",
      "$t:編帶:",
      {
        type: "button",
        text: "光",
        onClick: async (updateCB) => {

          await Lib.CNCSend({ "type": "AUX_IO_CTRL", "pin": SYS_OUT_PIN_DEF.Reel_L_Light1, "state": 1, aid: 0 })
          await Lib.CNCSend({ "type": "AUX_IO_CTRL", "pin": SYS_OUT_PIN_DEF.Reel_L_Light2, "state": 1, aid: 0 })

        }
      },
      {
        type: "button",
        text: "X", id: "IL1_X",
        onClick: async (updateCB) => {

          await Lib.CNCSend({ "type": "AUX_IO_CTRL", "pin": SYS_OUT_PIN_DEF.Reel_L_Light1, "state": 0, aid: 0 })
          await Lib.CNCSend({ "type": "AUX_IO_CTRL", "pin": SYS_OUT_PIN_DEF.Reel_L_Light2, "state": 0, aid: 0 })

        }
      },


      {
        type: "button",
        text: "下壓",
        onClick: async (updateCB) => {

          await Lib.CNCSend({ "type": "AUX_IO_CTRL", "pin": SYS_OUT_PIN_DEF.Reel_A_Run1, "state": 1, aid: 0 })

        }
      },
      {
        type: "button",
        text: "X", id: "RP1_X",
        onClick: async (updateCB) => {

          await Lib.CNCSend({ "type": "AUX_IO_CTRL", "pin": SYS_OUT_PIN_DEF.Reel_A_Run1, "state": 0, aid: 0 })

        }
      },



      "$\n",

      "$t:吸嘴: ",
      ...[0,1,2,3].map((i)=>({
          type: "button",
          text: "N"+i, id: "N"+i+"_open",
          onClick: async (updateCB) => {
            await Lib.CNCSend({ "type": "AUX_IO_CTRL", "pin": i, "state": 1, aid: 0 })

          }
      }))
      ,
      "$t:   ",
      {
        type: "button",
        text: "X", id: "Nclose",
        onClick: async (updateCB) => {
          await Lib.CNCSend({ "type": "AUX_IO_CTRL", "pin": 0, "state": 0, aid: 0 })
          await Lib.CNCSend({ "type": "AUX_IO_CTRL", "pin": 1, "state": 0, aid: 0 })
          await Lib.CNCSend({ "type": "AUX_IO_CTRL", "pin": 2, "state": 0, aid: 0 })
          await Lib.CNCSend({ "type": "AUX_IO_CTRL", "pin": 3, "state": 0, aid: 0 })

        }
      },



      "$\n",
      "$\n",

      {
        type: "button",
        text: "校正站打光",
        onClick: async (updateCB) => {

          await Lib.CNCSend({ "type": "AUX_IO_CTRL", "pin": SYS_OUT_PIN_DEF.Insp_L_Light1, "state": 1, aid: 0 })

        }
      },
      {
        type: "button",
        text: "X", id: "IL1_X",
        onClick: async (updateCB) => {

          await Lib.CNCSend({ "type": "AUX_IO_CTRL", "pin": SYS_OUT_PIN_DEF.Insp_L_Light1, "state": 0, aid: 0 })

        }
      },

    ]
  }


  let CycleRunStat={
    pausePromise:undefined,
    doPause:false,
  }


  let GCtrl_UI2 = ()=>({
    text: () => "GCtrl_UI2",
    opts: [
      {
        type: "button",
        text: "Release",
        onClick: async (updateCB) => {

          await G(`M121`)//dis
          let FA = `F${F} ACC${ACC}`
          await Enter_Z2SafeZone_n_Check();
          await G(`G1 X${300} Y${100} ${FA}`)
          await G(`G1 R1_0  R2_0  R3_0  R4_0`)
          await G(`M42 P0 S0`)
          await G(`M42 P1 S0`)
          await G(`M42 P2 S0`)
          await G(`M42 P3 S0`)
          await G(`M400`)

          await Enter_Z2SafeZone_n_Check();
          await Enter_Z2SafeZone_n_Check();
          await Enter_Z2SafeZone_n_Check();
        }
      },

      "$\n",


      


      {
        type: "button", key: "doPause",
        text:"LOAD:"+JSON.stringify(v.ReelPackingInfo),
        disabled:CycleRunStat.pausePromise===undefined,
        onClick: async (updateCB) => {
          v.ReelPackingInfo={
            // packingSeq:[75,-5,75,-5]//-160]
            // packingSeq:[1,-40,5500,-97,1]//-160]
            packingSeq:[5000,-97,1]//-160]
            // packingSeq:[-1,1,-2,2,-3,3,-4,4,-5,5,-6,6,-7,7,-8,8,-9,9,-10,10]
          }
          
          updateCB(UIStack_Current().UI)
        }
      },
      "$\n",

      {

        type: "button", key: "sssff",
        text: () => "AutoPick2",
        onClick: async (updateCB) => {
          if (localAbortObj.localAbort !== undefined) return;
          // if(localAbortObj.localAbort!==undefined)
          // {
          //   localAbortObj.localAbort.abort();
          //   localAbortObj.localAbort=undefined;
          // }
          localAbortObj.localAbort = new AbortController();
          console.log(localAbortObj);
          updateCB(UIStack_Current().UI)
          CYCLERun_test(Feeder_loc_Convert, Reel_loc_UIInfo,
            localAbortObj.localAbort.signal,
            async (info) => {
              // console.log(info);
              // if(info.type=="FeederPicking" || info.type=="ReelPicking"|| info.type=="ReelPlacing")
              // {
              //   await G(`M400`)
              //   await delay(200);
              // }
              if(CycleRunStat.doPause
                &&(info.type !== "STATISTIC")//blacklist
                // || (info.type == "ReelPlacing" && info.state=="begin")//whitelist
              )
              {              
                SigCfg_HACK_DisableTimeout();
                await new Promise((resolve, reject) => {

                  
                  CycleRunStat.pausePromise ={resolve,reject}
  
                  // setTimeout(() => {
                  //   reject("user input timeout")
                  // }, 50000);
                  updateCB(UIStack_Current().UI)
                  
                })

                SigCfg_HACK_EnableTimeout();
                CycleRunStat.pausePromise=undefined;
                updateCB(UIStack_Current().UI)
              }

              CycleRunStat.currentStateInfo=info;


              if (info.type == "STATISTIC") {
                CycleRunStat.statistic=info;
              }
              if (info.type == "ReelAdvancing") {
                console.log(">>>>", info);
              }
              if (info.type == "FeederPicking") {

              }

              if (info.type == "SideCheck") {

              }

              if (info.type == "ReelPicking" || info.type == "ReelPlacing") {
                // await G(`M400`)
                // await delay(200);
              }

              if(info.type == "CYCLE_STATE")
              {


                // if(info.state=="finish")
                updateCB(UIStack_Current().UI)
                console.log("CYCLE STATE....", info,v.ReelPackingInfo);
              }


              if(info.type == "ERROR")
              {


                CycleRunStat.errorInfo=info;
                // console.log("kjhsdilfdsajhfl",info)
                updateCB(UIStack_Current().UI)
              }

              if (info.type == "NGDrop") {

              }

              return true;
            })
          .then((r)=>{
            console.log("CYCLE DONE",r);

                  
            localAbortObj.localAbort.abort();
            console.log(localAbortObj.localAbort);
            localAbortObj.localAbort = undefined;
            updateCB(UIStack_Current().UI)
          })
        }


      },

      // {

      //   type: "button", key: "sssffd",
      //   text: () => "ReelRunTest",
      //   onClick: async (updateCB) => {
      //     if (localAbortObj.localAbort !== undefined) return;
      //     // if(localAbortObj.localAbort!==undefined)
      //     // {
      //     //   localAbortObj.localAbort.abort();
      //     //   localAbortObj.localAbort=undefined;
      //     // }
      //     localAbortObj.localAbort = new AbortController();

      //     ReelPositioning_test(Feeder_loc_Convert, Reel_loc_UIInfo, localAbortObj.localAbort.signal);
      //   }


      // },

      {
        type: "button", key: "doPause",
        text:CycleRunStat.doPause===false?"執行單動":"執行連續",
        disabled:CycleRunStat.pausePromise===undefined,
        onClick: async (updateCB) => {
          
          CycleRunStat.doPause=!CycleRunStat.doPause;
          updateCB(UIStack_Current().UI)
        }
      },
      {
        type: "button", key: "RunPause",
        text:CycleRunStat.pausePromise===undefined?"_______":"___>___",
        disabled:CycleRunStat.pausePromise===undefined,
        onClick: async (updateCB) => {
          
          if(CycleRunStat.pausePromise===undefined)
          {
            return;
          }
          CycleRunStat.pausePromise.resolve();
        }
      },

      {
        type: "button", key: "sssff",
        text: localAbortObj.localAbort===undefined ? "NOP" : "STOP",
        onClick: async (updateCB) => {

          console.log(localAbortObj.localAbort);
          if (localAbortObj.localAbort !== undefined) {
            localAbortObj.localAbort.abort();
            console.log(localAbortObj.localAbort);
            localAbortObj.localAbort = undefined;
          }

          // PackAutoTermFlag=true;
          // for(let i=0;;i++)
          // {
          //   if(PackAutoStopped)break;
          //   await delay(500);
          // }
        }
      },
      "$\n",
      "$\n",
      "$t:----檢驗數據----","$\n",
      "$t:"+FixedStringify(CycleRunStat.statistic),
      "$\n",
      "$t:----運行階段----","$\n",
      "$t:"+FixedStringify(CycleRunStat.currentStateInfo),
      "$\n",
      {
        type: "button", key: "----錯誤----",
        text:"----錯誤----",
        onClick: async (updateCB) => {
          CycleRunStat.errorInfo=undefined;
          updateCB(UIStack_Current().UI)

        }
      },"$\n",
      "$pre:"+((CycleRunStat.errorInfo===undefined)?"":CycleRunStat.errorInfo.e),
      "$pre:"+((CycleRunStat.errorInfo===undefined)?"":CycleRunStat.errorInfo.trace),

    ]
  })




  let lv = {}
  let hideTestFunction=true
  let latestCalibStat = undefined;
  let latestCalibLog = "請進行精細校正";
  //hideTestFunction?[]: 
  let cbInfo =()=>[
    {
      text: () => "UI... ",
      // onClick:(updateCB)=>{
      //   console.log("UI... ");
      //   hideOPT=!hideOPT;

      //   updateCB(UIStack_Current().UI)
      // },
      opts:[

        {
          type: "button", key: "1: 歸零並校正",
          text: () => "1: 歸零並校正",
          onClick: async (updateCB) => {

            let cInfo =()=> [

              {
                text: () => "CalibCenter",
                opts: [
                  {
                    type: "button", key: "back",
                    text: () => "<=",
                    onClick: async (updateCB) => {
                      UIStackBack(updateCB, { ok: "OK" });
                    }
                  },
                  "$t: ",
                  {
                    type: "button", key: "HOMING",
                    text: () => "機械歸零",
                    onClick: async (updateCB) => {

                      await SimpHoming();
                    }
                  },
                  "$t:           ",
                  {
                    type: "button", key: "DO_CALIB",
                    text: () => "精細校正",
                    onClick: async (updateCB) => {

                      let maxCalibCount = 3;
                      let i=0;
                      for(i=0;i<maxCalibCount;i++)
                      {
                        await HeadCalib();
  
                        let diffXYZ_mm_stat = await HeadVerificationSimpleStat();
                        console.log(diffXYZ_mm_stat);
                        // await HeadCalib();
                        latestCalibStat=diffXYZ_mm_stat;
                        
                        let maxAllowedError=0.05;
                        if(latestCalibStat.max<maxAllowedError)break;//good enough
                        latestCalibLog=`校正${i+1}/${maxCalibCount}次: 誤差${latestCalibStat.max}mm 超過${maxAllowedError}mm 再次進行精細校正`;
                        updateCB(UIStack_Current().UI)
                      }

                      latestCalibLog=`校正完畢 ${i+1}次: 誤差${latestCalibStat.max.toFixed(3)}mm `;
                      updateCB(UIStack_Current().UI)
                    }
                  },
                  {
                    type: "button", key: "DO_CHECK",
                    text: () => "驗證校正",
                    onClick: async (updateCB) => {


                      let diffXYZ_mm_stat = await HeadVerificationSimpleStat();
                      console.log(diffXYZ_mm_stat);
                      latestCalibStat=diffXYZ_mm_stat;
                      
                      updateCB(UIStack_Current().UI)
                    }
                  },
                  "$t:           ",
                  {
                    type: "button", key: "RST_CALIB",
                    text: () => "刪除校正資料",
                    onClick: async (updateCB) => {

                      v.CalibInfo = undefined
                    }
                  }
                  ,
                  "$\n",

                  {
                    type: "button", key: "RST_CALIB",
                    text: () => "print_CALIB",
                    onClick: async (updateCB) => {
                      console.log(v.CalibInfo);
                    }
                  }

                  ,"$\n"
                  ,"$\n"
                  ,"$\n"
                  ,"$t:"+latestCalibLog
                  ,"$\n"
                  ,`$pre:`+FixedStringify(latestCalibStat,3)

                ]
              },
              // JOG_UI,
              // Calib_UI
            ]//
            UIStackGo(updateCB, cInfo, (result) => {
              console.log(result)
            })
          }
        },"$\n",{
          type: "button", key: "FeederLocMapping",
          text: () => "2:料盤定位",


          onClick: async (updateCB) => {




            let cInfo = [
              {
                text: () => "",
                opts: [
                  {
                    type: "button", key: "back",
                    text: () => "<=",
                    onClick: async (updateCB) => {
                      UIStackBack(updateCB, { ok: "OK" });
                    }
                  },],
                callback: async (idx, key, updateCB) => {

                  // cbInfo[idx].text =  " <<<距離:"+advUnit+" 速度:"+F,

                  updateCB(cInfo)
                }, default: 0
              },
              FACC_setup_UI,
              JOG_UI,
              Feeder_loc_UI,
            ]
            UIStackGo(updateCB, cInfo, (result) => {
              console.log(result)
            })









          }

        },"$\n",{
          type: "button", key: "ReelLocMapping",
          text: () => "3:料帶定位",
          onClick: async (updateCB) => {

            let JOG_HIDE = true;
            let cInfo =()=> [

              {
                text: () => "Feeder Loc Mapping",
                opts: [
                  {
                    type: "button", key: "back",
                    text: () => "<=",
                    onClick: async (updateCB) => {
                      UIStackBack(updateCB, { ok: "OK" });
                    }
                  },],
                callback: async (idx, key, updateCB) => {

                  // cbInfo[idx].text =  " <<<距離:"+advUnit+" 速度:"+F,

                  updateCB(cInfo)
                }, default: 0
              },

              {
                ...FACC_setup_UI,
                text: () =>JOG_HIDE?"+ JOG": "- "+vof(FACC_setup_UI.text),
                onClick: (updateCB) => {
                  JOG_HIDE = !JOG_HIDE;

                  updateCB(cInfo)
                },
                opts:JOG_HIDE?[]:FACC_setup_UI.opts
              },
              JOG_HIDE?null:JOG_UI,
              // Feeder_loc_UI,
              Reel_loc_UI,
              // Reel_loc_UI,
            ]
            UIStackGo(updateCB, cInfo, (result) => {
              console.log(result)
            })
          }
        },"$\n",{
          type: "button", key: "ExeLocMapping",
          text: () => "4:走位定位確認",

        },"$\n",{
          type: "button", key: "PackUI",
          text: () => "5:包裝介面",
          onClick: async (updateCB) => {

            let SIMP_CTRL_IO_HIDE = false;
            let cInfo =()=> [

              {
                text: () => "Feeder Loc Mapping",
                opts: [
                  {
                    type: "button", key: "back",
                    text: () => "<=",
                    onClick: async (updateCB) => {
                      UIStackBack(updateCB, { ok: "OK" });
                    }
                  },],
                callback: async (idx, key, updateCB) => {

                  // cbInfo[idx].text =  " <<<距離:"+advUnit+" 速度:"+F,

                  updateCB(cInfo)
                }, default: 0
              },
              GCtrl_UI2(),
              ,
              {
                ...Simple_IO_Ctrl_UI,
                text: () =>SIMP_CTRL_IO_HIDE?"+ IO_CTRL": "- "+vof(Simple_IO_Ctrl_UI.text),
                onClick: (updateCB) => {
                  SIMP_CTRL_IO_HIDE = !SIMP_CTRL_IO_HIDE;

                  updateCB(cInfo)
                },
                opts:SIMP_CTRL_IO_HIDE?[]:Simple_IO_Ctrl_UI.opts
              },

              // Reel_loc_UI,
            ]
            UIStackGo(updateCB, cInfo, (result) => {
              console.log(result)
            })
          }
        }


      ],
    },
    {
      text: () => "測試 ",
      onClick:(updateCB)=>{
        hideTestFunction=!hideTestFunction;

        updateCB(UIStack_Current().UI)
      },
      opts:hideTestFunction?[]: 
      [
        ,
        {
          type: "button", key: "ReelTest",
          text: () => "ReelTest",
          onClick: async (updateCB) => {

            let speed_mmps=30;
            let distance_mm=100;

            let latestEncReading=0;

            let cInfo =  () => [

              {
                text: () => "ReelTest :"+`Speed:${speed_mmps} mm/s Distance:${distance_mm} mm`,
                opts: [
                
                  {
                    type: "button", key: "back",
                    text: () => "<=",
                    onClick: async (updateCB) => {
                      UIStackBack(updateCB, { ok: "OK" });
                    }
                  },
                  "$\n",
                  ...([20,50,100,200  ].map((v)=>(
                    {
                      type: "button",
                      text: "S"+v,
                      onClick: async (updateCB) => {
    
                        speed_mmps=v;
                        updateCB(UIStack_Current().UI)
                        
  
                      }
                  }))),
                  "$\n",
                  ...([40,100,200,500,1000].map((v)=>(
                    {
                      type: "button",
                      text: "D"+v,
                      onClick: async (updateCB) => {
    
                        distance_mm=v;
                        updateCB(UIStack_Current().UI)
                        
  
                      }
                  }))),



                  "$\n",
                  "$\n",

                  {
                    type: "button",
                    text: "Adv TestTest",
                    onClick: async (updateCB) => {
                      let start_time= new Date().getTime();
                      let distance_pulse=Math.round(distance_mm*SYS_CONFIG.A_Axis_Units_to_mm);

                      let encinfo_p=(await Lib.CNCSend({ "type": "AUX_GET_ENC", aid: 0 })).value 
                      await ReelGoAdv_s(distance_pulse,` F${6.6666*speed_mmps/SYS_CONFIG.A_Axis_Units_to_mm} ACC${500}`);
                      
                      await G(`M400`);
                      let encinfo_c=(await Lib.CNCSend({ "type": "AUX_GET_ENC", aid: 0 })).value
                      let elapsed_time= new Date().getTime()-start_time;
                      console.log(`>>>elapsed_time:${elapsed_time} ms  speed:${distance_mm/elapsed_time*1000} mm/s`)

                      let better_A_Axis_Units_to_mm=distance_pulse/(encinfo_c-encinfo_p);
                      console.log(`past:${encinfo_c-encinfo_p}  expect:${distance_mm}  diff:${encinfo_c-encinfo_p-distance_mm}`)
                      console.log(`better_A_Axis_Units_to_mm:${better_A_Axis_Units_to_mm}`)

                    }
                  },

                  {
                    type: "button",
                    text: "Adv GOGOGO!!!",
                    onClick: async (updateCB) => {
                      let start_time= new Date().getTime();
                      let distance=distance_mm;
                      await ReelGoAdv(distance,` F${6.6666*speed_mmps/SYS_CONFIG.A_Axis_Units_to_mm} ACC${100}`);
                      
                      await G(`M400`)
                      let elapsed_time= new Date().getTime()-start_time;
                      console.log(`>>>elapsed_time:${elapsed_time} ms  speed:${distance/elapsed_time*1000} mm/s`)

                    }
                  },


                  "$\n",
                  "$\n",
                  "$\n",


                  {
                    type: "button",
                    text: "RP1",
                    onClick: async (updateCB) => {

                      await Lib.CNCSend({ "type": "AUX_IO_CTRL", "pin": SYS_OUT_PIN_DEF.Reel_A_Run1, "state": 1, aid: 0 })

                    }
                  },
                  {
                    type: "button",
                    text: "X", id: "RP1_X",
                    onClick: async (updateCB) => {

                      await Lib.CNCSend({ "type": "AUX_IO_CTRL", "pin": SYS_OUT_PIN_DEF.Reel_A_Run1, "state": 0, aid: 0 })

                    }
                  },


                  "$\n",
                  "$\n",
                  "$\n",

                  {
                    type: "button",
                    text: "Reel0",
                    onClick: async (updateCB) => {

                      console.log("AUX_SET_ENC>>>>>", await Lib.CNCSend({ "type": "AUX_SET_ENC", value: 0, aid: 0 }));
                      ReelENC = 0;
                      // await ReelGoAdv(1);

                    }
                  },

                  "$\n",

                  {
                    type: "button",
                    text: "ReelRead:"+ReelENC,
                    onClick: async (updateCB) => {

                      ReelENC =(await Lib.CNCSend({ "type": "AUX_GET_ENC", aid: 0 })).value
                      // await ReelGoAdv(1);

                      updateCB(UIStack_Current().UI)
                    }
                  },

  


                ]
              },
              // JOG_UI,
              // Calib_UI
            ]//
            UIStackGo(updateCB, cInfo, (result) => {
              console.log(result)
            })
          }
        },
        {
          type: "button", key: "DDD",
          text: () => "TEST",
          onClick: async (updateCB) => {


            await G(`M120`)//dis
            let FA = `F${4000} ACC${1500}`
            // await Enter_Z2SafeZone_n_Check();
            await G(`G1 X${200} Y${260} ${FA}`)
            
            let ACC = "1000";
            let F = "2500";
            FA = `F${F} ACC${ACC}`
            for(let i=0;i<10;i++){
              

              for(let j=0;j<4;j++)
              {
                await G(`G1 X${200-j*40} Y${260} ${FA}`)
                await G("G04 P10")
              }
              
            }

            await G(`G1 X${40} Y${260} ${FA}`)
            console.log(`>>end`)
          }
        }
        // {
        //   type: "button", key: "DDD",
        //   text: () => ">><<",
        //   onClick: async (updateCB) => {
        //     console.log(T_Data.TD)
        //     let i=0;
        //     let len=T_Data.TD.length;
        //     for(let data of T_Data.TD){
        //       await Lib.CNCSend(data)

        //       if(i%10==0)
        //         console.log(`>>${i}/${len}`)

        //       if(i>100)break;
        //       i++;
        //     }

        //     console.log(`>>end`)
        //   }
        // }
      ]
    }


    // Feeder_loc_UI,
  ]
  

  UIStackGo(undefined, cbInfo)
  await waitUserCallBack(null, cbInfo)
}


function calcDist(speed1, speed2, acceleration) {
  return -(speed1 * speed1 - speed2 * speed2) / (2 * acceleration);
}


async function testInit() {

  {
    await G(`G01 X0 Y0 `)
    await G(`G92 X-5 Y-5 `)
    await G(`G01 X0 Y0 `)
    await G(`M400`)
    console.log(">>>>", await G(`M114`))



    return
  }
}
async function testtest() {

  {

    console.log("AUX_SET_ENC>>>>>", await Lib.CNCSend({ "type": "AUX_SET_ENC", value: 0, aid: 0 }));
    await G(`G92 A0`)

    await G(`M400`)

    console.log(">>>>", await G(`M114`))
    await G(`G01 A${1}`);
    // await G(`G01 A${1} X0`);

    await G(`G01.ENC A${2} ENC2 SMULT100 `);

    await delay(100);
    console.log("AUX_SET_ENC>>>>>", await Lib.CNCSend({ "type": "AUX_SET_ENC", value: 2, aid: 0 }));
    // await G(`G01 A1`);
    await G(`M400`)

    console.log(">>>>", await G(`M114`))

    return
  }


  console.log("testtest");


  let startSpeed = 1;
  let endSpeed = 1;
  let targetSpeed = 2000;
  let steps = 3000;
  let ACC = 1000;
  let DEA = 1;

  //10000 ->1
  //1000 ->3
  //100 ->9
  //10 ->500
  let stepsLeft = steps;
  let curSpeed = startSpeed;

  let T = 0.001;
  let time = 0;


  let pulseTrigV = 0;
  let pulseTrigCount = 0;
  let pulseTrigCountTar = 0;
  let CycleT = 0;
  for (let i = 0; i < 100000; i++) {

    CycleT += T;
    if (pulseTrigCount >= pulseTrigCountTar) {

      stepsLeft -= pulseTrigCountTar;

      if (stepsLeft == 0) break;
      // let aDist = calcDist(curSpeed,targetSpeed,ACC);
      let dDist = calcDist(endSpeed, curSpeed, DEA);


      let deAccBuffer = stepsLeft - dDist;
      if (deAccBuffer > 500)//Accel
      {
        let speedInc = ACC * CycleT;
        curSpeed += speedInc;

        if (curSpeed < Math.sqrt(ACC)) {
          curSpeed = Math.sqrt(ACC);
        }
        if (curSpeed > targetSpeed) {
          curSpeed = targetSpeed;
        }
      }
      else//de accel
      {
        let speedInc = DEA * CycleT;
        curSpeed -= speedInc;
        if (curSpeed < endSpeed) {
          curSpeed = endSpeed;
        }




      }


      let stepsCountPerBlock = (curSpeed + 1) * T;
      pulseTrigCount = 0;
      if (stepsCountPerBlock > 1) stepsCountPerBlock = Math.round(stepsCountPerBlock);

      pulseTrigCountTar = Math.ceil(curSpeed * T);
      pulseTrigV = stepsCountPerBlock;

      pulseTrigCount += pulseTrigV;

      console.log("CycleT", CycleT.toFixed(3), ">>>", pulseTrigCountTar, "dDist:", dDist.toFixed(2), "deAccBuffer", deAccBuffer.toFixed(2), "stepsLeft", stepsLeft, "curSpeed", curSpeed.toFixed(2));
      CycleT = 0;

    }
    else {
      pulseTrigCount += pulseTrigV;
      // console.log("time",time.toFixed(3), "SKIP....","pulseTrigCount",pulseTrigCount,"pulseTrigCountTar",pulseTrigCountTar);

    }


    time += T;

  }
}


async function MoveTest2() {


  for (let k = 0; k < 5; k++) {
    await Lib.CNCSend({ "type": "AUX_WAIT_FOR_FINISH", aid: k })
    // await Lib.CNCSend({ "type": "AUX_IO_CTRL", "pin": SYS_OUT_PIN_DEF.Reel_L_Light1, "state": 1, aid: k })
  }

  return;
}




async function MoveTest() {

  // await Enter_Z2SafeZone_n_Check();

  // await Exit_Z2SafeZone_n_Check();

  let obj = { quit: false, adC: [0, 0, 0, 0, 0, 0] };


  if (1) for (let k = 0; k < 5; k++)
    (async () => {

      let AUX_THREAD_ID = k;


      for (let i = 0; ; i++) {
        await Lib.CNCSend({ "type": "AUX_IO_CTRL", "pin": SYS_OUT_PIN_DEF.Reel_L_Light1, "state": 1, aid: AUX_THREAD_ID })

        await Lib.CNCSend({ "type": "AUX_DELAY", "P": 100, aid: 0, aid: AUX_THREAD_ID })
        await Lib.CNCSend({ "type": "AUX_IO_CTRL", "pin": SYS_OUT_PIN_DEF.Reel_L_Light1, "state": 0, aid: AUX_THREAD_ID })
        await Lib.CNCSend({ "type": "AUX_WAIT_FOR_FINISH", aid: AUX_THREAD_ID })
        obj.adC[AUX_THREAD_ID + 1]++;
        if (obj.quit) break;
      }

      console.log("side Thread DONE...", AUX_THREAD_ID, obj.quit)

    })();


  (async () => {

    await G(`M121`)//dis

    for (let i = 0; ; i++) {
      // await G(`M400`)
      let segs = 80
      let R = 50;
      for (let j = 0; j < segs; j++) {
        let t = 3.14159 * 2 * j / segs;
        await G(`G01 X${(R * Math.sin(t)).toFixed(3)} Y${(R * Math.cos(t)).toFixed(3)} F3000 ACC200`)

        // await G(`M42 P2 S${j%2==0?1:0}}`)

      }
      for (let j = 0; j < 0; j++) {
        await G(`G01 X0 Y0  ACC1000`)
        await G(`M42 P2 S0`)
        await G(`G01 X10 F1000 ACC400`)
        await G(`M42 P3 S0`)
        await G(`G01 Y10 F1000 ACC400`)
        await G(`M42 P3 S1`)
        await G(`G01 Z1_20 F500 ACC1000`)
        await G(`M42 P2 S1`)
        await G(`G01 Z1_60 F500 ACC1000`)
        await G(`G04 P40`);
      }
      await G(`M400`)
      await G(`M114`)
      // console.log(">>>>",await G(`M114`))
      obj.adC[0]++;
      if (obj.quit) break;
    }
    console.log("G Thread DONE...", obj.quit)


  })();

  let adC_pre = [...obj.adC];

  let sameAcc = 0;
  for (let i = 0; ; i++) {
    await delay(1000);
    let adC_cur = [...obj.adC];
    let adCDiff = adC_cur.map((v, idx) => v - adC_pre[idx]);
    let sameCLen = adCDiff.filter((v, idx) => v == 0).length;
    if (sameCLen != 0) {
      sameAcc += sameCLen;
    }
    else {
      sameAcc = 0;
    }
    // let someSame=adC_cur.filter((v,idx)=>v==adC_pre[idx]).length!=0;
    // NOT_ABORT();
    console.log("adCDiff:", adCDiff, " sameAcc:", sameAcc, " adC_cur:", adC_cur);
    adC_pre = adC_cur

    if (sameAcc > 6) {
      console.log("sameAcc over!!!!", sameAcc)
      break;
    }

    if (abortSig && abortSig.aborted) {
      break;
    }
  }

  console.log("EXIT called", obj)

  obj.quit = true;

  return
}



async function TESTPrmoise() {
  // let p = new Promise((resolve, reject) => {
  //   setTimeout(() => {
  //     resolve({
  //       a: 1,
  //       b: 2
  //     })
  //   }, 1000);
  // })
  let AUX_THREAD_ID=0;
  console.log("AUX_SET_ENC>>>>>", await Lib.CNCSend({ "type": "AUX_SET_ENC", value: 0, aid: AUX_THREAD_ID }));

  await G(`G01 X0 Y0 F2000 ACC4000`);
  await G(`M400`);

  await G(`G01.ENC_ES X100 Y1 AX_Y ENC15  F2000 ACC1000`);
  for(let i=0;i<20;i++){
    await delay(20)
    console.log("AUX_SET_ENC>>>>>", await Lib.CNCSend({ "type": "AUX_SET_ENC", value: i, aid: AUX_THREAD_ID }));
  }
  // let {a,b} = await p;
  // console.log("a,b",a,b)
}


async function testV2()
{
  // await Lib.CNCSend({ "type": "TTTT", vec:[1000,23],seg_time_us:100000 });
  // await Lib.CNCSend({ "type": "TTTT", vec:[-1000,23],seg_time_us:100000 });
  // await Lib.CNCSend({ "type": "TTTT", vec:[0,-46] ,seg_time_us:4600});
  let segs=160;
  let circle_segs=segs;
  let preP=undefined
  let prepreP=undefined

  let approachX=0;
  let approachY=0;

  let updatePeriod=30*1000;//200*20*1000/segs ;




  let filterInfo={
    locVec:[0,0],

    preSpeed:[0,0],
    alpha:0.3,
  }
  function locFilterUpdate(newLoc)
  {
    filterInfo.locVec=filterInfo.locVec.map((v,idx)=>
    {
      let diff=(newLoc[idx]-v)*filterInfo.alpha;
      let speed=diff/updatePeriod;

      {
        let speedDiff=speed-filterInfo.preSpeed[idx];
        let speedDiffLimit=0.002;
        if(speedDiff>speedDiffLimit)  speedDiff=speedDiffLimit;
        if(speedDiff<-speedDiffLimit) speedDiff=-speedDiffLimit;
  
        speed=filterInfo.preSpeed[idx]+speedDiff;
        speed*=0.9;
        if(speed>0.05) speed=0.05;
        if(speed<-0.05) speed=-0.05;
        filterInfo.preSpeed[idx]=speed;
  
      }


      return v+speed*updatePeriod;
    });
    // console.log(filterInfo.preSpeed);

    return filterInfo.locVec;
  }

  let preLocVec=[0,0];

  for(let i=0;i<segs*2;i++)
  {
    let x=10000*Math.sin(6*i/circle_segs*3.14159*2);
    let y=10000*Math.cos(i/circle_segs*3.14159*2);

    let locVec=locFilterUpdate([x,y]);
    let locAdvVec=locVec.map((locEle,idx)=>Math.round(locEle-preLocVec[idx]));
    preLocVec=locVec;
    
    let p= Lib.CNCSend({ "type": "TTTT", vec:locAdvVec,seg_time_us:updatePeriod });

    prepreP=preP;
    preP=p;
    if(prepreP!==undefined)await prepreP
  }

  // for(let i=0;i<segs;i++)
  // {
  //   let x=100000*(i%40<2?-1:1);
  //   let y=0;

  //   let locVec=locFilterUpdate([x,y]);
  //   let locAdvVec=locVec.map((locEle,idx)=>Math.round(locEle-preLocVec[idx]));
  //   preLocVec=locVec;
    
  //   let p= Lib.CNCSend({ "type": "TTTT", vec:locAdvVec,seg_time_us:updatePeriod });
  //   prepreP=preP;
  //   preP=p;
  //   if(prepreP!==undefined)await prepreP
  // }


  for(let i=0;i<300;i++)
  {
    let x=0;
    let y=0;

    let locVec=locFilterUpdate([x,y]);
    let locAdvVec=locVec.map((locEle,idx)=>Math.round(locEle-preLocVec[idx]));
    preLocVec=locVec;
    
    let p= Lib.CNCSend({ "type": "TTTT", vec:locAdvVec,seg_time_us:updatePeriod });
    prepreP=preP;
    preP=p;
    if(prepreP!==undefined)await prepreP
  }


  // await preP
}

; ({
  INIT,
  HeadGo,
  CNCTestRun,
  SimpHoming,
  test2,
  PackingCtrlPanelUI,
  testV2,
  testInit,
  testtest,
  CYCLERun_test,
  HeadCalib,
  MoveTest,

  TESTPrmoise,
})

