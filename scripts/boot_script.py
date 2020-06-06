import getopt, sys
import zipfile
import shutil
import urllib.request
import websockets
import asyncio
import json
import subprocess
import ntpath
import os
from time import sleep
from datetime import datetime

path_env=os.path.abspath("./")
path_local=os.path.abspath(sys.argv[0])

print("path_env=",path_env)
print("path_local=",path_local)


WebUI_Path=path_env+"/WebUI"
Core_Path=path_env+"/Core"


def path_leaf(path):
    head, tail = ntpath.split(path)
    return tail or ntpath.basename(head)

def fileDownload_w_progress(url,file_path,progressCount=5):

  try:
    resp = urllib.request.urlopen(url)
  except urllib.error.HTTPError as err:
    return False
  length = resp.getheader('content-length')
  if length:
    length = int(length)
    blocksize = length//progressCount
  else:
    return False

  print(length, blocksize)

  with  open(file_path, 'wb') as out_file:
    size = 0
    while True:
      buf1 = resp.read(blocksize)
      if not buf1:
        break
      out_file.write(buf1)
      size += len(buf1)
      if length:  
        print('{:.2f}\r done'.format(size/length), end='')
  
  return True


def parse_argv(argv):
  try:
    opts, args = getopt.getopt(argv[1:], "", 
    [
      'exeDir=',
      'old_exeDir=',
      'old_exeDir_bkName=', 
      'update_URL=', 
      'packPath=',

      'type=',
      'pre_wait_ms=',
      'port=',
      'env_path='
    ])
  except getopt.GetoptError as err:
    # print help information and exit:
    print("ERROR",err) 
    #print(err)  # will print something like "option -a not recognized"
    #usage()
    return None

  print(opts, args) 
  obj={}
  for o, a in opts:
    key=o[2:]
    obj[key]=a

  return obj


def update_param_check(obj):
  update_info={}
  try:
    obj["exeDir"]
    obj["old_exeDir"]
    obj["old_exeDir_bkName"]
    obj["update_URL"]
    obj["packPath"]
  except Exception as inst:
    print(type(inst))       # the exception instance
    print(inst.args)        # arguments stored in .args
    print(inst) 
    return None
  return obj
  #fileDownload_w_progress("https://github.com/MDMTseng/visSele/releases/download/pre_v1.0_hotfix-2/release_export.zip","release_export.zip")
    
#{"type":"update", "exeDir":"Xception", "old_exeDir":"Xception", 
# "old_exeDir_bkName":"Xception", "packPath":"", 
# "update_URL":"https://github.com/MDMTseng/visSele/releases/download/pre_v1.0_hotfix-2/release_export.zip", 
# "cmd_id":3}
def exe_update_file(update_info,file_dir_path="./"):

  print(update_info)
  CC=0

  doPackageBK=True
  if not os.path.isdir(update_info["old_exeDir"]):
    doPackageBK=False

  file_name=file_dir_path+path_leaf(update_info["update_URL"])


  #Step 1, download new update to tmp folder
  tmp_folder=file_dir_path+"/TMP"
  
  shutil.rmtree(tmp_folder, ignore_errors=True)

  os.makedirs(tmp_folder, exist_ok=True)

  if(not fileDownload_w_progress(update_info["update_URL"],tmp_folder+"/"+file_name)==True):
    return -1
  with zipfile.ZipFile(tmp_folder+"/"+file_name) as zf:
    zf.extractall(tmp_folder)

  tmp_binary_folder=os.path.splitext(tmp_folder+"/"+file_name)[0]


  #Step 2, update validation
  #assume it's a pass
  ret=os.system("cd "+tmp_binary_folder+"; python scripts/boot_script.py --type=validation ")
  print("CALL:",ret)
  if(ret):
    return -5
  #Step 3, put current binary to backup folder
  if(doPackageBK):
    backup_folder=file_dir_path+"BK"

    os.makedirs(backup_folder, exist_ok=True)
    bk_dst=backup_folder+"/"+update_info["old_exeDir_bkName"]
    retryC=0
    while True:
      if(retryC!=0):
        tmp_name=bk_dst+"_"+str(retryC)
      else:
        tmp_name=bk_dst
      retryC+=1
      print(tmp_name+"\n")
      if not os.path.isdir(tmp_name):
        bk_dst=tmp_name
        break
      
    dest = shutil.move(file_dir_path+"/"+update_info["old_exeDir"], bk_dst) #move current package to BK folder


  #Step 3.1 move newly updated binary to cur_local path

  shutil.move(tmp_binary_folder, file_dir_path+"/"+update_info["exeDir"]) 

  print("CC:",CC)
  CC+=1

  #Clean up tmp_folder

  shutil.rmtree(tmp_folder, ignore_errors=True)

  shutil.move(file_dir_path+"/"+update_info["exeDir"]/scripts/boot_script.py, 
    path_local) 


  # print(file_dir_path, update_info["exeDir"],file_name)
  # shutil.move(os.path.splitext(file_name)[0], update_info["exeDir"]) 
  # #shutil.rmtree(file_name, ignore_errors=True)
  # os.remove(file_name)  

  return 0


def cmd_exec(cmd):

  _type=cmd.get("type", None)
  ACK=-10
  if _type == "update":
    update_info=update_param_check(cmd)
    if update_info is not None:
      ACK=exe_update_file(update_info)
    #if(ACK==0):
      #replace the boot_script
      
  elif _type == "validation":
    # print("os.path.isdir("+Core_Path+"):",os.path.isdir(Core_Path))
    # print("os.path.isdir("+WebUI_Path+"):",os.path.isdir(WebUI_Path))
    if os.path.isdir(WebUI_Path) and os.path.isdir(Core_Path):
      ACK=0
  elif _type == "launch_core":

    env_path=cmd.get("env_path", "./")

    ret=os.system("cd "+env_path+"; "+Core_Path+'/visSele&')
    #os.spawnl(os.P_DETACH,"cd "+env_path+"; "+Core_Path+'/visSele')
  elif _type == "rerun":
    pass

  elif _type == "EXIT":
    termination=True
    ACK=0

  
  return ACK


async def recv_msg(websocket):
  termination=False
  while True:
    try:
      recv_text = await websocket.recv()
      print("recv_text:",recv_text) 
      recv_json = json.loads(recv_text)
      print("recv_text:",recv_text, "json:",recv_json) 


      cmd_id=recv_json.get("cmd_id", None)
      ret_err=cmd_exec(recv_json)
      if(ret_err == 0):
        ACK = True
      else:
        ACK = False

      _type=recv_json.get("type", None)

      if _type == "EXIT":
        termination=True
        ACK=True

      ret_json=json.dumps({'ACK': ACK, 'cmd_id': cmd_id})
      response_text = ret_json
      await websocket.send(response_text)
      if termination:
        websocket.close()
        break

    except 2:
      print("ERROR") 
      break


async def main_logic(websocket, path):
  try:
    print("go await") 
    await recv_msg(websocket)
  except getopt.GetoptError as err:
    # print help information and exit:
    print("ERROR") 
    #print(err)  # will print something like "option -a not recognized"
    #usage()
    sys.exit(2)
  
  asyncio.get_event_loop().stop()


def fileDownload(url,file_path):
    with urllib.request.urlopen(url) as response, open(file_path, 'wb') as out_file:
        shutil.copyfileobj(response, out_file)


if __name__ == "__main__":

  obj= parse_argv(sys.argv)
  _type=obj.get("type", None)
  if _type == "websocket-server":
  
    try:
      pre_wait_ms=int(obj.get("pre_wait_ms", ""))
      print("pre_wait_ms:",pre_wait_ms," ms")
      sleep(pre_wait_ms/1000.0)
    except ValueError:
      pass
    
    port = 5678
    try:
      port=int(obj.get("port", ""))
      print("port:",port)
    except ValueError:
      pass


    print("server Open: 'localhost',",port)
    start_server = websockets.serve(main_logic, 'localhost', port)
    loop = asyncio.get_event_loop()
    loop.run_until_complete(start_server)
    loop.run_forever()
  elif  _type is not None:
    retx=cmd_exec(obj)
    print(retx)
    sys.exit(retx)
  else:
    sys.exit(-33)


