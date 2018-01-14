
#include <lpc13xx.h>
#include <stdio.h>
#include "ocf_lpc134x_lib.h" //contains code for UART, Timer & to retarget printf().
#include <stdlib.h>
#include <math.h>

#define VREF       5.0 //Reference Voltage at VREFP pin, given VREFN = 0V(GND)
#define ADC_CLK_EN (1<<13) //Enable ADC clock
#define SEL_AD0    (1<<0) //Select Channel AD0
#define CLKDIV     15 // ADC clock-divider (ADC_CLOCK=PCLK/CLKDIV+1), yields 4.5Mhz ADC clock
#define ADC_PWRUP  (~(1<<4)) //setting it to 0 will power it up
#define START_CNV  (1<<24) //001 for starting the conversion immediately
#define ADC_DONE   (1U<<31) //define it as unsigned value or compiler will throw warning
#define ADCR_SETUP_SCM (CLKDIV<<8)



#define T6963_WR (1 << 10)
#define T6963_RD (1 << 9)
#define T6963_CE (1 << 11)
#define T6963_CD (1 << 8)
#define T6963_RESET (1 << 4)

void T6963_Init(void);
void T6963_WriteData(int data);
void T6963_WriteCommand(int command);
void lcd_busy_wait(void);
char lcd_in_sta(void);
void LCD_WriteChar(char c);
void LCD_WriteString(char * string);
void setCursor(int x, int y, int memory);
void ClearLine(int x, float y);
void drawLine(int x,float y);
void enable(void);

float signalADC[32];
float buf[32];
unsigned int result = 0;
float voltage = 0.0;
unsigned int bufmax = 0;

void Enable_FFT(float* signal);
void SampleADC(int s);
void  FFT(float *Rdat, float *Idat, int N, int LogN);


void ShowInterface();
void delay(void);

int main(void)
{
	SystemInit(); //Gets called by Startup code, sets CCLK=100Mhz, PCLK=25Mhz
	initUART(); //Initialize UART for printf()
	initTimer0(); //For delayMS()
	initTimer1();
	LPC_SYSCON->PDRUNCFG &= ADC_PWRUP; //Power-up ADC Block
	LPC_SYSCON->SYSAHBCLKCTRL |= ADC_CLK_EN; //Enable ADC clock
	LPC_ADC->CR = ADCR_SETUP_SCM | SEL_AD0; //Setup ADC Block

	LPC_IOCON->R_PIO0_11 = 0x2;
	LPC_IOCON->R_PIO1_1 = 0x1;
	LPC_GPIO1 -> DIR &=~ (1<<1);

	printf("Разработчик Лесик Александр \n Вывод Логов: \n");

	T6963_Init();
	ShowInterface();
	delayMS(10);

	SampleADC(32);

	while(1)
	{
	}
}

void T6963_Init(void)
{
delayLCDMS(10);
LPC_GPIO1->DIR = (T6963_WR | T6963_RD | T6963_CE | T6963_CD | T6963_RESET);
LPC_GPIO1->DATA = (T6963_WR | T6963_RD | T6963_CE | T6963_CD | T6963_RESET);

LPC_IOCON->RESET_PIO0_0 = 0x1;// для использования второй функции - pio
LPC_GPIO0 ->DIR = 0xFF;
LPC_GPIO0 -> DATA = 0x0;
LPC_GPIO1 -> DATA = 0x0;
delayLCDMS(10);
LPC_GPIO1 -> DATA |= T6963_CE;
LPC_GPIO1 -> DATA |= T6963_RESET;
LPC_GPIO1 -> DATA &=~ T6963_CD;
LPC_GPIO1 -> DATA |= T6963_RD;
LPC_GPIO1 -> DATA |= T6963_WR;

LPC_GPIO1 -> DATA  &=~ T6963_RESET;				/* set the reset line low */
delayLCDMS(10);
LPC_GPIO1 -> DATA |= T6963_RESET;
delayLCDMS(10);
T6963_WriteData(0x20); // начальный адрес для графики
T6963_WriteData(0x03);
T6963_WriteCommand(0x42);

T6963_WriteData(0x28);// graphics  line width
T6963_WriteData(0x00);
T6963_WriteCommand(0x43);

T6963_WriteData(0x00);// начальный адрес для текста
T6963_WriteData(0x00);
T6963_WriteCommand(0x40);

T6963_WriteData(0x28);// text char line width
T6963_WriteData(0x00);
T6963_WriteCommand(0x41);

T6963_WriteCommand(0x80); // Set ‘MODE SET’ to logical OR
T6963_WriteCommand(0x9E); // DISPLAY MODE

T6963_WriteData(0x00);
T6963_WriteData(0x00);
T6963_WriteCommand(0x24);// установка указателя

}

void ShowInterface(){
	setCursor(3,15,0x0);
	LCD_WriteString("0");

	setCursor(3, 115, 0x320); // перемещение курсора
	T6963_WriteData(0x07); // запись горизонтальной линии
	T6963_WriteCommand(0xC4);//no increment

	for(int i = 4;i<36 ;i++){
		setCursor(i, 115, 0x320); // перемещение курсора
		T6963_WriteData(0x3F); // запись горизонтальной линии
		T6963_WriteCommand(0xC4);//no increment
	}

	setCursor(35,14,0x0);
	T6963_WriteData(0x1E); // запись >
	T6963_WriteCommand(0xC4);//no increment

	setCursor(33,15,0x0);
	LCD_WriteString("32 n");

	for(int i = 0;i<114 ;i++){
			setCursor(3, 114-i, 0x320); // перемещение курсора
			T6963_WriteData(0x04); // запись вертикальной линии
			T6963_WriteCommand(0xC4);//no increment
		}
	setCursor(3,0,0x0);
	T6963_WriteData(0x3E); // запись >
	T6963_WriteCommand(0xC4);//no increment
};


void SampleADC(int s){ //выборка данных с АЦП

	for (int i=0; i<s; i++){
		LPC_ADC->CR |= START_CNV; //Start new Conversion
		while((LPC_ADC->DR0 & ADC_DONE) == 0); //Wait until conversion is finished
		result = (LPC_ADC->DR0>>6) & 0x3FF; //10 bit Mask to extract result
		voltage = ((float)result * VREF) / 1024;
		printf("AD0 = %dmV\n", (int)(voltage*1000)); //вывод log по UART
				//printf("count = %d\n", i);
		signalADC[i] = (voltage*1000); //запись в массив signalADC
		delayMS(500); //задержка полсекунды
	}
	Enable_FFT(signalADC);
}

void Enable_FFT(float* signal)
{
  float Re[32] = {0};
  float Im[32] = {0};// заполняем мнимую часть сигнала

  if ((LPC_GPIO1->DATA & (1<<1))) {//если нажата кнопка
  	for(int i=0; i<32; i++){
  		Re[i]  =  256 / 2 * (1 + cosf((2 * 3.14159265 / 32  * i * 10)));// заполняем действительную часть сигнала тестовой функцией
  	}
  }else{
  	for(int i=0; i<32; i++){
  	Re[i] = signal[i];// заполняем действительную часть сигнала АЦП
  	}
  }
  // NOTE: In this algorithm N and LogN can be only:
  //       N    = 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384;
  //       LogN = 2, 3,  4,  5,  6,   7,   8,   9,   10,   11,   12,   13,    14;
  FFT(Re, Im, 32, 5); //БПФ
  float res = 0;
  int max = 0;
  char buffer[10];
  char *s;

  setCursor(0,13-(bufmax/2)/8,0x00);
  LCD_WriteString("   ");

  for(int i=1; i<32; i++)
  {
	  if ( sqrtf(Re[i]*Re[i]+Im[i]*Im[i]) > max)  max = (int) ((sqrtf(Re[i]*Re[i]+Im[i]*Im[i])) + 0.5f); //находим макс
   }
  bufmax = max;
  setCursor(0,13-(max/2)/8,0x00);
  s = itoa(max,buffer,10);
  LCD_WriteString(s);

  for(int i=1; i<32; i++)
  {
	  ClearLine(i+3, buf[i]); // Очищаем дисплей
	  res = (sqrtf(Re[i]*Re[i]+Im[i]*Im[i])/2); //нормализуем и масштабируем результат
	  drawLine(i+3, res); //
	  printf("res = %10.6f\n", res);
	  buf[i]=res;
  }

  SampleADC(32);
}

void  FFT(float *Rdat, float *Idat, int N, int LogN)
{
  register int  i, j, n, k, io, ie, in, nn;
  float         ru, iu, rtp, itp, rtq, itq, rw, iw, sr;

  static const float Rcoef[14] =
  {  -1.0000000000000000F,  0.0000000000000000F,  0.7071067811865475F,
      0.9238795325112867F,  0.9807852804032304F,  0.9951847266721969F,
      0.9987954562051724F,  0.9996988186962042F,  0.9999247018391445F,
      0.9999811752826011F,  0.9999952938095761F,  0.9999988234517018F,
      0.9999997058628822F,  0.9999999264657178F
  };
  static const float Icoef[14] =
  {   0.0000000000000000F, -1.0000000000000000F, -0.7071067811865474F,
     -0.3826834323650897F, -0.1950903220161282F, -0.0980171403295606F,
     -0.0490676743274180F, -0.0245412285229122F, -0.0122715382857199F,
     -0.0061358846491544F, -0.0030679567629659F, -0.0015339801862847F,
     -0.0007669903187427F, -0.0003834951875714F
  };

  nn = N >> 1;
  ie = N;
  for(n=1; n<=LogN; n++) //элементарный шаг в алгоритме БПФ Nоп = N/2×log2N
  {
    rw = Rcoef[LogN - n];
    iw = Icoef[LogN - n];

    in = ie >> 1;
    ru = 1.0F;
    iu = 0.0F;
    for(j=0; j<in; j++)
    {
      for(i=j; i<N; i+=ie)
      {
        io       = i + in;
        rtp      = Rdat[i]  + Rdat[io];
        itp      = Idat[i]  + Idat[io];
        rtq      = Rdat[i]  - Rdat[io];
        itq      = Idat[i]  - Idat[io];
        Rdat[io] = rtq * ru - itq * iu;
        Idat[io] = itq * ru + rtq * iu;
        Rdat[i]  = rtp;
        Idat[i]  = itp;
      }

      sr = ru;
      ru = ru * rw - iu * iw;
      iu = iu * rw + sr * iw;
    }
    ie >>= 1;
  }

  for(j=i=1; i<N; i++)
  {
    if(i < j)
    {
      io       = i - 1;
      in       = j - 1;
      rtp      = Rdat[in];
      itp      = Idat[in];
      Rdat[in] = Rdat[io];
      Idat[in] = Idat[io];
      Rdat[io] = rtp;
      Idat[io] = itp;
    }

    k = nn;

    while(k < j)
    {
      j   = j - k;
      k >>= 1;
    }

    j = j + k;
  }

  rw = 1.0F / N;

  for(i=0; i<N; i++)
  {
    Rdat[i] *= rw;
    Idat[i] *= rw;
  }
}




void T6963_WriteData(int data)
{
lcd_busy_wait();//-ожидание готовности дисплея
LPC_GPIO0->DIR = 0xFF; //-конфигурирование порта 0 в output
LPC_GPIO0->DATA = data; // запись данных в порт 0

LPC_GPIO1->DATA &=~ T6963_CD; // CD устанавливаем в 0
LPC_GPIO1->DATA &=~ T6963_WR; // WR устанавливаем в 0
LPC_GPIO1->DATA |= T6963_RD; // RD устанавливаем в 1
enable();//CE pulsed low
}

void T6963_WriteCommand(int command)
{
lcd_busy_wait();//-ожидание готовности дисплея
LPC_GPIO0->DIR = 0xFF; //-конфигурирование порта 0 в output
LPC_GPIO0->DATA =  command; // запись данных в порт 0
LPC_GPIO1->DATA |= T6963_CD; // CD устанавливаем в 1
LPC_GPIO1->DATA &=~ T6963_WR; // WR устанавливаем в 0
LPC_GPIO1->DATA |= T6963_RD; // RD устанавливаем в 0
enable();//CE pulsed low
}

void lcd_busy_wait(void)
{
	char dat;
	do
	{
		dat=lcd_in_sta();// запрос состояния дисплея
	}
	while((dat & 0x03) != 0x03);		/* Выполняем до тех пор пока дисплей не будет готов, а именно STA1=1 && STA0=1 */
}

char lcd_in_sta(void)
{	/*
	Read Status - To check the Status of the T6963C controller -
*/
	char dat;
	LPC_GPIO0->DIR = 00000000;
	LPC_GPIO1 -> DATA |= T6963_CD;
	LPC_GPIO1 -> DATA &=~ T6963_RD;					/* set the RD/ line low */
	LPC_GPIO1 -> DATA |= T6963_WR;					/* make sure the WR/ line is high */
	LPC_GPIO1 -> DATA &=~ T6963_CE;					/* enable chip select */
	delayLCDMS(2); // задержка с учётом симуляции proteus
	dat=(unsigned char)(LPC_GPIO0->DATA);
	LPC_GPIO1 -> DATA |= T6963_CE; // CE устанавливаем в 1
	return(dat);
}
void enable(void)
{
	delayLCDMS(2);
	LPC_GPIO1 -> DATA &= ~ T6963_CE;//Enable=Low
	delayLCDMS(2);
	LPC_GPIO1 -> DATA |=  T6963_CE;//Enable=High
}
void LCD_WriteChar(char c)
{
	T6963_WriteData(c - 32); // запись символа
	T6963_WriteCommand(0xC0);
}
void LCD_WriteString(char * string) // печать строки
{
	int c=0;
	while (string[c]!='\0')
	{
		LCD_WriteChar(string[c]);
		c++;
	}
}

void setCursor(int x, int y, int memory){
	int address;
	address = memory + x + (0x28 * y);

	T6963_WriteData(address);
	T6963_WriteData(address >> 8);
	T6963_WriteCommand(0x24);
}

void drawLine(int x,float y){ // рисует линию
	for (int i = 0; i < y;i++)	{
		setCursor(x, (113-i), 0x320); // перемещение курсора
		T6963_WriteData(0x3E); // запись горизонтальной линии
		T6963_WriteCommand(0xC4);//no increment
	}
}

void ClearLine(int x, float y){ // очищает линию
	for (int i = 1; i < y;i++)	{
		setCursor(x, (113-i), 0x320); // перемещение курсора
		T6963_WriteData(0x00); // очистка строки
		T6963_WriteCommand(0xC4);//no increment
	}
}

void delay(void)
{
	int i=0,x=0;
	for(i=0; i<19999; i++){
		x++;
	}
}

