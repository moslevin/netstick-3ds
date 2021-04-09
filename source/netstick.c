// Netstick - Copyright (c) 2021 Funkenstein Software Consulting.  See LICENSE.txt
// for more details.
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <malloc.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <3ds.h>

#include "joystick.h"
#include "tlvc.h"
#include "slip.h"

//---------------------------------------------------------------------------
// SOC service stuff... from the examples.
#define SOC_ALIGN       0x1000
#define SOC_BUFFERSIZE  0x100000

//---------------------------------------------------------------------------
// Vendor/product IDs to register this device as on the server-side
#define NINTENDO_USB_VID	((uint16_t)(0x057E))
#define NINTENDO_USB_PID	((uint16_t)(0x1337)) // Fake product ID, since the 3ds isn't BT/USB compatbile
#define NINTENDO_3DS_NAME	"Nintendo 3DS"

//---------------------------------------------------------------------------
// Linux gamepad button IDs -- which is what netstickd is expecting on the 
// other side of the connection; we'll map the 3DS button IDs to the linux 
// IDs using a joystick map object

#define LINUX_ABS_X			    0x00
#define LINUX_ABS_Y			    0x01

#define LINUX_BTN_GAMEPAD		0x130
#define LINUX_BTN_SOUTH		    0x130
#define LINUX_BTN_A			    BTN_SOUTH
#define LINUX_BTN_EAST		    0x131
#define LINUX_BTN_B			    BTN_EAST
#define LINUX_BTN_C			    0x132
#define LINUX_BTN_NORTH		    0x133
#define LINUX_BTN_X			    BTN_NORTH
#define LINUX_BTN_WEST		    0x134
#define LINUX_BTN_Y			    BTN_WEST
#define LINUX_BTN_Z			    0x135
#define LINUX_BTN_TL			0x136
#define LINUX_BTN_TR			0x137
#define LINUX_BTN_TL2			0x138
#define LINUX_BTN_TR2			0x139
#define LINUX_BTN_SELECT		0x13a
#define LINUX_BTN_START		    0x13b
#define LINUX_BTN_MODE		    0x13c
#define LINUX_BTN_THUMBL		0x13d
#define LINUX_BTN_THUMBR		0x13e

#define LINUX_BTN_DPAD_UP		0x220
#define LINUX_BTN_DPAD_DOWN		0x221
#define LINUX_BTN_DPAD_LEFT		0x222
#define LINUX_BTN_DPAD_RIGHT	0x223

#define LINUX_KEY_UP			103
#define LINUX_KEY_LEFT		    105
#define LINUX_KEY_RIGHT		    106
#define LINUX_KEY_DOWN		    108

#define LINUX_KEY_CAMERA_UP		0x217
#define LINUX_KEY_CAMERA_DOWN	0x218
#define LINUX_KEY_CAMERA_LEFT	0x219
#define LINUX_KEY_CAMERA_RIGHT	0x21a

//---------------------------------------------------------------------------
// 3DS key bits in the HID report structure
#define NDS_KEY_A               0
#define NDS_KEY_B               1
#define NDS_KEY_SELECT          2
#define NDS_KEY_START           3
#define NDS_KEY_DPAD_RIGHT      4
#define NDS_KEY_DPAD_LEFT       5
#define NDS_KEY_DPAD_UP         6
#define NDS_KEY_DPAD_DOWN       7
#define NDS_KEY_R               8
#define NDS_KEY_L               9
#define NDS_KEY_X               10
#define NDS_KEY_Y               11
#define NDS_KEY_ZL              14
#define NDS_KEY_ZR              15
#define NDS_KEY_CSTICK_RIGHT    24
#define NDS_KEY_CSTICK_LEFT     25
#define NDS_KEY_CSTICK_UP       26
#define NDS_KEY_CSTICK_DOWN     27

//---------------------------------------------------------------------------
// Max/Min values for circle pad reports
#define NDS_CIRCLE_PAD_MIN      -156
#define NDS_CIRCLE_PAD_MAX      156

//---------------------------------------------------------------------------
// Indexes of the 3DS buttons in the report structure
#define NDS_IDX_A               0
#define NDS_IDX_B               1
#define NDS_IDX_SELECT          2
#define NDS_IDX_START           3
#define NDS_IDX_DPAD_RIGHT      4
#define NDS_IDX_DPAD_LEFT       5
#define NDS_IDX_DPAD_UP         6
#define NDS_IDX_DPAD_DOWN       7
#define NDS_IDX_R               8
#define NDS_IDX_L               9
#define NDS_IDX_X               10
#define NDS_IDX_Y               11
#define NDS_IDX_ZL              12
#define NDS_IDX_ZR              13
#define NDS_IDX_CSTICK_RIGHT    14
#define NDS_IDX_CSTICK_LEFT     15
#define NDS_IDX_CSTICK_UP       16
#define NDS_IDX_CSTICK_DOWN     17
#define NDS_BUTTON_COUNT        18      // Raw buttons

#define NDS_ABS_IDX_X           0
#define NDS_ABS_IDX_Y           1
#define NDS_ABS_AXIS_COUNT      2       // X/Y on circlepad (ToDo: Add motion controls)

#define NDS_REL_AXIS_COUNT      0       // ToDo: Add Touchscreen?

//---------------------------------------------------------------------------
#define RAW_REPORT_SIZE ((sizeof(uint8_t) * NDS_BUTTON_COUNT) + (sizeof(int32_t) * NDS_ABS_AXIS_COUNT) + (sizeof(int32_t) * NDS_REL_AXIS_COUNT))

//---------------------------------------------------------------------------
#define MAP_MAX ((size_t)(32))

#define EV_KEY  ((size_t)(1))
#define EV_REL  ((size_t)(2))
#define EV_ABS  ((size_t)(3))

//---------------------------------------------------------------------------
typedef struct {
    int buttons[MAP_MAX];
} js_index_map_t;

//---------------------------------------------------------------------------
static void js_index_map_init(js_index_map_t* indexMap_) {
    for (int i = 0; i < MAP_MAX; i++) {
        indexMap_->buttons[i] = -1;
    }
}

//---------------------------------------------------------------------------
static void js_index_map_set(js_index_map_t* indexMap_, int eventType_, int eventId_, int index_) {
    if (eventType_ == EV_KEY) {
        indexMap_->buttons[eventId_] = index_;
    }
}

//---------------------------------------------------------------------------
static int js_index_map_get_index(js_index_map_t* indexMap_, int eventType_, int eventId_) {
    if (eventType_ == EV_KEY) {
        return indexMap_->buttons[eventId_];
    }
    return -1;
}

//---------------------------------------------------------------------------
static bool encode_and_transmit(int sockFd_, uint16_t messageType_, void* data_, size_t dataLen_) {
    tlvc_data_t tlvc = {};
    tlvc_encode_data(&tlvc, messageType_, dataLen_, data_);

    slip_encode_message_t* encode = slip_encode_message_create(dataLen_);
    slip_encode_begin(encode);

    uint8_t* raw = (uint8_t*)&tlvc.header;
    for (size_t i = 0; i < sizeof(tlvc.header); i++) { slip_encode_byte(encode, *raw++); }

    raw = (uint8_t*)tlvc.data;
    for (size_t i = 0; i < tlvc.dataLen; i++) { slip_encode_byte(encode, *raw++); }

    raw = (uint8_t*)&tlvc.footer;
    for (size_t i = 0; i < sizeof(tlvc.footer); i++) { slip_encode_byte(encode, *raw++); }

    slip_encode_finish(encode);

    int toWrite  = encode->index;
    raw          = encode->encoded;

    bool died = false;
    
    int nWritten = send(sockFd_, raw, toWrite, 0);
    if (nWritten == -1) {
        printf("socket error: %d\n", errno);
        died = true;
    }
    

    slip_encode_message_destroy(encode);

    if (died) {
        printf("socket died during write\n");
        return false;
    }

    return true;
}

//---------------------------------------------------------------------------
// Initialize the configuration data structure for a 3DS joystick.  There's
// no joystick enumeration process, but we do have to map the 3DS button/axis 
// identifiers to their corresponding linux values.
static void jsproxy_3ds_config(js_config_t* config_, js_index_map_t* indexMap_) {
    // Set the device identifiers for the 3DS
    config_->pid = 0x1337; // Dummy value
    config_->vid = NINTENDO_USB_VID;
    strcpy(config_->name, NINTENDO_3DS_NAME);

    // Set the count of buttons/axis supported by the 3DS 
    config_->absAxisCount = NDS_ABS_AXIS_COUNT; 
    config_->buttonCount = NDS_BUTTON_COUNT;
    config_->relAxisCount = NDS_REL_AXIS_COUNT;
    
    // Set up the 3DS's absolution-axis identifiers, mapping the supported
    // absolute axis IDs to their corresponding IDs on linux
    config_->absAxis[NDS_ABS_IDX_X] = LINUX_ABS_X;
    config_->absAxis[NDS_ABS_IDX_Y] = LINUX_ABS_Y;
    
    // Set up the absolute axis limits for circle pad
    for (int i = 0; i < NDS_ABS_AXIS_COUNT; i++) {
        config_->absAxisMin[i] = NDS_CIRCLE_PAD_MIN;
        config_->absAxisMax[i] = NDS_CIRCLE_PAD_MAX;
    }
    
    // Set up the mappings between the 3DS's buttons and the corresponding
    // linux IDs.
    config_->buttons[NDS_IDX_A] = LINUX_BTN_EAST;
    config_->buttons[NDS_IDX_B] = LINUX_BTN_SOUTH;
    config_->buttons[NDS_IDX_SELECT] = LINUX_BTN_SELECT;
    config_->buttons[NDS_IDX_START] = LINUX_BTN_START;
    config_->buttons[NDS_IDX_DPAD_RIGHT] = LINUX_BTN_DPAD_RIGHT;
    config_->buttons[NDS_IDX_DPAD_LEFT] = LINUX_BTN_DPAD_LEFT;
    config_->buttons[NDS_IDX_DPAD_UP] = LINUX_BTN_DPAD_UP;
    config_->buttons[NDS_IDX_DPAD_DOWN] = LINUX_BTN_DPAD_DOWN;
    config_->buttons[NDS_IDX_R] = LINUX_BTN_TR;
    config_->buttons[NDS_IDX_L] = LINUX_BTN_TL; 
    config_->buttons[NDS_IDX_X] = LINUX_BTN_NORTH; 
    config_->buttons[NDS_IDX_Y] = LINUX_BTN_WEST;
    config_->buttons[NDS_IDX_ZL] = LINUX_BTN_TL2;
    config_->buttons[NDS_IDX_ZR] = LINUX_BTN_TR2;
    config_->buttons[NDS_IDX_CSTICK_RIGHT] = LINUX_KEY_CAMERA_RIGHT;
    config_->buttons[NDS_IDX_CSTICK_LEFT] = LINUX_KEY_CAMERA_LEFT;
    config_->buttons[NDS_IDX_CSTICK_UP] = LINUX_KEY_CAMERA_UP;
    config_->buttons[NDS_IDX_CSTICK_DOWN] = LINUX_KEY_CAMERA_DOWN;

    // We're going to use a lookup table to map the 3ds button enums to their indexes in the config object.    
    js_index_map_init(indexMap_);
    
    js_index_map_set(indexMap_, EV_KEY, NDS_KEY_A, NDS_IDX_A);
    js_index_map_set(indexMap_, EV_KEY, NDS_KEY_B, NDS_IDX_B);
    js_index_map_set(indexMap_, EV_KEY, NDS_KEY_SELECT, NDS_IDX_SELECT);
    js_index_map_set(indexMap_, EV_KEY, NDS_KEY_START, NDS_IDX_START);
    js_index_map_set(indexMap_, EV_KEY, NDS_KEY_DPAD_RIGHT, NDS_IDX_DPAD_RIGHT);
    js_index_map_set(indexMap_, EV_KEY, NDS_KEY_DPAD_LEFT, NDS_IDX_DPAD_LEFT);
    js_index_map_set(indexMap_, EV_KEY, NDS_KEY_DPAD_UP, NDS_IDX_DPAD_UP);
    js_index_map_set(indexMap_, EV_KEY, NDS_KEY_DPAD_DOWN, NDS_IDX_DPAD_DOWN);
    js_index_map_set(indexMap_, EV_KEY, NDS_KEY_R, NDS_IDX_R);
    js_index_map_set(indexMap_, EV_KEY, NDS_KEY_L, NDS_IDX_L);
    js_index_map_set(indexMap_, EV_KEY, NDS_KEY_X, NDS_IDX_X);
    js_index_map_set(indexMap_, EV_KEY, NDS_KEY_Y, NDS_IDX_Y);
    js_index_map_set(indexMap_, EV_KEY, NDS_KEY_ZL, NDS_IDX_ZL);
    js_index_map_set(indexMap_, EV_KEY, NDS_KEY_ZR, NDS_IDX_ZR);
    js_index_map_set(indexMap_, EV_KEY, NDS_KEY_CSTICK_RIGHT, NDS_IDX_CSTICK_RIGHT);
    js_index_map_set(indexMap_, EV_KEY, NDS_KEY_CSTICK_LEFT, NDS_IDX_CSTICK_LEFT);
    js_index_map_set(indexMap_, EV_KEY, NDS_KEY_CSTICK_UP, NDS_IDX_CSTICK_UP);
    js_index_map_set(indexMap_, EV_KEY, NDS_KEY_CSTICK_DOWN, NDS_IDX_CSTICK_DOWN);
}

//---------------------------------------------------------------------------
// Attempt to connect to the server, return open socket fd on success.
static int jsproxy_3ds_connect(const char* serverAddr_, uint16_t serverPort_)
{
    printf("connecting to %s:%d\n", serverAddr_, serverPort_);
    
    // Create the client socket address
    int sockFd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockFd < 0) {
        printf("error connecting socket: %d (%s)\n", errno, strerror(errno));
        return -1;
    }

    // Connect to the server
    struct sockaddr_in addr = {};

    addr.sin_family = AF_INET;
    inet_pton(AF_INET, serverAddr_, &(addr.sin_addr));
    addr.sin_port = htons(serverPort_);

    int rc = connect(sockFd, (struct sockaddr*)&addr, sizeof(addr));
    if (rc < 0) {
        printf("error connecting to server: %d (%s)\n", errno, strerror(errno));
        close(sockFd);
        return -1;
    }
    return sockFd;
}

//---------------------------------------------------------------------------
static bool jsproxy_3ds_update(int sockFd_, js_config_t* config_, js_index_map_t* indexMap_, void* reportData_, size_t reportSize_) {
    js_report_t report        = {};
    
    uint8_t*    rawReport     = reportData_;
    report.absAxis = (int32_t*)rawReport;
    report.relAxis = (int32_t*)(rawReport + (sizeof(int32_t) * config_->absAxisCount));
    report.buttons = (uint8_t*)(rawReport + (sizeof(int32_t) * config_->absAxisCount) + 
                      (sizeof(int32_t) * config_->relAxisCount));
                      
	circlePosition circle;
	hidCircleRead(&circle);
    uint32_t keys = hidKeysHeld();
  
    for (int bit = 0; bit < 32; bit++) {
        int idx = js_index_map_get_index(indexMap_, EV_KEY, bit);
        if (idx != -1) {
            int val = 0;
            if (keys & (1 << bit)) {
                val = 1;
            }
            report.buttons[idx] = val;
        }     
    }
    
    report.absAxis[0] = circle.dx;
    report.absAxis[1] = circle.dy;
    
    return encode_and_transmit(sockFd_, 1, rawReport, reportSize_);
}

//---------------------------------------------------------------------------
typedef struct {
    char host[64];
    bool hostFound;
    int port;
    bool portFound;
} host_config_t;

//---------------------------------------------------------------------------
static void jsproxy_parse_config_line(const char* line_, host_config_t* config_) {
    char key[64] = {};
    char value[64] = {};

    char* matchIdx = strstr(line_, ":");
    
    if (!matchIdx) {
        return;
    } 
    
    *matchIdx = ' ';
    
    sscanf(line_, "%64s %64s", key, value);

    if (0 == strcmp(key, "server")) {
        strcpy(config_->host, value);
        config_->hostFound = true;
    } else if (0 == strcmp(key, "port")) {
        config_->port = atoi(value);
        config_->portFound = true;
    }
}

//---------------------------------------------------------------------------
static bool jsproxy_read_config(const char* path_, host_config_t* config_) {

    // Open file and read contents into a buffer...
    FILE* configFile = fopen(path_, "r");
    
    if (configFile == NULL) {
        printf("Error opening %s\n", path_);
        return false;
    }
    
    fseek(configFile, 0, SEEK_END);
    
    int fileSize = ftell(configFile);
    
    fseek(configFile, 0, SEEK_SET);
    
    char* fileBuffer = (char*)malloc(fileSize);
    if (!fileBuffer) {
        printf("Error allocating %d bytes\n", fileSize);
        fclose(configFile);
        return false; 
    }
    
    int nRead = fread(fileBuffer, 1, fileSize, configFile);
    fclose(configFile);
    
    if (nRead != fileSize) {
        printf("Could not read entire file (got %d, expected %d)\n", nRead, fileSize);
        free(fileBuffer);
        return false;
    }
    
    // Destructively parse through the read configuration file and read-out key/value pairs.
    char* lineStart = fileBuffer;    
    char* curr = lineStart;
    while (*curr) {               
        if (*curr == '\n') { 
            *curr = '\0';            
            jsproxy_parse_config_line(lineStart, config_);
            lineStart = ++curr;
        } else if (*curr == '#') {
            *curr = '\0';            
            jsproxy_parse_config_line(lineStart, config_);

            // Silently consume the rest of the line
            curr++;            
            while (*curr && (*curr != '\n')) {
                curr++;
            }            
            lineStart = curr;
        } else {
            curr++;
        }
    }    
    jsproxy_parse_config_line(lineStart, config_);    
    free(fileBuffer);
    
    if (config_->hostFound && config_->portFound) {
        return true;
    }
    
    return false;
}
 	
//---------------------------------------------------------------------------
// Create objects for configuration data used in the program
static js_config_t config = {};
static js_index_map_t indexMap = {};
static host_config_t hostConfig = {};

// Static-allocate a chunk of memory for a report structure.
static uint8_t rawReport[RAW_REPORT_SIZE];

//---------------------------------------------------------------------------
int main(void)
{
    // ToDo: 3ds init stuff...
    gfxInitDefault();
 
    consoleInit(GFX_TOP, NULL);

    // Read config file on device
    if (!jsproxy_read_config("config.txt", &hostConfig)) {
        printf("Please create a file named config.txt in the \n"
               "app's directory, with lines containing \n"
               "the server's IP and port, as below:\n\n"
               "server:192.168.1.1\n"
               "port:1234\n\n"
               "\n"
               "Press any key to exit...\n");
        
        // Fail loop to run if we couldn't read the config file.
	    while (aptMainLoop())
	    {        
       		gspWaitForVBlank();
		    hidScanInput();
		    
	        uint32_t keys = hidKeysDown();
	        if (keys) {
	            break;
	        }
		        
            gfxFlushBuffers();
		    gfxSwapBuffers();
	    }
	    gfxExit();
	    return 0;
    } 
    
    // allocate buffer for SOC service
    uint32_t* socBuffer = (uint32_t*)memalign(SOC_ALIGN, SOC_BUFFERSIZE);
    socInit(socBuffer, SOC_BUFFERSIZE);

    // Initialize the configuration data for the app
    jsproxy_3ds_config(&config, &indexMap);
    
    // socket file descriptor    
    int sockFd = -1;
    
    while (aptMainLoop())
    {
	    gspWaitForVBlank();
	    hidScanInput();

        // If we're not connected -- try to connect
        if (sockFd == -1) {
            sockFd = jsproxy_3ds_connect(hostConfig.host, hostConfig.port);
            
            // Connection succeeded -- try to send configuration data.
            if (sockFd >= 0) {
                if (!encode_and_transmit(sockFd, 0, &config, sizeof(config))) {
                    close(sockFd);
                    sockFd = -1;
                } else {
                    printf("connected!\n");
                }
            }            
        } else {
            if (!jsproxy_3ds_update(sockFd, &config, &indexMap, rawReport, RAW_REPORT_SIZE)) {
                close(sockFd);
                sockFd = -1;
                printf("disconnected!\n");
            }
        }
        
        gfxFlushBuffers();
	    gfxSwapBuffers();
    }

    socExit();
    gfxExit();

    return 0;
}