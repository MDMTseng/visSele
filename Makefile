domake:build export zip_update

.PHONY:build export zip_update


build: 
	make -C $(abspath .)/InspectionCore domake
	cd UI/WebUI; npm run build ;


EXPFolder=export
export:
	-@rm -r $(EXPFolder)
	-@mkdir -p $(EXPFolder)/WebUI
	cd UI/WebUI; sh export.sh ../../$(EXPFolder)/WebUI
	-@mkdir -p $(EXPFolder)/Core
	make -C $(abspath .)/InspectionCore export EXPORT_PATH=$(abspath .)/$(EXPFolder)/Core
	cp -r scripts $(EXPFolder)


	
UPDATE_FILE_NAME=

ifeq ($(OS),Windows_NT)
	UPDATE_FILE_NAME=update_win
else
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Darwin)
		UPDATE_FILE_NAME=update_mac
	endif
endif



zip_update:
	-@rm -r $(UPDATE_FILE_NAME) $(UPDATE_FILE_NAME).zip
	python MakeUtil.py --type=zip --src_dir=$(EXPFolder) --dst_path=$(UPDATE_FILE_NAME).zip
