import React from 'react';
import { useState, useEffect, useRef, useMemo, useContext,createContext } from 'react';
import { useDispatch, useSelector } from "react-redux";
import { Layout, Button, Tabs, Slider, Menu, Divider, Dropdown, Popconfirm, Radio, InputNumber, Switch, Select } from 'antd';


import type { MenuProps, MenuTheme } from 'antd/es/menu';
import {
    UserOutlined, LaptopOutlined, NotificationOutlined, DownOutlined, MoreOutlined, PlayCircleFilled,PauseCircleOutlined,PauseCircleFilled,SettingOutlined,
    DisconnectOutlined, LinkOutlined,CameraOutlined,SyncOutlined,DeleteOutlined,ExclamationCircleOutlined,LoadingOutlined,StopOutlined,CloseOutlined,TagsOutlined,ToTopOutlined
} from '@ant-design/icons';

import clone from 'clone';

import { StoreTypes } from './redux/store';
import { EXT_API_ACCESS, EXT_API_CONNECTED, EXT_API_DISCONNECTED, EXT_API_REGISTER, EXT_API_UNREGISTER, EXT_API_UPDATE } from './redux/actions/EXT_API_ACT';


import { GetObjElement, ID_debounce, ID_throttle, ObjShellingAssign } from './UTIL/MISC_Util';

import { listCMDPromise } from './XCMD';
import { CameraSetupEditUI } from './CameraSetupEditUI';

import { VEC2D, SHAPE_ARC, SHAPE_LINE_seg, PtRotate2d,threePointToArc} from './UTIL/MathTools';

import { HookCanvasComponent, DrawHook_CanvasComponent, type_DrawHook_g, type_DrawHook } from './CanvasComp/CanvasComponent';
import { CORE_ID, CNC_PERIPHERAL_ID, BPG_WS, CNC_Perif, InspCamera_API } from './EXT_API';

import { Row, Col, Input, Tag, Modal, message, Space,Statistic,Avatar } from 'antd';

import {ITGlobalVariableContext,CompParam_GlobalVariable } from './App';

import { type_CameraInfo, type_IMCM } from './AppTypes';
import './basic.css';
import {CompParam_InspTarUI,IMCM_type,InspTarView_basicInfo,TestInputSelectUI,EDIT_PERMIT_FLAG} from "./SingleTargetVIEWUI_UTIL";

function findMainVecInfo(corners:number[][], seedIdx = -1) {
	const distArr = [];
	if (seedIdx === -1) {
			seedIdx = Math.floor(corners.length / 3);
	}
	const seedPt = corners[seedIdx];

	// console.log("seedPt",seedPt,"seedIdx",seedIdx);
	for (let i = 0; i < corners.length; i++) {
			if (i === seedIdx) continue;
			const curPt = corners[i];
			const dist = Math.hypot(seedPt[0] - curPt[0], seedPt[1] - curPt[1]);
			distArr.push({ dist: dist, idx: i });
	}
	if (distArr.length < 4) return null;

	distArr.sort((a, b) => a.dist - b.dist);

	// console.log("distArr",distArr);
	let minDistRatioIdx = -1;
	let minDistRatio = 999;
	for (let i = 0; i < 4; i++) {
			const distRatio = distArr[i + 3].dist / distArr[i].dist;
			if (minDistRatio > distRatio) {
					minDistRatio = distRatio;
					minDistRatioIdx = i;
			}
	}

	const nbInfo = distArr.slice(minDistRatioIdx, 4 + minDistRatioIdx) as any[];
	if (minDistRatio > 1.1) return null;





	let nbcenter={x:0,y:0};
	console.log("seedPt",seedPt,"nbInfo",nbInfo,"distArr",distArr,"minDistRatioIdx",minDistRatioIdx);
	nbInfo.forEach(nbEleInfo => {
			const idx = nbEleInfo.idx;
			const curPt = corners[idx];
			const vec = [curPt[0] - seedPt[0], curPt[1] - seedPt[1]];
			const norm = Math.hypot(vec[0], vec[1]);
			nbEleInfo.vec = vec;
			nbEleInfo.nvec = [vec[0] / norm, vec[1] / norm];
			nbcenter.x+=curPt[0];
			nbcenter.y+=curPt[1];
	});
	nbcenter.x/=nbInfo.length;
	nbcenter.y/=nbInfo.length;

	const vec1 = { vec: [...nbInfo[0].vec], nvec: [...nbInfo[0].nvec], info: [nbInfo[0]] } as any;
	const vec2 = { vec: [0, 0], info: [] } as any;

	nbInfo.forEach(nbEleInfo => {
			const nvecx = nbEleInfo.nvec;
			const dotP = vec1.nvec[0] * nvecx[0] + vec1.nvec[1] * nvecx[1];
			if (dotP > 0.8 || dotP < -0.8) {
					vec1.info.push(nbEleInfo);
			}
			if (dotP < 0.2 && dotP > -0.2) {
					vec2.info.push(nbEleInfo);
			}
	});

	if (vec1.info.length === 0 || vec2.info.length === 0) return null;

	vec1.vec = [...vec1.info[0].vec];
	vec1.dist = vec1.info[0].dist;
	vec2.vec = [...vec2.info[0].vec];
	vec2.dist = vec2.info[0].dist;

	let retInfo={} as any;
	if (Math.abs(vec1.vec[0]) > Math.abs(vec1.vec[1])) {
			if (vec1.vec[0] < 0) {
					vec1.vec[0] *= -1;
					vec1.vec[1] *= -1;
			}
			if (vec2.vec[1] < 0) {
					vec2.vec[0] *= -1;
					vec2.vec[1] *= -1;
			}
			retInfo= { vec1, vec2 };
	} else {
			if (vec1.vec[1] < 0) {
					vec1.vec[0] *= -1;
					vec1.vec[1] *= -1;
			}
			if (vec2.vec[0] < 0) {
					vec2.vec[0] *= -1;
					vec2.vec[1] *= -1;
			}
			retInfo= { vec1: vec2, vec2: vec1 };
	}
	let vec12_angle = Math.acos((retInfo.vec1.vec[0] * retInfo.vec2.vec[0] + retInfo.vec1.vec[1] * retInfo.vec2.vec[1])/(retInfo.vec1.dist*retInfo.vec2.dist))


	retInfo.angle = vec12_angle;
	retInfo.vecLength=(retInfo.vec1.dist+retInfo.vec2.dist)/2;
	retInfo.vecLengthDiff=retInfo.vec1.dist-retInfo.vec2.dist;
	retInfo.centerError=Math.hypot(nbcenter.x-seedPt[0],nbcenter.y-seedPt[1]);
	retInfo.pointIdx=seedIdx;
	return retInfo;
}


function findMainVecInfo_RANSAC(corners:number[][], tryCount:number=10) 
{
	let mvecInfos:any[]=[];
	let testShrink=corners.length/13;
	for(let i=0;i<tryCount;i++)
	{
		let seedIdx = Math.floor(i*(corners.length-testShrink*2)/tryCount+testShrink);



		
		let vecInfo = findMainVecInfo(corners, seedIdx);
		if(vecInfo!==null)
			mvecInfos.push(vecInfo);
		// console.log(vecInfo);
	}
	

	let vecLengthList = mvecInfos.map((info)=>info.vecLength).sort((a,b)=>a-b);
	console.log(mvecInfos,vecLengthList);


	let matchingCost:any[]=[];
	//find similar groups
	for(let i=0;i<mvecInfos.length-1;i++)
	{
		for(let j=i+1;j<mvecInfos.length;j++)
		{
			let cost = 
				Math.abs(mvecInfos[i].angle-mvecInfos[j].angle)*mvecInfos[i].vecLength+//angle diff arc len
				Math.abs(mvecInfos[i].vecLength-mvecInfos[j].vecLength)+//length diff
				Math.abs(mvecInfos[i].vecLengthDiff-mvecInfos[j].vecLengthDiff)//length diff diff
				+Math.abs(mvecInfos[i].centerError-mvecInfos[j].centerError)//center error
				// +(mvecInfos[i].vecLength+mvecInfos[j].vecLength)/2
				;
			matchingCost.push({
				idx1:mvecInfos[i].pointIdx,
				idx2:mvecInfos[j].pointIdx,
				mvec1:mvecInfos[i],
				mvec2:mvecInfos[j],
				cost:cost
			})
		}
	}

	//sort matchingCost by cost
	matchingCost.sort((a,b)=>a.cost-b.cost);
	console.log(matchingCost);

	let bestMatch = matchingCost[0];
	let bestPointIdx = -1;
	{
		let matchObjCostCount={} as any;
		for(let i=0;i<matchingCost.length;i++)
		{
			let curCost = matchingCost[i];
			if(matchObjCostCount[curCost.idx1]===undefined)matchObjCostCount[curCost.idx1]=1;
			else matchObjCostCount[curCost.idx1]++;
			if(matchObjCostCount[curCost.idx2]===undefined)matchObjCostCount[curCost.idx2]=1;
			else matchObjCostCount[curCost.idx2]++;

			console.log({...matchObjCostCount});
			if(matchObjCostCount[curCost.idx1]==4)
			{
				bestPointIdx = curCost.idx1;
				bestMatch = curCost;
				break;
			}
			if(matchObjCostCount[curCost.idx2]==4)
			{
				bestPointIdx = curCost.idx2;
				bestMatch = curCost;
				break;
			}
		}
	}






	let marginalMatch = matchingCost[Math.floor(matchingCost.length/10)];

	let costDiff = marginalMatch.cost-bestMatch.cost;
	if(costDiff>4)
		return null;

	
	return {
		matchingCostInfo:matchingCost,
		bestMatch:bestMatch,
		bestPointIdx:bestPointIdx,
		mvecInfos:mvecInfos,
	};
}


function findMarkMinDistanceIdx(marks:number[][],tarLoc:number[],allowedError:number=10)
{
	let minDist = allowedError;
	let minDistIdx = -1;

	for(let i=0;i<marks.length;i++)
	{
		let markLoc = marks[i];
		let xdiff=Math.abs(markLoc[0]-tarLoc[0]);
		let ydiff=Math.abs(markLoc[1]-tarLoc[1]);
		let blockDist=(xdiff+ydiff)/1.5;
		if(blockDist>minDist)continue;

		let dist = Math.hypot(xdiff,ydiff);
		if(dist<minDist)
		{
			minDist=dist;
			minDistIdx=i;
		}
	}
	if(minDistIdx==-1)
		return -1;

	return minDistIdx;

}



function FindXYMarkInfo(marks:number[][],seedIdx:number, vec1:any,vec2:any,allowedError:number=10)
{

	// console.log(vec1,vec2);
	let seedLoc = marks[seedIdx];
	let predictV1_p = [seedLoc[0]+vec1[0], seedLoc[1]+vec1[1]];
	let predictV1_n = [seedLoc[0]-vec1[0], seedLoc[1]-vec1[1]];

	let predictV2_p = [seedLoc[0]+vec2[0], seedLoc[1]+vec2[1]];
	let predictV2_n = [seedLoc[0]-vec2[0], seedLoc[1]-vec2[1]];

	// console.log(predictV1_p,predictV1_n,predictV2_p,predictV2_n);

	let idx1 = findMarkMinDistanceIdx(marks,predictV1_p,allowedError);
	let idx2 = findMarkMinDistanceIdx(marks,predictV1_n,allowedError);
	let idx3 = findMarkMinDistanceIdx(marks,predictV2_p,allowedError);
	let idx4 = findMarkMinDistanceIdx(marks,predictV2_n,allowedError);

	// console.log(idx1,idx2,idx3,idx4);
	let v1p={
		idx:idx1,
	} as any;
	if(idx1!=-1)
	{
		v1p.vec1=[
			marks[idx1][0]-seedLoc[0],
			marks[idx1][1]-seedLoc[1]
		]
		v1p.vec2=[...vec2]
	}

	let v1n={
		idx:idx2,
	} as any;
	if(idx2!=-1)
	{
		v1n.vec1=[
			seedLoc[0]-marks[idx2][0],
			seedLoc[1]-marks[idx2][1]
		]
		v1n.vec2=[...vec2]
	}

	let v2p={
		idx:idx3,
	} as any;
	if(idx3!=-1)
	{
		v2p.vec1=[...vec1]
		v2p.vec2=[
			marks[idx3][0]-seedLoc[0],
			marks[idx3][1]-seedLoc[1]
		]
	}

	let v2n={
		idx:idx4,
	} as any;
	if(idx4!=-1)
	{
		v2n.vec1=[...vec1]
		v2n.vec2=[
			seedLoc[0]-marks[idx4][0],
			seedLoc[1]-marks[idx4][1]
		]
	}




	return {
		v1p,v1n,v2p,v2n
	}
	// console.log(idx1,idx2,idx3,idx4);
}


function ChessBoard_MarkGrowth(marks:number[][]) 
{

	let tryCount=17;
	let mvecInfos = findMainVecInfo_RANSAC(marks, tryCount);

	if(mvecInfos===undefined || mvecInfos===null)return;
	console.log(mvecInfos);
	let bestMatch = mvecInfos?.bestMatch;
	if(bestMatch==undefined)return;
	// let bestMatchIdx = mvecInfos?.bestMatch?.idx1;
	let seedIdx = mvecInfos.bestPointIdx ;
	if(seedIdx==undefined)return;

	marks[seedIdx][2]=0;//init coord origin
	marks[seedIdx][3]=0;
	let seqRun = [{idx:seedIdx,vec1:bestMatch.mvec1.vec1.vec,vec2:bestMatch.mvec1.vec2.vec}];

	let vccordAdvTable={
		v1p:[1,0],
		v1n:[-1,0],
		v2p:[0,1],
		v2n:[0,-1]
	}

	let seed_vec1 = bestMatch.mvec1.vec1;
	let seed_vec2 = bestMatch.mvec1.vec2;

	let testCD=10000;
	while(seqRun.length>0)
	{
		if(testCD<=0)break;
		testCD--;
		let curPt = seqRun.pop() as {idx:number,vec1:number[],vec2:number[]};
		let curMark = marks[curPt.idx];

		let curCoord=[curMark[2],curMark[3]];
		let markInfos = FindXYMarkInfo(marks,curPt.idx,curPt.vec1,curPt.vec2,7) as {v1p:any,v1n:any,v2p:any,v2n:any};

		// console.log(markInfos);

		for(let key in markInfos)
		{
			let markInfo = markInfos[key as keyof typeof markInfos];
			if(markInfo.idx==-1)continue;

			let newCoord = [
				curCoord[0] + vccordAdvTable[key as keyof typeof vccordAdvTable][0],
				curCoord[1] + vccordAdvTable[key as keyof typeof vccordAdvTable][1]
			];
			let newMark = marks[markInfo.idx];
			if(newMark[2]==undefined)
			{
				newMark[2]=newCoord[0];
				newMark[3]=newCoord[1];

				// let v1 = 
				seqRun.push(markInfo);
			}
			else
			{
				// console.log("already has coord");
			}
		}

		
	}
	//MarkXYMarkFind
	
	console.log(marks,mvecInfos,seqRun);

	return marks;
}



function FileBrowsing({fsPath,filter,onFilesSelect}:{fsPath:string,filter:(fileInfo:any)=>boolean,onFilesSelect:(fileInfos:any[])=>void})
{
	const dispatch = useDispatch();
	const [BPG_API, setBPG_API] = useState<BPG_WS>(dispatch(EXT_API_ACCESS(CORE_ID)) as any);
	const [folderPathHistory, setFolderPathHistory] = useState<string[]>([fsPath]);
	const [folderContent, setFolderContent] = useState<any>(undefined);
	// let folderContent = await BPG_API.Folder_Struct(folderPath, 1);





	return <div>
		{/* <Button onClick={()=>{
			setFolderPathHistory([...folderPathHistory,folderPath]);
		}}>{"<"}</Button> */}




	</div>
}

export function SingleTargetVIEWUI_CameraCalib(props: CompParam_InspTarUI) {
	let { display, fsPath, EditPermitFlag, style = undefined, renderHook, def, report, onDefChange, defDoReload, UIOption, onUIOptionUpdate, showUIOptionConfigUI = false, APIExport ,cameraList} = props;
	const _this = useRef<any>({
		
		featureImgCanvas: document.createElement('canvas'),
		featureImgFeaturePoints:[],
		testLinePts:[],

	}).current;
	const dispatch = useDispatch();
	const [cacheDef, setCacheDef] = useState<any>(def);


	function onCacheDefChange(updatedDef: any,updateITCore:boolean=true) {

		if(updatedDef===undefined)
		{
				// onDefChange(undefined,false);
				return;   
		}
		console.log(updatedDef);
		setCacheDef(updatedDef);//set local cache


        if(updateITCore)
		(async () => {
				await BPG_API.InspTargetUpdate(updatedDef)//send to Core to update
				// await BPG_API.InspTargetExchange(cacheDef.id, { type: "revisit_cache_stage_info" });
		})()
		onDefChange(updatedDef, true);//send back to manager to be able to save
	}
	// const [defReport, setDefReport] = useState<any>(report);
	const [BPG_API, setBPG_API] = useState<BPG_WS>(dispatch(EXT_API_ACCESS(CORE_ID)) as any);
	const [Local_IMCM, setLocal_IMCM] =
		useState<IMCM_type | undefined>(undefined);


	const [currentFileBrowsingInfo, setCurrentFileBrowsingInfo] = useState<any>(undefined);
	const [analyzeInfoSet, setAnalyzeInfoSet] = useState<any[]>([]);



	useEffect(() => {
		console.log("fsPath:" + fsPath)
		setCacheDef(def);
		return (() => {
		});

	}, [def]);






	enum EditState {
		Normal_Show = 0,
		Calib_UI = 1,
		ChessBoardDataCollection = 2,
		UndistortImageTest = 3,

        CameraTakePhoto = 4,

		NA = -99999
	}

	type State_Info={
			state:EditState,
			data:any
	}
	const [editState, _setEditState] = useState<State_Info[]>([{"state":EditState.Normal_Show,data:undefined}]);


	useEffect(() => {
		

		console.log(def);
		BPG_API.InspTargetExchange(def.id, {
		  type: "stream_info",
		  downsample: display ? 1 : 10,
		  stream_id: def.stream_id
		});
	
		return (() => {
		});
	
	  }, [display]);
	

	if (display == false) return null;



	let currentState=editState[editState.length-1];


	function stateAction(stateTriple:State_Info[]) {//idx0:leave idx1:stay idx2:enter
		stateTriple.forEach((st, idx) => {

			switch (st.state)//current state
			{
					case EditState.Normal_Show:
							if (idx == 2)//enter
							{

							}
							else if (idx == 0)//leave
							{

							}
							break;

					case EditState.Calib_UI:
							if (idx == 2)//enter
							{
							}
							else if (idx == 0)//leave
							{
							}
							break;

						
					case EditState.ChessBoardDataCollection:
						if (idx == 2)//enter
						{
							(async ()=>{
								let folderinfo = await BPG_API.Folder_Struct(fsPath);
								console.log(folderinfo);
								setCurrentFileBrowsingInfo(folderinfo);



								// setFolderContent(folderinfo);
							})();
						}
						else if (idx == 0)//leave
						{
							setCurrentFileBrowsingInfo(undefined);
							setAnalyzeInfoSet([]);
							_this.featureImgFeaturePoints=[];


                            //recover camera setting

                            
                // await BPG_API.CameraSetup({...newCamInfo,frame_rate:curFrameRate},trigMode);



						}
						break;


					case EditState.CameraTakePhoto:
						if (idx == 2)//enter
						{
                            let camInfo = cacheDef.target_camera_info;

                            if(camInfo===undefined)break;
							(async ()=>{
                                await BPG_API.CameraSetup({...camInfo,frame_rate:15},2);
							})();
						}
						else if (idx == 0)//leave
						{
                // await BPG_API.CameraSetup({...newCamInfo,frame_rate:curFrameRate},trigMode);

                            let tcamInfo = cacheDef.target_camera_info;

                            if(tcamInfo===undefined)break;

                            let mainCameraSetup = cameraList.find((camInfo:type_CameraInfo)=>camInfo.id===tcamInfo.id);
                            if(mainCameraSetup===undefined)break;
                            (async ()=>{
                                await BPG_API.CameraSetup({...mainCameraSetup},2);
                                await BPG_API.CameraClearTriggerInfo();
                            })();

						}
						break;



					case EditState.UndistortImageTest:
							if (idx == 2)//enter
							{
								_this.testLinePts=[];
								(async ()=>{
									let folderinfo = await BPG_API.Folder_Struct(fsPath);
									console.log(folderinfo);
									setCurrentFileBrowsingInfo(folderinfo);
									// setFolderContent(folderinfo);
								})();
							}
							else if (idx == 0)//leave
							{
								_this.testLinePts=[];
							}
							break;

			}
		})
	}


	function popEditState() 
	{
		if(editState.length==1)
			return;
		let state3Ev: State_Info[] = [];//3 elements, leave,stay,enter
		let preState=editState[editState.length-2];
		state3Ev = [currentState, {state:EditState.NA,data:undefined}, preState]
		stateAction(state3Ev);

		let newStateList=[...editState];
		newStateList.pop();
		_setEditState(newStateList);
	}
	function pushEditState(newEditState: State_Info) {

		// _this.sel_region = 
		// _this.sel_region_type = undefined;
		// if (_this.canvasComp == undefined) return;
		//     _this.canvasComp.UserRegionSelect(undefined)

		let state3Ev: State_Info[] = [];//3 elements, leave,stay,enter
		if (newEditState?.state != currentState.state) {
			state3Ev = [currentState, {state:EditState.NA,data:undefined}, newEditState]
		}
		else
		{
			state3Ev = [{state:EditState.NA,data:undefined},newEditState,{state:EditState.NA,data:undefined} ]
		}
		stateAction(state3Ev);
		if(newEditState?.state != currentState.state)//new state
			_setEditState([...editState,newEditState]);
		else //update/replace latest same state with different data
		{
			let newStateList=[...editState];
			newStateList[newStateList.length-1]=newEditState;
			_setEditState(newStateList);
		}
	}





	async function AnalyzeChessBoardImage(folderPath:string,fileName:string)
	{

		let retInfo = await BPG_API.InspTargetExchange(def.id, {
			type: "extract_pattern_points",
			path:folderPath+"/"+fileName,
			maxCorners:3000,
			qualityLevel:0.003,
			minDistance:40,
			blockSize:11,
			useHarrisDetector:false,
			k:0.01,
			bilateralFilter:{
				d:0,
				sigmaColor:10,
				sigmaSpace:40
			},
			chessboard_filter:{
				N:30,
				r:50,
				blob_size_threshold:5,
				HL_diff_threshold:50
			}
		}) as any[];
		console.log(retInfo);


		let imgInfo=undefined;
		let markInfo=undefined;
		let IM = retInfo.find((p: any) => p.type == "IM");
		if (IM !== undefined) {
				// _this.featureImgCanvas.width = IM.image_info.width;
				// _this.featureImgCanvas.height = IM.image_info.height;

				// let ctx2nd = _this.featureImgCanvas.getContext('2d');
				
				// if(IM.image_info.image instanceof ImageData)
				// 		ctx2nd.putImageData(IM.image_info.image, 0, 0);
				// else if(IM.image_info.image instanceof HTMLImageElement)
				// 		ctx2nd.drawImage(IM.image_info.image, 0, 0);

				imgInfo=IM.image_info;

		}
		let RP = retInfo.find((p: any) => p.type == "RP");
		if(RP!==undefined)
		{


			let corners = RP.data.corners;
			
			// try to find 10 seed points to find the main vec
			// let tryCount=20;
			// let mvecInfos = findMainVecInfo_RANSAC(corners, tryCount);
			// let bestMatchIdx = mvecInfos?.bestMatch?.idx1;

			let marks = ChessBoard_MarkGrowth(corners);
			// console.log(marks);
			// _this.featureImgFeaturePoints = marks;
			markInfo=marks;
			// console.log(mvecInfos);

		}

		return {
			imgInfo,
			markInfo
		}
	}




	let EDIT_UI = null;

	function SetDisplayInfo(info:any)
	{
		_this.featureImgCanvas.width = info.imgInfo.width;
		_this.featureImgCanvas.height = info.imgInfo.height;

		let ctx2nd = _this.featureImgCanvas.getContext('2d');
					
		if(info.imgInfo.image instanceof ImageData)
				ctx2nd.putImageData(info.imgInfo.image, 0, 0);
		else if(info.imgInfo.image instanceof HTMLImageElement)
				ctx2nd.drawImage(info.imgInfo.image, 0, 0);

		_this.featureImgFeaturePoints = info.markInfo;

	}

	switch (currentState.state) {

			case EditState.Normal_Show:



			EDIT_UI = <>
							<Input maxLength={100} value={cacheDef.id} disabled
									style={{ width: "200px" }}
									onChange={(e) => {
									}} />
			
							{((EditPermitFlag & EDIT_PERMIT_FLAG.XXFLAGXX) == 0)?null:
									<>
									<InspTarView_basicInfo {...props} def={cacheDef} onDefChange={(newDef, ddd) => {
											onCacheDefChange(newDef);
									}} 
									
									defDoReload={()=>defDoReload()}
									
									/>
			
									
									</>
							}   

							<Button onClick={() => {
									console.log(">>>")
									pushEditState({state:EditState.Calib_UI,data:undefined});
									}}>校正模式</Button>


							enable_calibration:<Switch checked={cacheDef.enable_calibration} onChange={(checked)=>{
								BPG_API.InspTargetExchange(def.id, {
									type: "enable_calibration",
									enable:checked
								});
							}}  />


							<Button onClick={()=>{
								BPG_API.InspTargetExchange(def.id, {
									type: "enable_image_accumulator",
									enable:true
								});
							}}>開啟多禎平均</Button>

							<Button onClick={()=>{
								BPG_API.InspTargetExchange(def.id, {
									type: "enable_image_accumulator",
									enable:false
								});
							}}>關閉</Button>


					</>
					break;
			case EditState.Calib_UI:

					EDIT_UI = <>
{/* 
						<Button onClick={()=>{
							//find files in folder
							(async ()=>{
								let folderinfo = await BPG_API.Folder_Struct(fsPath);
								let imgFilesPaths=folderinfo.files
									.filter((fileInfo:any)=>fileInfo.name.endsWith(".png") || fileInfo.name.endsWith(".jpg") || fileInfo.name.endsWith(".bmp") || fileInfo.name.endsWith(".jpeg"))
									.map((fileInfo:any)=>fsPath+"/"+fileInfo.name);
								

								console.log(imgFilesPaths);
								let calibInfo = await BPG_API.InspTargetExchange(def.id, {
									type: "calibrate_from_images",
									img_paths:imgFilesPaths
								}) as any[];
								console.log(calibInfo);

								{
									let RP = calibInfo.find((p:any)=>p.type=="RP");
									if(RP===undefined) return;

									onCacheDefChange({...cacheDef, ...RP.data});

								}
							})();

						}}>測試儲存圖檔</Button> */}


						<Button onClick={()=>{
							pushEditState({state:EditState.ChessBoardDataCollection,data:undefined});
						}}>棋盤校正</Button>

						<Button onClick={()=>{
							pushEditState({state:EditState.UndistortImageTest,data:undefined});
						}}>觀測校正圖像</Button>




					</>
					break;

			case EditState.ChessBoardDataCollection:
				if(currentFileBrowsingInfo===undefined)
				{
					EDIT_UI=<>請稍候....</>;
					break;
				}
				let folderPath = currentFileBrowsingInfo.path;

				EDIT_UI=<>
					<Button disabled={analyzeInfoSet.length==0} onClick={()=>{

						(async ()=>{
							let firstInfo = analyzeInfoSet[0];
							console.log(firstInfo);
							

							let markInfoList=analyzeInfoSet.map((info:any)=>
								{
									let markFilteredInfo = info.markInfo.filter((mark:any)=>mark[2]!==undefined && mark[3]!==undefined);
									let markLocs = markFilteredInfo.map((mark:any)=>[mark[0],mark[1]]);

									let coordMinX=999999;
									let coordMinY=999999;
									let markCoords = markFilteredInfo.map((mark:any)=>{
										if(mark[2]<coordMinX) coordMinX=mark[2];
										if(mark[3]<coordMinY) coordMinY=mark[3];
										return [mark[2],mark[3],0];
									});

									markCoords.forEach((coord:any)=>{
										coord[0]-=coordMinX;
										coord[1]-=coordMinY;
									});



									return {
										mark_locs:markLocs,
										mark_coords:markCoords
									};
								}
							);
							console.log(markInfoList);
							let retInfo = await BPG_API.InspTargetExchange(def.id, {
								type: "calc_calib_params_from_mark_info_list",
								mark_info_list:markInfoList,
								image_width:firstInfo.imgInfo.width,
								image_height:firstInfo.imgInfo.height
							}) as any[];

							let RP = retInfo.find((p:any)=>p.type=="RP");
							if(RP===undefined) return;
							console.log(RP.data);
							onCacheDefChange({...cacheDef, ...RP.data});
							console.log(retInfo);
						})();
					}}>校正相機</Button>

					<Button disabled={analyzeInfoSet.length!=1} onClick={()=>{

						let firstInfo = analyzeInfoSet[0];
						console.log(">>>",firstInfo.markInfo);
						let markInfo = firstInfo.markInfo;
						//find info on 4 cornors

						let TRC_idx=-1,TRC_Min_Score=9999999;
						let TLC_idx=-1,TLC_Min_Score=9999999;
						let BRC_idx=-1,BRC_Min_Score=9999999;
						let BLC_idx=-1,BLC_Min_Score=9999999;

						let coordMinX=9999999;
						let coordMinY=9999999;
						for(let i=0;i<markInfo.length;i++)
						{
							let mark = markInfo[i];
							if(mark.length<4) continue;//skip, no coord info
							let coord = [mark[2],mark[3]];
							if(coord[0]<coordMinX) coordMinX=coord[0];
							if(coord[1]<coordMinY) coordMinY=coord[1];

							let TRC_Score = coord[0]+coord[1];
							if(TRC_Score<TRC_Min_Score) {TRC_Min_Score=TRC_Score;TRC_idx=i;}

							let TLC_Score = -coord[0]+coord[1];//distance to top left corner
							if(TLC_Score<TLC_Min_Score) {TLC_Min_Score=TLC_Score;TLC_idx=i;}

							let BRC_Score = coord[0]-coord[1];//distance to bottom right corner
							if(BRC_Score<BRC_Min_Score) {BRC_Min_Score=BRC_Score;BRC_idx=i;}

							let BLC_Score = -coord[0]-coord[1];//distance to bottom left corner
							if(BLC_Score<BLC_Min_Score) {BLC_Min_Score=BLC_Score;BLC_idx=i;}

						}

						console.log(TRC_idx,markInfo[TRC_idx]);
						console.log(TLC_idx,markInfo[TLC_idx]);
						console.log(BRC_idx,markInfo[BRC_idx]);
						console.log(BLC_idx,markInfo[BLC_idx]);

						

						(async ()=>{
							
							let retInfo = await BPG_API.InspTargetExchange(def.id, {
								type: "calc_perspective_transform_from_4points_set",
								"4_points_set":[
									{mark:[markInfo[TRC_idx][0],markInfo[TRC_idx][1]],coord:[markInfo[TRC_idx][2]-coordMinX,markInfo[TRC_idx][3]-coordMinY]},
									{mark:[markInfo[TLC_idx][0],markInfo[TLC_idx][1]],coord:[markInfo[TLC_idx][2]-coordMinX,markInfo[TLC_idx][3]-coordMinY]},
									{mark:[markInfo[BRC_idx][0],markInfo[BRC_idx][1]],coord:[markInfo[BRC_idx][2]-coordMinX,markInfo[BRC_idx][3]-coordMinY]},
									{mark:[markInfo[BLC_idx][0],markInfo[BLC_idx][1]],coord:[markInfo[BLC_idx][2]-coordMinX,markInfo[BLC_idx][3]-coordMinY]}
								]
							}) as any[];

							let RP = retInfo.find((p:any)=>p.type=="RP");
							if(RP===undefined) return;
							console.log(RP.data);
							onCacheDefChange({...cacheDef, ...RP.data});

							console.log(retInfo);


						})();



					}}>校正視角傾斜</Button>

                    
                    <Dropdown overlay={<Menu>
                        {cameraList.map((camera, index) => (
                            <Menu.Item 
                            key={index}
                            onClick={()=>{
                                onCacheDefChange({...cacheDef,target_camera_info:{...camera}});
                            }}
                            >{camera.id+":"+camera.side_name}</Menu.Item>
                        ))}
                    </Menu>}>
                        <Button>
                            {cacheDef.target_camera_info?.side_name || cacheDef.target_camera_info?.id || "Select Camera..."}<DownOutlined />
                        </Button>
                    </Dropdown>


                    
                    <Button disabled={cacheDef.target_camera_info===undefined} onClick={()=>{
                        //take photo
                        // BPG_API.InspTargetExchange(def.id, {
                        //     type: "take_photo",
                        // });
                        // console.log(cameraList);

                        pushEditState({state:EditState.CameraTakePhoto,data:undefined});
                    }}>拍照</Button>

                    
					{
						analyzeInfoSet.map((info,idx)=>{
							return <><Button key={idx} onClick={()=>{
								// setCurrentFileAnalyzeInfo(info);
								SetDisplayInfo(info);

								setAnalyzeInfoSet([...analyzeInfoSet]);//Hack to update state

							}}>{info.fileName}</Button>
							<Button key={idx+"DEL"} style={{marginRight:"10px"}} danger onClick={()=>{
								let newInfoSet = [...analyzeInfoSet];
								newInfoSet.splice(idx,1);
								setAnalyzeInfoSet(newInfoSet);
							}}>X</Button>
							</>
						})
					}
					<Divider />



					{currentFileBrowsingInfo.files
						.filter((fileInfo:any)=>//only image files
							fileInfo.name.endsWith(".png") || fileInfo.name.endsWith(".jpg") || 
							fileInfo.name.endsWith(".bmp") || fileInfo.name.endsWith(".jpeg"))
						.map((fileInfo:any)=>{
						return <Button key={fileInfo.name} disabled={analyzeInfoSet.find((info:any)=>info.fileName==fileInfo.name)!==undefined} onClick={()=>{
							(async ()=>{
								let retInfo = await AnalyzeChessBoardImage(folderPath,fileInfo.name);

								let newInfo={
									fileName:fileInfo.name,
									...retInfo
								}
								SetDisplayInfo(newInfo);
								setAnalyzeInfoSet([...analyzeInfoSet,newInfo]);
								
								console.log(retInfo);
							})();
						}}>{fileInfo.name}</Button>
					})}
                    
				</>
					break;
			case EditState.UndistortImageTest:
				if(currentFileBrowsingInfo===undefined)
				{
					EDIT_UI=<>請稍候....</>;
					break;
				}
				EDIT_UI=<>
					<Button onClick={()=>{
						BPG_API.InspTargetExchange(def.id, {
							type: "save_latest_undistorted_image",
							// path:folderPath+"/"+currentFileBrowsingInfo.files[0].name
						});
					}}>儲存圖片</Button>

					<Button onClick={()=>{
						//remove latest test point
						let newTestLinePts = [..._this.testLinePts];
						newTestLinePts.pop();
						_this.testLinePts = newTestLinePts;
						//Hack to update state
						setAnalyzeInfoSet([...analyzeInfoSet]);
					}}>刪除測試線</Button>


					<Button onClick={()=>{
						//test point alignment
						(async ()=>{


							let retInfo = await BPG_API.InspTargetExchange(def.id, {
								type: "get_refine_points_from_latest_undistorted_image",
								points:_this.testLinePts.map((pt:any)=>[pt.x,pt.y]),
								window_size:30,
								zero_zone_size:4
							}) as any[];
							console.log(">>>",retInfo);
							let RP = retInfo.find((p:any)=>p.type=="RP");
							if(RP===undefined) return;
							console.log(RP.data);
							let newPTs = RP.data.points.map((p:any)=>({x:p[0],y:p[1]}));
							_this.testLinePts = newPTs;
							setAnalyzeInfoSet([...analyzeInfoSet]);
						})();
					}}>測試點對齊</Button>



					{currentFileBrowsingInfo.files
						.filter((fileInfo:any)=>//only image files
							fileInfo.name.endsWith(".png") || fileInfo.name.endsWith(".jpg") || 
							fileInfo.name.endsWith(".bmp") || fileInfo.name.endsWith(".jpeg"))
						.map((fileInfo:any)=>{
						let folderPath = currentFileBrowsingInfo.path;
						return <Button key={fileInfo.name} onClick={()=>{
							(async ()=>{
								
								let imgInfo = await BPG_API.InspTargetExchange(def.id, {
									type: "undistort_image",
									path:folderPath+"/"+fileInfo.name
								}) as any[];
								console.log(imgInfo);



								let IM = imgInfo.find((p: any) => p.type == "IM");
								if (IM !== undefined) {
										_this.featureImgCanvas.width = IM.image_info.width;
										_this.featureImgCanvas.height = IM.image_info.height;

										let ctx2nd = _this.featureImgCanvas.getContext('2d');
										
										if(IM.image_info.image instanceof ImageData)
												ctx2nd.putImageData(IM.image_info.image, 0, 0);
										else if(IM.image_info.image instanceof HTMLImageElement)
												ctx2nd.drawImage(IM.image_info.image, 0, 0);
								}
								//Hack to update state
								setAnalyzeInfoSet([...analyzeInfoSet]);
							})();
						}}>{fileInfo.name}</Button>
					})}
				</>
				break;
            

            case EditState.CameraTakePhoto:

                EDIT_UI=<div style={{width:"100%",backgroundColor:"white"}}>
                <CameraSetupEditUI camSetupInfo={cacheDef.target_camera_info} CoreAPI={BPG_API} onCameraSetupUpdate={(newCamInfo:type_CameraInfo|undefined)=>{
                    console.log(newCamInfo);
                    if(newCamInfo===undefined) return;

                    onCacheDefChange({...cacheDef,target_camera_info:{...newCamInfo}},false);
                    (async function(){
                        console.log(newCamInfo);
                        let trigMode=newCamInfo.trigger_mode;
                        let capFrameRate=60;
                        let curFrameRate=newCamInfo.frame_rate;
                        if(curFrameRate>capFrameRate)
                        curFrameRate=capFrameRate;
                        await BPG_API.CameraSetup({...newCamInfo,frame_rate:curFrameRate},trigMode);

                    })()
                        
                }} 
                imageSavePath={fsPath}
                />
                </div>
                break;
    }

	return <div style={{ ...style }} className={"overlayCon"}>

		<div className={"overlay scroll"} style={{width:currentState.state===EditState.CameraTakePhoto?"100%":undefined}} >

			{editState.length>1&&<Button danger onClick={()=>{popEditState()}}>{"<"}</Button>}
			{EDIT_UI}


		</div>


		

		<HookCanvasComponent style={{}} dhook={(ctrl_or_draw: boolean, g: type_DrawHook_g, canvas_obj: DrawHook_CanvasComponent) => {
			// _this.canvasComp = canvas_obj;

			let ctx = g.ctx;
			let mouseOnCanvas = canvas_obj.VecX2DMat(g.mouseStatus, g.worldTransform_inv);

			let camMag = canvas_obj.camera.GetCameraScale();
			if(ctrl_or_draw)
			{//control phase

				if(currentState.state===EditState.UndistortImageTest)
					{
						if(g.mouseStatus.status==1 && g.mouseStatus.pstatus==0)
						{
							{
								_this.testLinePts.push(mouseOnCanvas);
							}
						}
						return;
					}
				return;
			}
			

			{
				g.ctx.save();
				// let scale = _this.featureInfoExt.IM.image_info.scale;
				// g.ctx.scale(scale, scale);
				g.ctx.translate(-0.5, -0.5);
				g.ctx.drawImage(_this.featureImgCanvas, 0, 0);
				g.ctx.restore();
			}

			if(_this.featureImgFeaturePoints?.length>0)
			{

				g.ctx.strokeStyle="red";
				let size=5/camMag;
				g.ctx.lineWidth=2/camMag;
				g.ctx.save();
				for(let i=0;i<_this.featureImgFeaturePoints.length;i++)
				{
					let mark = _this.featureImgFeaturePoints[i];
					let pointLoc={x:mark[0],y:mark[1]};
					let coordLoc={x:mark[2],y:mark[3]};
					//draw cross
					g.ctx.beginPath();
					g.ctx.moveTo(pointLoc.x, pointLoc.y-size);
					g.ctx.lineTo(pointLoc.x, pointLoc.y+size);
					g.ctx.moveTo(pointLoc.x-size, pointLoc.y);
					g.ctx.lineTo(pointLoc.x+size, pointLoc.y);
					g.ctx.stroke();
					g.ctx.closePath();


					if(camMag>0.5 && coordLoc.x!=undefined && coordLoc.y!=undefined)
					{

						//draw coord text at pointLoc
						g.ctx.fillStyle="red";
						g.ctx.font=`${12}px Arial`;
						g.ctx.fillText(`${coordLoc.x.toFixed(0)},${coordLoc.y.toFixed(0)}`,pointLoc.x,pointLoc.y);
					}
				}
				g.ctx.restore();
				
			}


			{//draw mouse position on canvas
				g.ctx.save();
				g.ctx.fillStyle="red";
				g.ctx.font=`${30/camMag}px Arial`;
				g.ctx.fillText(`${mouseOnCanvas.x.toFixed(2)},${mouseOnCanvas.y.toFixed(2)}`,mouseOnCanvas.x,mouseOnCanvas.y);
				g.ctx.restore();
			}

			if(currentState.state===EditState.UndistortImageTest)
			{//draw line
				for(let i=1;i<_this.testLinePts.length;i+=2)
				{
					let pt1=_this.testLinePts[i-1];
					let pt2=_this.testLinePts[i];
					g.ctx.strokeStyle="red";
					g.ctx.lineWidth=4/camMag;
					g.ctx.beginPath();
					g.ctx.moveTo(pt1.x,pt1.y);
					g.ctx.lineTo(pt2.x,pt2.y);
					g.ctx.stroke();
					g.ctx.closePath();
					let distance=Math.hypot(pt1.x-pt2.x,pt1.y-pt2.y);
					g.ctx.fillStyle="red";
					g.ctx.font=`${32/camMag}px Arial`;
					g.ctx.fillText(`${distance.toFixed(3)}`,(pt2.x),(pt2.y));
				}

				if(_this.testLinePts.length%2==1)
				{//draw cross at latest point
					let pt=_this.testLinePts[_this.testLinePts.length-1];
					g.ctx.strokeStyle="red";

					g.ctx.lineWidth=12/camMag;
					let size=15/camMag;
					g.ctx.beginPath();
					g.ctx.moveTo(pt.x, pt.y-size);
					g.ctx.lineTo(pt.x, pt.y+size);
					g.ctx.moveTo(pt.x-size, pt.y);
					g.ctx.lineTo(pt.x+size, pt.y);
					g.ctx.stroke();

					g.ctx.beginPath();
					g.ctx.moveTo(pt.x, pt.y);
					//to mouse
					g.ctx.lineTo(mouseOnCanvas.x, mouseOnCanvas.y);
					g.ctx.stroke();

					g.ctx.closePath();

					let distance=Math.hypot(pt.x-mouseOnCanvas.x,pt.y-mouseOnCanvas.y);
					g.ctx.fillStyle="orange";
					g.ctx.font=`${32/camMag}px Arial`;
					g.ctx.fillText(`D:${distance.toFixed(3)}`,(mouseOnCanvas.x),(mouseOnCanvas.y+25/camMag));


				}
			}
		}} />
	</div>;
}
