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
from datetime import datetime

path_env=os.path.abspath("./")
path_local=os.path.abspath(sys.argv[0])

print("path_env=",path_env)
print("path_local=",path_local)


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
    

def exe_update_file(update_info,file_dir_path="./"):


  if not os.path.isdir(update_info["old_exeDir"]):
    return False

  file_name=file_dir_path+path_leaf(update_info["update_URL"])


  #Step 1, download new update to tmp folder
  tmp_folder=file_dir_path+"/TMP"
  
  shutil.rmtree(tmp_folder, ignore_errors=True)

  os.makedirs(tmp_folder, exist_ok=True)

  if(not fileDownload_w_progress(update_info["update_URL"],tmp_folder+"/"+file_name)==True):
    return False
  with zipfile.ZipFile(tmp_folder+"/"+file_name) as zf:
    zf.extractall(tmp_folder)

  tmp_binary_folder=os.path.splitext(tmp_folder+"/"+file_name)[0]


  #Step 2, update validation
  #assume it's a pass



  #Step 3, put current binary to backup folder

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


  #Clean up tmp_folder

  shutil.rmtree(tmp_folder, ignore_errors=True)



  # print(file_dir_path, update_info["exeDir"],file_name)
  # shutil.move(os.path.splitext(file_name)[0], update_info["exeDir"]) 
  # #shutil.rmtree(file_name, ignore_errors=True)
  # os.remove(file_name)  

  return True

async def recv_msg(websocket):
  termination=False
  while True:
    try:
      recv_text = await websocket.recv()
      print("recv_text:",recv_text) 
      recv_json = json.loads(recv_text)
      print("recv_text:",recv_text, "json:",recv_json) 

      cmd_id=recv_json.get("cmd_id", None)
      _type=recv_json.get("type", None)

      ACK=False
      if _type == "update":
        update_info=update_param_check(recv_json)
        if update_info is not None:
          ACK=exe_update_file(update_info)
      elif _type == "check_version":
        pass
      elif _type == "launch_core":
        subprocess.call('core/visele')

      elif _type == "launch_program":
        subprocess.call('core/visele')

      elif _type == "EXIT":
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

def main():
    try:
        opts, args = getopt.getopt(sys.argv[1:], "ho:v", 
        [
          'packUrl=',
          'exeDir=',
          'packPath=', 
          'old_exeDir=', 
          'old_exeDir_newName='
        ])
    except getopt.GetoptError as err:
        # print help information and exit:
        print("ERROR") 
        #print(err)  # will print something like "option -a not recognized"
        #usage()
        sys.exit(2)

    print(opts, args) 

    exeDir=""
    old_exeDir=""
    old_exeDir_newName=""
    packPath=""
    for o, a in opts:

      print(o) 
      if o == "--packPath":
        packPath=a
      if o == "--exeDir":
        exeDir=a
      elif o == "--old_exeDir":
        #usage()
        old_exeDir=a
      elif o == "--old_exeDir_newName":
        old_exeDir_newName=a
    

    print(packPath,exeDir, old_exeDir,old_exeDir_newName)

    shutil.rmtree(exeDir, ignore_errors=True)
    with zipfile.ZipFile(packPath) as zf:
      zf.extractall(exeDir)
    # ...

if __name__ == "__main__":

  start_server = websockets.serve(main_logic, 'localhost', 5678)
  loop = asyncio.get_event_loop()
  loop.run_until_complete(start_server)
  loop.run_forever()
