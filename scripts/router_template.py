from subprocess import Popen
import os
import shutil
import sys
path_env=os.path.abspath("./")
path_local=os.path.abspath(sys.argv[0])

path_env=os.path.abspath("./")
path_script=os.path.abspath(sys.argv[0])
path_local=os.path.dirname(path_script)

print("path_env=",path_env)
print("path_script=",path_script)
print("path_local=",path_local)

BIN_DIR="$TARGET_PATH_NAME$"

if __name__ == "__main__":
  print("BIN_DIR:",BIN_DIR)
  arg_call=sys.argv[:]
  arg_call[0]=path_local+"/"+BIN_DIR+"/scripts/boot_script.py"
  print(sys.argv[:],arg_call)
  #Popen(arg_call,cwd=path_env) # go back to your program
  print(sys.executable)
  os.execl(sys.executable,sys.executable, *arg_call[:])