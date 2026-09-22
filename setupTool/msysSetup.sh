# SUPERSEDED -- use ./setupTool/devSetup.sh
#
# This file was the setup answer until 2026-09-10 and had gone stale in ways
# that mattered: no mingw-w64-x86_64-opencv, which is the build's one hard
# dependency (find_package(OpenCV REQUIRED)); no ccache, which is the
# difference between a 285s and a 23s full rebuild; no --needed, so every run
# reinstalled everything; and a closing note pointing at a hand-placed
# C:\OpenCV-MinGW from the CoreHub era that nothing reads any more.
#
# devSetup.sh installs the same things and more, is idempotent, checks the
# pieces pacman does not own (Node.js, PlatformIO), and prints what it found.
#
#   ./setupTool/devSetup.sh              # in the MSYS2 MINGW64 shell
#   ./setupTool/devSetup.sh --upgrade    # ... and pacman -Syu first
#
exec "$(dirname "${BASH_SOURCE[0]:-$0}")/devSetup.sh" "$@"
