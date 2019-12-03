import zipfile
import re
from PIL import Image , ImageChops 
import io



import os

def createFolder(directory):
  try:
    if not os.path.exists(directory):
      os.makedirs(directory)
  except OSError:
    print ('Error: Creating directory. ' +  directory)
      

def getLayerSection(gcode_parsed,layer_idx):
  if(layer_idx<0 or layer_idx>=len(gcode_parsed["layer_indices"])):
    return None

  layer_line_idx = gcode_parsed["layer_indices"][layer_idx]
  next_layer_line_idx=-1
  if(layer_idx == len(gcode_parsed["layer_indices"])-1):
    next_layer_line_idx=gcode_parsed["gcode_end_idx"]
  else:
    next_layer_line_idx=gcode_parsed["layer_indices"][layer_idx+1]

  return gcode_parsed["gcode_line_arr"][layer_line_idx:next_layer_line_idx]

def LayerParse(gcode):
  gcode_line_arr = gcode.splitlines()
  layer_indices=[]
  idx=0
  for line in gcode_line_arr:
    if b"LAYER_START:" in line:
      layer_indices.append(idx)
    idx+=1
  
  lastLayerIdx=layer_indices[len(layer_indices)-1]

  gcode_end_idx=-1
  for idx in range(lastLayerIdx,len(gcode_line_arr)):
    line=gcode_line_arr[idx]
    if b"END_GCODE_BEGIN" in line:
      gcode_end_idx=idx
      break

  return {'gcode': gcode, 'gcode_line_arr': gcode_line_arr, 
  'layer_indices':layer_indices,
  "gcode_end_idx":gcode_end_idx,
  "last_line_idx":len(gcode_line_arr)-1}




def getLayerSection_image_name(gcode_parsed,layer_idx):
  section = getLayerSection(gcode_parsed,layer_idx)
  if(section == None):
    return None
  for line in section:
    if b"M6054" in line:
      return re.search(r'\"(.+)\"', line.decode("utf-8") ).group(1)
  return None


def getLayerSection_height(gcode_parsed,layer_idx):
  section = getLayerSection(gcode_parsed,layer_idx)
  if(section == None):
    return None
  idx=0
  for line in section:
    if b"G4 P" in line:
      pLine=section[idx-1]
      return float(re.search(r'Z(.+) F', pLine.decode("utf-8") ).group(1))
    idx+=1
  return None



def floorIdxJump(floor_gcode_parsed,floor_clear_top_layer_idx,floor_clear_btn_layer_idx,object_gcode_parsed,object_layer_idx):
  if(object_layer_idx<floor_clear_btn_layer_idx):return object_layer_idx
  object_layer_count = len(object_gcode_parsed["layer_indices"])
  if(object_layer_count<floor_clear_top_layer_idx):
    return object_layer_idx+floor_clear_top_layer_idx-object_layer_count

  return object_layer_idx


archive_floor = zipfile.ZipFile('floor.zip', 'r')
gcode_floor = archive_floor.read('run.gcode')
gcode_floor_Info=LayerParse(gcode_floor)
floor_clear_top_layer_idx=786
floor_clear_btn_layer_idx=20




archive_printObj = zipfile.ZipFile('_20x20x20.zip', 'r')
gcode_printObj = archive_printObj.read('run.gcode')
gcode_printObj_Info=LayerParse(gcode_printObj)





directory="output"
createFolder(directory)

maxIdx=1000
for idx in range(0,maxIdx):

  img_floor=None
  img_printObj=None
  floor_idx = floorIdxJump(gcode_floor_Info,floor_clear_top_layer_idx,floor_clear_btn_layer_idx,gcode_printObj_Info,idx)

  name_floor = getLayerSection_image_name(gcode_floor_Info,floor_idx)
  if(name_floor!=None):
    data = archive_floor.read(name_floor)
    dataEnc = io.BytesIO(data)
    img_floor = Image.open(dataEnc)
  

  name_printObj = getLayerSection_image_name(gcode_printObj_Info,idx)
  if(name_printObj!=None):
    data2 = archive_printObj.read(name_printObj)
    dataEnc2 = io.BytesIO(data2)
    img_printObj = Image.open(dataEnc2)
  
  img_mix=None
  if(img_floor!=None and img_printObj!=None):
    img_mix = ImageChops.logical_or(img_floor.convert("1") , img_printObj.convert("1") )#.convert("P")
  elif(img_floor!=None):
    img_mix = img_floor
  elif(img_printObj!=None):
    img_mix = img_printObj
  else:
    break

  print(str(idx*100//maxIdx)+"%. idx:"+str(idx)+"  floor_idx:"+str(floor_idx))
  #im3.show()
  img_mix.save( directory+"/"+str(idx+1)+".png", "png" )



print(getLayerSection_height(gcode_floor_Info,len(gcode_floor_Info["layer_indices"])-1))
print(getLayerSection_height(gcode_printObj_Info,len(gcode_printObj_Info["layer_indices"])-1))



print(gcode_floor_Info["gcode_end_idx"])
print(getLayerSection(gcode_floor_Info,200))