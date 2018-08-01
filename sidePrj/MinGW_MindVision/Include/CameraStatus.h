#ifndef __CAMERA_STATUS_DEF__
#define __CAMERA_STATUS_DEF__

typedef int CameraSdkStatus; 


/* Commonly used macro */
#define SDK_SUCCESS(_FUC_)              (_FUC_ == CAMERA_STATUS_SUCCESS)

#define SDK_UNSUCCESS(_FUC_)            (_FUC_ != CAMERA_STATUS_SUCCESS)

#define SDK_UNSUCCESS_RETURN(_FUC_,RET) if((RET = _FUC_) != CAMERA_STATUS_SUCCESS)\
                                        {\
                                            return RET;\
                                        }

#define SDK_UNSUCCESS_BREAK(_FUC_)      if(_FUC_ != CAMERA_STATUS_SUCCESS)\
                                        {\
                                            break;\
                                        }


/* Common mistakes  */

#define CAMERA_STATUS_SUCCESS							0	// The operation was successful
#define CAMERA_STATUS_FAILED							-1 // The operation failed
#define CAMERA_STATUS_INTERNAL_ERROR					-2 // Internal error
#define CAMERA_STATUS_UNKNOW							-3 // unknown error
#define CAMERA_STATUS_NOT_SUPPORTED						-4 // This feature is not supported
#define CAMERA_STATUS_NOT_INITIALIZED					-5 // Initialization is not completed
#define CAMERA_STATUS_PARAMETER_INVALID					-6 // Invalid parameter
#define CAMERA_STATUS_PARAMETER_OUT_OF_BOUND			-7 // The parameter is out of bounds
#define CAMERA_STATUS_UNENABLED							-8 // not enabled
#define CAMERA_STATUS_USER_CANCEL						-9 // Manually canceled, such as roi panel Click Cancel to return
#define CAMERA_STATUS_PATH_NOT_FOUND					-10 // did not find the corresponding path in the registry
#define CAMERA_STATUS_SIZE_DISMATCH						-11 // Get the size of the image data does not match the defined size
#define CAMERA_STATUS_TIME_OUT							-12 // Timeout error
#define CAMERA_STATUS_IO_ERROR							-13 // Hardware IO error
#define CAMERA_STATUS_COMM_ERROR						-14 // Communication error
#define CAMERA_STATUS_BUS_ERROR							-15 // Bus error
#define CAMERA_STATUS_NO_DEVICE_FOUND					-16 // No device found
#define CAMERA_STATUS_NO_LOGIC_DEVICE_FOUND				-17 // No logic found
#define CAMERA_STATUS_DEVICE_IS_OPENED					-18 // The device is already open
#define CAMERA_STATUS_DEVICE_IS_CLOSED					-19 // device is off
#define CAMERA_STATUS_DEVICE_VEDIO_CLOSED				-20 // When the device video is not turned on and the video related function is invoked, the error is returned if the camera video is not turned on.
#define CAMERA_STATUS_NO_MEMORY							-21 // Not enough system memory
#define CAMERA_STATUS_FILE_CREATE_FAILED				-22 // Failed to create file
#define CAMERA_STATUS_FILE_INVALID						-23 // Invalid file format
#define CAMERA_STATUS_WRITE_PROTECTED					-24 // write-protected, not writable
#define CAMERA_STATUS_GRAB_FAILED						-25 // Data collection failed
#define CAMERA_STATUS_LOST_DATA							-26 // data loss, incomplete
#define CAMERA_STATUS_EOF_ERROR							-27 // Frame end not received
#define CAMERA_STATUS_BUSY								-28 // is busy (the last operation is still in progress), this operation can not be carried out
#define CAMERA_STATUS_WAIT								-29 // need to wait (the conditions for the operation is not established), you can try again
#define CAMERA_STATUS_IN_PROCESS						-30 // in progress, has been manipulated
#define CAMERA_STATUS_IIC_ERROR							-31 // IIC transmission error
#define CAMERA_STATUS_SPI_ERROR							-32 // SPI transmission error
#define CAMERA_STATUS_USB_CONTROL_ERROR					-33 // USB control transfer error
#define CAMERA_STATUS_USB_BULK_ERROR					-34 // USB BULK transmission error
#define CAMERA_STATUS_SOCKET_INIT_ERROR					-35 // network transport package initialization failed
#define CAMERA_STATUS_GIGE_FILTER_INIT_ERROR			-36 // Network camera kernel filter driver initialization failed, please check whether the driver is installed correctly, or reinstall.
#define CAMERA_STATUS_NET_SEND_ERROR					-37 // network data sent wrong
#define CAMERA_STATUS_DEVICE_LOST						-38 // Lost connection with network camera, heartbeat detection timeout
#define CAMERA_STATUS_DATA_RECV_LESS					-39 // less bytes received than requested
#define CAMERA_STATUS_FUNCTION_LOAD_FAILED				-40 // Failed to load program from file
#define CAMERA_STATUS_CRITICAL_FILE_LOST				-41 // The necessary files to run the program is missing.
#define CAMERA_STATUS_SENSOR_ID_DISMATCH				-42 // The firmware and the program do not match because the wrong firmware was downloaded.
#define CAMERA_STATUS_OUT_OF_RANGE						-43 // The parameter is out of range.
#define CAMERA_STATUS_REGISTRY_ERROR					-44 // Setup registration error. Please reinstall the program, or run the installation directory Setup / Installer.exe
#define CAMERA_STATUS_ACCESS_DENY						-45 // No access. Specified camera has been occupied by other programs, and then apply for access to the camera, will return to the state. (One camera can not be accessed by multiple programs at the same time)
#define CAMERA_STATUS_CAMERA_NEED_RESET					-46 // Indicates that the camera needs to be reset before it can be used normally. In this case, please power off the camera or restart the operating system before it can be used normally.
#define CAMERA_STATUS_ISP_MOUDLE_NOT_INITIALIZED		-47 // ISP module not initialized
#define CAMERA_STATUS_ISP_DATA_CRC_ERROR				-48 // data validation error
#define CAMERA_STATUS_MV_TEST_FAILED					-49 // Data test failed
#define CAMERA_STATUS_INTERNAL_ERR1						-50 // Internal error 1
#define CAMERA_STATUS_U3V_NO_CONTROL_EP					-51 // U3V control endpoint not found
#define CAMERA_STATUS_U3V_CONTROL_ERROR					-52 // U3V control communication error





//Same criteria as AIA
/*#define CAMERA_AIA_SUCCESS							0x0000 */
#define CAMERA_AIA_PACKET_RESEND						0x0100 // The frame needs to be retransmitted
#define CAMERA_AIA_NOT_IMPLEMENTED						0x8001 // Device does not support the command
#define CAMERA_AIA_INVALID_PARAMETER					0x8002 // illegal command parameters
#define CAMERA_AIA_INVALID_ADDRESS						0x8003 // Inaccessible address
#define CAMERA_AIA_WRITE_PROTECT						0x8004 // access to the object can not be written
#define CAMERA_AIA_BAD_ALIGNMENT						0x8005 // The address accessed is not aligned as required
#define CAMERA_AIA_ACCESS_DENIED						0x8006 // No access
#define CAMERA_AIA_BUSY									0x8007 // Command is processing
#define CAMERA_AIA_DEPRECATED							0x8008 // 0x8008-0x0800B 0x800F This command has been discarded
#define CAMERA_AIA_PACKET_UNAVAILABLE					0x800C // package is invalid
#define CAMERA_AIA_DATA_OVERRUN							0x800D // data overflow, the data is usually received more than needed
#define CAMERA_AIA_INVALID_HEADER						0x800E // Some areas in the packet header do not match the protocol
#define CAMERA_AIA_PACKET_NOT_YET_AVAILABLE				0x8010 // Image subcontracting data not yet ready, mostly used in trigger mode, application access timeout
#define CAMERA_AIA_PACKET_AND_PREV_REMOVED_FROM_MEMORY	0x8011 // Need to visit the sub-package does not exist. Used for retransmission data is not in the buffer zone
#define CAMERA_AIA_PACKET_REMOVED_FROM_MEMORY			0x8012 // CAMERA_AIA_PACKET_AND_PREV_REMOVED_FROM_MEMORY
#define CAMERA_AIA_NO_REF_TIME							0x0813 // No reference clock source. Multi-time synchronization command execution
#define CAMERA_AIA_PACKET_TEMPORARILY_UNAVAILABLE		0x0814 // Due to channel bandwidth issues, the current subpackage is temporarily unavailable and needs to be accessed later
#define CAMERA_AIA_OVERFLOW								0x0815 // device-side data overflow, usually the queue is full
#define CAMERA_AIA_ACTION_LATE							0x0816 // Command execution has expired valid for the specified time
#define CAMERA_AIA_ERROR								0x8FFF // error



                                        

#endif
