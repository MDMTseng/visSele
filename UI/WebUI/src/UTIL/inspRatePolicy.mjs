// How fast an inspection preview runs. ONE definition, for every screen that
// starts one. No imports -- see tools/webctl/unit_insprate.mjs.
//
// There were two, and they had drifted: InspectionUI asked the camera for 10
// fps in CI while 快速驗證 asked for 8, and neither number was written down
// anywhere as a decision. Nobody chose 8; it is simply what the other screen
// happened to say. Two copies of a number that is supposed to be the same
// number is a difference waiting to be reported as a bug -- which is how this
// module came to exist.
//
// WHAT THIS DOES NOT SET, deliberately: the core's preview ceiling
// (OK/NG/NA_MAX_FPS), which is reset to 6 at the start of every CI/FI session
// and is the actual limit on what reaches the screen. That belongs to the
// 運算核心 panel, and CoreStatusPanel's own comment says why nothing else may
// write it: "two writers, one of them re-firing on every inspection start, is
// how a setting becomes unexplainable." So this module governs what the CAMERA
// is asked for, and the core governs what the preview is allowed to show.

// CI is somebody watching one part at a time. It used to be
// setCameraSpeed_LOW (2 fps), which was too sluggish to work with; the
// walk-away case is handled by the idle auto-exit, not by crawling the rate.
export const CI_FRAME_RATE = 10;

// FI is production: the camera is triggered by the plate, so "highest" is a
// ceiling that the trigger never reaches rather than a rate anything runs at.
export const FI_FRAME_RATE = 9999999;

// mode is "FI" or "CI"; anything else is treated as CI, because a screen that
// cannot say which it is should not be asking for production speed.
export function inspFrameRate(mode) {
  return (mode === 'FI') ? FI_FRAME_RATE : CI_FRAME_RATE;
}

// Apply it to a CameraTransferCtrl. Kept here so a caller cannot pick the rate
// from one place and the method from another.
export function applyInspFrameRate(cameraCtrl, mode) {
  if (!cameraCtrl || typeof cameraCtrl.setCameraFrameRate !== 'function') return false;
  cameraCtrl.setCameraFrameRate(inspFrameRate(mode));
  return true;
}
