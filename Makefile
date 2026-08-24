
domake:CORE Launcher

CORE:build_APP_Core export_APP_Core zip_APP_Core
Launcher:build_APP_Launcher export_APP_Launcher zip_APP_Launcher


EXPORT_ALL: export_APP_Core export_APP_Launcher 

domake_ALL:build export_APP_Core zip_APP_Core export_APP_Launcher zip_APP_Launcher
.PHONY:CORE Launcher export_APP_Core export_APP_Launcher zip_APP_Core


build_APP_Core: 
	# cmake --build $(abspath .)/InspectionCore/CORE0_1
	(cd $(abspath .)/InspectionCore/Core0_1/ ; make -f Makefile_mods runCMake)
	# make -C $(abspath .)/InspectionCore/CORE0_1 domake
	(cd UI/WebUI; npm run build )
	(cd UI/InspectionMonitor/; npm run build )


build_APP_Launcher: 
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
	(cd $(abspath .)/InspectionCore/Core0_1/ ; make -f Makefile_mods export_binary EXPORT_PATH=$(abspath .)/$(EXP_APP_Core_Folder)/Core)

	
	#Export scripts
	cp -r scripts $(EXP_APP_Core_Folder)
	
	-@mkdir -p $(EXP_APP_Core_Folder)/scripts/InspMonitor
	cp -r UI/InspectionMonitor/build/* $(EXP_APP_Core_Folder)/scripts/InspMonitor

	-@mkdir -p $(EXP_APP_Core_Folder)/scripts/apollo_gql_server
	cp -r DB/apollo_gql_server/schema $(EXP_APP_Core_Folder)/scripts/apollo_gql_server
	cp -r DB/apollo_gql_server/server $(EXP_APP_Core_Folder)/scripts/apollo_gql_server
	cp -r DB/apollo_gql_server/node_modules $(EXP_APP_Core_Folder)/scripts/
	

	
	
export_APP_Launcher:
	#Export APP_Launcher
	-@mkdir -p $(EXPFolder)/APP_Launcher
	cd UI/Electron_XPLAT; sh export.sh ../../$(EXPFolder)/APP_Launcher


	
UPDATE_APP_Core_NAME=
	
UPDATE_APP_Launcher_NAME=

ifeq ($(OS),Windows_NT)
	UPDATE_APP_Core_NAME=update_win
	UPDATE_APP_Launcher_NAME=APPL_win
else
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Darwin)
		UPDATE_APP_Core_NAME=update_mac
		UPDATE_APP_Launcher_NAME=APPL_mac
	endif
endif



zip_APP_Core:

	-@cd $(EXPFolder) ;rm -r $(UPDATE_APP_Core_NAME) $(UPDATE_APP_Core_NAME).zip
	cd $(EXPFolder) ;python ../MakeUtil.py --type=zip --src_dir=APP_Core --dst_path=$(UPDATE_APP_Core_NAME).zip


	
zip_APP_Launcher:
	-@cd $(EXPFolder) ; rm -r $(UPDATE_APP_Launcher_NAME) $(UPDATE_APP_Launcher_NAME).zip
	cd $(EXPFolder) ;python ../MakeUtil.py --type=zip --src_dir=APP_Launcher --dst_path=$(UPDATE_APP_Launcher_NAME).zip


# =============================================================================
# v2 launcher (UI/Launcher) -- separate targets on purpose
# =============================================================================
#
# The v1 targets above are untouched: UI/Electron_XPLAT still builds and
# packages exactly as before, so a machine can be serviced with either shell
# while the new one is being proven.
#
# The v2 package differs from update_win.zip in three ways:
#
#   * info.json sits at the ROOT of the package, not under scripts/. The
#     launcher reads it to name the payload directory, and it must not have to
#     dig for it.
#   * manifest.json carries a SHA256 for EVERY file. The launcher refuses a
#     package without one, refuses a listed file that is missing, and refuses a
#     file on disk that the manifest does not list.
#   * scripts/ carries ONE file: boot.js. The old one carried
#     apollo_gql_server, its node_modules and the InspMonitor build -- a
#     GraphQL server and a MongoDB client that this machine's
#     machine_setting.json does not point at (the db URLs are disabled with a
#     trailing underscore, the monitor URL is a remote host). The v2 launcher
#     runs no server.
#
# The .debug files ARE included. They are ~31 MB, and they are what makes a
# field crash dump symbolicate into function names instead of addresses; that
# is the entire value of the crash-dump work, and a service package is exactly
# where they need to be.

V2_EXPORT      = export_v2
# Named by VERSION, not "payload", so the assembled folder is itself a valid
# entry in an app root. That makes the development loop one command: build,
# then point the launcher's app root at $(V2_EXPORT)/ and it finds this version
# -- no zip, no SHA256 pass over 244 MB, no install step. (With no current.json
# present the launcher falls back to the newest valid version, so there is not
# even a version to select.)
V2_PAYLOAD     = $(V2_EXPORT)/$(V2_VERSION)
V2_VERSION    ?= $(shell python -c "import json;print(json.load(open('scripts/info.json'))['version'])")

# The core build needs g++, cmake and OpenCV from MSYS2's mingw64 toolchain.
# Putting it on PATH here rather than expecting the caller to have done it:
# forgetting is the normal case, and the failure it produces (cmake not found,
# or worse, a DIFFERENT toolchain found) is not obviously about PATH.
#
# It is prepended, not replaced, so the rest of the environment survives.
MINGW_BIN     ?= /c/msys64/mingw64/bin
V2_PATH        = PATH="$(MINGW_BIN):$$PATH"

.PHONY: v2 v2_app build_v2_core build_v2_webui export_v2_payload package_v2 \
        build_v2_launcher clean_v2 check_v2_toolchain

# Everything: build, assemble, package, and build the launcher itself.
v2: v2_app package_v2 build_v2_launcher

# One command, core + WebUI + assembled application folder, and nothing else.
#
# Stops at the folder on purpose. That folder is directly runnable by the
# launcher (point its app root at $(V2_EXPORT) and select the version), so the
# edit-build-run loop does not have to go through a zip, a SHA256 pass over
# 244 MB and an install every time.
v2_app: check_v2_toolchain build_v2_core build_v2_webui export_v2_payload
	@echo ""
	@echo "==> application assembled: $(V2_PAYLOAD)"
	@echo "    version : $(V2_VERSION)"
	@ls -1 $(V2_PAYLOAD) | sed 's/^/    /'
	@echo ""
	@echo "    run it   :  set the launcher's app root to $(abspath $(V2_EXPORT))"
	@echo "                (no current.json needed -- it falls back to the newest valid version)"
	@echo "    ship it  :  make package_v2   -> an installable, hash-verified zip"

# Fail here, with a sentence, rather than 200 lines into a cmake run.
check_v2_toolchain:
	@test -x "$(MINGW_BIN)/g++.exe" -o -x "$(MINGW_BIN)/g++" || { \
	  echo "MINGW_BIN=$(MINGW_BIN) has no g++ -- set MINGW_BIN to your MSYS2 mingw64 bin"; \
	  exit 1; }
	@command -v npm >/dev/null || { echo "npm not on PATH (needed for the WebUI build)"; exit 1; }
	@command -v python >/dev/null || { echo "python not on PATH (needed to make the manifest)"; exit 1; }

build_v2_core:
	($(V2_PATH); cd $(abspath .)/InspectionCore ; ./build.sh -p win-mingw-msys -e dist/win)

build_v2_webui:
	($(V2_PATH); cd UI/WebUI; npm run build )

export_v2_payload:
	-@rm -rf $(V2_PAYLOAD)
	-@mkdir -p $(V2_PAYLOAD)/Core $(V2_PAYLOAD)/WebUI
	cp -r InspectionCore/dist/win/. $(V2_PAYLOAD)/Core/
	cp -r UI/WebUI/dist/. $(V2_PAYLOAD)/WebUI/
	cp scripts/info.json $(V2_PAYLOAD)/info.json
	# scripts/boot.js is what makes this a launchable version rather than a
	# folder of files. It is the ONLY place that knows this application's
	# layout -- the executable's name, the chdir argument, the control port,
	# where the UI is, and what has to exist before any of it can run.
	-@mkdir -p $(V2_PAYLOAD)/scripts
	cp scripts/boot.js $(V2_PAYLOAD)/scripts/boot.js

# make_package.py stamps the version into info.json BEFORE hashing, so the
# manifest can never describe a file that has since been rewritten.
package_v2:
	python UI/Launcher/tools/make_package.py $(V2_PAYLOAD) \
	    $(V2_EXPORT)/update_$(V2_VERSION)_win.zip --version=$(V2_VERSION)

build_v2_launcher:
	(cd UI/Launcher; npm install --no-audit --no-fund; npm run package)

# The launcher's own self-tests. selftest exercises install/verify/select/
# supervise/stop against a REAL package and a REAL core; shell_smoke launches
# the actual Electron shell and checks that contextIsolation is really on.
.PHONY: test_v2
test_v2:
	(cd UI/Launcher; node tools/selftest.mjs \
	    ../../$(V2_EXPORT)/update_$(V2_VERSION)_win.zip \
	    $(abspath .)/InspectionCore/Core0_1)
	(cd UI/Launcher; node tools/shell_smoke.mjs)

clean_v2:
	-@rm -rf $(V2_EXPORT)
	-@rm -rf UI/Launcher/release-builds
