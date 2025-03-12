import {UtilUI_JsAPP} from "./UtilUI_JsAPP"
import {CompParam_UtilUI,CompParam_UIOption} from "./SingleTargetVIEWUI_UTIL"
import { useMemo } from 'react';

const COMPONENT_MAP = {
    "JsAPP":UtilUI_JsAPP
}
type UtilUIType = keyof typeof COMPONENT_MAP;

function isValidUtilUIType(type: string): type is UtilUIType {
    return type in COMPONENT_MAP;
}


// 5. Export available types for external use (optional)
const UtilUI_TYPES = Object.keys(COMPONENT_MAP) as UtilUIType[];


// 2. Modify the UtilUI_MUX component to use a key for forcing re-renders
function UtilUI_MUX(param: CompParam_UtilUI & CompParam_UIOption) {
    const Component = useMemo<React.ComponentType<CompParam_UtilUI & CompParam_UIOption> | null>(() => {
        const type = param?.UIOption?.ittype;
        
        if (!type || !isValidUtilUIType(type)) {
            console.warn(`Invalid or missing component type: ${type}`);
            return null;
        }

        return COMPONENT_MAP[type as UtilUIType];
    }, [param?.UIOption?.ittype]);

    if (!Component) {
        return null;
    }

    // Add key prop to force re-render on HMR
    return <Component {...param} />;
}
// function UtilUI_MUX(param: CompParam_UtilUI & CompParam_UIOption) {
//   const type = param?.UIOption?.ittype;
  

//   switch (type) {
//       case 'JsAPP':
//           return <UtilUI_JsAPP {...param} />;
//       default:
//           return null;
//   }
// }



export {UtilUI_TYPES,UtilUI_MUX}