#include <plib.h>
#include "myAccelerometer.h"

#define ACC_MASTER_SPI_CLK_DIVIDER 10

#define ID_REGISTER 0x00
#define DATA_FORMAT_REG 0x31

//4 mg per LSB
#define FULL_RES_FLAG (1<<3) 

// the number read out of XYZ bytes should be multiplied 
// by this number to get the "g" data
// if full resolution is not used, this number changes
#define RESOLUTION 4 

#define DECI 0.1

// +/- 8g
#define RANGE_8g 2 

#define POWER_CNTL_REG 0X2D
#define MEASURE_ON (1<<3)

#define OUTPUT_RATE_REG 0X2C
#define OUTPUT_RATE_3_13 0x6
   
#define READ_COMMAND 0X80
#define WRITE_COMMAND 0X0
#define SINGLE_BYTE 0X0
#define MULTI_BYTE 0X40
#define DATA_START_REG 0x32

#define DATA_REG_COUNT 6
#define BYTE_BITS 8

#define INT_ENABLE_REGISTER 0x2E 
#define INT_MAP_REGISTER 0x2F


#define ACT_INACT_CTL_REGISTER 0x27
#define ACT_INACT_CTL 0x77

#define ACT_THRESHOLD_REGISTER 0x24
#define ACT_THRESHOLD 0x20

#define INACT_THRESHOLD_REGISTER 0x25
#define INACT_THRESHOLD 20

//Inact time 1sec/LSB
#define INACT_TIME_REGISTER 0x26
#define INACT_TIME 3



void setAccelReg(SpiChannel chn, unsigned int address, unsigned int data) 
{
    SpiChnPutC(chn, WRITE_COMMAND | address);
    SpiChnPutC(chn, data);
    SpiChnGetC(chn); //get rid of garbage
    SpiChnGetC(chn);


}

unsigned char getAccelReg(SpiChannel chn, unsigned int address)
{
    unsigned char temp;
    SpiChnPutC(chn, (address | READ_COMMAND));
    SpiChnPutC(chn, 0);
    SpiChnGetC(chn);
    temp = SpiChnGetC(chn);
    return temp;

}

void getAccelData(SpiChannel chn, short *xyzArray) 
{
    // create the command to read multiple bytes from Acc

    
    unsigned char dataArray[6];
    // collect 2 bytes for each of x, y and z axis and put them in dataArray
    SpiChnPutC(chn, READ_COMMAND | MULTI_BYTE | 0x32);
    
    int i;
    for (i =0; i < 6; i++)
    {
        SpiChnPutC(chn, 0);
    }
    
    SpiChnGetC(chn);
    int j;
    for (j=0; j < 6; j++)
    {
        dataArray[j] = SpiChnGetC(chn);
    }
    
    // combine high and low bytes to create a short for each of x, y and z
    
    xyzArray[0] = (dataArray[1] << 8 | dataArray[0]);
    xyzArray[1] = (dataArray[3] << 8 | dataArray[2]);
    xyzArray[2] = (dataArray[5] << 8 | dataArray[4]);
    
    

    
 }

void initAccelerometer(SpiChannel chn)
{
    setAccelReg(chn, POWER_CNTL_REG, MEASURE_ON);
    setAccelReg(chn, DATA_FORMAT_REG, FULL_RES_FLAG | RANGE_8g);
    setAccelReg(chn, OUTPUT_RATE_REG, OUTPUT_RATE_3_13);
    
    
    setAccelReg(chn, ACT_INACT_CTL_REGISTER, ACT_INACT_CTL);
    setAccelReg(chn, ACT_THRESHOLD_REGISTER, ACT_THRESHOLD);
    setAccelReg(chn, INACT_THRESHOLD_REGISTER, INACT_THRESHOLD);
    setAccelReg(chn, INACT_TIME_REGISTER, INACT_TIME);
    
    BYTE EnabledInterrupts = ACT_BIT_MASK | INACT_BIT_MASK;
    
    setAccelReg(chn, INT_MAP_REGISTER, EnabledInterrupts);
    setAccelReg(chn, INT_ENABLE_REGISTER, EnabledInterrupts);
       
    
}

void initAccMasterSPI(SpiChannel chn)
{
    SpiChnOpen(chn, SPI_OPEN_MSTEN 
                    | SPI_OPEN_MSSEN 
                    | SPI_OPEN_CKP_HIGH 
                    | SPI_OPEN_ENHBUF 
                    | SPI_OPEN_MODE8, ACC_MASTER_SPI_CLK_DIVIDER);
}

