import getopt, sys
import zipfile
import shutil
import urllib.request
import websockets
import asyncio
import json
import subprocess
import ntpath
import signal
import os
from time import sleep
from datetime import datetime
import platform

path_env=os.path.abspath("./")
path_script=os.path.abspath(sys.argv[0])
path_local=os.path.dirname(os.path.dirname(path_script))

print("path_env=",path_env)
print("path_script=",path_script)
print("path_local=",path_local)


BIN_DIR="Xception"



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
      'bk_name_append=', 
      'update_URL=', 

      'type=',
      'pre_wait_ms=',
      'port=',
      'env_path=',
      'dst_dir='
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

def ddd():
  pass

def update_param_check(obj):
  update_info={}
  try:
    obj["bk_name_append"]
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
def exe_update_file(update_info,file_dir_path=""):

  print(update_info)
  CC=0

  doPackageBK=True
  if not os.path.isdir(BIN_DIR):
    doPackageBK=False


  #Step 1, download new update to tmp folder
  tmp_folder=file_dir_path+"/TMP"
  
  shutil.rmtree(tmp_folder, ignore_errors=True)

  os.makedirs(tmp_folder, exist_ok=True)

  if "update_URL" in update_info:
    upzip_path=update_info["update_URL"]
  else:
    upzip_path=None
  if(upzip_path is not None and upzip_path.startswith( 'http' )):
    file_name=file_dir_path+path_leaf(upzip_path)
    if(not fileDownload_w_progress(upzip_path,tmp_folder+"/"+file_name)==True):
      return -1
    with zipfile.ZipFile(tmp_folder+"/"+file_name) as zf:
      zf.extractall(tmp_folder)
  else:
    if(upzip_path is None):
      upzip_path=path_env
      
    if os.path.isdir(upzip_path):
      upzip_path+="/update.zip"
    
    if not os.path.isfile(upzip_path):
      return -2

    file_name="update"
    dest = shutil.move(upzip_path, tmp_folder+"/"+file_name+".zip") 
    
    try:
      with zipfile.ZipFile(tmp_folder+"/"+file_name+".zip") as zf:
        zf.extractall(tmp_folder)
    except zipfile.BadZipFile:
      return -3
    
    #check if the path_env has an update.zip
    
    

  tmp_binary_folder=os.path.splitext(tmp_folder+"/"+file_name)[0]

  print("TTT::",tmp_folder,file_name,tmp_binary_folder)

  #Step 2, update validation
  #assume it's a pass
  #ret=os.system("cd "+tmp_binary_folder+"; python scripts/boot_script.py --type=validation ")
  
  shutil.copy2( tmp_binary_folder+"/scripts/boot_script.py", tmp_folder)
  ret = subprocess.call(["python", tmp_folder+"/boot_script.py", "--type=validation"], cwd=tmp_binary_folder)
  print("CALL:",ret)
  if(ret!= 0):
    return -5

  #Step 3, put current binary to backup folder
  # if(doPackageBK):
  #   backup_folder=file_dir_path+"BK"

  #   os.makedirs(backup_folder, exist_ok=True)
  #   bk_dst=backup_folder+"/"+BIN_DIR+update_info["bk_name_append"]
  #   retryC=0
  #   while True:
  #     if(retryC!=0):
  #       tmp_name=bk_dst+"_"+str(retryC)
  #     else:
  #       tmp_name=bk_dst
  #     retryC+=1
  #     print(tmp_name+"\n")
  #     if not os.path.isdir(tmp_name):
  #       bk_dst=tmp_name
  #       break
      
  #   dest = shutil.move(file_dir_path+"/"+BIN_DIR, bk_dst) #move current package to BK folder


    
  #Step 3.1 move newly updated binary to cur_local path
  ret = subprocess.call(["python", tmp_folder+"/boot_script.py", "--type=deploy", '--dst_dir='+os.path.abspath(file_dir_path)+"/"], cwd=tmp_binary_folder)

  print("CALL:",ret)
  if(ret!= 0):
    return -6
  #shutil.move(tmp_binary_folder, file_dir_path+"/"+update_info["exeDir"])  #don't do deploy here

  #print("CC:",CC)
  #CC+=1

  #Clean up tmp_folder

  shutil.rmtree(tmp_folder, ignore_errors=True)

  # shutil.move(file_dir_path+"/"+update_info["exeDir"]+"/scripts/boot_script.py", 
  #   path_local) 


  return 0



def gen_avaliable_new_folder_name(path):
  retryC=0
  path_Name=path
  while True:
    if(retryC!=0):
      tmp_name=path_Name+"_"+str(retryC)
    else:
      tmp_name=path_Name
    retryC+=1
    print(tmp_name+"\n")
    if not os.path.isdir(tmp_name):
      path_Name=tmp_name
      break
  return path_Name



CORE_PIPE=None
def cmd_exec(cmd):
  infoObj={}
  _type=cmd.get("type", None)
  ACK=-10
  if _type == "update":
    update_info=update_param_check(cmd)
    if update_info is not None:
      ACK=exe_update_file(update_info,path_env)
      if(ACK==0):
        infoObj
      
  elif _type == "un_deploy":#in case of deploy error, roll back
    pass
    
  elif _type == "deploy":
    print("dst_dir:",cmd["dst_dir"])
    if os.path.isdir(WebUI_Path) and os.path.isdir(Core_Path):
      if(platform.system()!="Windows"):
        ACK=os.system("chmod +x "+Core_Path+"/visSele")
      availPath = gen_avaliable_new_folder_name(cmd["dst_dir"]+"/"+BIN_DIR)
      availPath = availPath.replace('\\', "/")

      common_prefix = os.path.commonprefix([path_env, availPath])
      print("path_env:",path_env,"  availPath:",availPath)
      print("Before rel availPath:",availPath)
      rel_availPath = os.path.relpath(availPath, common_prefix)
      print("After  rel availPath:",rel_availPath)

      with open("scripts/router_template.py", "rt") as fin:
        with open(cmd["dst_dir"]+"/router.py", "wt") as fout:
            for line in fin:
                fout.write(line.replace('$TARGET_PATH_NAME$', rel_availPath))

      
      if(platform.system()=="Windows"):
        _path_env=os.path.abspath("./").replace("/","\\").replace("\\\\","\\")
        availPath=availPath.replace("/","\\").replace("\\\\","\\")
      else:
        _path_env=os.path.abspath("./").replace("\\","/").replace("//","/")
        availPath=availPath.replace("\\","/")
      print("_path_env:",_path_env)
      print("availPath:",availPath)
      
      #os.makedirs(availPath, exist_ok=True)
      shutil.copytree(_path_env, availPath)
      #shutil.copy2("scripts/boot_script.py", cmd["dst_dir"]+"/boot_script.py")
      ACK=0
    
  elif _type == "validation":
    print("check:"+WebUI_Path)
    print("check:"+Core_Path)
    if os.path.isdir(WebUI_Path) and os.path.isdir(Core_Path):
      ACK=0
    else:
      print("validation FAILED")
  elif _type == "get_UI_url":
    infoObj["url"]=path_local+"/WebUI/index.html"
    ACK=0
  elif _type == "launch_core":
    global CORE_PIPE
    if(CORE_PIPE is None):
      env_path=cmd.get("env_path", "./")
      CORE_PIPE = subprocess.Popen([path_local+"/Core/visSele"], cwd=env_path)
      print("==================RUN==================")
      
      ACK=0
    else:
      ACK=-30

  elif _type == "kill_core":
    if(CORE_PIPE is not None):
      CORE_PIPE.send_signal(signal.SIGTERM)
      CORE_PIPE.terminate()
      CORE_PIPE.kill()
      #os.killpg(os.getpgid(CORE_PIPE.pid), signal.SIGTERM)
      CORE_PIPE=None
      ACK=0
    else:
      ACK=-30
  elif _type == "poll_core":
    if(CORE_PIPE is not None):
      infoObj["poll_code"]=CORE_PIPE.poll()
      infoObj["pid"]=CORE_PIPE.pid
      ACK=0
    else:
      ACK=-30

  if _type == "reload":
    ACK=0
  elif _type == "EXIT":
    ACK=0
  
  
  return ACK,infoObj


async def recv_msg(websocket):
  termination=False
  while True:
    try:
      recv_text = await websocket.recv()
      print("recv_text:",recv_text) 
      recv_json = json.loads(recv_text)
      print("recv_text:",recv_text, "json:",recv_json) 


      cmd_id=recv_json.get("cmd_id", None)
      ret_err,info=cmd_exec(recv_json)
      if(ret_err == 0):
        ACK = True
      else:
        ACK = False

      _type=recv_json.get("type", None)

      if _type == "reload":
        ACK=True

      if _type == "EXIT":
        ACK=True

      ret_json=json.dumps({'ACK': ACK, 'cmd_id': cmd_id,**info})
      response_text = ret_json
      await websocket.send(response_text)

      
      if _type == "reload":
        websocket.close()
        #TODO: the os.execv doesn't seem to reload the file content, so it might not work as we want
        #supposedly the script sould be reloaded with updated "file"

        global CORE_PIPE
        if(CORE_PIPE is not None):
          CORE_PIPE.send_signal(signal.SIGTERM)
          CORE_PIPE.terminate()
          CORE_PIPE.kill()
          #os.killpg(os.getpgid(CORE_PIPE.pid), signal.SIGTERM)
          CORE_PIPE=None


        arg_call=sys.argv[:]
        arg_call[0]=path_env+"/router.py"
        os.execl(sys.executable,sys.executable, *arg_call[:])
        break

      if _type == "EXIT":
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
    retx,info=cmd_exec(obj)
    print(retx)
    sys.exit(retx)
  else:
    sys.exit(-33)


