#include "wm8978.h"
#include "DriverUtil.h"
#include "esp_log.h"

const char *WM_TAG = "WM8978";
static uint16_t WM8978_REGVAL[58]=
{
	0X0000,0X0000,0X0000,0X0000,0X0050,0X0000,0X0140,0X0000,
	0X0000,0X0000,0X0000,0X00FF,0X00FF,0X0000,0X0100,0X00FF,
	0X00FF,0X0000,0X012C,0X002C,0X002C,0X002C,0X002C,0X0000,
	0X0032,0X0000,0X0000,0X0000,0X0000,0X0000,0X0000,0X0000,
	0X0038,0X000B,0X0032,0X0000,0X0008,0X000C,0X0093,0X00E9,
	0X0000,0X0000,0X0000,0X0000,0X0003,0X0010,0X0010,0X0100,
	0X0100,0X0002,0X0001,0X0001,0X0039,0X0039,0X0039,0X0039,
	0X0001,0X0001
}; 

uint8_t WM8978_Write_Reg(uint8_t reg,uint16_t val)
{
	uint8_t res;
	uint8_t RegAddr;
	uint8_t RegValue;
	RegAddr=(reg<<1)|((uint8_t)((val>>8)&0x01)); //reg address + data highest bit

	RegValue=(uint8_t)(val&0XFF);

	res=IIC_Write_One_Byte((WM8978_ADDR << 1) ,RegAddr,RegValue);


	if(res==0)
		WM8978_REGVAL[reg]=val;
	return res;
}

uint16_t WM8978_Read_Reg(uint8_t reg)
{  
	return WM8978_REGVAL[reg];	
} 


void WM8978_CLK_Cfg() {
//#define REG_CLOCK_GEN			((uint16_t)(6 << 9))
#define CLKSEL_PLL				(1 << 8)	// Default value
#define MCLK_DIV2				(2 << 5)	// Default value
#define BCLK_DIV8				(3 << 2)
#define MS						(1)

	uint16_t regval = CLKSEL_PLL | MCLK_DIV2 | BCLK_DIV8;
	regval = CLKSEL_PLL | MCLK_DIV2 | BCLK_DIV8 | (regval & MS);
	WM8978_Write_Reg(6, regval);
}

uint8_t WM8978_Init(void)
{
	ESP_LOGI(WM_TAG, "init");

	uint8_t Res;
	Res=WM8978_Write_Reg(0,0);							//soft reset WM8978
	if(Res){
		ESP_LOGE(WM_TAG, "reset failed!");
		return 1;							            //reset failed, WM8978 exception
	}
	else{
		ESP_LOGI(WM_TAG, "reset succeed");
	}

	WM8978_Write_Reg(1,0X1B);							//R1,MICEN 1(MIC enabled),BIASEN 1(emu on),VMIDSEL[1:0]:11(5K)
	WM8978_Write_Reg(2,0X1B0);							//R2, ROUT1,LOUT1 output enabled(headphone),BOOSTENR,BOOSTENL enabled
	WM8978_Write_Reg(3,0X6C);							//R3, LOUT2,ROUT2 output enabled(speaker on),RMIX,LMIX enabled

	//WM8978_Write_Reg(6,0);						    //R6, MCLK from out
	WM8978_CLK_Cfg();

	WM8978_Write_Reg(43,1<<4);							//R43,INVROUT2 inverted, drive speaker
	WM8978_Write_Reg(47,1<<8);							//R47,PGABOOSTL,left MIC got 20 db
	WM8978_Write_Reg(48,1<<8);							//R48,PGABOOSTR, right MIC got 20 db
	WM8978_Write_Reg(49,1<<1);							//R49,TSDEN, open hot protecting
	WM8978_Write_Reg(10,1<<3);							//R10,SOFTMUTE closed,128x sample rate, best SNR
	WM8978_Write_Reg(14,1<<3);							//R14,ADC 128x sample rate
	
	WM8978_I2S_Cfg(2,0);								//I2S work mode
	//audio output setting
	WM8978_ADDA_Cfg(1,0);								//open DAC
	WM8978_Input_Cfg(0,0,0);							//close input channel
	WM8978_Output_Cfg(1,0);								//open DAC output
//record setting
//	WM8978_ADDA_Cfg(0,1);								//open ADC
//	WM8978_Input_Cfg(1,1,0);							//open input channel(MIC&LINE IN)
//	WM8978_Output_Cfg(0,1);								//open BYPASS output
//	WM8978_MIC_Gain(46);								//MIC db setting
	WM8978_HPvol_Set(25,25);
	WM8978_SPKvol_Set(60);
	
	return 0;
}

//WM8978 DAC/ADC config
//adcen:adc enable(1)/disable(0)
//dacen:dac enable(1)/disable(0)
void WM8978_ADDA_Cfg(uint8_t dacen,uint8_t adcen)
{
	uint16_t regval;
	regval=WM8978_Read_Reg(3);							//read R3
	if(dacen)
		regval|=3<<0;									//R3 set lowest 2 bits to 1,enable DACR&DACL
	else 
		regval&=~(3<<0);								//R3 set lowest 2 bits to 0,disable DACR&DACL.
	WM8978_Write_Reg(3,regval);
	regval=WM8978_Read_Reg(2);							//read R2
	if(adcen)
		regval|=3<<0;			        				//R2 set lowest bits to 1, enable ADCR&ADCL
	else 
		regval&=~(3<<0);				  				//R2 set lowest bits to 0, disable ADCR&ADCL.
	WM8978_Write_Reg(2,regval);							//R2
}

//WM8978 input config
//micen:MIC enable(1)/disable(0)
//lineinen:Line In enable(1)/disable(0)
//auxen:aux enable(1)/disable(0)
void WM8978_Input_Cfg(uint8_t micen,uint8_t lineinen,uint8_t auxen)
{
	uint16_t regval;  
	regval=WM8978_Read_Reg(2);
	if(micen)
		regval|=3<<2;									//open INPPGAENR,INPPGAENL(MIC的PGA放大)
	else 
		regval&=~(3<<2);								//close INPPGAENR,INPPGAENL.
 	WM8978_Write_Reg(2,regval);
	regval=WM8978_Read_Reg(44);
	if(micen)
		regval|=3<<4|3<<0;								//enable LIN2INPPGA,LIP2INPGA,RIN2INPPGA,RIP2INPGA.
	else 
		regval&=~(3<<4|3<<0);							//disable LIN2INPPGA,LIP2INPGA,RIN2INPPGA,RIP2INPGA.
	WM8978_Write_Reg(44,regval);
	if(lineinen)
		WM8978_LINEIN_Gain(5);							//LINE IN 0dB
	else 
		WM8978_LINEIN_Gain(0);							//disable LINE IN
	if(auxen)
		WM8978_AUX_Gain(7);								//AUX 6dB
	else 
		WM8978_AUX_Gain(0);								//disable AUX input
}

//WM8978 MIC db config(not include BOOST's 20dB, MIC-->ADC input db)
//gain:0~63:-12dB~35.25dB,0.75dB/Step
void WM8978_MIC_Gain(uint8_t gain)
{
	gain&=0X3F;
	WM8978_Write_Reg(45,gain);						//R45,left channel PGA config
	WM8978_Write_Reg(46,gain|1<<8);					//R46,right channel PGA config
}

//WM8978 L2/R2(Line In)db config(L2/R2-->ADC input db)
//gain:0~7,0 means channel mute,1~7,对应-12dB~6dB,3dB/Step
void WM8978_LINEIN_Gain(uint8_t gain)
{
	uint16_t regval;
	gain&=0X07;
	regval=WM8978_Read_Reg(47);
	regval&=~(7<<4);								//reset old config
 	WM8978_Write_Reg(47,regval|gain<<4);
	regval=WM8978_Read_Reg(48);
	regval&=~(7<<4);								//reset old config
 	WM8978_Write_Reg(48,regval|gain<<4);
} 

//WM8978 AUXR,AUXL(PWM audio config)db config(AUXR/L-->ADC input db)
//gain:0~7,0 channel mute,1~7,对应-12dB~6dB,3dB/Step
void WM8978_AUX_Gain(uint8_t gain)
{
	uint16_t regval;
	gain&=0X07;
	regval=WM8978_Read_Reg(47);
	regval&=~(7<<0);								//reset old config
 	WM8978_Write_Reg(47,regval|gain<<0);
	regval=WM8978_Read_Reg(48);
	regval&=~(7<<0);								//reset old config
 	WM8978_Write_Reg(48,regval|gain<<0);
}  

//WM8978 output setting
//dacen:DAC output enabled(1)/disabled(0)
//bpsen:Bypass output(record,include MIC,LINE IN,AUX等) enabled(1)/disabled(0)
void WM8978_Output_Cfg(uint8_t dacen,uint8_t bpsen)
{
	uint16_t regval=0;
	if(dacen)
		regval|=1<<0;								//DAC output enabled
	if(bpsen)
	{
		regval|=1<<1;								//BYPASS enabled
		regval|=5<<2;								//0dB
	} 
	WM8978_Write_Reg(50,regval);
	WM8978_Write_Reg(51,regval);
}

//speaker volume
void WM8978_HPvol_Set(uint8_t voll,uint8_t volr)
{
	voll&=0X3F;
	volr&=0X3F;
	if(voll==0)voll|=1<<6;							//volume is 0, mute on
	if(volr==0)volr|=1<<6;							//volume is 0, mute on
	WM8978_Write_Reg(52,voll);						//R52, left channel volume
	WM8978_Write_Reg(53,volr|(1<<8));				//R53, right channel volume
}

//speaker volume
//voll:left channel volume(0~63)
void WM8978_SPKvol_Set(uint8_t volx)
{
	volx&=0X3F;
	if(volx==0)volx|=1<<6;							//volume is 0, mute on
 	WM8978_Write_Reg(54,volx);					    //R54, left channel audio volume
	WM8978_Write_Reg(55,volx|(1<<8));				//R55, right channel audio volume
}

//I2S working mode
//fmt:0,LSB;1,MSB;2,I2S;3,PCM/DSP;
//len:0,16bist;1,20bits;2,24bits;3,32bits;
void WM8978_I2S_Cfg(uint8_t fmt,uint8_t len)
{
	fmt&=0x02;
	len&=0x03;
	WM8978_Write_Reg(4,(fmt<<3)|(len<<5));	//R4,WM8978 working mode
}
