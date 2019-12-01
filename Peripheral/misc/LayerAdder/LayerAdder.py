import zipfile
import re
from PIL import Image 
import io

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

archive_floor = zipfile.ZipFile('floor.zip', 'r')
gcode_floor = archive_floor.read('run.gcode')
gcode_floor_Info=LayerParse(gcode_floor)


archive_printObj = zipfile.ZipFile('_20x20x20.zip', 'r')
gcode_printObj = archive_printObj.read('run.gcode')
gcode_printObj_Info=LayerParse(gcode_printObj)

nameX = getLayerSection_image_name(gcode_floor_Info,200)
print(nameX)
data = archive_floor.read(nameX)
dataEnc = io.BytesIO(data)
img = Image.open(dataEnc)
img.show()



print(getLayerSection_height(gcode_floor_Info,len(gcode_floor_Info["layer_indices"])-1))
print(getLayerSection_height(gcode_printObj_Info,len(gcode_printObj_Info["layer_indices"])-1))



print(gcode_floor_Info["gcode_end_idx"])
print(getLayerSection(gcode_floor_Info,200))