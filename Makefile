domake:build export




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