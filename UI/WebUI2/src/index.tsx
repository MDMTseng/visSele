import React from 'react';
import ReactDOM from 'react-dom';
import App from './App';
import reportWebVitals from './reportWebVitals';
import 'antd/dist/antd.css'; 
import { Provider } from 'react-redux';

import { PointsFitCircle,intersectPoint, VEC2D} from './UTIL/MathTools';
import {ReduxStoreSetUp} from './redux/store';
import { ResultType } from 'antd/lib/result';


let StoreX = ReduxStoreSetUp({});
ReactDOM.render(
  <React.StrictMode>
    <Provider store={StoreX}>
      <App />
    </Provider>
  </React.StrictMode>,
  document.getElementById('root')
);

// If you want to start measuring performance in your app, pass a function
// to log results (for example: reportWebVitals(console.log))
// or send to an analytics endpoint. Learn more: https://bit.ly/CRA-vitals
reportWebVitals();



/*



let _YLocInfo=[
  {
      "Y_Loc": 179,
      "report": [
          {
              "idx": 0,
              "components": [
                {
                    "area": 409,
                    "x": 1361,
                    "y": 871
                },
                {
                    "area": 40,
                    "x": 2361,
                    "y": 271
                }
              ]
          },
          {
              "idx": 1,
              "components": [
                  {
                      "area": 463,
                      "x": 1130,
                      "y": 871
                  }
              ]
          },
          {
              "idx": 2,
              "components": [
                  {
                      "area": 1222,
                      "x": 906,
                      "y": 871
                  }
              ]
          },
          {
              "idx": 3,
              "components": [
                  {
                      "area": 1110,
                      "x": 676,
                      "y": 871
                  }
              ]
          }
      ]
  },
  {
      "Y_Loc": 179.5,
      "report": [
          {
              "idx": 0,
              "components": [
                  {
                      "area": 400,
                      "x": 1372,
                      "y": 872
                  }
              ]
          },
          {
              "idx": 1,
              "components": [
                  {
                      "area": 315,
                      "x": 1149,
                      "y": 868
                  }
              ]
          },
          {
              "idx": 2,
              "components": [
                  {
                      "area": 1216,
                      "x": 917,
                      "y": 871
                  }
              ]
          },
          {
              "idx": 3,
              "components": [
                  {
                      "area": 1101,
                      "x": 688,
                      "y": 871
                  }
              ]
          }
      ]
  },
  {
      "Y_Loc": 180,
      "report": [
          {
              "idx": 0,
              "components": [
                  {
                      "area": 444,
                      "x": 1384,
                      "y": 872
                  }
              ]
          },
          {
              "idx": 1,
              "components": [
                  {
                      "area": 628,
                      "x": 1158,
                      "y": 870
                  }
              ]
          },
          {
              "idx": 2,
              "components": [
                  {
                      "area": 1253,
                      "x": 929,
                      "y": 871
                  }
              ]
          },
          {
              "idx": 3,
              "components": [
                  {
                      "area": 1107,
                      "x": 699,
                      "y": 870
                  }
              ]
          }
      ]
  },
  {
      "Y_Loc": 180.5,
      "report": [
          {
              "idx": 0,
              "components": [
                  {
                      "area": 476,
                      "x": 1396,
                      "y": 872
                  }
              ]
          },
          {
              "idx": 1,
              "components": [
                  {
                      "area": 783,
                      "x": 1168,
                      "y": 872
                  }
              ]
          },
          {
              "idx": 2,
              "components": [
                  {
                      "area": 1244,
                      "x": 941,
                      "y": 871
                  }
              ]
          },
          {
              "idx": 3,
              "components": [
                  {
                      "area": 1105,
                      "x": 711,
                      "y": 870
                  }
              ]
          }
      ]
  },
  {
      "Y_Loc": 181,
      "report": [
          {
              "idx": 0,
              "components": [
                  {
                      "area": 517,
                      "x": 1406,
                      "y": 873
                  }
              ]
          },
          {
              "idx": 1,
              "components": [
                  {
                      "area": 829,
                      "x": 1179,
                      "y": 872
                  }
              ]
          },
          {
              "idx": 2,
              "components": [
                  {
                      "area": 1244,
                      "x": 952,
                      "y": 871
                  }
              ]
          },
          {
              "idx": 3,
              "components": [
                  {
                      "area": 1085,
                      "x": 722,
                      "y": 871
                  }
              ]
          }
      ]
  }
]


let _R11Info=[
  {
      "R11_angle": -7,
      "report": [
          {
              "idx": 0,
              "components": [
                  {
                      "area": 454,
                      "x": 1385,
                      "y": 897
                  }
              ]
          },
          {
              "idx": 1,
              "components": [
                  {
                      "area": 860,
                      "x": 1160,
                      "y": 925
                  }
              ]
          },
          {
              "idx": 2,
              "components": [
                  {
                      "area": 1131,
                      "x": 934,
                      "y": 952
                  }
              ]
          },
          {
              "idx": 3,
              "components": [
                  {
                      "area": 1036,
                      "x": 706,
                      "y": 980
                  }
              ]
          }
      ]
  },
  {
      "R11_angle": -3.5,
      "report": [
          {
              "idx": 0,
              "components": [
                  {
                      "area": 424,
                      "x": 1383,
                      "y": 884
                  }
              ]
          },
          {
              "idx": 1,
              "components": [
                  {
                      "area": 532,
                      "x": 1158,
                      "y": 900
                  }
              ]
          },
          {
              "idx": 2,
              "components": [
                  {
                      "area": 1183,
                      "x": 930,
                      "y": 913
                  }
              ]
          },
          {
              "idx": 3,
              "components": [
                  {
                      "area": 1042,
                      "x": 701,
                      "y": 926
                  }
              ]
          }
      ]
  },
  {
      "R11_angle": 0,
      "report": [
          {
              "idx": 0,
              "components": [
                  {
                      "area": 423,
                      "x": 1383,
                      "y": 873
                  }
              ]
          },
          {
              "idx": 1,
              "components": [
                  {
                      "area": 619,
                      "x": 1158,
                      "y": 872
                  }
              ]
          },
          {
              "idx": 2,
              "components": [
                  {
                      "area": 1202,
                      "x": 929,
                      "y": 873
                  }
              ]
          },
          {
              "idx": 3,
              "components": [
                  {
                      "area": 1098,
                      "x": 699,
                      "y": 873
                  }
              ]
          }
      ]
  },
  {
      "R11_angle": 3.5,
      "report": [
          {
              "idx": 0,
              "components": [
                  {
                      "area": 467,
                      "x": 1385,
                      "y": 860
                  }
              ]
          },
          {
              "idx": 1,
              "components": [
                  {
                      "area": 862,
                      "x": 1157,
                      "y": 847
                  }
              ]
          },
          {
              "idx": 2,
              "components": [
                  {
                      "area": 1068,
                      "x": 929,
                      "y": 836
                  }
              ]
          },
          {
              "idx": 3,
              "components": [
                  {
                      "area": 1104,
                      "x": 701,
                      "y": 821
                  }
              ]
          }
      ]
  },
  {
      "R11_angle": 7,
      "report": [
          {
              "idx": 0,
              "components": [
                  {
                      "area": 485,
                      "x": 1385,
                      "y": 848
                  }
              ]
          },
          {
              "idx": 1,
              "components": [
                  {
                      "area": 844,
                      "x": 1158,
                      "y": 821
                  }
              ]
          },
          {
              "idx": 2,
              "components": [
                  {
                      "area": 1158,
                      "x": 933,
                      "y": 793
                  }
              ]
          },
          {
              "idx": 3,
              "components": [
                  {
                      "area": 1093,
                      "x": 706,
                      "y": 765
                  }
              ]
          }
      ]
  }
]

let sysInfo = ReportCalcSysInfo(_R11Info,_YLocInfo);
console.log(sysInfo);

let adjInfoG=[0,1,2,3].map(idx=>SysInfoCalcCompensation(sysInfo,-1,idx)).map(info=>({...info,theda360:info.theda*180/Math.PI}));

console.log(adjInfoG);

*/