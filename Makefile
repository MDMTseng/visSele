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


	
UPDATE_FILE_NAME=update
zip_update:
	-@rm -r $(UPDATE_FILE_NAME) $(UPDATE_FILE_NAME).zip
	cp -r $(EXPFolder) $(UPDATE_FILE_NAME)
	python MakeUtil.py --type=zip --src_dir=$(UPDATE_FILE_NAME) --dst_path=$(UPDATE_FILE_NAME).zip
	-@rm -r $(UPDATE_FILE_NAME)
