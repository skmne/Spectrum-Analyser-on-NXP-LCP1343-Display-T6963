

#define THRE        (1<<5) //Transmit Holding Register Empty
#define MULVAL      1
#define DIVADDVAL   0
#define Ux_FIFO_EN  (1<<0)
#define Rx_FIFO_RST (1<<1)
#define Tx_FIFO_RST (1<<2)
#define DLAB_BIT    (1<<7)
#define LINE_FEED   0x0A //LF, For Linux, MAC and Windows Terminals
#define CARRIAGE_RETURN 0x0D //CR, For Windows Terminals (CR+LF).

#define PRESCALE (72000-1)
#define PRESCALE1 (72000-1)

void initUART(void);
void initTimer0(void);
void initTimer1(void);
void delayMS(unsigned int milliseconds);
void uartWrite(char data);
void delayLCDMS(unsigned int milliseconds);
//#endif
