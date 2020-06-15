

import getopt, sys
import zipfile
import shutil
import ntpath
import os

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

def parse_argv(argv):
  try:
    opts, args = getopt.getopt(argv[1:], "", 
    [
      'type=',
      'src_dir=',
      'dst_path='

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


def zip_add_dir(path, ziph):
  # ziph is zipfile handle
  for root, dirs, files in os.walk(path):
    for file in files:
      #print(root,file)
      ziph.write(os.path.join(root, file))


def cmd_exec(cmd):
  infoObj={}
  _type=cmd.get("type", None)
  ACK=-10
  if _type == "zip":
    
    if os.path.isfile(cmd["dst_path"]):
      print("Err:dst_path:",cmd["dst_path"]," exists")
    else:
      # print("src_dir",cmd["src_dir"])
      # print("dst_path",cmd["dst_path"])
      zf = zipfile.ZipFile(cmd["dst_path"], mode='w')
      zip_add_dir(cmd["src_dir"],zf )
      zf.close()
      ACK=0
    pass
      
  elif _type == "un_deploy":#in case of deploy error, roll back
    pass
  elif _type == "EXIT":
    ACK=0
  
  
  return ACK,infoObj

if __name__ == "__main__":

  obj= parse_argv(sys.argv)
  _type=obj.get("type", None)

  if  _type is not None:
    retx,info=cmd_exec(obj)
    print(retx)
    sys.exit(retx)
  else:
    sys.exit(-33)


