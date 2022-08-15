
# InspTar cycle
```


_________________________________________
|          V                V          <| (the dispatched stageInfo will 
|          V                V          ^| do filter In each InspTar in list
|          V                V          ^| and stage in each accepted InspTar)
|   ____________       ____________    ^| 
|  |   Filter   |     |   Filter   |   ^| 
|__|____________|_____|____________|___^| --call inspTarProcess
|  |            |     |            |   ^|   will call every InspTar
|  |            |     |            |   ^|   as a thread to process 
|  | InspTar A  |     |InspTar B   |   ^|   and wai for all InspTar ends
|  |____________|     |____________|   ^|   Then go over again
|    dispatch   stageInfo[s]     >>    ^|
|                                       |
|InspTarManager ________________＿______| ---------^


src =dispatch> {StageInfo} 





```







# Measure inspection
```
camera 
          ={StageInfo_Image}> 
lens correction Process
...
          ={StageInfo_Image}> 
Locating
          ={StageInfo_Orientation}> 
feature matching
          ={StageInfo_feature}> 
feature measure
          ={StageInfo_Measure}> 
Collects
          ={StageInfo_CAT}> 
          ={}>Transfer

```

# surface check
```
camera 
          ={StageInfo_Image}> 
lens correction Process
...
          ={StageInfo_Image}> 
Locating
          ={StageInfo_Orientation}> 
surface insp
          ={StageInfo_Measure}> 
Collects
          ={StageInfo_CAT}> 
          ={}>Transfer
```