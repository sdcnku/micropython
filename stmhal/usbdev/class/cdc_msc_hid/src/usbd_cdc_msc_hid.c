#include "usbd_ioreq.h"
#include "usbd_cdc_msc_hid.h"

#define USBD_CONFIG_DESC_SIZE       (98+9)
#define USB_CDC_HID_CONFIG_DESC_SIZ (100)
#define CDC_IFACE_NUM               (0)
#define MSC_IFACE_NUM               (2)
#define DBG_IFACE_NUM               (3)
#define HID_IFACE_NUM_WITH_CDC      (0)
#define HID_IFACE_NUM_WITH_MSC      (1)
#define HID_IN_EP_WITH_CDC          (0x81)
#define HID_IN_EP_WITH_MSC          (0x83)

#define USB_DESC_TYPE_ASSOCIATION   (0x0b)

#define CDC_CMD_PACKET_SIZE           8
#define CDC_DATA_IN_PACKET_SIZE       CDC_DATA_FS_MAX_PACKET_SIZE
#define CDC_DATA_OUT_PACKET_SIZE      CDC_DATA_FS_MAX_PACKET_SIZE

#define MSC_MAX_PACKET                0x40
#define USB_MSC_CONFIG_DESC_SIZ       32
#define BOT_GET_MAX_LUN               0xFE
#define BOT_RESET                     0xFF

#define HID_MAX_PACKET                0x04
#define USB_HID_DESC_SIZ              9
#define HID_MOUSE_REPORT_DESC_SIZE    74
#define HID_KEYBOARD_REPORT_DESC_SIZE 63
#define HID_DESCRIPTOR_TYPE           0x21
#define HID_REPORT_DESC               0x22
#define HID_REQ_SET_PROTOCOL          0x0B
#define HID_REQ_GET_PROTOCOL          0x03
#define HID_REQ_SET_IDLE              0x0A
#define HID_REQ_GET_IDLE              0x02

static int usbd_config = 1;
static int dbg_xfer_length=0;
extern void usbdbg_data_in(void *buffer, int length);
extern void usbdbg_data_out(void *buffer, int length);
extern void usbdbg_control(void *buffer, uint8_t brequest, uint16_t wlength);
__ALIGN_BEGIN static uint8_t dbg_xfer_buffer[MSC_MAX_PACKET] __ALIGN_END;

typedef enum {
  HID_IDLE = 0,
  HID_BUSY,
} HID_StateTypeDef;

typedef struct {
  uint32_t             Protocol;
  uint32_t             IdleState;
  uint32_t             AltSetting;
  HID_StateTypeDef     state;
} USBD_HID_HandleTypeDef;

static uint8_t usbd_mode;
static uint8_t hid_in_ep;
static uint8_t hid_iface_num;

static USBD_CDC_ItfTypeDef *CDC_fops;
static USBD_StorageTypeDef *MSC_fops;

static USBD_CDC_HandleTypeDef CDC_ClassData;
static USBD_MSC_BOT_HandleTypeDef MSC_BOT_ClassData;
static USBD_HID_HandleTypeDef HID_ClassData;

// I don't think we can make these descriptors constant because they are
// modified (perhaps unnecessarily) by the USB driver.

// USB Standard Device Descriptor
__ALIGN_BEGIN static uint8_t USBD_CDC_MSC_HID_DeviceQualifierDesc[USB_LEN_DEV_QUALIFIER_DESC] __ALIGN_END = {
    USB_LEN_DEV_QUALIFIER_DESC,
    USB_DESC_TYPE_DEVICE_QUALIFIER,
    0x00,
    0x02,
    0x00,
    0x00,
    0x00,
    0x40, // same for CDC and MSC (latter being MSC_MAX_PACKET), HID is 0x04
    0x01,
    0x00,
};

// USB CDC MSC device Configuration Descriptor
__ALIGN_BEGIN static uint8_t USBD_CONFIG_DESC1[USBD_CONFIG_DESC_SIZE] __ALIGN_END = {
    //--------------------------------------------------------------------------
    // Configuration Descriptor
    0x09,                           // bLength: Configuration Descriptor size
    USB_DESC_TYPE_CONFIGURATION,    // bDescriptorType: Configuration
    LOBYTE(USBD_CONFIG_DESC_SIZE),  // wTotalLength: no of returned bytes
    HIBYTE(USBD_CONFIG_DESC_SIZE),
    0x04,                           // bNumInterfaces: 4 interfaces
    0x01,                           // bConfigurationValue: Configuration value
    0x00,                           // iConfiguration: Index of string descriptor describing the configuration
    0x80,                           // bmAttributes: bus powered; 0xc0 for self powered
    0xfa,                           // bMaxPower: in units of 2mA

    //==========================================================================
    // Interface Association for CDC VCP
    //--------------------------------------------------------------------------
    0x08,                           // bLength: 8 bytes
    USB_DESC_TYPE_ASSOCIATION,      // bDescriptorType: IAD
    CDC_IFACE_NUM,                  // bFirstInterface: first interface for this association
    0x02,                           // bInterfaceCount: nummber of interfaces for this association
    0x02,                           // bFunctionClass: ?
    0x02,                           // bFunctionSubClass: ?
    0x01,                           // bFunctionProtocol: ?
    0x00,                           // iFunction: index of string for this function

    //--------------------------------------------------------------------------
    // Interface Descriptor
    0x09,                           // bLength: Interface Descriptor size
    USB_DESC_TYPE_INTERFACE,        // bDescriptorType: Interface
    CDC_IFACE_NUM,                  // bInterfaceNumber: Number of Interface
    0x00,                           // bAlternateSetting: Alternate setting
    0x01,                           // bNumEndpoints: One endpoints used
    0x02,                           // bInterfaceClass: Communication Interface Class
    0x02,                           // bInterfaceSubClass: Abstract Control Model
    0x01,                           // bInterfaceProtocol: Common AT commands
    0x00,                           // iInterface:

    // Header Functional Descriptor
    0x05,                           // bLength: Endpoint Descriptor size
    0x24,                           // bDescriptorType: CS_INTERFACE
    0x00,                           // bDescriptorSubtype: Header Func Desc
    0x10,                           // bcdCDC: spec release number
    0x01,                           // ?

    // Call Management Functional Descriptor
    0x05,                           // bFunctionLength
    0x24,                           // bDescriptorType: CS_INTERFACE
    0x01,                           // bDescriptorSubtype: Call Management Func Desc
    0x00,                           // bmCapabilities: D0+D1
    CDC_IFACE_NUM + 1,              // bDataInterface: 1

    // ACM Functional Descriptor
    0x04,                           // bFunctionLength
    0x24,                           // bDescriptorType: CS_INTERFACE
    0x02,                           // bDescriptorSubtype: Abstract Control Management desc
    0x02,                           // bmCapabilities

    // Union Functional Descriptor
    0x05,                           // bFunctionLength
    0x24,                           // bDescriptorType: CS_INTERFACE
    0x06,                           // bDescriptorSubtype: Union func desc
    CDC_IFACE_NUM + 0,              // bMasterInterface: Communication class interface
    CDC_IFACE_NUM + 1,              // bSlaveInterface0: Data Class Interface

    // Endpoint 2 Descriptor
    0x07,                           // bLength: Endpoint Descriptor size
    USB_DESC_TYPE_ENDPOINT,         // bDescriptorType: Endpoint
    CDC_CMD_EP,                     // bEndpointAddress
    0x03,                           // bmAttributes: Interrupt
    LOBYTE(CDC_CMD_PACKET_SIZE),    // wMaxPacketSize:
    HIBYTE(CDC_CMD_PACKET_SIZE),
    0x20,                           // bInterval: polling interval in frames of 1ms

    //--------------------------------------------------------------------------
    // Data class interface descriptor
    0x09,                           // bLength: Endpoint Descriptor size
    USB_DESC_TYPE_INTERFACE,        // bDescriptorType: interface
    CDC_IFACE_NUM + 1,              // bInterfaceNumber: Number of Interface
    0x00,                           // bAlternateSetting: Alternate setting
    0x02,                           // bNumEndpoints: Two endpoints used
    0x0A,                           // bInterfaceClass: CDC
    0x00,                           // bInterfaceSubClass: ?
    0x00,                           // bInterfaceProtocol: ?
    0x00,                           // iInterface:

    // Endpoint OUT Descriptor
    0x07,                               // bLength: Endpoint Descriptor size
    USB_DESC_TYPE_ENDPOINT,             // bDescriptorType: Endpoint
    CDC_OUT_EP,                         // bEndpointAddress
    0x02,                               // bmAttributes: Bulk
    LOBYTE(CDC_DATA_FS_MAX_PACKET_SIZE),// wMaxPacketSize:
    HIBYTE(CDC_DATA_FS_MAX_PACKET_SIZE),
    0x00,                               // bInterval: ignore for Bulk transfer

    // Endpoint IN Descriptor
    0x07,                               // bLength: Endpoint Descriptor size
    USB_DESC_TYPE_ENDPOINT,             // bDescriptorType: Endpoint
    CDC_IN_EP,                          // bEndpointAddress
    0x02,                               // bmAttributes: Bulk
    LOBYTE(CDC_DATA_FS_MAX_PACKET_SIZE),// wMaxPacketSize:
    HIBYTE(CDC_DATA_FS_MAX_PACKET_SIZE),
    0x00,                               // bInterval: ignore for Bulk transfer

    //==========================================================================
    // MSC Interface Descriptor
    //--------------------------------------------------------------------------
    0x09,                           // bLength: Interface Descriptor size
    USB_DESC_TYPE_INTERFACE,        // bDescriptorType: interface descriptor
    MSC_IFACE_NUM,                  // bInterfaceNumber: Number of Interface
    0x00,                           // bAlternateSetting: Alternate setting
    0x02,                           // bNumEndpoints
    0x08,                           // bInterfaceClass: MSC Class
    0x06,                           // bInterfaceSubClass : SCSI transparent
    0x50,                           // nInterfaceProtocol
    0x00,                           // iInterface:

    // Endpoint IN descriptor
    0x07,                           // bLength: Endpoint descriptor length
    USB_DESC_TYPE_ENDPOINT,         // bDescriptorType: Endpoint descriptor type
    MSC_IN_EP,                      // bEndpointAddress: IN, address 3
    0x02,                           // bmAttributes: Bulk endpoint type
    LOBYTE(MSC_MAX_PACKET),         // wMaxPacketSize
    HIBYTE(MSC_MAX_PACKET),
    0x00,                           // bInterval: ignore for Bulk transfer

    // Endpoint OUT descriptor
    0x07,                           // bLength: Endpoint descriptor length
    USB_DESC_TYPE_ENDPOINT,         // bDescriptorType: Endpoint descriptor type
    MSC_OUT_EP,                     // bEndpointAddress: OUT, address 3
    0x02,                           // bmAttributes: Bulk endpoint type
    LOBYTE(MSC_MAX_PACKET),         // wMaxPacketSize
    HIBYTE(MSC_MAX_PACKET),
    0x00,                           // bInterval: ignore for Bulk transfer

    //==========================================================================
    // OpenMV Debug Interface Descriptor
    //--------------------------------------------------------------------------
    0x09,                           // bLength: Interface Descriptor size
    USB_DESC_TYPE_INTERFACE,        // bDescriptorType: interface descriptor
    DBG_IFACE_NUM,                  // bInterfaceNumber: Number of Interface
    0x00,                           // bAlternateSetting: Alternate setting
    0x00,                           // bNumEndpoints
    0xFF,                           // bInterfaceClass: Vendor Specific
    0x00,                           // bInterfaceSubClass
    0x00,                           // nInterfaceProtocol
    0x00,                           // iInterface
};

// USB CDC DEBUG device Configuration Descriptor
__ALIGN_BEGIN static uint8_t USBD_CONFIG_DESC2[USBD_CONFIG_DESC_SIZE] __ALIGN_END = {
    //--------------------------------------------------------------------------
    // Configuration Descriptor
    0x09,                           // bLength: Configuration Descriptor size
    USB_DESC_TYPE_CONFIGURATION,    // bDescriptorType: Configuration
    LOBYTE(USBD_CONFIG_DESC_SIZE),  // wTotalLength: no of returned bytes
    HIBYTE(USBD_CONFIG_DESC_SIZE),
    0x04,                           // bNumInterfaces: 4 interfaces
    0x01,                           // bConfigurationValue: Configuration value
    0x00,                           // iConfiguration: Index of string descriptor describing the configuration
    0x80,                           // bmAttributes: bus powered; 0xc0 for self powered
    0xfa,                           // bMaxPower: in units of 2mA

    //==========================================================================
    // Interface Association for CDC VCP
    0x08,                           // bLength: 8 bytes
    USB_DESC_TYPE_ASSOCIATION,      // bDescriptorType: IAD
    CDC_IFACE_NUM,                  // bFirstInterface: first interface for this association
    0x02,                           // bInterfaceCount: nummber of interfaces for this association
    0x02,                           // bFunctionClass: ?
    0x02,                           // bFunctionSubClass: ?
    0x01,                           // bFunctionProtocol: ?
    0x00,                           // iFunction: index of string for this function

    //--------------------------------------------------------------------------
    // Interface Descriptor
    0x09,                           // bLength: Interface Descriptor size
    USB_DESC_TYPE_INTERFACE,        // bDescriptorType: Interface
    CDC_IFACE_NUM,                  // bInterfaceNumber: Number of Interface
    0x00,                           // bAlternateSetting: Alternate setting
    0x01,                           // bNumEndpoints: One endpoints used
    0x02,                           // bInterfaceClass: Communication Interface Class
    0x02,                           // bInterfaceSubClass: Abstract Control Model
    0x01,                           // bInterfaceProtocol: Common AT commands
    0x00,                           // iInterface:

    // Header Functional Descriptor
    0x05,                           // bLength: Endpoint Descriptor size
    0x24,                           // bDescriptorType: CS_INTERFACE
    0x00,                           // bDescriptorSubtype: Header Func Desc
    0x10,                           // bcdCDC: spec release number
    0x01,                           // ?

    // Call Management Functional Descriptor
    0x05,                           // bFunctionLength
    0x24,                           // bDescriptorType: CS_INTERFACE
    0x01,                           // bDescriptorSubtype: Call Management Func Desc
    0x00,                           // bmCapabilities: D0+D1
    CDC_IFACE_NUM + 1,              // bDataInterface: 1

    // ACM Functional Descriptor
    0x04,                           // bFunctionLength
    0x24,                           // bDescriptorType: CS_INTERFACE
    0x02,                           // bDescriptorSubtype: Abstract Control Management desc
    0x02,                           // bmCapabilities

    // Union Functional Descriptor
    0x05,                           // bFunctionLength
    0x24,                           // bDescriptorType: CS_INTERFACE
    0x06,                           // bDescriptorSubtype: Union func desc
    CDC_IFACE_NUM + 0,              // bMasterInterface: Communication class interface
    CDC_IFACE_NUM + 1,              // bSlaveInterface0: Data Class Interface

    // Endpoint 2 Descriptor
    0x07,                           // bLength: Endpoint Descriptor size
    USB_DESC_TYPE_ENDPOINT,         // bDescriptorType: Endpoint
    CDC_CMD_EP,                     // bEndpointAddress
    0x03,                           // bmAttributes: Interrupt
    LOBYTE(CDC_CMD_PACKET_SIZE),    // wMaxPacketSize:
    HIBYTE(CDC_CMD_PACKET_SIZE),
    0x20,                           // bInterval: polling interval in frames of 1ms

    //--------------------------------------------------------------------------
    // Data class interface descriptor
    0x09,                           // bLength: Endpoint Descriptor size
    USB_DESC_TYPE_INTERFACE,        // bDescriptorType: interface
    CDC_IFACE_NUM + 1,              // bInterfaceNumber: Number of Interface
    0x00,                           // bAlternateSetting: Alternate setting
    0x02,                           // bNumEndpoints: Two endpoints used
    0x0A,                           // bInterfaceClass: CDC
    0x00,                           // bInterfaceSubClass: ?
    0x00,                           // bInterfaceProtocol: ?
    0x00,                           // iInterface:

    // Endpoint OUT Descriptor
    0x07,                               // bLength: Endpoint Descriptor size
    USB_DESC_TYPE_ENDPOINT,             // bDescriptorType: Endpoint
    CDC_OUT_EP,                         // bEndpointAddress
    0x02,                               // bmAttributes: Bulk
    LOBYTE(CDC_DATA_FS_MAX_PACKET_SIZE),// wMaxPacketSize:
    HIBYTE(CDC_DATA_FS_MAX_PACKET_SIZE),
    0x00,                               // bInterval: ignore for Bulk transfer

    // Endpoint IN Descriptor
    0x07,                               // bLength: Endpoint Descriptor size
    USB_DESC_TYPE_ENDPOINT,             // bDescriptorType: Endpoint
    CDC_IN_EP,                          // bEndpointAddress
    0x02,                               // bmAttributes: Bulk
    LOBYTE(CDC_DATA_FS_MAX_PACKET_SIZE),// wMaxPacketSize:
    HIBYTE(CDC_DATA_FS_MAX_PACKET_SIZE),
    0x00,                               // bInterval: ignore for Bulk transfer

    //==========================================================================
    // Dummy Interface Descriptor
    // This is needed to keep the same debug interface number
    //--------------------------------------------------------------------------
    0x09,                               // bLength: Interface Descriptor size
    USB_DESC_TYPE_INTERFACE,            // bDescriptorType: interface descriptor
    MSC_IFACE_NUM,                      // bInterfaceNumber: Number of Interface
    0x00,                               // bAlternateSetting: Alternate setting
    0x00,                               // bNumEndpoints
    0xFF,                               // bInterfaceClass: Vendor Specific
    0x00,                               // bInterfaceSubClass
    0x00,                               // nInterfaceProtocol
    0x00,                               // iInterface

    //==========================================================================
    // OpenMV Debug Interface Descriptor
    //--------------------------------------------------------------------------
    0x09,                               // bLength: Interface Descriptor size
    USB_DESC_TYPE_INTERFACE,            // bDescriptorType: interface descriptor
    DBG_IFACE_NUM,                      // bInterfaceNumber: Number of Interface
    0x00,                               // bAlternateSetting: Alternate setting
    0x02,                               // bNumEndpoints
    0xFF,                               // bInterfaceClass: Vendor Specific
    0x00,                               // bInterfaceSubClass
    0x00,                               // nInterfaceProtocol
    0x00,                               // iInterface:

    // Endpoint IN descriptor
    0x07,                               // bLength: Endpoint descriptor length
    USB_DESC_TYPE_ENDPOINT,             // bDescriptorType: Endpoint descriptor type
    MSC_IN_EP,                          // bEndpointAddress: IN, address 3
    0x02,                               // bmAttributes: Bulk endpoint type
    LOBYTE(MSC_MAX_PACKET),             // wMaxPacketSize
    HIBYTE(MSC_MAX_PACKET),
    0x00,                               // bInterval: ignore for Bulk transfer

    // Endpoint OUT descriptor
    0x07,                               // bLength: Endpoint descriptor length
    USB_DESC_TYPE_ENDPOINT,             // bDescriptorType: Endpoint descriptor type
    MSC_OUT_EP,                         // bEndpointAddress: OUT, address 3
    0x02,                               // bmAttributes: Bulk endpoint type
    LOBYTE(MSC_MAX_PACKET),             // wMaxPacketSize
    HIBYTE(MSC_MAX_PACKET),
    0x00,                               // bInterval: ignore for Bulk transfer
};

/* USB HID device Configuration Descriptor */
__ALIGN_BEGIN static uint8_t USBD_HID_Desc[USB_HID_DESC_SIZ] __ALIGN_END = {
  0x09,         /*bLength: HID Descriptor size*/
  HID_DESCRIPTOR_TYPE, /*bDescriptorType: HID*/
  0x11,         /*bcdHID: HID Class Spec release number*/
  0x01,
  0x00,         /*bCountryCode: Hardware target country*/
  0x01,         /*bNumDescriptors: Number of HID class descriptors to follow*/
  0x22,         /*bDescriptorType*/
  HID_MOUSE_REPORT_DESC_SIZE,/*wItemLength: Total length of Report descriptor*/
  0x00,
};

__ALIGN_BEGIN static uint8_t HID_MOUSE_ReportDesc[HID_MOUSE_REPORT_DESC_SIZE] __ALIGN_END = {
  0x05,   0x01,
  0x09,   0x02,
  0xA1,   0x01,
  0x09,   0x01,

  0xA1,   0x00,
  0x05,   0x09,
  0x19,   0x01,
  0x29,   0x03,

  0x15,   0x00,
  0x25,   0x01,
  0x95,   0x03,
  0x75,   0x01,

  0x81,   0x02,
  0x95,   0x01,
  0x75,   0x05,
  0x81,   0x01,

  0x05,   0x01,
  0x09,   0x30,
  0x09,   0x31,
  0x09,   0x38,

  0x15,   0x81,
  0x25,   0x7F,
  0x75,   0x08,
  0x95,   0x03,

  0x81,   0x06,
  0xC0,   0x09,
  0x3c,   0x05,
  0xff,   0x09,

  0x01,   0x15,
  0x00,   0x25,
  0x01,   0x75,
  0x01,   0x95,

  0x02,   0xb1,
  0x22,   0x75,
  0x06,   0x95,
  0x01,   0xb1,

  0x01,   0xc0
};

void USBD_SelectMode(uint32_t mode) {
    // save mode
    usbd_mode = mode;

    // set up HID parameters if HID is selected
    if (mode & USBD_MODE_HID) {
        if (mode & USBD_MODE_CDC) {
            hid_in_ep = HID_IN_EP_WITH_CDC;
            hid_iface_num = HID_IFACE_NUM_WITH_CDC;
        } else if (mode & USBD_MODE_MSC) {
            hid_in_ep = HID_IN_EP_WITH_MSC;
            hid_iface_num = HID_IFACE_NUM_WITH_MSC;
        }
    }
}

static uint8_t USBD_CDC_MSC_HID_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx) {
    if (pdev->dev_speed == USBD_SPEED_HIGH) {
        // can't handle high speed
        return 1;
    }

    if (usbd_mode & USBD_MODE_CDC) {
        // CDC interface endpoints
        USBD_LL_OpenEP(pdev, CDC_IN_EP, USBD_EP_TYPE_BULK, CDC_DATA_IN_PACKET_SIZE);
        USBD_LL_OpenEP(pdev, CDC_OUT_EP, USBD_EP_TYPE_BULK, CDC_DATA_OUT_PACKET_SIZE);
        USBD_LL_OpenEP(pdev, CDC_CMD_EP, USBD_EP_TYPE_INTR, CDC_CMD_PACKET_SIZE);

        // Init physical Interface components
        CDC_fops->Init();

        // Init Xfer states
        CDC_ClassData.TxState =0;
        CDC_ClassData.RxState =0;

        // Prepare Out endpoint to receive next packet
        USBD_LL_PrepareReceive(pdev, CDC_OUT_EP, CDC_ClassData.RxBuffer, CDC_DATA_OUT_PACKET_SIZE);
    }

    if (usbd_mode & USBD_MODE_MSC) {
        // MSC interface endpoints
        USBD_LL_OpenEP(pdev, MSC_OUT_EP, USBD_EP_TYPE_BULK, MSC_MAX_PACKET);
        USBD_LL_OpenEP(pdev, MSC_IN_EP, USBD_EP_TYPE_BULK, MSC_MAX_PACKET);

        if (usbd_config == 1) {
            // MSC uses the pClassData pointer because SCSI and BOT reference it
            pdev->pClassData = &MSC_BOT_ClassData;

            // Init the BOT layer
            MSC_BOT_Init(pdev);
        }
    }

    if (usbd_mode & USBD_MODE_HID) {
        // HID interface endpoints
        HID_ClassData.state = HID_IDLE;
        USBD_LL_OpenEP(pdev, hid_in_ep, USBD_EP_TYPE_INTR, HID_MAX_PACKET);
    }

    return 0;
}

static uint8_t USBD_CDC_MSC_HID_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx) {
    if (usbd_mode & USBD_MODE_CDC) {
        // CDC interface endpoints
        USBD_LL_CloseEP(pdev, CDC_IN_EP);
        USBD_LL_CloseEP(pdev, CDC_OUT_EP);
        USBD_LL_CloseEP(pdev, CDC_CMD_EP);

        // DeInit physical Interface components
        CDC_fops->DeInit();
    }

    if (usbd_mode & USBD_MODE_MSC) {
        // MSC interface endpoints
        USBD_LL_CloseEP(pdev, MSC_IN_EP);
        USBD_LL_CloseEP(pdev, MSC_OUT_EP);


        if (usbd_config == 1) {
            // DeInit the BOT layer
            MSC_BOT_DeInit(pdev);

            // clear the pointer
            pdev->pClassData = NULL;
        }
    }

    if (usbd_mode & USBD_MODE_HID) {
        // HID interface endpoints
        USBD_LL_CloseEP(pdev, hid_in_ep);
    }

    return 0;
}

static void soft_disconnect(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
    PCD_HandleTypeDef *hpcd = pdev->pData;
    USB_OTG_GlobalTypeDef *USBx = hpcd->Instance;

    if (usbd_config != cfgidx) {
        USB_DevDisconnect(USBx);
        usbd_config = cfgidx;
        USB_DevConnect(USBx);
    }
}

static uint8_t USBD_CDC_MSC_HID_Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req)
{
    switch (req->bmRequest & USB_REQ_TYPE_MASK) {
        // Class request
        case USB_REQ_TYPE_CLASS:
            // req->wIndex is the recipient interface number
            if ((usbd_mode & USBD_MODE_CDC) && req->wIndex == CDC_IFACE_NUM) {
                // CDC component
                if (req->wLength) {
                    if (req->bmRequest & 0x80) {
                        // device-to-host request
                        CDC_fops->Control(req->bRequest, (uint8_t*)CDC_ClassData.data, req->wLength);
                        USBD_CtlSendData(pdev, (uint8_t*)CDC_ClassData.data, req->wLength);
                    } else {
                        // host-to-device request
                        CDC_ClassData.CmdOpCode = req->bRequest;
                        CDC_ClassData.CmdLength = req->wLength;
                        USBD_CtlPrepareRx(pdev, (uint8_t*)CDC_ClassData.data, req->wLength);
                    }
                } else {
                    // Not a Data request
                    // Transfer the command to the interface layer
                    return CDC_fops->Control(req->bRequest, NULL, req->wValue);
                }
            } else if ((usbd_mode & USBD_MODE_MSC) && req->wIndex == MSC_IFACE_NUM) {
                // MSC component
                switch (req->bRequest) {
                    case BOT_GET_MAX_LUN:
                        if ((req->wValue  == 0) && (req->wLength == 1) && ((req->bmRequest & 0x80) == 0x80)) {
                            MSC_BOT_ClassData.max_lun = MSC_fops->GetMaxLun();
                            USBD_CtlSendData(pdev, (uint8_t *)&MSC_BOT_ClassData.max_lun, 1);
                        } else {
                            USBD_CtlError(pdev, req);
                            return USBD_FAIL;
                        }
                        break;

                    case BOT_RESET:
                      if((req->wValue  == 0) && (req->wLength == 0) && ((req->bmRequest & 0x80) != 0x80)) {
                         MSC_BOT_Reset(pdev);
                      } else {
                         USBD_CtlError(pdev , req);
                         return USBD_FAIL;
                      }
                      break;

                    default:
                       USBD_CtlError(pdev, req);
                       return USBD_FAIL;
                }
            } else if ((usbd_mode & USBD_MODE_HID) && req->wIndex == hid_iface_num) {
                switch (req->bRequest) {
                    case HID_REQ_SET_PROTOCOL:
                        HID_ClassData.Protocol = (uint8_t)(req->wValue);
                        break;

                    case HID_REQ_GET_PROTOCOL:
                        USBD_CtlSendData (pdev, (uint8_t *)&HID_ClassData.Protocol, 1);
                        break;

                    case HID_REQ_SET_IDLE:
                        HID_ClassData.IdleState = (uint8_t)(req->wValue >> 8);
                        break;

                    case HID_REQ_GET_IDLE:
                        USBD_CtlSendData (pdev, (uint8_t *)&HID_ClassData.IdleState, 1);
                        break;

                    default:
                        USBD_CtlError(pdev, req);
                        return USBD_FAIL;
                }
            }
            break;

        // Interface & Endpoint request
        case USB_REQ_TYPE_STANDARD:
            if ((usbd_mode & USBD_MODE_MSC) && req->wIndex == MSC_IFACE_NUM) {
                switch (req->bRequest) {
                    case USB_REQ_GET_INTERFACE :
                        USBD_CtlSendData(pdev, (uint8_t *)&MSC_BOT_ClassData.interface, 1);
                        break;

                    case USB_REQ_SET_INTERFACE :
                        MSC_BOT_ClassData.interface = (uint8_t)(req->wValue);
                        break;

                    case USB_REQ_CLEAR_FEATURE:
                        /* Flush the FIFO and Clear the stall status */
                        USBD_LL_FlushEP(pdev, (uint8_t)req->wIndex);

                        /* Re-activate the EP */
                        USBD_LL_CloseEP (pdev , (uint8_t)req->wIndex);
                        if((((uint8_t)req->wIndex) & 0x80) == 0x80) {
                            /* Open EP IN */
                            USBD_LL_OpenEP(pdev, MSC_IN_EP, USBD_EP_TYPE_BULK, MSC_MAX_PACKET);
                        } else {
                            /* Open EP OUT */
                            USBD_LL_OpenEP(pdev, MSC_OUT_EP, USBD_EP_TYPE_BULK, MSC_MAX_PACKET);
                        }
                        /* Handle BOT error */
                        MSC_BOT_CplClrFeature(pdev, (uint8_t)req->wIndex);
                        break;
                }
            } else if ((usbd_mode & USBD_MODE_HID) && req->wIndex == hid_iface_num) {
                switch (req->bRequest) {
                    case USB_REQ_GET_DESCRIPTOR: {
                      uint16_t len = 0;
                      const uint8_t *pbuf = NULL;
                      if (req->wValue >> 8 == HID_REPORT_DESC) {
                        len = MIN(HID_MOUSE_REPORT_DESC_SIZE , req->wLength);
                        pbuf = HID_MOUSE_ReportDesc;
                      } else if (req->wValue >> 8 == HID_DESCRIPTOR_TYPE) {
                        len = MIN(USB_HID_DESC_SIZ , req->wLength);
                        pbuf = USBD_HID_Desc;
                      }
                      USBD_CtlSendData(pdev, (uint8_t*)pbuf, len);
                      break;
                     }

                    case USB_REQ_GET_INTERFACE:
                      USBD_CtlSendData (pdev, (uint8_t *)&HID_ClassData.AltSetting, 1);
                      break;

                    case USB_REQ_SET_INTERFACE:
                      HID_ClassData.AltSetting = (uint8_t)(req->wValue);
                      break;
                }
            }
            break;

        // OpenMV Vendor Request ------------------------------
        case USB_REQ_TYPE_VENDOR:
            if (req->bmRequest & USB_REQ_RECIPIENT_INTERFACE) {
                if (req->bRequest==0xFF) {
                    soft_disconnect(pdev, req->wValue);
                    break;
                }
                int wValue = req->wValue;
                if (req->bRequest==2) {
                    wValue<<=2; //TODO: This is a Hack for big frames
                }
                int bytes = MIN(wValue, MSC_MAX_PACKET);
                dbg_xfer_length =wValue - bytes;

                usbdbg_control(dbg_xfer_buffer, req->bRequest, req->wValue);
                if (req->wValue) {
                    // There's a data phase
                    if (req->bmRequest & 0x80) { /* Device to host */
                        /* call user callback */
                        usbdbg_data_in(dbg_xfer_buffer, bytes);
                        /* Fill IN endpoint fifo with first packet */
                        USBD_LL_Transmit (pdev, MSC_IN_EP, dbg_xfer_buffer, bytes);
                    } else { /* Host to device */
                        /* Prepare Out endpoint to receive next packet */
                        USBD_LL_PrepareReceive(pdev, MSC_OUT_EP, (uint8_t*)(dbg_xfer_buffer), bytes);
                    }
                }
            }
            break;
    }
    return USBD_OK;
}

static uint8_t USBD_CDC_MSC_HID_EP0_RxReady(USBD_HandleTypeDef *pdev) {
  if((CDC_fops != NULL) && (CDC_ClassData.CmdOpCode != 0xFF)) {
    CDC_fops->Control(CDC_ClassData.CmdOpCode, (uint8_t *)CDC_ClassData.data, CDC_ClassData.CmdLength);
      CDC_ClassData.CmdOpCode = 0xFF;
  }
  return USBD_OK;
}

static uint8_t USBD_CDC_MSC_HID_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum) {
    if ((usbd_mode & USBD_MODE_CDC) && (epnum == (CDC_IN_EP & 0x7f) || epnum == (CDC_CMD_EP & 0x7f))) {
        CDC_ClassData.TxState = 0;
    } else if ((usbd_mode & USBD_MODE_MSC) && epnum == (MSC_IN_EP & 0x7f)) {
        switch (usbd_config){
            case 1:
                MSC_BOT_DataIn(pdev, epnum);
                break;
            case 2: {
                int bytes = MIN(dbg_xfer_length, MSC_MAX_PACKET);
                if (dbg_xfer_length) {
                    usbdbg_data_in(dbg_xfer_buffer, bytes);
                    /* Fill IN endpoint fifo with packet */
                    USBD_LL_Transmit (pdev, MSC_IN_EP, dbg_xfer_buffer, bytes);
                    dbg_xfer_length -= bytes;
                }
                break;
            }
        }
    } else if ((usbd_mode & USBD_MODE_HID) && epnum == (hid_in_ep & 0x7f)) {
        /* Ensure that the FIFO is empty before a new transfer, this condition could
        be caused by  a new transfer before the end of the previous transfer */
        HID_ClassData.state = HID_IDLE;
    }

    return USBD_OK;
}

static uint8_t USBD_CDC_MSC_HID_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum) {
    if ((usbd_mode & USBD_MODE_CDC) && epnum == (CDC_OUT_EP & 0x7f)) {
        /* Get the received data length */
        CDC_ClassData.RxLength = USBD_LL_GetRxDataSize (pdev, epnum);
        /* USB data will be immediately processed, this allow next USB traffic being
        NAKed till the end of the application Xfer */
        CDC_fops->Receive(CDC_ClassData.RxBuffer, &CDC_ClassData.RxLength);
    } else if ((usbd_mode & USBD_MODE_MSC) && epnum == (MSC_OUT_EP & 0x7f)) {
        switch (usbd_config) {
            case 1:
                MSC_BOT_DataOut(pdev, epnum);
                break;
            case 2: {
                int dbg_xfer_length;
                dbg_xfer_length = USBD_LL_GetRxDataSize(pdev, epnum);
                usbdbg_data_out(dbg_xfer_buffer, dbg_xfer_length);
                /* Prepare Out endpoint to receive next packet */
                USBD_LL_PrepareReceive(pdev, MSC_OUT_EP,
                    (uint8_t*)(dbg_xfer_buffer), MSC_MAX_PACKET);
                break;
            }
        }
    }

    return USBD_OK;
}

static uint8_t *USBD_CDC_MSC_HID_GetCfgDesc(uint16_t *length) {
    /* NOTE: cfgidx is 0 based */
    switch (usbd_config) {
        case 1:
            *length = sizeof(USBD_CONFIG_DESC1);
            return USBD_CONFIG_DESC1;
        case 2:
        default:
            *length = sizeof(USBD_CONFIG_DESC2);
            return USBD_CONFIG_DESC2;
    }
}

uint8_t *USBD_CDC_MSC_HID_GetDeviceQualifierDescriptor (uint16_t *length) {
    *length = sizeof(USBD_CDC_MSC_HID_DeviceQualifierDesc);
    return USBD_CDC_MSC_HID_DeviceQualifierDesc;
}

uint8_t USBD_CDC_RegisterInterface(USBD_HandleTypeDef *pdev, USBD_CDC_ItfTypeDef *fops) {
    if (fops == NULL) {
        return USBD_FAIL;
    } else {
        CDC_fops = fops;
        return USBD_OK;
    }
}

/**
  * @brief  USBD_CDC_SetTxBuffer
  * @param  pdev: device instance
  * @param  pbuff: Tx Buffer
  * @retval status
  */
uint8_t  USBD_CDC_SetTxBuffer  (USBD_HandleTypeDef   *pdev,
                                uint8_t  *pbuff,
                                uint16_t length)
{
  CDC_ClassData.TxBuffer = pbuff;
  CDC_ClassData.TxLength = length;  
  
  return USBD_OK;  
}


/**
  * @brief  USBD_CDC_SetRxBuffer
  * @param  pdev: device instance
  * @param  pbuff: Rx Buffer
  * @retval status
  */
uint8_t  USBD_CDC_SetRxBuffer  (USBD_HandleTypeDef   *pdev,
                                   uint8_t  *pbuff)
{
  CDC_ClassData.RxBuffer = pbuff;
  
  return USBD_OK;
}

/**
  * @brief  USBD_CDC_DataOut
  *         Data received on non-control Out endpoint
  * @param  pdev: device instance
  * @param  epnum: endpoint number
  * @retval status
  */
uint8_t  USBD_CDC_TransmitPacket(USBD_HandleTypeDef *pdev) {
    if(CDC_ClassData.TxState == 0) {
      
      /* Transmit next packet */
      USBD_LL_Transmit(pdev,
                       CDC_IN_EP,
                       CDC_ClassData.TxBuffer,
                       CDC_ClassData.TxLength);
      
      /* Tx Transfer in progress */
      CDC_ClassData.TxState = 1;
      return USBD_OK;
    }
    else
    {
      return USBD_BUSY;
    }
}


/**
  * @brief  USBD_CDC_ReceivePacket
  *         prepare OUT Endpoint for reception
  * @param  pdev: device instance
  * @retval status
  */
uint8_t USBD_CDC_ReceivePacket(USBD_HandleTypeDef *pdev) {
    // Suspend or Resume USB Out process
    if (pdev->dev_speed == USBD_SPEED_HIGH) {
        return USBD_FAIL;
    }

    // Prepare Out endpoint to receive next packet */
    USBD_LL_PrepareReceive(pdev,
                           CDC_OUT_EP,
                           CDC_ClassData.RxBuffer,
                           CDC_DATA_OUT_PACKET_SIZE);

    return USBD_OK;
}

uint8_t USBD_MSC_RegisterStorage(USBD_HandleTypeDef *pdev, USBD_StorageTypeDef *fops) {
    if (fops == NULL) {
        return USBD_FAIL;
    } else {
        MSC_fops = fops;
        pdev->pUserData = fops; // MSC uses pUserData because SCSI and BOT reference it
        return USBD_OK;
    }
}

uint8_t USBD_HID_SendReport(USBD_HandleTypeDef *pdev, uint8_t *report, uint16_t len) {
    if (pdev->dev_state == USBD_STATE_CONFIGURED) {
        if (HID_ClassData.state == HID_IDLE) {
            HID_ClassData.state = HID_BUSY;
            USBD_LL_Transmit(pdev, hid_in_ep, report, len);
        }
    }
    return USBD_OK;
}

// CDC + MSC interface class callbacks structure
USBD_ClassTypeDef USBD_CDC_MSC_HID = {
    USBD_CDC_MSC_HID_Init,
    USBD_CDC_MSC_HID_DeInit,
    USBD_CDC_MSC_HID_Setup,
    NULL, // EP0_TxSent
    USBD_CDC_MSC_HID_EP0_RxReady,
    USBD_CDC_MSC_HID_DataIn,
    USBD_CDC_MSC_HID_DataOut,
    NULL, // SOF
    NULL, // IsoINIncomplete
    NULL, // IsoOUTIncomplete
    USBD_CDC_MSC_HID_GetCfgDesc,
    USBD_CDC_MSC_HID_GetCfgDesc,
    USBD_CDC_MSC_HID_GetCfgDesc,
    USBD_CDC_MSC_HID_GetDeviceQualifierDescriptor,
};
