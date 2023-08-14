
import React from 'react';
import { useState, useEffect, useRef,useLayoutEffect } from 'react';

import { GetObjElement, ID_debounce, ID_throttle, ObjShellingAssign } from './MISC_Util';



export function useDivDimensions(divRef:React.RefObject<HTMLDivElement | undefined>) {
    const [dimensions, setDimensions] = useState([0, 0]);
  
  
    const _this = useRef<any>({
      isInitSabled:false,
      bkDim:[0,0],
      debounce_id:-1}).current;
  
    useLayoutEffect(() => {
  
      function updateSize()
      {
        if (divRef.current) {
          const { current } = divRef
          const boundingRect = current.getBoundingClientRect()
          const { width, height } = boundingRect
          if(width==0 && height==0)return;
          const rwh  = [  Math.round(width), Math.round(height) ]
          if(rwh[0]==_this.bkDim[0] &&rwh[1]==_this.bkDim[1])return;

          //console.log(">>>");
          _this.debounce_id =
          ID_debounce(_this.debounce_id, () => {

            _this.isInitSabled=true;
            const boundingRect = current.getBoundingClientRect()
            const { width, height } = boundingRect
            const rwh  = [  Math.round(width), Math.round(height) ]
            //console.log("updateSize",rwh);
            setDimensions(rwh)
            _this.bkDim=rwh;
          }, () => _this.debounce_id = undefined,_this.isInitSabled?100:100,false);



          _this.bkDim=rwh;
        }
      }
  
      const ro =new ResizeObserver(updateSize);
      
      ro.observe(divRef.current as Element);
      return () =>{
        console.log("REMOVE....");
        ro.unobserve(divRef.current as Element);
      }
    }, []);
    return dimensions;
  }
  