#include "USB_PD_core.h"
#include "i2c.h"
#include "USBPD_spec_defines.h"
#include "PostProcessEvents.h"

volatile int PostProcess_AttachTransition = 0;
volatile int PostProcess_PD_MessageReceived = 0;
volatile int PostProcess_SRC_PDO_Received = 0;
volatile int PostProcess_PSRDY_Received = 0;
volatile int PostProcess_IrqReceived = 0;

extern USB_PD_StatusTypeDef PD_status[USBPORT_MAX] ;
extern USB_PD_I2C_PORT STUSB45DeviceConf[USBPORT_MAX];


static char MsgBuffer[32];
static int idx_head = 0;
static int idx_tail = 0;
//"C" = CtrlMsg
//"D" = DataMsg


int Push_PD_MessageReceived(char MessageType, char MessageValue)
{
    MsgBuffer[idx_head] = MessageType;
    idx_head++;
    
    MsgBuffer[idx_head] = MessageValue;
    idx_head++;
    
    if(idx_tail == idx_head)
    {
        //error: buffer overflow
        return -1;
    }
    if(idx_head > 31) idx_head = 0;
    
    PostProcess_PD_MessageReceived++;
    
    return 0;
}

int Push_PD_MessageReceived1(char MessageType)
{
    MsgBuffer[idx_head] = MessageType;
    idx_head++;
    
    if(idx_tail == idx_head)
    {
        //error: buffer overflow
        return -1;
    }
    if(idx_head > 31) idx_head = 0;
    
    PostProcess_PD_MessageReceived++;
    
    return 0;
}

int Pop_PD_MessageReceived(void)
{
    char MessageType;
    
    if(idx_tail == idx_head)
    {
        //error: buffer empty
        return -1; //no data available
    }
    
    MessageType = MsgBuffer[idx_tail];
    MsgBuffer[idx_tail] = 0; //clear
    
    idx_tail++;
    if(idx_tail > 31) idx_tail = 0;
    
    return MessageType;
}

static char IrqBuffer[32];
static int irq_idx_head = 0;
static int irq_idx_tail = 0;

int Push_IrqReceived(char MessageType)
{
    PostProcess_IrqReceived++;
    
    IrqBuffer[irq_idx_head] = MessageType;
    
    irq_idx_head++;
    
    if(irq_idx_tail == irq_idx_head)
    {
        //error: buffer overflow
        return -1;
    }
    if(irq_idx_head > (32-1)) irq_idx_head = 0;
    
    return 0;
}

int Pop_IrqReceived(void)
{
    char MessageType;
    
    if(irq_idx_tail == irq_idx_head)
    {
        //error: buffer empty
        return -1; //no data available
    }
    
    MessageType = IrqBuffer[irq_idx_tail];
    IrqBuffer[irq_idx_tail] = 0; //clear
    
    irq_idx_tail++;
    if(irq_idx_tail > (32-1)) irq_idx_tail = 0;
    
    return MessageType;
}



int PostProcess_Events()
{
    int Usb_Port = 0;
    int Status;
    unsigned int Address;
    
    if( PostProcess_IrqReceived != 0)
    {
        int IrqStatus;
        
        while( PostProcess_IrqReceived != 0)
        {
            PostProcess_IrqReceived--;
            
#if DEBUG_PRINTF
            IrqStatus = Pop_IrqReceived();
            printf("irq_%02X ", IrqStatus);
#endif
        }
    }
    
    
    if( PostProcess_AttachTransition == 1)
    {
        PostProcess_AttachTransition = 0;
        
        //printf("Trans. occurred on ATTACH_STATUS bit: ");
        printf("\r\n=== CABLE: ");
        
        if( (PD_status[Usb_Port].Port_Status.d8 & STUSBMASK_ATTACHED_STATUS) == VALUE_ATTACHED)
        {
            //printf("Attached ");
            
            uint8_t Data;
            Address = TYPE_C_STATUS; //[Read]
            Status = I2C_Read_USB_PD(STUSB45DeviceConf[Usb_Port].I2cBus, STUSB45DeviceConf[Usb_Port].I2cDeviceID_7bit, Address , &Data, 1 );
            if(( Data & MASK_REVERSE) == 0)
            {
                printf("Attached [CC1] ");
            }
            else
            {
                printf("Attached [CC2] ");
            }
        }
        else
        {
            Clear_PDO_FROM_SRC(0);
            printf("Not-attached ");
        }
        
        printf("\r\n");
    }
    
    
    if( PostProcess_PD_MessageReceived != 0)
    {
        int MessageType;
        
        PostProcess_PD_MessageReceived--;
        
        while( (MessageType = Pop_PD_MessageReceived() ) != -1)
        {
#if DEBUG_PRINTF
            if(MessageType == 'C')
            {
                printf("CtrlMsg");
                int HeaderMessageType = Pop_PD_MessageReceived();
                printf("%02X", HeaderMessageType);
                
                switch (HeaderMessageType )
                {   
                case USBPD_CTRLMSG_Reserved1:
                    break;
                    
                case USBPD_CTRLMSG_GoodCRC:
                    printf("(GoodCRC)");
                    break;
                    
                case USBPD_CTRLMSG_Accept:
                    printf("(Accept)");
                    break;
                    
                case USBPD_CTRLMSG_Reject:
                    break;
                    
                case USBPD_CTRLMSG_PS_RDY:
                    printf("(PS_RDY)");
                    break;
                    
                case USBPD_CTRLMSG_Get_Source_Cap:
                    break;
                    
                case USBPD_CTRLMSG_Get_Sink_Cap:
                    break;
                    
                case USBPD_CTRLMSG_Wait:
                    break;
                    
                case USBPD_CTRLMSG_Soft_Reset:
                    break;
                    
                case USBPD_CTRLMSG_Not_Supported:
                    break;
                    
                case USBPD_CTRLMSG_Get_Source_Cap_Extended:
                    break;
                    
                case USBPD_CTRLMSG_Get_Status:
                    break;
                    
                case USBPD_CTRLMSG_FR_Swap:
                    break;
                    
                case USBPD_CTRLMSG_Get_PPS_Status:
                    break;
                    
                case USBPD_CTRLMSG_Get_Country_Codes:
                    break;
                    
                default:
                    break;
                }
                
                printf(" ");
            }
            else if(MessageType == 'D')
            {
                printf("DataMsg");
                int HeaderMessageType = Pop_PD_MessageReceived();
                printf("%02X", HeaderMessageType);
                
                switch ( HeaderMessageType )
                {
                case USBPD_DATAMSG_Source_Capabilities :
                    printf("(SourceCap)");
                    break;
                case USBPD_DATAMSG_Request:  /* get request message */
                    printf("(Request)");
                    break;
                case USBPD_DATAMSG_Sink_Capabilities: /* receive Sink cap  */
                    printf("(SinkCap)");
                    break;
                    
                case USBPD_DATAMSG_Vendor_Defined:  /* VDM message */ 
                    printf("(VDM)");
                    break;
                default :
                    break;
                }
                
                printf(" ");
            }
            else
            {
                printf("Empty ");
            }
            
#endif
        } //END while
    }
    
    
    
    
    if( PostProcess_SRC_PDO_Received != 0)
    {
        PostProcess_SRC_PDO_Received--;
        Print_PDO_FROM_SRC(Usb_Port);
        
        //Read_RDO(Usb_Port);
        //Print_RDO(Usb_Port);
        
        /*
        for(int i=0, j=0; i < PDO_FROM_SRC_Num[Usb_Port]; i++, j+=4)
        {
        // RcvPacket : Header(16bit) + Object1(32bit) + Object2(32bit) + ...
        uint32_t MessageRcv32;
        MessageRcv32 = DataRW[j+0];
        MessageRcv32 |= DataRW[j+1] << 8;
        MessageRcv32 |= DataRW[j+2] << 16;
        MessageRcv32 |= DataRW[j+3] << 24;
        
        int tmp32;
        int Voltage_50mV = USBPD_FIXPDOSRC_Voltage_in50mV_units(MessageRcv32);
        float Voltage =  Voltage_50mV * 0.050f;
        
        int MaximumCurrent_10mV = USBPD_FIXPDOSRC_MaximumCurrent_in10mA_units(MessageRcv32);
        float MaximumCurrent = MaximumCurrent_10mV * 0.010f;
        
        int Peak = USBPD_FIXPDOSRC_PeakCurrent(MessageRcv32);
        
        printf("Voltage= %f, ", Voltage);
        printf("MaximumCurrent= %f, ", MaximumCurrent);
        printf("Peak= %d ", Peak);
        printf("\r\n");
    }
        */
        
        return 0;
    }
    
    if( PostProcess_PSRDY_Received != 0)
    {
        PostProcess_PSRDY_Received--;
        
        printf("\r\n");
        Print_RDO(Usb_Port);
    }
    
    
    
    return 0;
}
