/******************************************************************************
��    ��:
    Gige Vision���״̬����,Ϊ�˷�����ģ������궨���GigE Vision specification 
    �ļ��и����˺���ϸ��ע��˵����
    
�汾��¼��



	
******************************************************************************/
#ifndef _GIGEVISIONDEF_H_
#define _GIGEVISIONDEF_H_



//#define GIGE_PROTOCOL_VERSION_2_0  


typedef struct gvcp_cmd_head_s{
    unsigned char status[2];
    unsigned char answer[2];
    unsigned char lenth[2];//������ͷ������
    unsigned char id[2];
}gvcp_cmd_head_t;


typedef struct gvcp_cmd_s{
    gvcp_cmd_head_t   head;
    unsigned char   data[4];
}gvcp_cmd_t;

typedef struct gvcp_discovery_ack_s{
    gvcp_cmd_head_t head;
    unsigned char spec_version_major[2];
    unsigned char spec_version_minor[2];
    unsigned char device_mode[4];
    unsigned char reserved0[2];
    unsigned char mac_addr[6];
    unsigned char ip_config_options[4];
    unsigned char ip_config_current[4];
    unsigned char reserved1[12];
    unsigned char current_ip[4];
    unsigned char reserved2[12];
    unsigned char current_net_mask[4];
    unsigned char reserved3[12];
    unsigned char default_gateway[4];
    unsigned char manufacturer_name [32];
    unsigned char model_name [32];
    unsigned char device_version [32];
    unsigned char manufacturer_specific_information[48];
    unsigned char serial_number [16];
    unsigned char user_defined_name [16];
}gvcp_discovery_ack_t;

typedef struct gvcp_forceip_s{
    gvcp_cmd_head_t head;
    unsigned char reserved; 
    unsigned char reserved0;
    unsigned char mac_addr[6];
    unsigned char reserved1[12];
    unsigned char static_ip[4];
    unsigned char reserved2[12];
    unsigned char static_netmask[4];
    unsigned char reserved3[12];
    unsigned char static_gateway[4];
}gvcp_forceip_t;

//�Զ����IP����
typedef struct gvcp_custom_forceip_s{
    gvcp_cmd_head_t head;
    unsigned char mac_addr[6];
    unsigned char static_ip[4];
    unsigned char static_netmask[4];
    unsigned char static_gateway[4];
    unsigned char alloc_mode;
    unsigned char ipsave;
    unsigned char reserved3[36];
}gvcp_custom_forceip_t;

//�Զ���Ľ���bootloader������
#define  ENTERBOOTLOADER_STR  "MindVision EnterBootloader"
typedef struct gvcp_custom_enterbootloader_s{
    gvcp_cmd_head_t head;
    unsigned char mac_addr[6];
    unsigned char chk_string[34];
}gvcp_custom_enterbootloader_t;

//�Զ���Ľ����ϻ���ȵ�����
#define  ENTERAGINGTESTSTR  "MindVision factory mod is aging test"
typedef struct gvcp_custom_enteragingtest_s{
    gvcp_cmd_head_t head;
    unsigned char chk_string[64];
}gvcp_custom_enteragingtest_t;

/************************* �Ĵ��� *****************************/
//ֻ���Ĵ���
#ifdef GIGE_PROTOCOL_VERSION_2_0
#define GVR_PROTOCOL_VERSION_DEF                0x00020000
#define GVR_GVSP_CONFIGURATION_DEF              0x40000000 //64bit_block_id_enable
#else
#define GVR_PROTOCOL_VERSION_DEF                0x00010002
#define GVR_GVSP_CONFIGURATION_DEF              0x00000000 //64bit_block_id_disable
#endif
#define GVR_DEVICE_MODE_DEF                     0x80000001//0x80000000;	//big-endian,Transmitter,single link config    
#define GVR_NET_INTERFACE_CAPABILITY_DEF        0x00000007//0x80000007;//PAUSE_reception,support dhcp��lla��persistent ip
#define GVR_NET_INTERFACE_NUMBER_DEF            0x00000001	

#define GVR_NUMBER_OF_MESSAGE_CHANNEL_DEF       0x0 //�ݲ�֧��message channel
#define GVR_NUMBER_OF_STREAM_CHANNEL_DEF        0x00000001
#define GVR_NUMBER_OF_ACTION_REG_DEF            0
#define GVR_ACTION_DEVICE_KEY_DEF               0
#define GVR_NUMBER_OF_ACTIVE_LINKS_DEF          0x00000001
#define GVR_GVSP_CAPABILITY_DEF                 0x80000000//SCSPx_supported,support blockid64  V1.2��ֻ��bit1��Ч
#define GVR_MESSAGE_CHANNEL_CAPABILITY_DEF      0x0//�ݲ�֧��message channel
//#define GVR_GVCP_CAPABILITY_DEF               0xD8000002	//not support packresend
#define GVR_GVCP_CAPABILITY_DEF                 0xF8600007//0xD8400006;
#define GVR_TIMESTAMP_TICK_FREQUNENCY_HIGH_DEF  0
#define GVR_TIMESTAMP_TICK_FREQUNENCY_LOW_DEF   1000000  //��֧������ʱ�����Ƶ��
#define GVR_DISCOVERY_ACK_DELAY_DEF             0
#define GVR_PENDING_TIMEOUT_DEF                 500   //MAX pending time 500ms
#define GVR_CONTROL_SWITCHOVER_KEY_DEF          0x0//��ʱ��֧��
#define GVR_PHYSICAL_LINK_CONFIG_CAPABILITY_DEF 0x00000001//single_link_configuration (SL)
#define GVR_PHYSIC_LINK_CONFIG_DEF              0x0//0: Single Link Configuration
#define GVR_IEEE_1588_STATUS_DEF                0x0//��ʱ��֧��
#define GVR_SCHEDULED_AC_QUEUE_SIZE_DEF         0x0//��ʱ��֧��
#define GVR_MESSAGE_CHANNEL_PORT_DEF            0x00000000
#define GVR_STREAM_CHANNEL_CAPABILITY_DEF       0x0

        
//�ɶ�д�ļĴ���
typedef struct gvcp_reg_s{
    unsigned char   dev_mac_addr[6];
    unsigned int    net_interface_configuration;
    unsigned int    current_ip;
    unsigned int    current_mask;
    unsigned int    current_gateway;
    unsigned char   device_ver[32];
    unsigned char   maufactrurer_name[32];
    unsigned char   model_name[32];
    unsigned char   manufacturer_info[48];
    unsigned char   serial_num[16];
    unsigned char   userdef_name[16];
    unsigned int    persistent_ip;
    unsigned int    persistent_mask;
    unsigned int    persistent_gateway;
    unsigned int    link_speed;
    unsigned int    heartbeat_timeout;
    unsigned int    timestamp_ctrl;
    unsigned int    timestamp_value_h;
    unsigned int    timestamp_value_l;
    unsigned int    gvcp_configuration;
    unsigned int    ctrl_channel_privilege;
    unsigned int    primary_app_port;
    unsigned int    primary_app_ip;
    unsigned int    stream_channel_port;
    unsigned int    stream_channel_packsize;
    unsigned int    stream_channel_packet_delay;
    unsigned int    stream_channel_dest_addr;
    unsigned int    stream_channel_src_port;
    unsigned int    stream_channel_config;
	
	/* �Զ���ļĴ�����XML�������� */
	unsigned int    pixel_fmt;
	unsigned int    payload_size;
	unsigned int    acquisition_mod;
    unsigned int    strb_mod;
    unsigned int    img_width;
    unsigned int    img_height;
    unsigned int    h_offs;
    unsigned int    v_offs;
    unsigned int    img_hblk;
    unsigned int    img_vblk;
    unsigned int    res_mod;
    unsigned int    res_sel;
    unsigned int    img_mirror;
	unsigned int    trig_count;
    unsigned int    trig_action;
    unsigned int    frame_speed;
	unsigned int    set_exptime;
	float           set_again; 
    unsigned int    set_expmod;
    unsigned int    set_ae;
    unsigned int    set_ae_target;
    unsigned int    set_anti_flick;
    unsigned int    isp_wb_once;
    unsigned int    isp_gamma;
    unsigned int    isp_contrast;
    unsigned int    isp_saturation;
    unsigned int    isp_rgb_gain;
    unsigned int    isp_rgb_matrixsel;    
    unsigned int    isp_bl ;   
    unsigned int    isp_rgb_matrix_wr_pos;
    unsigned int    isp_deffect;
    unsigned int    user_slector;
    unsigned int    user_default_selector;
    
    
    /* ��չ�ӿ� */
    unsigned int    ssr_slave_addr;
    unsigned int    isp_rgbacc_roi_h;  
    unsigned int    isp_rgbacc_roi_v; 
    unsigned int    isp_yacc_roi_h;
    unsigned int    isp_yacc_roi_v;  
    unsigned int    fw_boot_ver;
    unsigned int    fw_app_ver;
    unsigned int    fw_fpga_ver;
    unsigned int    resend_blkid;
    unsigned int    resend_firstid;
    unsigned int    resend_lastid;

}gvcp_reg_t;

//--------------------GIGE NET CONFIGURATION--------------------------------------------------
#define GVCP_UDP_PORT                       3956
#define GVSP_UDP_PORT                       3959

//---------------------GVCP FORMAT-------------------------------------------------------------
#define GVCP_HEAD_SIZE                      8
#define GVCP_DATA_OFFSET                    GVCP_HEAD_SIZE
    
#define GET_GVCP_ACK_STATUS(PHEAD)          ((PHEAD[0] << 8) |  PHEAD[1]) 

#define GET_GVCP_CMD(PHEAD)                 ((PHEAD[2] << 8) |  PHEAD[3]) 

#define GET_GVCP_ACK_DATA_LEN(PHEAD)        ((PHEAD[4] << 8) |  PHEAD[5])                                    
#define SET_GVCP_ACK_DATA_LEN(PHEAD,LEN)    {PHEAD[4] = (LEN >> 8)&0XFF; PHEAD[5] = LEN&0XFF}

#define GET_GVCP_CMD_ID(PHEAD)              ((PHEAD[6] << 8) |  PHEAD[7]) 

//---------------------Bootstrap Registers(GVR:GIGE VISION REGISTER)------------------------------
/*
Version Register
This register indicates the version of the GigE Vision specification implemented by this device. Version 2.0
of this specification shall return 0x00020000.
0 �C 15 major_version This field represents the major version of the specification. For instance, GigE
Vision version 2.0 would have the major version set to 2.
16 �C 31 minor_version This field represents the minor version of the specification. For instance, GigE
Vision version 2.0 would have the minor version set to 0.
Access type: Read

gige�İ汾��Ĭ����2.0
*/
#define GVR_PROTOCOL_VERSION                0X0  

/*
Device Mode Register
This register indicates the character set used by the various strings present in the bootstrap registers and
other device-specific information, such as the link configuration and the device class.
Access type: Read

Bits          Name                                              Description
0            endianess(E)                       Endianess might be used to interpret multi-byte data for READMEM and
                                                WRITEMEM commands. This represents the endianess of all device��s registers
                                                (bootstrap registers and manufacturer-specific registers).
                                                  0: reserved, do not use
                                                  1: big-endian device
                                                  GigE Vision 2.0 devices MUST always report big-endian and set this bit to 1.
                                                  ��˺�С�ˣ�gige���õĴ��
                                                  
1 �C 3       device_class (DC)                  This field represents the class of the device. It must take one of the following
                                                values
             �豸����                           0: Transmitter   ����
                                                1: Receiver      ����
                                                2: Transceiver   �շ�
                                                3: Peripheral    ����
                                                other: reserved

                                                
                                                
4 �C 7       current_link_configuration CLC)    Indicate the current Physical Link configuration of the device
                                                0: Single Link Configuration
                                                1: Multiple Links Configuration
                                                2: Static LAG Configuration
                                                3: Dynamic LAG Configuration
                                                Note: when Multiple Links Configuration and LAG are used concurrently, the
                                                the device shall report the configuration with the highest value.
                                                
8 �C 23      reserved Always 0.                 Ignore when reading.

*/
#define GVR_DEVICE_MODE                     0X0004

/*
Device MAC Address Registers
These registers store the MAC address of the given network interface. If link aggregation is used, this is the
MAC address used by the link aggregation group.
[R-431cd] A device MUST implement the Device MAC Address registers on all of its supported
network interfaces. Both registers (High Part and Low Part) MUST be available. Devices
must return GEV_STATUS_INVALID_ADDRESS for unsupported network interfaces.
[R27-10cd]
An application needs to access the high part before accessing the low part:

High Part
Bits                Name                            Description
0 �C 15      reserved Always 0.             Ignore when reading.
16 �C 31     mac_address                    Hold the upper two bytes of the Device MAC address

Low Part
Bits                Name                            Description
0 �C 31      mac_address                    Hold the lower four bytes of the Device MAC address
*/
#define GVR_DEVICE_MAC_ADDR_HIGH            0X0008
#define GVR_DEVICE_MAC_ADDR_LOW             0X000C


/*
Network Interface Capability Registers
These registers indicate the network and IP configuration schemes supported on the given network interface.
Multiple schemes can be supported simultaneously. If link aggregation is used, this is the IP configuration
scheme supported by the link aggregation group.
[R-432cd] The device MUST implement the Network Interface Capability register on all of its
supported network interfaces. Devices must return
GEV_STATUS_INVALID_ADDRESS for unsupported network interfaces.
Access type: Read
    Bits                Name                        Description
    0           PAUSE_reception (PR)             Set to 1 if the network interface can handle PAUSE frames.
    1           PAUSE_generation (PG)            Set to 1 if the network interface can generate PAUSE frames.
  2 �C 28       reserved Always 0.               Ignore when reading.
    29          LLA (L)                          Link-local address is supported. Always 1
    30          DHCP (D)                         DHCP is supported. Always 1
    31          Persistent_IP (P)                1 if Persistent IP is supported, 0 otherwise.
*/
#define GVR_NET_INTERFACE_CAPABILITY        0X0010

/*
Network Interface Configuration Registers
These registers indicate which network and IP configurations schemes are currently activated on the given
network interface. If link aggregation is used, this is the current IP configuration scheme used by the link
aggregation group.
[R-433cd] The device MUST implement the Network Interface Configuration register on all of its
supported network interfaces. Devices must return
GEV_STATUS_INVALID_ADDRESS for unsupported network interfaces. [R27-12cd]

    Bits            Name                                Description
     0          PAUSE_reception (PR)            Set to 1 to enable the reception of PAUSE frames on this network interface.
                                                Factory default is device-specific and might be hard-coded if not configurable.
                                                New settings to take effect on next link negotiation. This setting must persist
                                                across device reset.
     1          PAUSE_generation (PG)           Set to 1 to enable the generation of PAUSE frames from this network interface.
                                                Factory default is device-specific and might be hard-coded if not configurable.
                                                New settings to take effect on next link negotiation. This setting must persist
                                                across device reset.
   2 �C 28      reserved Always 0.              Ignore when reading.
     29         LLA (L)                         Link-local address is activated. Always 1.
     30         DHCP (D)                        DHCP is activated on this interface. Factor default is 1.
     31         Persistent_IP (P)               Persistent IP is activated on this interface. Factory default is 0.

*/
#define GVR_NET_INTERFACE_CONFIGURATION     0X0014

/*
Current IP Address Registers
These registers report the IP address for the given network interface once it has been configured. If link
aggregation is used, this is the IP address used by the link aggregation group.
[R-434cd] The device MUST support the Current IP Address register on all of its supported
network interfaces. Devices must return GEV_STATUS_INVALID_ADDRESS for
unsupported interfaces. [R27-13cd]
Access type: Read

  Bits         Name                 Description
0 �C 31     IPv4_address    IP address for this network interface
*/
#define GVR_CURRENT_IP                      0X0024

/*
Current Subnet Mask Registers
These registers provide the subnet mask of the given interface. If link aggregation is used, this is the subnet
mask used by the link aggregation group.
[R-435cd] The device MUST support the Current Subnet Mask register on all of its supported
network interfaces. Devices must return GEV_STATUS_INVALID_ADDRESS for
unsupported network interfaces. [R27-14cd]
  Access type: Read

  Bits       Name                Description
0 �C 31  IPv4_subnet_mask       Subnet mask for this network interface
*/
#define GVR_CURRENT_MASK                    0X0034

/*
Current Default Gateway Registers
These registers indicate the default gateway IP address to be used on the given network interface. If link
aggregation is used, this is the default gateway used by the link aggregation group.
[R-436cd] The device MUST support the Current Default Gateway register on all of its supported
network interfaces. Devices must return GEV_STATUS_INVALID_ADDRESS for
unsupported network interfaces. [R27-15cd]
  Access type: Read

  Bits          Name                            Description
0 �C 31     IPv4_default_gateway        Default gateway for this network interface
*/
#define GVR_CURRENT_GATEWAY                 0X0044

/*
Manufacturer Name Register
This register stores a string containing the manufacturer name. This string uses the character set indicated in
the Device Mode register.
[R-437cd] A device MUST implement the Manufacturer Name register.
Access type: Read
Length :     32 bytes

Bits       Name                                    Description
 -     manufacturer_name String         indicating the manufacturer name. Returned in DISCOVERY_ACK and
                                        in the TXT record of DNS-SD.
����������
*/
#define GVR_MAUFACTRURER_NAME               0X0048

/*
Model Name Register
This register stores a string containing the device model name. This string uses the character set indicated in
the Device Mode register.
[R-438cd] A device MUST implement the Model Name register.
Length :32 bytes
Access type: Read
Bits                Name                                Description
 -          model_name String                 indicating the device model. Returned in DISCOVERY_ACK and in the
                                              TXT record of DNS-SD.

�豸����
*/
#define GVR_MODEL_NAME                      0X0068

/*
Device Version Register
This register stores a string containing the version of the device. This string uses the character set indicated
in the Device Mode register. The XML device description file should also provide this information to
ensure the device matches the description file.
[R-439cd] A device MUST implement the Device Version register.
Length :32 bytes
Access type :Read
Bits                Name                                Description
 -          device_version String               indicating the version of the device. Returned in DISCOVERY_ACK
                                                and in the TXT record of DNS-SD.
�豸�İ汾�ţ�XML�е�GVR_DEVICE_VERSIONҲҪ��֮ƥ��
*/
#define GVR_DEVICE_VERSION                  0X0088

/*
Manufacturer Info Register
This register stores a string containing additional manufacturer-specific information about the device. This
string uses the character set indicated in the Device Mode register.
[R-440cd] A device MUST implement the Manufacturer Info register.
Length: 48 bytes
Access type: Read
Bits                Name                                Description
 -      manufacturer_specific_information       String providing additional information about this device. Returned in
                                                DISCOVERY_ACK and in the TXT record of DNS-SD.
������Ϣ
*/
#define GVR_MANUFACTURER_INFO               0X00A8

/*
Serial Number Register
This register is optional. It can store a string containing the serial number of the device. This string uses the
character set indicated in the Device Mode register.
An application can check bit 1 of the GVCP Capability register at address 0x0934 to check if the serial
number register is supported by the device.
[O-441cd] A device SHOULD implement the Serial Number register.
Length :16 bytes
Access type :Read
Bits                Name                                Description
 -              serial_number                   String providing the serial number of this device. If supported, returned in
                                                DISCOVERY_ACK and in the TXT record of DNS-SD.
*/
//�豸���к�
#define GVR_SERIAL_NUMER                    0X00D8

/*
User-defined Name Register
This register is optional. It can store a user-programmable string providing the name of the device. This
string uses the character set indicated in the Device Mode register.
An application can check bit 0 of the GVCP Capability register at address 0x0934 to check if the userdefined
name register is supported by the device.
[O-442cd] A device SHOULD implement the User-defined Name register.
[CR-443cd] When the User-defined Name register is supported, the device MUST provide
persistent storage to memorize the user-defined name. This name shall remain readable
across power-up cycles. The state of this register is stored in non-volatile memory as
soon as the User-defined Name register is set by an application. [CR27-18cd]
Length :16 bytes
Access type :Read/Write

Bits                Name                                Description
 -              user_defined_name               String providing the device name. If supported, returned in DISCOVERY_ACK
                                                and in the TXT record of DNS-SD.

*/
//�����洢�û��趨���豸��
#define GVR_USERDEF_NAME                    0X00E8

/*
First URL Register
This register stores the first URL to the XML device description file. This string uses the character set
indicated in the Device Mode register. The first URL is used as the first choice by the application to
retrieve the XML device description file, except if a Manifest Table is available.
[R-444cd] A device MUST implement the First URL register, even if a Manifest Table is present.
Length: 512 bytes
Access type: Read
Bits            Name                                        Description
-           first_URL                           String providing the first URL to the XML device description file.
*/
#define GVR_FIRST_URL                       0X0200

/*
Second URL Register
This register stores the second URL to the XML device description file. This string uses the character set
indicated in the Device Mode register. This URL is an alternative if the application was unsuccessful to
retrieve the device description file using the first URL.
[R-445cd] A device MUST implement the Second URL register, even if a Manifest Table is
present.
Length: 512 bytes
Access type: Read
Bits            Name                                        Description
-           first_URL String                    providing the first URL to the XML device description file.
*/
#define GVR_SECOND_URL                      0X0400

/*
Number of Network Interfaces Register
This register indicates the number of network interfaces supported by this device. This is generally
equivalent to the number of Ethernet connectors on the device, except when Link Aggregation is used. In
this case, the physical interfaces regrouped by the Aggregator are counted as one virtual interface.
[R-446cd] A device MUST implement the Number of Network Interfaces register.
A device need to support at least one network interfaces (the primary interface = interface #0). A device can
support at most four network interfaces.
Note that interface #0 is the only one supporting GVCP. Additional network interfaces only supports stream
channels in order to increase available bandwidth out of the device.
Length: 4 bytes
Access type :Read
Bits             Name                                       Description
0 �C 28     reserved Always 0.                  Ignore when reading.
29 �C 31    number_of_interfaces                Indicates the number of network interfaces. This field takes a value from 1 to 4.
                                                All other values are invalid.
                                                �����豸֧������ӿڵ�������ȡֵ1~4
*/
#define GVR_NET_INTERFACE_NUMBER            0X0600

/*
Persistent IP Address Registers
These optional registers indicate the persistent IP address for the given network interface. They are only
used when the device boots with the Persistent IP configuration scheme. If link aggregation is used, this is
the persistent IP address used by the link aggregation group.
[CR-447cd] When Persistent IP is supported, the device MUST implement Persistent IP Address
register on all of its supported network interfaces. Devices must return
GEV_STATUS_INVALID_ADDRESS for unsupported network interfaces. [CR27-21cd]
Length :4 bytes
Access type :Read/Write
  Bits          Name                                        Description
0 �C 31      persistent_IP_address              IPv4 persistent IP address for this network interface
*/
//�豸�Ĺ̶�IP
#define GVR_PERSISTENT_IP                   0X064C

/*
Persistent Subnet Mask Registers
These optional registers indicate the persistent subnet mask associated with the persistent IP address on the
given network interface. They are only used when the device boots with the Persistent IP configuration
scheme. If link aggregation is used, this is the persistent subnet mask used by the link aggregation group.
[CR-448cd] When Persistent IP is supported, the device MUST implement Persistent Subnet Mask
register on all of its supported network interfaces. Devices must return
GEV_STATUS_INVALID_ADDRESS for unsupported network interfaces. [CR27-22cd]
  Bits          Name                                        Description
0 �C 31     persistent_subnet_mask              IPv4 persistent subnet mask for this network interface
*/
#define GVR_PERSISTENT_MASK                 0X065C

/*
Persistent Default Gateway Registers
These optional registers indicate the persistent default gateway for the given network interface. They are
only used when the device boots with the Persistent IP configuration scheme. If link aggregation is used,
this is the persistent default gateway used by the link aggregation group.
[CR-449cd] When Persistent IP is supported, the device MUST implement Persistent Default
Gateway register on all of its supported network interfaces. Devices must return
GEV_STATUS_INVALID_ADDRESS for unsupported network interfaces. [CR27-23cd]
  Bits        Name                                          Description
0 �C 31   persistent_default_gateway            IPv4 persistent default gateway for this network interface
*/
#define GVR_PERSISTENT_GATEWAY              0X066C

/*
Link Speed Registers
These optional registers provide the Ethernet link speed (in Mbits per second) for the given network
interface. This can be used to compute the transmission speed. If link aggregation is used, this is aggregated
link speed of all links participating in the aggregator.
[CR-450cd] When Link Speed registers are supported, the device MUST implement a Link Speed
register on all of its supported network interfaces. Devices must return
GEV_STATUS_INVALID_ADDRESS for unsupported network interfaces. [CR27-38cd]
For instance, if a device only implements a single network interface, then it shall only support one link
speed registers assigned to network interface #0. A link speed of 1 gigabit per second is equal to 1000 Mbits
per second while a link speed of 10 GigE is equal to 10 000 Mbits per second.
A capability bit in the GVCP Capability register (bit 3 at address 0x0934) indicates if these registers are
supported.
Length :4 bytes
Access type: Read

  Bits          Name                                    Description
0 �C 31      link_speed                         Ethernet link speed value in Mbps. 0 if Link is down.
*/
#define GVR_LINK_SPEED                      0X0670

/*
Number of Message Channels Register
This register reports the number of message channels supported by this device.
[R-451cd] A device MUST implement the Number of Message Channels register.
A device may support at most 1 message channel, but it is allowed to support no message channel  
Access type :Read
Bits            Name                                    Description
0 �C 31 number_of_message_channels              Number of message channels supported by this device. Can be 0 or 1.
*/
//���֧��1����Ϣͨ������Ҳ����û����Ϣͨ������ֵ��Ϊ1Ҳ��Ϊ0
#define GVR_NUMBER_OF_MESSAGE_CHANNEL       0X0900

/*
Number of Stream Channels Register
This register reports the number of stream channels supported by this device.
[R-452cd] A device MUST implement the Number of Stream Channels register.
A product can support from 0 up to 512 stream channels.
Access type :Read
Bits                    Name                             Description
0 �C 31         number_of_stream_channels       Number of stream channels supported by this device. A value from 0 to 512.
*/
//��ͨ��֧��0~512
#define GVR_NUMBER_OF_STREAM_CHANNEL        0X0904

/*
Number of Action Signals Register
This optional register reports the number of action signal supported by this device.
[CR-453cd] If a device supports Action commands, then it MUST implement the Number of Action
Signals register.
Access type :Read
Bits                    Name                            Description
0 �C 31         number_of_action_signals        Number of action signals supported by this device. A value from 0 to 128.
*/
#define GVR_NUMBER_OF_ACTION_REG            0X0908

/*
Action Device Key Register
This optional register provides the device key to check the validity of action commands. The action device
key is write-only to hide the key if CCP is not in exclusive access mode. The intention is for the Primary
Application to have absolute control. The Primary Application can send the key to a Secondary Application.
This mechanism prevents a rogue application to have access to the ACTION_CMD mechanism.
[CR-454cd] If a device supports Action commands, then it MUST implement the Action Device Key
register.
Length :4 bytes
Access type :Write-only
Bits                        Name                        Description
0 �C 31         action_device_key                Device key to check the validity of action commands
*/
#define GVR_ACTION_DEVICE_KEY               0X090C

/*
Number of Active Links
This register indicates the current number of active links. A link is considered active as soon as it is
connected to another Ethernet device. This happens after Ethernet link negotiation is completed.
[R-455cd] A device MUST implement the Number of Active Links register
Length: 4 bytes
Access type: Read
Bits                        Name                        Description
0 �C 27             reserved Always 0.          Ignore when reading.
28 �C 31            number_of_active_links      A value indicating the number of active links. This number must be 
                                                lower or equal to the Number of Network Interfaces reported at 
                                                address 0x0600.Single network interface device shall report a 
                                                value of 1.
���ڻ���ӵ�������1~4
*/
#define GVR_NUMBER_OF_ACTIVE_LINKS          0X0910

/*
GVSP Capability Register
This register reports the optional GVSP stream channel features supported by this device.
[R-456cd] A transmitter, receiver or transceiver device MUST implement the GVSP Capability
register.
Length 4 :bytes
Access type :Read
Bits                        Name                        Description
 0                  SCSPx_supported (SP)        Indicates the SCSPx registers (stream channel source port) are available for all
                                                supported stream channels.
 1        legacy_16bit_block_id_supported (LB)  Indicates this GVSP transmitter or receiver can support 16-bit block_id. Note
                                                that GigE Vision 2.0 transmitters and receivers MUST support 64-bit block_id64.
                                                Note: When 16-bit block_id are used, then 24-bit packet_id must be used.
                                                When 64-bit block_id64 are used, then 32-bit packet_id32 must be used.
2 �C 31             Reserved Always 0.          Ignore when reading.
*/
#define GVR_GVSP_CAPABILITY                 0X092C

/*
Message Channel Capability Register
This register reports the optional message channel features supported by this device.
[R-457cd] A device MUST implement the Message Channel Capability register.
Length :4 bytes
Access type :Read
Bits                        Name                        Description
 0                  MCSP_supported (SP)         Indicates the MCSP register (message channel source port) is available for the
                                                message channel.
1 �C 31             reserved Always 0.          Ignore when reading.
*/
#define GVR_MESSAGE_CHANNEL_CAPABILITY      0X0930

/*
GVCP Capability Register
This register reports the optional GVCP commands and bootstrap registers supported by this device on its
Control Channel. When supported, some of these features are enabled through the GVCP Configuration
register (at address 0x0954).
[R-458cd] A device MUST implement the GVCP Capability register.
Length: 4 bytes
Access type: Read

Bits         Name                                           Description
0       user_defined_name (UN)                      User-defined name register is supported.
1       serial_number (SN)                          Serial number register is supported.
2       heartbeat_disable (HD)                      Heartbeat can be disabled.  ����
3       link_speed_register (LS)                    Link Speed registers are supported.
4       CCP_application_port_IP_register (CAP)      CCP Application Port and IP address registers are supported.
5       manifest_table (MT)                         Manifest Table is supported. When supported, the application must use the
                                                    Manifest Table to retrieve the XML device description file.
6       test_data (TD)                              Test packet is filled with data from the LFSR generator.
7       discovery_ACK_delay(DD)                     Discovery ACK Delay register is supported.
8       writable_discovery_ACK_delay (WD)           When Discovery ACK Delay register is supported, this bit indicates that the
                                                    application can write it. If this bit is 0, the register is read-only.
9       extended_status_code_for_1_1 (ES)           Support generation of extended status codes introduced in specification 1.1:
                                                    GEV_STATUS_PACKET_RESEND,
                                                    GEV_STATUS_PACKET_NOT_YET_AVAILABLE,
                                                    GEV_STATUS_PACKET_AND_PREV_REMOVED_FROM_MEMORY and
                                                    GEV_STATUS_PACKET_REMOVED_FROM_MEMORY.
10      primary_application_switchover (PAS)        Primary application switchover capability is supported.
11      unconditional_ACTION(UA)                    Unconditional ACTION_CMD is supported (i.e. ACTION_CMD are processed
                                                    when the primary control channel is closed).
12      IEEE1588_support(PTP)                       Support for IEEE 1588 PTP
13      extended_status_code_for_2_0 (ES2)          Support generation of extended status codes introduces in specification 2.0:
                                                    GEV_STATUS_PACKET_TEMPORARILY_UNAVAILABLE,
                                                    GEV_STATUS_OVERFLOW, GEV_STATUS_NO_REF_TIME.
14      scheduled_action_command (SAC)              Scheduled Action Commands are supported. When this bit is set, this also
                                                    indicates that the Scheduled Action Command Queue Size register is
                                                    present.
15 �C 24 reserved Always 0.                         Ignore when reading.    
25      ACTION (A)                                  ACTION_CMD and ACTION_ACK are supported.
26      PENDING_ACK (PA)                            PENDING_ACK is supported.
27      EVENTDATA (ED)                              EVENTDATA_CMD and EVENTDATA_ACK are supported.
28      EVENT (E)                                   EVENT_CMD and EVENT_ACK are supported.
29      PACKETRESEND (PR)                           PACKETRESEND_CMD is supported.
30      WRITEMEM (W)                                WRITEMEM_CMD and WRITEMEM_ACK are supported.
31      concatenation (C)                           Multiple operations in a single message are supported.
*/
#define GVR_GVCP_CAPABILITY                 0X0934

/*
Heartbeat Timeout Register
This register indicates the current heartbeat timeout in milliseconds.
[R-459cd] A device MUST implement the Heartbeat Timeout register.
A value smaller than 500 ms SHOULD be defaulted to 500 ms by the device. In this case,
the Heartbeat Timeout register content is changed to reflect the actual value used by the
device. [O27-24cd]
Length: 4 bytes
Access type :Read/Write
Bits        Name                    Description
0 �C 31     timeout           Heartbeat timeout in milliseconds (minimum is 500 ms)
*/
#define GVR_HEARTBEAT_TIMEOUT               0X0938

/*
Timestamp Tick Frequency Registers
These optional registers indicate the number of timestamp tick during 1 second. This corresponds to the
timestamp frequency in Hertz. For example, a 100 MHz clock would have a value of 100 000 000 =
0x05F5E100. They are combined to form a 64-bit value.
If IEEE 1588 is used, then the Timestamp Tick Frequency register must be 1 000 000 000 Hz (equivalent
to 1 GHz). This indicates that the timestamp value is reported in units of 1 ns. But the precision of the
timestamp register might be worst than 1 ns (i.e. the timestamp register does not necessarily takes all
possible values when it increments, it can skip some value depending on the precision of the IEEE 1588
clock).
Note the timestamp counter is also used to compute stream channel inter-packet delay.
No timestamp is supported if both of these registers are equal to 0.
[CR-461cd] If a device supports a device timestamp counter, then it MUST implement the
Timestamp Tick Frequency registers. Both registers MUST be available (High Part and
Low Part). If its value is set to 0 or if this register is not available, then no timestampcounter is
present. This means no mechanism is offered to control inter-packet delay. [O27-25cd]
An application needs to access the high part before accessing the low part:

High Part
Bits        Name                    Description
0 �C 31 timestamp_frequency     Timestamp frequency, upper 32-bit
Low Part
Bits        Name                    Description
0 �C 31 timestamp_frequency     Timestamp frequency, lower 32-bit

Access type :Read

*/
#define GVR_TIMESTAMP_TICK_FREQUNENCY_HIGH  0X093C
#define GVR_TIMESTAMP_TICK_FREQUNENCY_LOW   0X0940

/*
Timestamp Control Register
This optional register is used to control the timestamp counter.
[CR-462cd] If a device supports a device timestamp counter, then it MUST implement the
Timestamp Control register.
[CR-463ca] If a timestamp counter exists, an application MUST not attempt to read this register. This
register is write-only. [CR27-26ca]
Bits            Name                Description
0 �C 29         reserved            Always 0.
30              latch (L)           Latch current timestamp counter into Timestamp Value register 
                                    at address 0x0948.
31              reset (R)           Reset timestamp 64-bit counter to 0. It is not possible to reset the 
                                    timestamp when operating with an IEEE 1588 disciplined clock.
*/
#define GVR_TIMESTAMP_CONTROL               0X0944

/*
Timestamp Value Registers
These optional registers report the latched value of the timestamp counter. It is necessary to latch the 64-bit
timestamp value to guarantee its integrity when performing the two 32-bit read accesses to retrieve the
higher and lower 32-bit portions.
[CR-465cd] If a device supports a device timestamp counter, then it MUST implement the
Timestamp Value registers. Both registers MUST be available (High Part and Low Part)
[CR-466ca] If an application wants to retrieve the 64-bit value of the timestamp counter and the
timestamp counter is supported by the device, it MUST write 1 into bit 30 (latch) of
Timestamp Control register for the free-running timestamp counter to be copied into
these registers. [CR27-28ca]
[CR-467cd] The Timestamp Value register MUST always be 0 if timestamp is not supported. [CR27-
29cd]
These registers do not necessarily take all possible values when they increments: they can skip some value
depending on the precision of the clock source relative to the Timestamp Tick Frequency register. For
instance, if the device supports IEEE 1588, it must report a tick frequency of 1 GHz. But the quality of the
IEEE 1588 master clock might not provide that level of accuracy.
An application needs to access the high part before accessing the low part:
High Part
Bits        Name                        Description
0 �C 31  latched_timestamp_value     Latched timestamp value, upper 32-bit
Low Part
Bits        Name                        Description
0 �C 31  latched_timestamp_value     Latched timestamp value, lower 32-bit

Access type :Read
*/
#define GVR_TIMESTAMP_VALUE_HIGH            0X0948   
#define GVR_TIMESTAMP_VALUE_LOW             0X094C  

/*
Discovery ACK Delay Register
This optional register indicates the maximum randomized delay in milliseconds the device will wait upon
reception of a DISCOVERY_CMD before it will send back the DISCOVERY_ACK. This randomized
delay can take any value from 0 second up to the value indicated by this bootstrap register.
[O-468cd] A device SHOULD implement the Discovery ACK Delay register.
[CR-469cd] If Discovery ACK Delay register is writable, then its value MUST be persistent and
stored in non-volatile memory to be re-used on next device reset or power cycle.
[CR27-41cd]
[CR-470cd] If Discovery ACK Delay is supported, the maximum factory default value for this
register MUST be lower or equal to 1000 ms (1 second). [CR27-42cd]
The previous requirement is necessary to ensure backward compatibility with version 1.0 of the
specification.
This register can be read-only if the device only supports a fixed value. Furthermore a device is allowed to
set this register has read-only with a value of 0 if it does not support such a randomized delay. Bit 8
(writable_discovery_ACK_delay) of the GVCP Capability register (at address 0x0934) indicates if this
register is writable.
An application should retrieve information from the XML device description file to see if this register is
writable and to determine the maximum value it can take.

  Bits          Name                    Description
0 �C 15     reserved Always 0.      Ignore when reading.
16 �C 31        delay               Maximum random delay in ms to wait before sending DISCOVERY_ACK
                                    upon reception of DISCOVERY_CMD.
Access type: Read/(Write)

*/
#define GVR_DISCOVERY_ACK_DELAY             0X0950

/*
GVCP Configuration Register
This optional register provides additional control over GVCP. These additional functions must be indicated
by the GVCP Capability register (at address 0x0934).
For instance, it can be used to disable Heartbeat when this capability is supported. This can be useful for
debugging purposes.
[O-471cd] A device SHOULD implement the GVCP Configuration register.

    Bits            Name                                Description
   0�C11       reserved                     Always 0.Ignore when reading.
    12          IEEE1588_PTP_enable(PTP)    Enable usage of the IEEE 1588 Precision Time Protocol to source the
                                            timestamp register. Only available when the IEEE1588_support bit of the
                                            GVCP Capability register is set. When PTP is enabled, the Timestamp Control
                                            register cannot be used to reset the timestamp. Factory default is devicespecific.
                                            When PTP is enabled or disabled, the value of Timestamp Tick Frequency and
                                            Timestamp Value registers might change to reflect the new time domain.
    13    extended_status_code_for_2_0 (ES2)    Enable the generation of extended status codes introduced in specification 2.0:
                                                GEV_STATUS_PACKET_TEMPORARILY_UNAVAILABLE,
                                                GEV_STATUS_OVERFLOW, GEV_STATUS_NO_REF_TIME. Only
                                                available when the extended_status_code_for_2_0 bit of the GVCP Capability
                                                register is set.
                                                Factory default must be 0.
   14�C27        reserved                   Always 0. Ignore when reading.
    28    unconditional_ACTION_enable (UA)  Enable unconditional action command mode where ACTION_CMD are
                                            processed even when the primary control channel is closed. Only available
                                            when the unconditional_ACTION bit of the GVCP Capability register is set.
                                            Factory default must be 0.
                                            Note: unconditional_ACTION_enable is provided to allow some level of
                                            protection against inappropriate reception of action commands. This might be
                                            useful as a security measure in industrial control for instance.
    29    extended_status_code_for_1_1(ES)  Enable generation of extended status codes introduced in specification 1.1:
                                            GEV_STATUS_PACKET_RESEND,
                                            GEV_STATUS_PACKET_NOT_YET_AVAILABLE,
                                            GEV_STATUS_PACKET_AND_PREV_REMOVED_FROM_MEMORY and
                                            GEV_STATUS_PACKET_REMOVED_FROM_MEMORY. Only available if
                                            the extended_status_code_for_1_1 bit of the GVCP Capability register is set
                                            Factory default must be 0.
    30     PENDING_ACK_enable(PE)           Enable generation of PENDING_ACK by this device. Only available when the
                                            PENDING_ACK capability bit of the GVCP Capability register is set is set.
                                            Factory default must be 0.
    31     heartbeat_disable (HD)           Disable heartbeat. Only available when heartbeat_disable capability bit of the
                                            GVCP Capability register is set is set. Factory default must be 0.

Access type :Read/Write
Length :4 bytes
*/
#define GVR_GVCP_CONFIGURATION              0X0954

/*
Pending Timeout Register
This register indicates the longest GVCP command execution time before an ACK is returned. The packet
travel time on the network is not accounted for by this register.
[R-472cd] A device MUST implement the Pending Timeout register.
Two scenarios exist:
1. If PENDING_ACK is disabled, then this register represents the worst-case single GVCP command
execution time (not including concatenated read/write access).
2. If PENDING_ACK is enabled, then this register represents the maximum amount of time it takes
before a PENDING_ACK is issued.
Therefore, a device might change the value of this read-only register when PENDING_ACK is enabled or
disabled through the GVCP Configuration register (bit 30, PENDING_ACK_enable field, at address
0x0954).
The Application can use this value to deduce a suitable ACK timeout to wait for when it issues the various
GVCP commands.
Length :4 bytes
Access type :Read

Bits            Name                        Description
0 �C 31     max_execution_time      Provide the worst-case execution time (in ms) before the device will issue a
                                    PENDING_ACK, if supported and enabled, to notify the application to
                                    extend the ACK timeout for the current GVCP command. This time is for
                                    non-concatenated read/write accesses.
��ִ��GVCP�������ACK�����ʱ��
*/
#define GVR_PENDING_TIMEOUT                 0X0958

/*
Control Switchover Key Register
This optional register provides the key to authenticate primary application switchover requests. The register
is write-only to hide the key from secondary applications. The primary intent is to have a mechanism to
control who can take control over a device. The primary application or a higher level system management
entity can send the key to an application that would request control over a device.
[CR-473cd] If a device supports primary application switchover, then it MUST implement the
Control Switchover Key register.
Length: 4 bytes
Access type: Write-only
  Bits          Name                        Description
 0 �C 15     reserved                Always 0.
16 �C 31  control_switchover_key     Key to authenticate primary application switchover requests
*/
#define GVR_CONTROL_SWITCHOVER_KEY          0X095C

/*
GVSP Configuration Register
This optional register provides additional global control over GVSP configuration. These additional
functions are indicated by the GVSP Capability register (at address 0x092C).
[O-474cd] A device SHOULD implement the GVSP Configuration register.
Length :4 bytes
Access type :Read/Write
Bits        Name                     Description
0       reserved                Always 0. Ignore when reading.
1    64bit_block_id_enable(BL)  Enable the 64-bit block_id64 for GVSP. This bit cannot be reset if the 
                                stream channels do not support the standard ID mode (i.e. EI field is always 0).
                                Note: If a transmitter supports 16-bit block_id, as reported by bit 1
                                (legacy_16bit_block_id_supported) of the GVSP Capability register (at
                                address 0x092C), then it SHOULD have this bit set to 0 as the factory default to
                                start in 16-bit block_id mode. Otherwise, it cannot be advertised as a GigE
                                Vision 1.x device.
                                Note: When 16-bit block_ids are used, then 24-bit packet_ids must be used.
                                When 64-bit block_id64s are used, then 32-bit packet_id32s must be used.
2�C31    reserved               Always 0. Ignore when reading.
*/
#define GVR_GVSP_CONFIGURATION              0X0960

/*
Physical Link Configuration Capability Register
This register indicates the physical link configuration supported by this device.
[R-475cd] A device MUST implement the Physical Link Configuration Capability register.
Length: 4 bytes
Access type :Read
Bits            Name                        Description
0�C27       reserved Always 0.          Ignore when reading.
28         dynamic_link_aggregation_group_configuration(dLAG)
                                        This device supports dynamic LAG configuration.
29        static_link_aggregation_group_configuration(sLAG)
                                        This device supports static LAG configuration.
30 multiple_links_configuration (ML)    This device supports multiple link (ML) configuration.
31  single_link_configuration (SL)      This device supports single link (SL) configuration.
*/
#define GVR_PHYSICAL_LINK_CONFIG_CAPABILITY 0X0964

/*
Physical Link Configuration Register
This register indicates the principal physical link configuration currently enabled on this device. It should be
used in conjunction with the Physical Link Configuration Capability register to determine which values
are valid. After a change in configuration, it is necessary to restart the device (either by power cycling the
device or by some other mean) in order for the new setting to take effect.
[R-476cd] A device MUST implement the Physical Link Configuration register.
Length :4 bytes
Access type :Read/Write
  Bits          Name                                    Description
0 �C 29     reserved                                Always 0. Ignore when reading.
30 - 31     link_configuration Principal            Physical Link Configuration to use on next restart/power-up of the device.
                                                    0: Single Link Configuration
                                                    1: Multiple Links Configuration
                                                    2: Static LAG Configuration
                                                    3: Dynamic LAG Configuration
                                                    Note: IP configuration is not sufficient for the device to use the new link
                                                    configuration. Hence FORCEIP_CMD cannot be used to use the new link
                                                    configuration settings.

*/
#define GVR_PHYSIC_LINK_CONFIG              0X0968

/*
IEEE 1588 Status Register
This optional register indicates the state of the IEEE 1588 clock.
[CR-477cd] If a device supports IEEE 1588, then it MUST implement the IEEE 1588 Status
register.
Length: 4 bytes
Access type :Read
 Bits       Name                                Description
0�C27     reserved                           Always 0. Ignore when reading.
28�C31  clock_status                         Provides the state of the IEEE 1588 clock. Values of this field must match the IEEE
                                            1588 PTP port state enumeration (INITIALIZING, FAULTY, DISABLED,
                                            LISTENING, PRE_MASTER, MASTER, PASSIVE, UNCALIBRATED, SLAVE).
                                            Please refer to IEEE 1588 for additional information.
*/
#define GVR_IEEE_1588_STATUS                0X096C

/*
Scheduled Action Command Queue Size Register
This optional register reports the number of Scheduled Action Commands that can be queued at any given
time. This is essentially the size of the queue. This register is present when bit 14
(scheduled_action_command) of the GVCP Capability register is set
[CR-478cd] If a device supports Scheduled Action Commands, then it MUST implement the
Scheduled Action Command Queue Size register.
Length :4 bytes
Access type: Read
Bits            Name            Description
0 �C 31     queue_size      Indicates the size of the queue. This number represents the maximum
                            number of Scheduled Action Commands that can be pending at a given
                            point in time.

*/
#define GVR_SCHEDULED_AC_QUEUE_SIZE         0X0970

/*
Control Channel Privilege Register (CCP)   ͬһʱ��ֻ�ܱ�һ��Ӧ�ó���ռ�ã�����Ӧ�ó����з��ʵ�Ȩ��
This register is used to grant privilege to an application. Only one application is allowed to control the
device. This application is able to write into device��s registers. Other applications can read device��s register
only if the controlling application does not have the exclusive privilege.
[R-479cd] A device MUST implement the Control Channel Privilege register.

Since exclusive access is more restrictive than control access (without or with switchover enabled), the
device must be in exclusive access mode if the exclusive_access bit is set independently of the state of the
control_access and control_switchover_enable bits. Control access is defined according to Table 28-2.

Table 28-2: Control Access Definition
control_switchover_enable   control_access      exclusive_access        Access Mode
        x                        0                      0               Open access.
        0                        1                      0               Control access.  
        1                        1                      0               Control access with switchover enabled.
        x                        x                      1               Exclusive access.  ��ռ


Length :4 bytes
Access type :Read/Write

  Bits              Name                                      Description
0 �C 15     control_switchover_key                  This field is applicable only when the device supports the primary
                                                    application switchover capability. It is taken into account only when the
                                                    control_switchover_enable bit (bit 29) was set prior to the current
                                                    transaction writing to the CCP register and the transaction comes from a
                                                    different application. In this case, the primary application switchover will
                                                    occur only if the value written to this field matches the value in the Control
                                                    Switchover Key bootstrap register.
                                                    No matter if the primary application switchover capability is supported or
                                                    not, this field always reads as zero.
16 �C 28            reserved                        Always 0. Ignore when reading.
   29    control_switchover_enable(CSE)             This bit is applicable only when the device supports the primary application
                                                    switchover capability. The application writing a 1 to this bit enables another
                                                    application to request exclusive or control access to the device (without or
                                                    with switchover enabled). This bit is used in conjunction with the
                                                    control_access bit (bit 30). It is ��don��t care�� when the exclusive_access bit
                                                    (bit 31) is set.
                                                    This bit always reads as zero when the primary application switchover
                                                    capability is not supported.
   30       control_access (CA)                     The application writing a 1 to this bit requests control access to the device
                                                    (without or with switchover enabled depending of the value written to the
                                                    control_switchover_enable bit). If a device grants control access, no other
                                                    application can control the device if the control_switchover_enable bit is
                                                    set to zero. Otherwise, another application can request to take over
                                                    exclusive or control access of the device (without or with switchover
                                                    enabled). Other applications are still able to monitor the device.
   31       exclusive_access (EA)                   The application writing a 1 to this bit requests exclusive access to the
                                                    device. If a device grants exclusive access, no other application can control
                                                    or monitor the device. exclusive_access has priority over control_access
                                                    when both bits are set.

control_switchover_enable  control_access  exclusive_access  Access Mode 
            x                       0               0           Open access. 
            0                       1               0           Control access. 
            1                       1               0           Control access with switchover enabled. 
            x                       x               1           Exclusive access. 


*/
#define GVR_CONTROL_CHANNEL_PRIVILEGE       0x0A00


/*
Primary Application Port Register
This optional register provides UDP port information about the primary application holding the control
channel privilege.
[O-480cd] A device SHOULD implement the Primary Application Port register.

  Bits          Name                            Description
0 �C 15         reserved                    Always 0. Ignore when reading.
16 �C 31  primary_application_port          The UDP source port of the primary application. This value must be 0 when
                                            no primary application is bound to the device (CCP register equal to 0).
Length: 4 bytes
Access type :Read  
�������½�����ʱ��������Ķ˿ں�
*/
#define GVR_PRIMARY_APP_PORT                0X0A04

/*
Primary Application IP Address Register
This optional register provides IP address information about the primary application holding the control
channel privilege.
[O-481cd] A device SHOULD implement the Primary Application IP Address register.
  Bits          Name                                Description
0 �C 31   primary_application_IP_address        The IPv4 address of the primary application. This must be a unicast address.
                                                This value must be 0 when no primary application is bound to the device
                                                (CCP register equal to 0).
Length :4 bytes
Access type: Read
�������½�����ʱ���������IP
*/
#define GVR_PRIMARY_APP_IP                  0X0A14

/*
Message Channel Port Register (MCP)
This optional register provides port information about the message channel.
[CR-482cd] If a device supports a Message Channel, then it MUST implement the Message
Channel Port register.

  Bits            Name                    Description
0 �C 11         reserved            Always 0. Ignore when reading.
12 �C 15  network_interface_index   Always 0 in this version. Only the primary interface (#0) supports GVCP.
16 �C 31       host_port            The port to which the device must send messages. Setting this value to 0
                                    closes the message channel.    ������˿ڷ�����Ϣ��Ϊ0ʱ��ʾ��Ϣ�˿ڹر�

Address 0x0B00 [optional]
Length :4 bytes
Access type :Read/Write
*/
#define GVR_MESSAGE_CHANNEL_PORT            0X0B00

/*
Message Channel Destination Address Register (MCDA)
This optional register indicates the destination IP address for the message channel.
[CR-483cd] If a device supports a Message Channel, then it MUST implement the Message
Channel Destination Address register.

Bits            Name                    Description
0 �C 31     channel_destination_IP      Message channel destination IPv4 address. The destination address can be a
                                        multicast or a unicast address.
										��Ϣ���յ��ַ�������ǹ㲥��ַ��Ҳ�����ǵ���

Address 0x0B10 [optional]
Length :4 bytes
Access type :Read/Write
*/
#define GVR_MESSAGE_CHANNEL_DESTI_ADDR      0X0B10

/*
Message Channel Transmission Timeout Register (MCTT)
This optional register provides the transmission timeout value in milliseconds.
[CR-484cd] If a device supports a Message Channel, then it MUST implement the Message
Channel Transmission Timeout register.
[CR-485cd] This register indicates the amount of time the device MUST wait for acknowledge after a
message is sent on the message channel before timeout when an acknowledge is
requested when a message channel is supported. [CR27-32cd]

Address 0x0B14 [optional]
Length :4 bytes
Access type :Read/Write

Bits        Name                        Description
0 �C 31    timeout      Transmission timeout value in ms. Set to 0 to disable acknowledge on message channel.
*/
#define GVR_MESSAGE_CHANNEL_TRANS_TIMEOUT   0X0B14

/*
Message Channel Retry Count Register (MCRC)
This optional register indicates the number of retransmissions allowed when a message channel message
times out.
[CR-486cd] If a device supports a Message Channel, then it MUST implement the Message
Channel Retry Count register.

Address 0x0B18 [optional]
Length: 4 bytes
Access type :Read/Write

Bits        Name            Description
0 �C 31  retry_count     Number of retransmissions allowed on the message channel.
						�ش�����
*/
#define GVR_MESSAGE_CHANNEL_RETRY_COUNT     0X0B18

/*
Message Channel Source Port Register (MCSP)
This optional register indicates the source UDP port for the message channel. The purpose of this
information is to allow the host application to take measures to ensure asynchronous event traffic from the
device is allowed back to the host. The expected usage of this is to send dummy UDP packets from the
application��s message channel reception port to that device��s source port, thus making the message channel
traffic appear to be requested by the application instead of unsolicited.
[CO-487cd] If a device supports a Message Channel, then it SHOULD implement the Message
Channel Source Port register.

Address 0x0B1C [optional]
Length :4 bytes
Access type :Read-Only

Bits        Name                Description
0 - 15  reserved            Always 0. Ignore when reading.
16 - 31 source_port         Indicates the UDP source port message channel traffic will be
                            generated from while the message channel is open.

*/
#define GVR_MESSAGE_CHANNEL_SRC_PORT        0X0B1C

/*
Stream Channel Port Registers (SCPx)
These registers provide port information for the given stream channel.
[CR-488cd] If a device supports stream channels, it MUST implement the Stream Channel Port
register for all of its supported stream channels.
Address 0x0D00 + 0x40 * x
with 0 �� x < 512.
[optional for 0 < x < 512 for GVSP transmitters and receivers]
[Not applicable for peripherals]
Length :4 bytes
Access type :Read/Write

 Bits               Name                            Description
   0              direction (D)         This bit is read-only and reports the direction of the stream channel. It must
                                        take one of the following values
                                        0: Transmitter
                                        1: Receiver
1 �C 11            reserved             Always 0. Ignore when reading.
12�C15    network_interface_index       Index of network interface to use (from 0 to 3). Specific streams might be
                                        hard-coded to a specific network interfaces. Therefore, this field might not
                                        be programmable on certain devices. It is read-only for this case.
                                        For link aggregation configuration, only a single network interface is made
                                        visible by the device.
16 - 31           host_port             The port to which a GVSP transmitter must send data stream. The port from
                                        which a GVSP receiver may receive data stream. Setting this value to 0
                                        closes the stream channel. For a GVSP transmitter, the stream channel
                                        block ID must be reset to 1 when the stream channel is opened.

*/
#define GVR_STREAM_CHANNEL_PORT             0X0D00

/*
Stream Channel Packet Size Registers (SCPSx)
These registers indicate the packet size in bytes for the given stream channel. This is the total packet size,
including all headers (IP header = 20 bytes, UDP header = 8 bytes and GVSP header). For GVSP
transmitters, they also provide a way to set the IP header ��don��t fragment�� bit and to send stream test packet
to the GVSP receiver.
[CR-489cd] If a device supports stream channels, it MUST implement the Stream Channel Packet
Size register for all of its supported stream channels.
Write access to this register is not possible while streaming (transmission) is active on this channel
The packet size is used by the application to determine where it has to copy the data in the receiving buffer.
As such, it is important that this value remains the same once it has been configured by the application.
[R-490st] A GVSP transmitter is allowed to round the value of the packet_size field to the nearest
supported value when the application writes to the SCPSx register. But the transmitter
MUST not change the packet size value without having the application directly writing
into this register.
For instance, it is allowed for the GVSP transmitter to round a packet size value to a multiple of 4 bytes, but
it is not allowed for the transmitter to change the packet size if the application changes the data format (such
as the image width or pixel format for a camera device).
It is recommended for the application to read back the packet size after writing to SCPSx since the value
could have been rounded by the GVSP transmitter. The application should also ensure the packet size
contains a valid value before asking for a Test Packet.

Address 0x0D04 + 0x40 * x
with 0 �� x < 512.
[optional for 0 < x < 512 for GVSP transmitters and receivers]
[Not applicable for peripherals]
Length :4 bytes
Access type :Read/Write

  Bits              Name                                    Description
   0         fire_test_packet (F)                   When this bit is set and the stream channel is a transmitter, the GVSP
                                                    transmitter will fire one test packet of size specified by bits 16 to 31. The
                                                    ��don��t fragment�� bit of IP header must be set for this test packet. If the
                                                    GVSP transmitter supports the LFSR generator, then the test packet
                                                    payload will be filled with data according to this polynomial. Otherwise,
                                                    the payload data is ��don��t care��.
                                                    When the stream channel is a receiver, this bit doesn��t have any effect and
                                                    always reads as zero.
   1        do_not_fragment (D)                     For GVSP transmitters, this bit is copied into the ��don��t fragment�� bit of IP
                                                    header of each stream packet. It can be used by the application to prevent IP
                                                    fragmentation of packets on the stream channel.
                                                    When the stream channel is a receiver, this bit doesn��t have any effect and
                                                    always reads as zero.
   2        pixel_endianess (P)                     Endianess of multi-byte pixel data for this stream.
                                                    0: little-endian
                                                    1: big-endian
                                                    This is an optional feature. A GVSP transmitter or receiver that does not
                                                    support this feature must support little-endian and always leave that bit
                                                    clear.
3 �C 15         reserved                            Always 0. Ignore when reading.
16 �C 31        packet_size                         For GVSP transmitters, this field represents the stream packet size to send
                                                    on this channel, except for data leader and data trailer; and the last data
                                                    packet which might be of smaller size (since packet size is not necessarily a
                                                    multiple of block size for stream channel). The value is in bytes. For a
                                                    camera, it can be accessed through the GevSCPSPacketSize mandatory
                                                    feature.
                                                    If a GVSP transmitter cannot support the requested packet_size, then it
                                                    must not fire a test packet when requested to do so.
                                                    For GVSP receivers, this field represents the maximum GVSP packet size
                                                    supported by the receiver. It is read-only.
*/
#define GVR_STREAM_CHANNEL_PACK_SIZE        0X0D04

/*
Stream Channel Packet Delay Registers (SCPDx)
These registers indicate the minimal delay (in timestamp counter unit) to insert between each packet
transmitted on a given physical link for the stream channel. This can be used as a crude flow-control
mechanism if the GVSP receiver cannot keep up with the packets coming from the device.
Note: The above essentially means that each network interface must have a separate timer per stream to
measure the inter-packet delay.
As an addition to the inter-packet delay, a device might elect to support the PAUSE option of IEEE 802.3.
[CR-491cd] If a device supports stream channels, it MUST implement the Stream Channel Packet
Delay register for all of its supported stream channels.
This delay normally uses the same granularity as the timestamp counter to ensure a very high precision in
the packet delay. This counter is typically of very high frequency. Inter-packet delay is typically very small.
If the timestamp is not supported, then this register has no effect.

Address 0x0D08 + 0x40 * x
with 0 �� x < 512.
[optional for 0 < x < 512 for GVSP transmitters]
[Not applicable for GVSP receivers and peripherals]
Length :4 bytes
Access type :Read/Write
Bits        Name        Description
0 �C 31    delay        Inter-packet delay in timestamp ticks.
*/
#define GVR_STREAM_CHANNEL_PACKET_DELAY     0X0D08

/*
Stream Channel Destination Address Registers (SCDAx)
For GVSP transmitters, these registers indicate the destination IP address for the given stream channel. For
GVSP receivers, these registers indicate the destination IP address from which the given receiver may
receive data stream from.
[R-492cd] If a device supports stream channels, it MUST implement the Stream Channel
Destination Address register for all of its supported Stream Channels.
For GVSP transmitters, write access to this register is not possible while streaming is active on this channel.

Address :0x0D18 + 0x40 * x
with 0 �� x < 512.
[optional for 0 < x < 512 for GVSP transmitters and receivers]
[Not applicable for peripherals]
Length :4 bytes
Access type: Read/Write
Factory Default 0 if transmitter (SCPx_direction field is set to 0)
Device-specific if receiver (SCPx_direction field is set to 1)

  Bits         Name                         Description
0 �C 31   channel_destination_IP        Stream channel destination IPv4 address. The destination address can be a
                                        multicast or a unicast address.
*/
#define GVR_STREAM_CHANNEL_DEST_ADDR        0X0D18

/*
Stream Channel Source Port Registers (SCSPx)
These optional registers indicate the source UDP port for the given GVSP transmitter stream channel. The
purpose of this information is to allow the host application to take measures to ensure streaming traffic from
the device is allowed back to the GVSP receiver. The expected usage of this is to send dummy UDP packets
from the GVSP receiver stream reception port to that GVSP transmitter source port, thus making the
streaming traffic appear to be requested by the GVSP receiver instead of unsolicited.
[CO-493cd] If a device supports stream channels, it SHOULD implement the Stream Channel
Source Port register for all of its supported stream channels.

Address:0x0D1C + 0x40 * x
with 0 �� x < 512.
[optional for 0 �� x < 512 for GVSP transmitters]
[Not applicable for GVSP receivers and peripherals]
Length :4 bytes
Access type :Read-Only
Factory Default :0 if unavailable

  Bits      Name                    Description
0 - 15    reserved Always 0.        Ignore when reading.
16 - 31   source_port Indicates     the UDP source port GVSP traffic will be generated from while the
                                    streaming channel is open.

*/
#define GVR_STREAM_CHANNEL_SRC_PORT         0X0D1C

/*
Stream Channel Capability Registers (SCCx)
These optional registers provide a list of capabilities specific to the given stream channel.
[CO-494cd] If a device supports stream channels, it SHOULD implement the Stream Channel
Capability register for all of its supported stream channels.

Address 0x0D20 + 0x40 * x
with 0 �� x < 512.
[optional for 0 �� x < 512 for GVSP transmitters and receivers]
[Not applicable for peripherals]
Length 4 bytes
Access type Read
Factory Default Device-specific

 Bits       Name                            Description
  0 big_and_little_endian_supported (BE)    Indicates this stream channel supports both big and little-endian. Note that all
                                            stream channels must support little-endian.
  1  IP_reassembly_supported (R)            For GVSP receivers, indicates this stream channel supports the reassembly of
                                            fragmented IP packets. Otherwise, it reads as zero.
 2-26     reserved                          Always 0. Ignore when reading.
  27   multi_zone_supported(MZ)             Set to 1 to indicate this stream channel supports the SCZx and SCZDx
                                            registers.
  28 packet_resend_destination_option (PRD) Indicates if an alternate destination exists for Packet resend request. This is only
                                            valid for GVSP transmitter.
                                            For GVSP transmitter:
                                            0: No alternative, always use the destination specified by SCDAx.
                                            1: Use the destination indicated by the PRD flag of the SCCFGx register.
                                            For GVSP receiver:
                                            must be 0.
  29   all_in_transmission_supported (AIT)  For GVSP transmitters, indicates that the stream channel supports the All-in
                                            Transmission mode. Otherwise, it reads as zero.
  30 unconditional_streaming_supported (US) For GVSP transmitters, indicates that the stream channel supports unconditional
                                            streaming capabilities. Otherwise, it reads as zero.
  31  extended_chunk_data_supported (EC)    Indicates that the stream channel supports the deprecated extended chunk data
                                            payload type. Otherwise, it reads as zero.                                          
*/
#define GVR_STREAM_CHANNEL_CAPABILITY       0X0D20

/*
Stream Channel Configuration Registers (SCCFGx)
These optional registers provide additional control over optional features specific to the given stream
channel. These optional features must be indicated by the Stream Channel Capability register of the given
stream channel.
For instance, they can be used to enable GVSP transmitters to use the All-in Transmission mode.
[CO-495cd] If a device supports stream channels, it SHOULD implement the Stream Channel
Configuration register for all of its supported stream channels.

Address 0x0D24 + 0x40 * x
with 0 �� x < 512.
[optional for 0 �� x < 512 for GVSP transmitters and receivers]
[Not applicable for peripherals]
Length 4 bytes
Access type Read/Write
Factory Default 0

  Bits              Name                                  Description
0 �C 27           reserved                              Always 0. Ignore when reading.
  28     packet_resend_destination (PRD)                Enable the alternate IP destination for stream packet resent due to a Packet
                                                        Resend request. This is only valid for GVSP transmitter.
                                                        For GVSP transmitter:
                                                        0: Use SCDAx
                                                        1: Use the source IP address provided in the PACKETRESEND_CMD packet
                                                        For GVSP receiver:
                                                        don��t care.
  29     all_in_transmission_enabled (AIT)              Enable GVSP transmitter (direction field of SCPx set to 0) to use the single
                                                        packet per data block All-in Transmission mode.
  30    unconditional_streaming_enabled (US)            Enable GVSP transmitters (direction field of SCPx set to 0) to continue to
                                                        stream if the control channel of its associated device is closed or regardless of
                                                        any ICMP messages, such as the destination unreachable messages, received by
                                                        the device associated to them. This bit has no effect for GVSP receivers
                                                        (direction field of SCPx set to 1). In the latter case, it always reads back as
                                                        zero.
  31    extended_chunk_data_enable (EC)                 Enable GVSP transmitters (direction field of SCPx set to 0) to use the
                                                        deprecated extended chunk data payload type. This bit has no effect for GVSP
                                                        receivers (direction field of SCPx set to 1). In the latter case, it always reads
                                                        back as zero.
*/
#define GVR_STREAM_CHANNEL_CONFIG           0X0D24

/*
Stream Channel Zone Registers (SCZx)
These optional registers indicate the number of zones in a Multi-zone Image data block transmitted on the
corresponding GVSP transmitter stream channels.
This register is available when bit 27 of the corresponding SCCx register is set.
[CR-496cd] If a device supporting stream channels also supports the Multi-zone Image payload type,
then it MUST implement the Stream Channel Zone register for each stream channel
supporting the Multi-zone Image payload type.

Address 0x0D28 + 0x40 * x
with 0 �� x < 512.
[optional for 0 �� x < 512 for GVSP transmitters]
[Not applicable for GVSP receivers and peripherals]
Length 4 bytes
Access type Read-Only
Factory Default Device-specific

  Bits              Name                            Description
0 �C 26         reserved                            Always 0. Ignore when reading.
27 �C 31    additional_zones                        Reports the number of additional zones in a block transmitted on the
                                                    corresponding stream channel. The number of zones is equal to the value of this
                                                    field plus one.
*/
#define GVR_STREAM_CHANNEL_ZONE             0X0D28

/*
Stream Channel Zone Direction Registers (SCZDx)
These optional registers indicate the transmission direction of each zone in a Multi-zone Image data block
transmitted on the corresponding GVSP transmitter stream channels.
This register is available when bit 27 of the corresponding SCCx register is set.
[CR-497cd] If a device supporting stream channels supports the Multi-zone Image payload type, the it
MUST implement the Stream Channel Zone Direction register for each stream
channel supporting the Multi-zone Image payload type.

Address 0x0D2C + 0x40 * x
with 0 �� x < 512.
[optional for 0 �� x < 512 for GVSP transmitters]
[Not applicable for GVSP receivers and peripherals]
Length 4 bytes
Access type Read-Only
Factory Default Device-specific

  Bits              Name                                Description
0 �C 31 zone_transmission_direction (Zx)        Reports the transmission direction of each zone.
                                                bit 0 (msb) - Direction of zone 0. Reports the transmission direction of zone ID
                                                0. When set to zero, the zone is transmitted top-down. Otherwise, it is
                                                transmitted bottom up.
                                                bit 1 - Direction of zone 1. Reports the transmission direction of zone ID 1.
                                                When set to zero, the zone is transmitted top-down. Otherwise, it is transmitted
                                                bottom up.
                                                ��
                                                bit 31 (lsb) - Direction of zone 31. Reports the transmission direction of zone
                                                ID 31. When set to zero, the zone is transmitted top-down. Otherwise, it is
                                                transmitted bottom up.

*/
#define GVR_STREAM_CHANNEL_ZONE_DIRECTION  0X0D2C

/*
Manifest Table
The optional Manifest Table starts with a header indicating the number of entries in the table followed by a
list of entries, each describing one document. This table is only present if the manifest_table capability bit
(bit 5) is set in the GVCP Capability register (at address 0x0934).
The Manifest Table is optional for a GigE Vision device. Even if this Manifest table is present, the device
must provide valid entries in the First URL and Second URL registers.
[O-498cd] A device SHOULD implement the Manifest Table.
[R-499ca] An application MUST support Manifest Tables.
For 8-byte entries in the Manifest Table, the application needs to access the high part (bit 0 to 31, lowest
address) before accessing the low part (bit 32 to 63, highest address).

Address 0x9000 [optional]
Length 512 bytes
Access type Read
Factory Default Device-specific

Address         Name           Support   Type  Length(bytes)           Description
0x9000     ManifestHeader        M*       R      8           Header of the manifest table0x9000+8 ManifestEntry_1 M* R 8 First 
                                                             entry in the manifest table
0x9000+8    ManifestEntry_1      M*       R      8           First entry in the manifest table
0x9000+16   ManifestEntry_2      O        R      8           Second entry in the manifest table
��               ��              ��       ��     ��             ��
0x9000+8*N  ManifestEntry_N      O        R      8           Entry N in the manifest table
��               ��               ��      ��     ��             ...
0x9000+8*63 ManifestEntry_63     O        R      8           Last entry in the manifest table

More info about Manifest,see Gige Vision Specification 2.0 pdf [28.60 Manifest Table]
*/
#define GVR_MANIFEST_TABLE_ADDR             0X9000



/* gige vision XML Description File Mandatory Features */
//�ⲿ�ֵļĴ�����gige vision�й涨����Ҫ��XML��֧�ֵģ���ַ���û��Զ��壬��XML�еĵ�ַ����һ��

/*
Feature Name 			Interface 		   Access 		Units 					Description
��Width�� 				IInteger 			R/(W) 		pixels 			Width of the image output by the camera on the stream channel.
��Height�� 				IInteger 			R/(W) 		pixels 			Height of the image output by the camera on the stream channel.
																For linescan sources, this represents the height of the frame created 
																by combining consecutive lines together.
��PixelFormat�� 		IEnumeration 		R/(W) 		N/A 			Format of the pixel output by the camera on the stream channel. Refer 
																to the Pixel format section (p.238)
��PayloadSize�� 		IInteger 			R 			Bytes 			Maximum number of bytes transferred for eachimage on the stream channel, 
																including any end-ofline,end-of-frame statistics or other stamp data. This
																is the maximum total size of data payload for a	block. UDP and GVSP headers 
																are not considered.
																Data leader and data trailer are not included.
																This is mainly used by the application software to determine size of image 
																buffers to allocate (largest buffer possible for current mode of operation).
																For example, an image with no statistics or stamp data as PayloadSize equals 
																to (width x height x pixel size) in bytes. It is strongly recommended to 
																retrieve PayloadSize from the camera instead of relying on the above formula.
��GevSCPSPacketSize�� 	IInteger 			R/W 		bytes 			Size of the GVSP packets to transmit during acquisition. This reflects the 
																packet_size field of the SCPS register associated to this stream channel.
																This feature MUST explicitly state the minimum,maximum and increment value 
																supported by the camera for GevSCPSPacketSize.
��AcquisitionMode�� 	IEnumeration 		R/(W) 		N/A 	Used by application software to set the mode of acquisition. This feature indicates 
																how image are sequenced out of the camera (continuously, single shot, multi-shot, ��)
																Only a single value is mandatory for GigE Vision.{��Continuous��}
																Continuous: Configures the camera to stream an infinite sequence of images that must 
																be stopped explicitly by the application.
																Note that the AcquistionMode can have more than this single entry. However, only this 
																one is mandatory.
																The ��AcquisitionStart�� and ��AcquisitionStop��features are used to control the 
																acquisition state.
��AcquisitionStart�� 	ICommand 			(R)/W 		N/A 	Start image acquisition using the specified acquisition mode.
��AcquisitionStop�� 	ICommand 			(R)/W 		N/A 	Stop image acquisition using the specified acquisition mode.
*/

/* �Զ���ĵ�ַ�� 0x10000000��ʼ */
#define GVR_IMG_WIDTH 						0x10000000


#define GVR_IMG_HEIGHT 						0x10000004


#define GVR_PIXEL_FORMAT				0x10000008

#define GVR_PAYLOADSIZE					0x1000000C

#define GVR_GEV_SCPS_PACKET_SIZE		GVR_STREAM_CHANNEL_PACK_SIZE
/*
0: ERS_Continuous
1: ERS_Edge_Trigger
2: GRR_Edge_Trigger
3: ERS_Level_Trigger
4: GRR_Level_Trigger
*/          
#define GVR_ACQUISITION_MODE			0x10000010

#define GVR_ACQUISITION_START			0x10000014

#define GVR_ACQUISITION_STOP			0x10000018


/*�������µļĴ�����ַΪ�豸�Զ��壬Gige VisionЭ����δ������涨��������Ҫ���ṩ��XML�ļ��и���
�Ĵ�������ϸ˵��*/

/*************** ϵͳ��� *****************/
/* �汾�� */
#define GVR_GET_FIRMWARE_VER_BOOT                  0x10000020
#define GVR_GET_FIRMWARE_VER_APP                  0x10000024
#define GVR_GET_FIRMWARE_VER_FPGA                 0x10000028
//д1 ����̼�����
#define GVR_DEVICE_FWUPGRADE											0x1000002C    

/* FLASHд����λ */
#define GVR_FLASH_WRITE_PROTECT                 0x10000030


//0: 	None 1:  PersistentIP 2:	DHCP 3:	LLA  4:  ForceIP
#define GVR_IP_CONFIG_STATUS                   0x10000034

//bit 0: sneosr �Ĵ�����ַ��� 0Ϊ8bit 1Ϊ16bit
//bit 24~31: sensor i2c��ַ
#define GVR_SENSOR_IIC_CONFIG_ADDR           0x1000003C



/*************** ͼ����� *****************/

//16~23: 0:skip  1:bin  2:sum .......  
//24~31: 0:full  1:2X2  2:3X3  3:4X4......
#define GVR_IMG_RES_MOD											0x10000100


#define GVR_IMG_OFFSET_X                    0x10000104

#define GVR_IMG_OFFSET_Y                    0x10000108

#define GVR_IMG_HBLANK											0x1000010C

#define GVR_IMG_VBLANK  										0x10000110


//bit0=1 Reverse y  bit1=1 Reverse x
#define GVR_IMG_MIRROR_XY      0x10000114


//�ֱ���ѡ��
#define GVR_IMG_RESOLUTION      0x10000118


/*
Bit             Name            Description
0-30            Reservered      Always 0
31              Action          Do the trigger action,GVR_STREAM_CHANNEL_MODE
                                must set in Trigger mode
Access mode :Write only
*/
#define GVR_TRIGGER_ACTION                  0x1000011C

/*
The count of frames when software or hardware trigger
Bit         Name            Descrition
0-31       count           the count of freams to trigger
*/
#define GVR_TRIGGER_FRAME_COUNT             0x10000120


//0: Mannual 1: Auto
#define GVR_SET_EXPOSURE_MOD                   0x10000124

#define GVR_SET_AUTO_EXPOSURE_TARGET         0x10000128

//��ȡ�ع����Сֵ��ֻ��
#define GVR_GET_EXPOSURE_TIME_MIN                  0x1000012C

#define  GVR_SET_EXPOSURE_TIME                   0x10000130

/*
0: enable or disable
24~31:light frequency
*/
#define GVR_SET_ANTI_FLICK                   0x10000134

#define GVR_SET_ANALOG_GAIN                   0x10000138

#define GVR_FRAME_SPEED                          0x1000013C


/*
0: �Զ����������
1���ֶ���̿���
*/
#define GVR_TRIG_STORBE_MOD                      0x10000140

/*************** ISP���   0x1000400 *****************/
/*
GVR_ISP_WB_ONCE
When write 1 to this register and ISP is enable,device will
do a white balance operation.
Bit             Name            Description
0-30            Reservered      Always 0.
31              Action          Write 1 to peform the action.
*/
#define GVR_ISP_WB_ONCE                     0x10000200


/*
GVR_ISP_GAMMA
Set or Get the Device ISP gamma value from this register.
Bit             Name            Description
0-15            Reservered      Always 0.
16-31           gamma           the value of gamma.
*/
#define GVR_ISP_GAMMA                       0x10000204

/*
GVR_ISP_GAMMA
Set or Get the Device ISP contrast value from this register.
Bit             Name            Description
0-15            Reservered      Always 0.
16-31           contrast        the value of contrast.
*/
#define GVR_ISP_CONTRAST                    0x10000208

/*
GVR_ISP_SATURATION
Set or Get the Device ISP saturation value from this register.
Bit             Name            Description
0-15            Reservered      Always 0.
16-31           saturation      the value of saturation.
*/
#define GVR_ISP_SATURATION                  0x1000020C

/*
GVR_ISP_SATURATION
Set or Get the Device ISP RGB gain value from this register.
Bit             Name            Description
0-1            Reservered       Always 0.
2-11            Red gain        Red color gain    
12-21           Green gain      Green color gain   
22-31           Blue gain       Blue color gain
*/
#define GVR_ISP_RGB_GATIN                   0x10000210

//��ƽ�������У��
#define GVR_ISP_R_GATIN_CORR					0x10000214
#define GVR_ISP_G_GATIN_CORR					0x10000218
#define GVR_ISP_B_GATIN_CORR					0x1000021C

/*
GVR_ISP_BLACK_LEVEL
Bit             Name            Description
0-7            Reservered       Always 0.
8-15            Red        
16-23           Green     
24-31           Blue        
*/
#define GVR_ISP_BLACK_LEVEL                 0x10000220

/*
GVR_ISP_RGBACC_ROI_H
ISP Statistic Range Set
Bit             Name            Description
0-15            h_start         Horizontal start.  
16-31           h_end           Horizontal end;
*/
#define GVR_ISP_RGBACC_ROI_H                   0x10000224

/*
GVR_ISP_RGBACC_ROI_V
ISP Statistic Range Set
Bit             Name            Description
0-15            v_start         Vertical start.  
16-31           v_end           Vertical end;
*/
#define GVR_ISP_RGBACC_ROI_V                    0x1000022C


/*
GVR_ISP_YACC_ROI_H
ISP Statistic Range Set
Bit             Name            Description
0-15            h_start         Horizontal start.  
16-31           h_end           Horizontal end;
*/
#define GVR_ISP_YACC_ROI_H                   0x10000230

/*
GVR_ISP_YACC_ROI_V
ISP Statistic Range Set
Bit             Name            Description
0-15            v_start         Vertical start.  
16-31           v_end           Vertical end;
*/
#define GVR_ISP_YACC_ROI_V                    0x10000234

//ѡ����ɫУ������
//24~31  : 0xff �Զ�ѡ��  0x0~0xfe���ֶ�ѡ��  
#define GVR_ISP_RGB_MATRIX_SEL                  0x10000238

//��������
#define GVR_ISP_RGB_MATRIX_NUM									0x1000023c

//д������ָ��
#define GVR_ISP_RGB_MATRIX_WR_POS					0x10000240

/*
��ɫУ������
*/
#define GVR_ISP_RGB_MATRIX00_DAT                  0x10000244
#define GVR_ISP_RGB_MATRIX01_DAT                  0x10000248
#define GVR_ISP_RGB_MATRIX02_DAT                  0x1000024C
#define GVR_ISP_RGB_MATRIX10_DAT                  0x10000250
#define GVR_ISP_RGB_MATRIX11_DAT                  0x10000254
#define GVR_ISP_RGB_MATRIX12_DAT                  0x10000258
#define GVR_ISP_RGB_MATRIX20_DAT                  0x1000025C
#define GVR_ISP_RGB_MATRIX21_DAT                  0x10000260
#define GVR_ISP_RGB_MATRIX22_DAT                  0x10000264
#define GVR_ISP_RGB_CORR_RGAIN_DAT								0x10000268
#define GVR_ISP_RGB_CORR_GGAIN_DAT								0x1000026C
#define GVR_ISP_RGB_CORR_BGAIN_DAT								0x10000270

/*
Ӳ��ISP�������
*/
#define GVR_SET_ISP_DEFECT                          0x10000274


/*************** �����������*****************/
//0: default   1:userset 1    2: user set2
#define GVR_USER_SET_SELECTOR                   0x10000400

//CommandValue = 1
#define GVR_USER_SET_LOAD                   0x10000404

//CommandValue = 1
#define GVR_USER_SET_SAVE                   0x10000408

//GVR_USER_SET_DEFAULT_SELECTOR  ���豸�ϵ��ʹ����һ�����
//0: default   1:userset 1    2: user set2
#define GVR_USER_SET_DEFAULT_SELECTOR      0x1000040C




/*************** ���� *****************/

//�ط���
#define GVR_DBG_RESEND_BLOCKID                0x10002000   
#define GVR_DBG_RESEND_FIRSTID                0x10002004   
#define GVR_DBG_RESEND_LASTID                 0x10002008
#define GVR_DBG_RESEND_ACTION                 0x1000200C


/*************** EEPROM�洢�ռ�  ���2MB*****************/
#define GVR_EEPROM_ADDR_START               0x10004000
#define GVR_EEPROM_ADDR_END                 0x10203FFF

/* FPGA�ڴ� 128KB*/
#define GVR_FPGA_RAM_DEFECT_START               0x10204000 //���ʵ� 16K 4000���ʵ�
#define GVR_FPGA_RAM_DEFECT_END                 0x10223FFF


/*************** XML FILE ���512K ****************/
#define GVR_LOCAL_XML_FILE_START                0x10224000

#define GVR_LOCAL_XML_FILE_END                  0x102A3FFF



/*#define GVR_*/

//---------------------Command and Acknowledge Values---------------------------------------------
/*Device and application MUST use the following value to represent the various messages.
Notice the value of the acknowledge message is always the associated command message
+ 1 (when such a command message exists), with PACKETRESEND_CMD and
PENDING_ACK being exceptions*/

/*Discovery Protocol Control*/
#define DISCOVERY_CMD  						0x0002
#define DISCOVERY_ACK  						0x0003

#define FORCEIP_CMD  						0x0004
#define FORCEIP_ACK  						0x0005

/*Streaming Protocol Control*/
#define PACKETRESEND_CMD  					0x0040
#define GVSP_INVALID_ON_GVCP_CHANNEL 		0x0041 

/* Device Memory Access*/
#define READREG_CMD  						0x0080
#define READREG_ACK  						0x0081

#define WRITEREG_CMD  						0x0082
#define WRITEREG_ACK  						0x0083

#define READMEM_CMD  						0x0084
#define READMEM_ACK  						0x0085

#define WRITEMEM_CMD  	                    0x0086
#define WRITEMEM_ACK                        0x0087

#define PENDING_ACK                         0x0089

/*Asynchronous Events*/
#define EVENT_CMD                           0x00C0
#define EVENT_ACK                           0x00C1

#define EVENTDATA_CMD                       0x00C2
#define EVENTDATA_ACK                       0x00C3

/*Miscellaneous*/
#define ACTION_CMD                          0x0100
#define ACTION_ACK                          0x0101


//�Զ���

/*Sensor register access*/
#define IIC_READREG_CMD  				    0xf000
#define IIC_READREG_ACK  					0xf001

#define IIC_WRITEREG_CMD  						0xf002
#define IIC_WRITEREG_ACK  						0xf003

/*FPGA register access*/
#define FPGA_READREG_CMD  				    0xf004
#define FPGA_READREG_ACK  					0xf005

#define FPGA_WRITEREG_CMD  					0xf006
#define FPGA_WRITEREG_ACK  					0xf007

#define IPCONFIG_CMD  						0xF008
#define IPCONFIG_ACK  						0xF009

#define ENTER_BOOTLOADER_CMD  						0xF00A
#define ENTER_BOOTLOADER_ACK  						0xF00B

#define ENTER_AGING_TEST_CMD  						0xF00C
#define ENTER_AGING_TEST_ACK  						0xF00D



//-------------------status code-----------------------------------------------------------------------------------
/*This section lists the various status codes that can be returned in an acknowledge message or a GVSP
header. This specification defines two categories of status code:
1. Standard status code
2. Device-specific status code
Standard status codes are defined in this specification. Refer to appendix 2 for additional information about
supporting those codes.
[O-239cd] A device SHOULD return the status code that provides as much information as possible
about the source of error. [O18-1cd]
[R-240cd] If a device cannot return a more descriptive status code, the device MUST at least return
the GEV_STATUS_ERROR status code. [R18-2cd]
Status register is mapped as follows:

   0             1               2-3           4-15
severity device_specific_flag   reserved    status_value


severity 0 = info, 1 = error
device_specific_flag 0 for standard GigE Vision status codes,
1 for device-specific status codes
reserved Set to 0 on transmission, ignore on reception.
status_value Actual value of status code

*/


typedef unsigned short gev_sta_t;

/*Command executed successfully*/
#define GEV_STATUS_SUCCESS              0x0000 

/*Only applies to packet being resent. This flag is
preferred over the GEV_STATUS_SUCCESS when
the GVSP transmitter sends a resent packet. This can
be used by a GVSP receiver to better monitor packet
resend. Note that bit 15 of the GVSP packet flag field
must be set for a retransmitted GVSP packet when
block_id64 are used.*/
#define GEV_STATUS_PACKET_RESEND        0x0100

/*Command is not supported by the device*/
#define GEV_STATUS_NOT_IMPLEMENTED      0x8001

/*At least one parameter provided in the command is
invalid (or out of range) for the device*/
#define GEV_STATUS_INVALID_PARAMETER    0x8002

/*An attempt was made to access a non-existent address
space location.*/
#define GEV_STATUS_INVALID_ADDRESS      0x8003

/*The addressed register cannot be written to*/
#define GEV_STATUS_WRITE_PROTECT        0x8004

/*A badly aligned address offset or data size was
specified.*/
#define GEV_STATUS_BAD_ALIGNMENT        0x8005

/*An attempt was made to access an address location
which is currently/momentary not accessible. This
depends on the current state of the device, in
particular the current privilege of the application.*/
#define GEV_STATUS_ACCESS_DENIED        0x8006

/*A required resource to service the request is not
currently available. The request may be retried at a
later time.*/
#define GEV_STATUS_BUSY                 0x8007

/*Not used starting with GigE Vision 2.0.
Consult GigE Vision 1.2 for a detailed list.*/
#define GEV_STATUS_DEPRECATED           0x8008//0x8008-0x0800B  0x800F  

/*The requested packet is not available anymore.*/
#define GEV_STATUS_PACKET_UNAVAILABLE   0x800C

/*Internal memory of GVSP transmitter overrun
(typically for image acquisition)*/
#define GEV_STATUS_DATA_OVERRUN         0x800D

/*The message header is not valid. Some of its fields do
not match the specification.*/
#define GEV_STATUS_INVALID_HEADER       0x800E

/*The requested packet has not yet been acquired. Can
be used for linescan cameras device when line trigger
rate is slower than application timeout.*/
#define GEV_STATUS_PACKET_NOT_YET_AVAILABLE 0x8010

/*The requested packet and all previous ones are not
available anymore and have been discarded from the
GVSP transmitter memory. An application associated
to a GVSP receiver should not request retransmission
of these packets again.*/
#define GEV_STATUS_PACKET_AND_PREV_REMOVED_FROM_MEMORY  0x8011

/*The requested packet is not available anymore and
has been discarded from the GVSP transmitter
memory. However, applications associated to GVSP
receivers can still continue using their internal resend
algorithm on earlier packets that are still outstanding.
This does not necessarily indicate that any previous
data is actually available, just that the application
should not just assume everything earlier is no longer
available.*/
#define GEV_STATUS_PACKET_REMOVED_FROM_MEMORY           0x8012

/*The device is not synchronized to a master clock to be
used as time reference. Typically used when
Scheduled Action Commands cannot be scheduled at
a future time since the reference time coming from
IEEE 1588 is not locked.*/
#define GEV_STATUS_NO_REF_TIME                          0x0813

/*The packet cannot be resent at the moment due to
temporary bandwidth issues and should be requested
again in the future*/
#define GEV_STATUS_PACKET_TEMPORARILY_UNAVAILABLE       0x0814

/*A device queue or packet data has overflowed.
Typically used when Scheduled Action Commands
queue is full and cannot take the addition request.*/
#define GEV_STATUS_OVERFLOW                             0x0815

/*The requested scheduled action command was
requested at a time that is already past. Only available
starting with GEV 2.0 when an action_time is
specified.*/
#define GEV_STATUS_ACTION_LATE                          0x0816

/*Generic error. Try to avoid and use a more descriptive
status code from list above.*/
#define GEV_STATUS_ERROR                                0x8FFF

//----------------------------GVSP PIXEL FORMAT DEFINE------------------------------------
/* Indicate if pixel is monochrome or RGB*/
#define GVSP_PIX_MONO                           0x01000000
#define GVSP_PIX_RGB                            0x02000000 // deprecated in version 1.1
#define GVSP_PIX_COLOR                          0x02000000
#define GVSP_PIX_CUSTOM                         0x80000000
#define GVSP_PIX_COLOR_MASK                     0xFF000000
/* Indicate effective number of bits occupied by the pixel (including padding).
// This can be used to compute amount of memory required to store an image.*/
#define GVSP_PIX_OCCUPY1BIT                     0x00010000
#define GVSP_PIX_OCCUPY2BIT                     0x00020000
#define GVSP_PIX_OCCUPY4BIT                     0x00040000
#define GVSP_PIX_OCCUPY8BIT                     0x00080000
#define GVSP_PIX_OCCUPY12BIT                    0x000C0000
#define GVSP_PIX_OCCUPY16BIT                    0x00100000
#define GVSP_PIX_OCCUPY24BIT                    0x00180000
#define GVSP_PIX_OCCUPY32BIT                    0x00200000
#define GVSP_PIX_OCCUPY36BIT                    0x00240000
#define GVSP_PIX_OCCUPY48BIT                    0x00300000
#define GVSP_PIX_EFFECTIVE_PIXEL_SIZE_MASK      0x00FF0000
#define GVSP_PIX_EFFECTIVE_PIXEL_SIZE_SHIFT     16
/* Pixel ID: lower 16-bit of the pixel formats*/
#define GVSP_PIX_ID_MASK                        0x0000FFFF
#define GVSP_PIX_COUNT                          0x46 /* next Pixel ID available*/

/*27.1 Mono buffer format defines*/
#define GVSP_PIX_MONO1P             (GVSP_PIX_MONO | GVSP_PIX_OCCUPY1BIT | 0x0037)
#define GVSP_PIX_MONO2P             (GVSP_PIX_MONO | GVSP_PIX_OCCUPY2BIT | 0x0038)
#define GVSP_PIX_MONO4P             (GVSP_PIX_MONO | GVSP_PIX_OCCUPY4BIT | 0x0039)
#define GVSP_PIX_MONO8              (GVSP_PIX_MONO | GVSP_PIX_OCCUPY8BIT | 0x0001)
#define GVSP_PIX_MONO8S             (GVSP_PIX_MONO | GVSP_PIX_OCCUPY8BIT | 0x0002)
#define GVSP_PIX_MONO10             (GVSP_PIX_MONO | GVSP_PIX_OCCUPY16BIT | 0x0003)
#define GVSP_PIX_MONO10_PACKED      (GVSP_PIX_MONO | GVSP_PIX_OCCUPY12BIT | 0x0004)
#define GVSP_PIX_MONO12             (GVSP_PIX_MONO | GVSP_PIX_OCCUPY16BIT | 0x0005)
#define GVSP_PIX_MONO12_PACKED      (GVSP_PIX_MONO | GVSP_PIX_OCCUPY12BIT | 0x0006)
#define GVSP_PIX_MONO14             (GVSP_PIX_MONO | GVSP_PIX_OCCUPY16BIT | 0x0025)
#define GVSP_PIX_MONO16             (GVSP_PIX_MONO | GVSP_PIX_OCCUPY16BIT | 0x0007)

/*27.2 Bayer buffer format defines*/
#define GVSP_PIX_BAYGR8             (GVSP_PIX_MONO | GVSP_PIX_OCCUPY8BIT | 0x0008)
#define GVSP_PIX_BAYRG8             (GVSP_PIX_MONO | GVSP_PIX_OCCUPY8BIT | 0x0009)
#define GVSP_PIX_BAYGB8             (GVSP_PIX_MONO | GVSP_PIX_OCCUPY8BIT | 0x000A)
#define GVSP_PIX_BAYBG8             (GVSP_PIX_MONO | GVSP_PIX_OCCUPY8BIT | 0x000B)
#define GVSP_PIX_BAYGR10            (GVSP_PIX_MONO | GVSP_PIX_OCCUPY16BIT | 0x000C)
#define GVSP_PIX_BAYRG10            (GVSP_PIX_MONO | GVSP_PIX_OCCUPY16BIT | 0x000D)
#define GVSP_PIX_BAYGB10            (GVSP_PIX_MONO | GVSP_PIX_OCCUPY16BIT | 0x000E)
#define GVSP_PIX_BAYBG10            (GVSP_PIX_MONO | GVSP_PIX_OCCUPY16BIT | 0x000F)
#define GVSP_PIX_BAYGR12            (GVSP_PIX_MONO | GVSP_PIX_OCCUPY16BIT | 0x0010)
#define GVSP_PIX_BAYRG12            (GVSP_PIX_MONO | GVSP_PIX_OCCUPY16BIT | 0x0011)
#define GVSP_PIX_BAYGB12            (GVSP_PIX_MONO | GVSP_PIX_OCCUPY16BIT | 0x0012)
#define GVSP_PIX_BAYBG12            (GVSP_PIX_MONO | GVSP_PIX_OCCUPY16BIT | 0x0013)
#define GVSP_PIX_BAYGR10_PACKED     (GVSP_PIX_MONO | GVSP_PIX_OCCUPY12BIT | 0x0026)
#define GVSP_PIX_BAYRG10_PACKED     (GVSP_PIX_MONO | GVSP_PIX_OCCUPY12BIT | 0x0027)
#define GVSP_PIX_BAYGB10_PACKED     (GVSP_PIX_MONO | GVSP_PIX_OCCUPY12BIT | 0x0028)
#define GVSP_PIX_BAYBG10_PACKED     (GVSP_PIX_MONO | GVSP_PIX_OCCUPY12BIT | 0x0029)
#define GVSP_PIX_BAYGR12_PACKED     (GVSP_PIX_MONO | GVSP_PIX_OCCUPY12BIT | 0x002A)
#define GVSP_PIX_BAYRG12_PACKED     (GVSP_PIX_MONO | GVSP_PIX_OCCUPY12BIT | 0x002B)
#define GVSP_PIX_BAYGB12_PACKED     (GVSP_PIX_MONO | GVSP_PIX_OCCUPY12BIT | 0x002C)
#define GVSP_PIX_BAYBG12_PACKED     (GVSP_PIX_MONO | GVSP_PIX_OCCUPY12BIT | 0x002D)
#define GVSP_PIX_BAYGR16            (GVSP_PIX_MONO | GVSP_PIX_OCCUPY16BIT | 0x002E)
#define GVSP_PIX_BAYRG16            (GVSP_PIX_MONO | GVSP_PIX_OCCUPY16BIT | 0x002F)
#define GVSP_PIX_BAYGB16            (GVSP_PIX_MONO | GVSP_PIX_OCCUPY16BIT | 0x0030)
#define GVSP_PIX_BAYBG16            (GVSP_PIX_MONO | GVSP_PIX_OCCUPY16BIT | 0x0031)

/*27.3 RGB Packed buffer format defines*/
#define GVSP_PIX_RGB8               (GVSP_PIX_COLOR | GVSP_PIX_OCCUPY24BIT | 0x0014)
#define GVSP_PIX_BGR8               (GVSP_PIX_COLOR | GVSP_PIX_OCCUPY24BIT | 0x0015)
#define GVSP_PIX_RGBA8              (GVSP_PIX_COLOR | GVSP_PIX_OCCUPY32BIT | 0x0016)
#define GVSP_PIX_BGRA8              (GVSP_PIX_COLOR | GVSP_PIX_OCCUPY32BIT | 0x0017)
#define GVSP_PIX_RGB10              (GVSP_PIX_COLOR | GVSP_PIX_OCCUPY48BIT | 0x0018)
#define GVSP_PIX_BGR10              (GVSP_PIX_COLOR | GVSP_PIX_OCCUPY48BIT | 0x0019)
#define GVSP_PIX_RGB12              (GVSP_PIX_COLOR | GVSP_PIX_OCCUPY48BIT | 0x001A)
#define GVSP_PIX_BGR12              (GVSP_PIX_COLOR | GVSP_PIX_OCCUPY48BIT | 0x001B)
#define GVSP_PIX_RGB16              (GVSP_PIX_COLOR | GVSP_PIX_OCCUPY48BIT | 0x0033)
#define GVSP_PIX_RGB10V1_PACKED     (GVSP_PIX_COLOR | GVSP_PIX_OCCUPY32BIT | 0x001C)
#define GVSP_PIX_RGB10P32           (GVSP_PIX_COLOR | GVSP_PIX_OCCUPY32BIT | 0x001D)
#define GVSP_PIX_RGB12V1_PACKED     (GVSP_PIX_COLOR | GVSP_PIX_OCCUPY36BIT | 0X0034)
#define GVSP_PIX_RGB565P            (GVSP_PIX_COLOR | GVSP_PIX_OCCUPY16BIT | 0x0035)
#define GVSP_PIX_BGR565P            (GVSP_PIX_COLOR | GVSP_PIX_OCCUPY16BIT | 0X0036)

/*27.4 YUV and YCbCr Packed buffer format defines*/
#define GVSP_PIX_YUV411_8_UYYVYY    (GVSP_PIX_COLOR | GVSP_PIX_OCCUPY12BIT | 0x001E)
#define GVSP_PIX_YUV422_8_UYVY      (GVSP_PIX_COLOR | GVSP_PIX_OCCUPY16BIT | 0x001F)
#define GVSP_PIX_YUV422_8           (GVSP_PIX_COLOR | GVSP_PIX_OCCUPY16BIT | 0x0032)
#define GVSP_PIX_YUV8_UYV           (GVSP_PIX_COLOR | GVSP_PIX_OCCUPY24BIT | 0x0020)
#define GVSP_PIX_YCBCR8_CBYCR       (GVSP_PIX_COLOR | GVSP_PIX_OCCUPY24BIT | 0x003A)
#define GVSP_PIX_YCBCR422_8         (GVSP_PIX_COLOR | GVSP_PIX_OCCUPY16BIT | 0x003B)
#define GVSP_PIX_YCBCR422_8_CBYCRY      (GVSP_PIX_COLOR | GVSP_PIX_OCCUPY16BIT | 0x0043)
#define GVSP_PIX_YCBCR411_8_CBYYCRYY    (GVSP_PIX_COLOR | GVSP_PIX_OCCUPY12BIT | 0x003C)
#define GVSP_PIX_YCBCR601_8_CBYCR       (GVSP_PIX_COLOR | GVSP_PIX_OCCUPY24BIT | 0x003D)
#define GVSP_PIX_YCBCR601_422_8         (GVSP_PIX_COLOR | GVSP_PIX_OCCUPY16BIT | 0x003E)
#define GVSP_PIX_YCBCR601_422_8_CBYCRY  (GVSP_PIX_COLOR | GVSP_PIX_OCCUPY16BIT | 0x0044)
#define GVSP_PIX_YCBCR601_411_8_CBYYCRYY    (GVSP_PIX_COLOR | GVSP_PIX_OCCUPY12BIT | 0x003F)
#define GVSP_PIX_YCBCR709_8_CBYCR           (GVSP_PIX_COLOR | GVSP_PIX_OCCUPY24BIT | 0x0040)
#define GVSP_PIX_YCBCR709_422_8             (GVSP_PIX_COLOR | GVSP_PIX_OCCUPY16BIT | 0x0041)
#define GVSP_PIX_YCBCR709_422_8_CBYCRY      (GVSP_PIX_COLOR | GVSP_PIX_OCCUPY16BIT | 0x0045)
#define GVSP_PIX_YCBCR709_411_8_CBYYCRYY    (GVSP_PIX_COLOR | GVSP_PIX_OCCUPY12BIT | 0x0042)

/*27.5 RGB Planar buffer format defines*/
#define GVSP_PIX_RGB8_PLANAR        (GVSP_PIX_COLOR | GVSP_PIX_OCCUPY24BIT | 0x0021)
#define GVSP_PIX_RGB10_PLANAR       (GVSP_PIX_COLOR | GVSP_PIX_OCCUPY48BIT | 0x0022)
#define GVSP_PIX_RGB12_PLANAR       (GVSP_PIX_COLOR | GVSP_PIX_OCCUPY48BIT | 0x0023)
#define GVSP_PIX_RGB16_PLANAR       (GVSP_PIX_COLOR | GVSP_PIX_OCCUPY48BIT | 0x0024)

#endif /*_GIGEVISIONDEF_H_*/



