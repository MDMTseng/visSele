
import React from 'react';
import { useState, useEffect, useRef,useLayoutEffect } from 'react';



export function useDivDimensions(divRef:React.RefObject<HTMLDivElement | undefined>) {
    const [dimensions, setDimensions] = useState([0, 0]);
  
  
    const _this = useRef({isInitSable:false,initQueryInterval:-1,bkClientRect:{width:-1,height:-1},bkDim:[0,0]}).current;
  
    useLayoutEffect(() => {
  
      function updateSize()
      {
        if (divRef.current) {
          _this.isInitSable=true;
          const { current } = divRef
          const boundingRect = current.getBoundingClientRect()
          const { width, height } = boundingRect
          if(width==0 && height==0)return;
          const rwh  = [  Math.round(width), Math.round(height) ]
          if(rwh[0]==_this.bkDim[0] &&rwh[1]==_this.bkDim[1])return;
          setDimensions(rwh)
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
  