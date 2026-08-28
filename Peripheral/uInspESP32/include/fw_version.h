#pragma once
// Build identity, defined in the generated src/fw_version_gen.c.
//
// Separate from Data_JsonRaw_Layer::VERSION, which is the hand-maintained
// PROTOCOL version: that one identifies the wire format, these identify the
// image. FW_GIT_HASH carries a "-dirty" suffix when the tree had uncommitted
// changes, because then the image is NOT the commit it names.
#ifdef __cplusplus
extern "C" {
#endif
extern const char *const FW_GIT_HASH;
extern const char *const FW_BUILD_TIME;
#ifdef __cplusplus
}
#endif
