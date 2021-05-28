
#include "CameraLayer_Aravis.hpp"
#include "logctrl.h"
#include <arv.h>

CameraLayer_Aravis::CameraLayer_Aravis(CameraLayer_Callback cb, void *context) : CameraLayer(cb, context), arv_option_entries{
                                                                                                               {"name", 'n', 0, G_OPTION_ARG_STRING,
                                                                                                                &arv_option_camera_name, "Camera name", NULL},
                                                                                                               {"snapshot", 's', 0, G_OPTION_ARG_NONE,
                                                                                                                &arv_option_snaphot, "Snapshot", NULL},
                                                                                                               {"frequency", 'f', 0, G_OPTION_ARG_DOUBLE,
                                                                                                                &arv_option_frequency, "Acquisition frequency", NULL},
                                                                                                               {"trigger", 't', 0, G_OPTION_ARG_STRING,
                                                                                                                &arv_option_trigger, "External trigger", NULL},
                                                                                                               {"software-trigger", 'o', 0, G_OPTION_ARG_DOUBLE,
                                                                                                                &arv_option_software_trigger, "Emit software trigger", NULL},
                                                                                                               {"width", 'w', 0, G_OPTION_ARG_INT,
                                                                                                                &arv_option_width, "Width", NULL},
                                                                                                               {"height", 'h', 0, G_OPTION_ARG_INT,
                                                                                                                &arv_option_height, "Height", NULL},
                                                                                                               {"h-binning", '\0', 0, G_OPTION_ARG_INT,
                                                                                                                &arv_option_horizontal_binning, "Horizontal binning", NULL},
                                                                                                               {"v-binning", '\0', 0, G_OPTION_ARG_INT,
                                                                                                                &arv_option_vertical_binning, "Vertical binning", NULL},
                                                                                                               {"exposure", 'e', 0, G_OPTION_ARG_DOUBLE,
                                                                                                                &arv_option_exposure_time_us, "Exposure time (µs)", NULL},
                                                                                                               {"gain", 'g', 0, G_OPTION_ARG_INT,
                                                                                                                &arv_option_gain, "Gain (dB)", NULL},
                                                                                                               {"auto", 'a', 0, G_OPTION_ARG_NONE,
                                                                                                                &arv_option_auto_socket_buffer, "Auto socket buffer size", NULL},
                                                                                                               {"packet-size-adjustment", 'j', 0, G_OPTION_ARG_STRING,
                                                                                                                &arv_option_packet_size_adjustment, "Packet size adjustment",
                                                                                                                "{never|always|once|on-failure|on-failure-once}"},
                                                                                                               {"no-packet-resend", 'r', 0, G_OPTION_ARG_NONE,
                                                                                                                &arv_option_no_packet_resend, "No packet resend", NULL},
                                                                                                               {"packet-request-ratio", 'q', 0, G_OPTION_ARG_DOUBLE,
                                                                                                                &arv_option_packet_request_ratio, "Packet resend request limit as a frame packet number ratio [0..2.0]", NULL},
                                                                                                               {"packet-timeout", 'p', 0, G_OPTION_ARG_INT,
                                                                                                                &arv_option_packet_timeout, "Packet timeout (ms)", NULL},
                                                                                                               {"frame-retention", 'm', 0, G_OPTION_ARG_INT,
                                                                                                                &arv_option_frame_retention, "Frame retention (ms)", NULL},
                                                                                                               {"gv-stream-channel", 'c', 0, G_OPTION_ARG_INT,
                                                                                                                &arv_option_gv_stream_channel, "GigEVision stream channel id", NULL},
                                                                                                               {"gv-packet-delay", 'y', 0, G_OPTION_ARG_INT,
                                                                                                                &arv_option_gv_packet_delay, "GigEVision packet delay (ns)", NULL},
                                                                                                               {"gv-packet-size", 'i', 0, G_OPTION_ARG_INT,
                                                                                                                &arv_option_gv_packet_size, "GigEVision packet size (bytes)", NULL},
                                                                                                               {"chunks", 'u', 0, G_OPTION_ARG_STRING,
                                                                                                                &arv_option_chunks, "Chunks", NULL},
                                                                                                               {"realtime", '\0', 0, G_OPTION_ARG_NONE,
                                                                                                                &arv_option_realtime, "Make stream thread realtime", NULL},
                                                                                                               {"high-priority", '\0', 0, G_OPTION_ARG_NONE,
                                                                                                                &arv_option_high_priority, "Make stream thread high priority", NULL},
                                                                                                               {"no-packet-socket", '\0', 0, G_OPTION_ARG_NONE,
                                                                                                                &arv_option_no_packet_socket, "Disable use of packet socket", NULL},
                                                                                                               {"debug", 'd', 0, G_OPTION_ARG_STRING,
                                                                                                                &arv_option_debug_domains, "Debug domains", NULL},
                                                                                                               {"bandwidth-limit", 'b', 0, G_OPTION_ARG_INT,
                                                                                                                &arv_option_bandwidth_limit, "Desired USB3 Vision device bandwidth limit", NULL},
                                                                                                               {NULL}}
{




}

CameraLayer_Aravis::~CameraLayer_Aravis()
{
}
CameraLayer::status CameraLayer_Aravis::SetMirror(int Dir, int en)
{
  return CameraLayer::NAK;
}
CameraLayer::status CameraLayer_Aravis::SetROIMirror(int Dir, int en)
{
  return CameraLayer::NAK;
}

CameraLayer::status CameraLayer_Aravis::L_TriggerMode(int type)
{
  return CameraLayer::NAK;
}

CameraLayer::status CameraLayer_Aravis::SetROI(float x, float y, float w, float h, int zw, int zh)
{

  return CameraLayer::NAK;
}
CameraLayer::status CameraLayer_Aravis::GetROI(float *x, float *y, float *w, float *h, int *zw, int *zh)
{
  return CameraLayer::NAK;
}

CameraLayer::status CameraLayer_Aravis::TriggerMode(int type)
{
  return CameraLayer::NAK;
}

CameraLayer::status CameraLayer_Aravis::TriggerCount(int count)
{
  return CameraLayer::NAK;
}

CameraLayer::status CameraLayer_Aravis::Trigger()
{
  return CameraLayer::NAK;
}

CameraLayer::status CameraLayer_Aravis::RUN()
{
  return CameraLayer::NAK;
}

void CameraLayer_Aravis::ContTriggerThread()
{
}

void CameraLayer_Aravis::ContTriggerThreadTermination()
{
}

CameraLayer::status CameraLayer_Aravis::SetResolution(int width, int height)
{
  return CameraLayer::NAK;
}
CameraLayer::status CameraLayer_Aravis::SetAnalogGain(int gain)
{
  return CameraLayer::NAK;
}

CameraLayer::status CameraLayer_Aravis::L_SetFrameRateMode(int mode)
{
  return CameraLayer::NAK;
}

CameraLayer::status CameraLayer_Aravis::SetOnceWB()
{
  return CameraLayer::NAK;
}

CameraLayer::status CameraLayer_Aravis::SetFrameRateMode(int mode)
{
  return CameraLayer::NAK;
}

CameraLayer::status CameraLayer_Aravis::SnapFrame()
{
  return CameraLayer::NAK;
}

CameraLayer::status CameraLayer_Aravis::GetAnalogGain(int *ret_min, int *ret_max)
{
  return CameraLayer::NAK;
}

CameraLayer::status CameraLayer_Aravis::SetExposureTime(double time_us)
{
  return CameraLayer::NAK;
}

CameraLayer::status CameraLayer_Aravis::GetExposureTime(double *ret_time_ms)
{
  return CameraLayer::NAK;
}

acvImage *CameraLayer_Aravis::GetFrame()
{
  return &img;
}