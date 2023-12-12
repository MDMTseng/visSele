import React from 'react';
import { useState, useEffect,useRef,useMemo,useContext, useCallback,memo } from 'react';
import { useDispatch, useSelector } from "react-redux";
import { Layout, Button, Tabs, Slider, Menu, Divider, Dropdown, Popconfirm, Radio, InputNumber, Switch, Select } from 'antd';


import type { MenuProps, MenuTheme } from 'antd/es/menu';
import {
    UserOutlined, LaptopOutlined, NotificationOutlined, DownOutlined, MoreOutlined, PlayCircleFilled,PauseCircleOutlined,PauseCircleFilled,
    DisconnectOutlined, LinkOutlined,CameraOutlined,SyncOutlined,DeleteOutlined,ExclamationCircleOutlined,LoadingOutlined,StopOutlined
} from '@ant-design/icons';

import clone from 'clone';

import { StoreTypes } from './redux/store';
import { EXT_API_ACCESS, EXT_API_CONNECTED, EXT_API_DISCONNECTED, EXT_API_REGISTER, EXT_API_UNREGISTER, EXT_API_UPDATE } from './redux/actions/EXT_API_ACT';


import { GetObjElement, ID_debounce, ID_throttle, ObjShellingAssign } from './UTIL/MISC_Util';

import { listCMDPromise } from './XCMD';


import { VEC2D, SHAPE_ARC, SHAPE_LINE_seg, PtRotate2d } from './UTIL/MathTools';

import { HookCanvasComponent, DrawHook_CanvasComponent, type_DrawHook_g, type_DrawHook } from './CanvasComp/CanvasComponent';
import { CORE_ID, CNC_PERIPHERAL_ID, BPG_WS, CNC_Perif, InspCamera_API } from './EXT_API';

import { Row, Col, Input, Tag, Modal, message, Space,Statistic,Avatar } from 'antd';


import { type_CameraInfo, type_IMCM } from './AppTypes';
import './basic.css';


import {
    InspTargetUI_MUX } from './InspTarView';

import ReactFlow, {
    MiniMap,
    Controls,
    Background,
    useNodesState,
    useEdgesState,
    addEdge,
    Handle,
    Position,
    NodeResizer
  } from 'reactflow';
  // import    from 'reactflow';


const initialNodes = [
    { id: '1', position: { x: 0, y: 0 }, data: { label: '1' } },
    { id: '2', position: { x: 0, y: 100 }, data: { label: '2' } },
    { id: '3', position: { x: 0, y: 200 }, data: { label: '3' } },
  ];
  
  const initialEdges = [{ id: 'e1-2', source: '1', target: '2' },{ id: 'e2-3', source: '2', target: '3' },{ id: 'e1-3', source: '1', target: '3' }];
  




let Orientation_ShapeBasedMatching_NodeUI= (({ data, isConnectable , selected}:{data:any,isConnectable:boolean,selected:boolean}) => {

    const [UPDC, setUPDC] = useState(0);
    const _this = useRef<any>({windowSize:{width:100,height:100}}).current;
    // console.log("Orientation_ShapeBasedMatching_NodeUI",data,_this);

    useEffect(() => {
      // console.log("Orientation_ShapeBasedMatching_NodeUI",data._.nodeInfo);
        if(data._.nodeInfo!==undefined)
        {
          
          _this.windowSize={width:data._.nodeInfo.width,height:data._.nodeInfo.height};
          setUPDC(UPDC+1);
        }
    }, [data]);

    if(data._.it===undefined)return null;
    //Add NodeResizer to the node
    return (
      <>
      <div className="nowheel" style={{width:_this.windowSize.width,height:_this.windowSize.height}} >
        
        <NodeResizer  color="#ff0071" isVisible={selected}  minWidth={100} minHeight={30} onResize={(event, params)=>{_this.windowSize=params}} />
        <Handle
          type="target"
          position={Position.Top}
          style={{ background: '#555' }}
          onConnect={(params) => console.log('handle onConnect', params)}
          isConnectable={isConnectable}
        />
          <div style={{width:"100%",background:"#BBB"}} className="custom-drag-handle">
            [SBM] {" "+data._.it.id}

          </div>
          {/* {data.it.type}<br/> */}
          


          <InspTargetUI_MUX 
              display={true} 
              style={{float:"left",width:"100%",height:"100%",overflow:"scroll",borderColor:"#AAA",borderStyle:"solid",borderWidth:"2px",borderRadius:"10px"}} 
              EditPermitFlag={0}
              key={"sdsdff"} 
              systemInspTarList={data._.defConfig}
              def={data._.it} 
              report={undefined} 
              fsPath={data._.defConfig.path+"/it_"+data._.it.id}
              renderHook={undefined} 
              onDefChange={(new_rule,doInspUpdate=true)=>{}}
              APIExport={(apis)=>{}}

              UIOption={undefined}
              showUIOptionConfigUI={false}
              onUIOptionUpdate={(newUIOption)=>{
                console.log(newUIOption)
              }}
            />




        <Handle
          type="source"
          position={Position.Bottom}
          id="a"
          style={{ bottom: "-5px", background: '#555' }}
          isConnectable={isConnectable}
        />
        </div>

      </>
    );
  });
  
  
const nodeTypes = {
    Orientation_ShapeBasedMatching: Orientation_ShapeBasedMatching_NodeUI,
};

function NodeFlow_DEMO({defConfig,nodeInfo,onNodesInfoChange,onNodeEvent}:{defConfig:any,nodeInfo:typeof initialNodes,onNodesInfoChange:(changes: typeof initialNodes) => void,onNodeEvent:(event:any)=>void}) {
    const [nodes, setNodes, _onNodesChange] = useNodesState(initialNodes);
    const [edges, setEdges, onEdgesChange] = useEdgesState(initialEdges);


    function onNodesChange(nodesChangeEvents:any)
    {
        let draggingEnd=nodesChangeEvents.find((it:any)=>it.dragging==false)!==undefined;
        if(draggingEnd)
        {
            // console.log("draggingEnd",draggingEnd,"onNodesChange",nodesChangeEvents,nodes);
            console.log("onNodesChange",nodesChangeEvents,nodes);
            onNodesInfoChange(nodes.map(node=>({...node,data:{...node.data,_:undefined}})));
        }
        onNodeEvent(nodesChangeEvents);
        _onNodesChange(nodesChangeEvents);



        // let new_nodeInfo=newNodes.map((node:any)=>({id:node.id,position:node.position,data:node.data}));
        // onNodesChange(newNodes);
        // onNodesInfoChange(newNodes);
    }

  
  
    const onConnect = useCallback((params) => setEdges((eds) => addEdge(params, eds)), [setEdges]);
    // console.log(defConfig,nodeInfo);
  
    useEffect(() => {
      let nodes=defConfig.InspTars_main.map((it:any,idx:any)=>
      {
        let foundNodeInfo=nodeInfo.find((node:any)=>node.id==it.id) as any;
        // if(foundNodeInfo!==undefined)return foundNodeInfo;
        // if(foundNodeInfo===undefined)foundNodeInfo={};


        let width=foundNodeInfo?.width;
        let height=foundNodeInfo?.height;
        let new_nodeInfo={
          id:it.id,
          position:{x:idx*5,y:idx*5},

          ...foundNodeInfo,
          type:(it.type=="Orientation_ShapeBasedMatching")?it.type:undefined,
          data:{label:it.id,_:{defConfig,it,idx,nodeInfo:
            foundNodeInfo}}
            ,

          style:foundNodeInfo?.style||{width:width,height:height},
          dragHandle:(it.type=="Orientation_ShapeBasedMatching")?'.custom-drag-handle':undefined,
          "resizing": false
        }





        return new_nodeInfo;
        }).filter((node:any)=>node.id!=="ImTran")
  

    //   console.log("nodes",nodes);
      let animated_edges={};
      let edges=defConfig.InspTars_main.map((it:any,idx:number,arr:any[])=>{
        let cid=it.id as string;
        let cand_it=arr
          .filter((sit:any)=>sit.match_tags.find((tag:string|string[])=>
          {
            if(tag==cid)return true;

            if(Array.isArray(tag))
            {   
                let subTagMatched=tag.find((subtag:string)=>subtag==cid)!==undefined;
                // console.log(cid,"subTagMatched",subTagMatched,tag);
                animated_edges[cid+'-'+sit.id]=true;
                return subTagMatched;
            }

            return false;
        })!==undefined)
        
        // console.log("               ",animated_edges);
        return cand_it
          .map((it:any)=>it.id)
          .map((id:string)=>{
            let edgeId=`${cid}-${id}`;
            // console.log("               ",id);
            return { id: edgeId, source:cid, target: id ,animated:animated_edges[edgeId]}
            })
      }).flat()
    //   console.log(nodes,edges);
      setNodes(nodes);
      setEdges(edges);
  
    },[defConfig,nodeInfo])
  
    // console.log(nodes);
    return (
      <ReactFlow
        nodes={nodes}
        edges={edges}
        onNodesChange={onNodesChange}
        onEdgesChange={onEdgesChange}
        onConnect={onConnect}
        nodeTypes={nodeTypes}
      >
        <MiniMap />
        <Controls />
        <Background />
      </ReactFlow>
    );
  }
  


export function DDDD({defConfig, nodeInfo,onNodesInfoChange, nodeUpdateMinInterval=500,onNodeEvent=(event)=>{}}: { defConfig: any, nodeInfo?: any[], onNodesInfoChange: (changes: any[]) => void,nodeUpdateMinInterval:number,onNodeEvent:(event:any)=>void }) {

    const [nodes, setNodes] = useState<any[]>([]);
    const _this = useRef<any>({}).current;

    useEffect(() => {
        setNodes(nodeInfo===undefined?[]:nodeInfo)
    }, [nodeInfo]);



    // console.log(nodes,nodeInfo);
    return <NodeFlow_DEMO defConfig={defConfig} nodeInfo={nodes} onNodesInfoChange={
        (new_nodeInfo: any) => {

            _this.debounce_Info_UPDATE=ID_debounce(_this.debounce_Info_UPDATE,()=>{
                onNodesInfoChange(new_nodeInfo);
            },()=>_this.debounce_Info_UPDATE=undefined,nodeUpdateMinInterval);


            setNodes(new_nodeInfo);
        }
    }
    onNodeEvent={onNodeEvent}
    
    />
}