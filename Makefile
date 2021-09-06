
domake:CORE Container

CORE:build_APP_Core export_APP_Core zip_APP_Core
Container:build_APP_Container export_APP_Container zip_APP_Container

domake_ALL:build export_APP_Core zip_APP_Core export_APP_Container zip_APP_Container
.PHONY:CORE Container export_APP_Core export_APP_Container zip_APP_Core


build_APP_Core: 
	make -C $(abspath .)/InspectionCore/CORE0_1 domake
	cd UI/WebUI; npm run build ;

build_APP_Container: 
	cd UI/Electron_XPLAT; npm run packaging ;



EXPFolder=export
EXP_APP_Core_Folder=$(EXPFolder)/APP_Core
export_APP_Core:
	#del & make dir
	-@rm -r $(EXPFolder)
	-@mkdir -p $(EXP_APP_Core_Folder)

	#Export WebUI 
	-@mkdir -p $(EXP_APP_Core_Folder)/WebUI
	cd UI/WebUI; sh export.sh ../../$(EXP_APP_Core_Folder)/WebUI
	
	#Export Core
	-@mkdir -p $(EXP_APP_Core_Folder)/Core
	make -C $(abspath .)/InspectionCore/CORE0_1 export EXPORT_PATH=$(abspath .)/$(EXP_APP_Core_Folder)/Core

	
	#Export scripts
	cp -r scripts $(EXP_APP_Core_Folder)
	
	
export_APP_Container:
	#Export APP_Container
	-@mkdir -p $(EXPFolder)/APP_Container
	cd UI/Electron_XPLAT; sh export.sh ../../$(EXPFolder)/APP_Container


	
UPDATE_APP_Core_NAME=
	
UPDATE_APP_Container_NAME=

ifeq ($(OS),Windows_NT)
	UPDATE_APP_Core_NAME=update_win
	UPDATE_APP_Container_NAME=APPC_win
else
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Darwin)
		UPDATE_APP_Core_NAME=update_mac
		UPDATE_APP_Container_NAME=APPC_mac
	endif
endif



zip_APP_Core:

	-@cd $(EXPFolder) ;rm -r $(UPDATE_APP_Core_NAME) $(UPDATE_APP_Core_NAME).zip
	cd $(EXPFolder) ;python ../MakeUtil.py --type=zip --src_dir=APP_Core --dst_path=$(UPDATE_APP_Core_NAME).zip


	
zip_APP_Container:
	-@cd $(EXPFolder) ; rm -r $(UPDATE_APP_Container_NAME) $(UPDATE_APP_Container_NAME).zip
	cd $(EXPFolder) ;python ../MakeUtil.py --type=zip --src_dir=APP_Container --dst_path=$(UPDATE_APP_Container_NAME).zip
