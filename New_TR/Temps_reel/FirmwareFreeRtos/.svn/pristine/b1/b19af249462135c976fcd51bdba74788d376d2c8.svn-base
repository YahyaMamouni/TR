typedef union
{
    struct
    {
        unsigned char id;
        unsigned char rtr;
        unsigned short  val;
    } data;
    long  msg;
} CanFrame;

int  CanRxFrameAvaible(void);
void CanRxReadFrame(CanFrame* data);

int CanTxReady(void);
void CanTxSendFrame(CanFrame data);
void CanTxSendFrameFromIsr(CanFrame com,BaseType_t *pxHigherPriorityTaskWoken);

void* HWCanTx(void* p);
void* HWCanRx(void* p);

void HardwareInit(int CanRxInterruptNo_,int CanTxInterruptNo_);
void HardwareHalt(void);

void SensorColorInit(unsigned int _SensorColorInterruptNo);
unsigned short SensorColorRead(void);
