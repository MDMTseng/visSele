

import React, { useState, useEffect, useRef ,createRef,useLayoutEffect} from 'react';

import { Layout,Button,Tabs,Slider,Menu, Divider,Dropdown,Popconfirm,Radio, InputNumber, Switch,Select } from 'antd';
import { Row, Col,Input,Tag,Modal,message,Space } from 'antd';
import { UserOutlined, LaptopOutlined, NotificationOutlined,DownOutlined,
  DisconnectOutlined,LinkOutlined } from '@ant-design/icons';
import {useDivDimensions} from "./UTIL/ReactUtils"
import { GetObjElement, ID_debounce, ID_throttle, ObjShellingAssign } from './UTIL/MISC_Util';

import 'react-grid-layout/css/styles.css'
import { Responsive, WidthProvider } from "react-grid-layout";
const ResponsiveReactGridLayout = WidthProvider(Responsive);







// export function DraggableGridLayout_DEMO ({ children,layouts,cols }:
//   {
//     children:React.ReactNode,
//     layouts?: ReactGridLayout.Layouts | undefined,
//     cols?:{[P: string]: number}|undefined
//   })  {



export function ResponsiveReactGridLayoutX (props:
  ReactGridLayout.ResponsiveProps & ReactGridLayout.WidthProviderProps & {rows:number})  {

  const _this = useRef<any>({}).current;
  const divRef = useRef<HTMLDivElement>(null)
  const [width, height] = useDivDimensions(divRef);

  
  useLayoutEffect(() => {

    _this.trigTO=
    ID_throttle(_this.trigTO,()=>{
      window.dispatchEvent(new Event('resize'));
    },()=>_this.trigTO=undefined,500);


    console.log(height,props.rows);
  }, [width, height]);

  
    const onDrop = (event: any) => {
      console.log(event);
    };
  
    return (
      <div className="App" style={{width:"100%",height:"100%",overflow:"hidden"}}  ref={divRef}>
        <ResponsiveReactGridLayout
          {...props}
          rowHeight={(height)/props.rows-10}
          //  onDrop={(e) => onDrop(e)} 
          //  onLayoutChange={(curL,allL)=>{
          //   console.log(curL,allL)
          //  }}
          //  className="layout" 
          //  layouts={layouts} 
          //  breakpoints={{ lg: 1200, md: 996, sm: 768, xs: 480, xxs: 0 }}
          //  cols={{ lg: 5, md: 5, sm: 5, xs: 5, xxs: 5 }}
          //  // rowHeight={300}
          //  // width={1000}
          //  resizeHandles={["se"]}
          //  isDroppable={true}
          // //  autoSize={true}
          //  onWidthChange={(containerWidth,margin,cols,cpadding)=>{
          //   console.log(containerWidth,margin,cols,cpadding);
          //  }}
          
           >

          {/* <div key="a" style={{ backgroundColor: "#ccc" }}><span>a</span></div>
          <div key="b" style={{ backgroundColor: "#ccc" }}>b</div>
          <div key="c" style={{ backgroundColor: "#ccc" }}>c</div>
          <div key="d" style={{ backgroundColor: "#ccc" }}>d</div> */}
        </ResponsiveReactGridLayout>
      </div>
    );
  }
  


