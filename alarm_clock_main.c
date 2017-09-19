////////////////////////////////////////////////////////////////////////////////
// ECE 2534:        Lab04
//
// File name:       alarm_clock_main - A simple alarm clock
//
// Description:     Set the clock and alarm. LEDS turn on when the alarm matches
//                  with clock, to turn them off shake the accelerometer for 5 seconds.
// Resources:       timer2 is configured to interrupt every 1.0ms, and
//                  external interrupt is configured to interrupt when there is activity
//                  or inactivity.
//
// FOR GTA:         If alarm is not shaken(inactive) for 3 seconds, count will reset.
//                  It should be shaken at 1.25g.
//                  Everything specified works to my knowledge.
//                  
// How to use:      Set Clock: Press Button 1, then use Button1 and Button2 to increment
//                             or decrement the chosen time value(Starts with day of the week.
//                             Press Button3 to move through the time values and end the set up.
//                  Set Alarm: Press Button 2, rest works the same way as Set Clock.
//                  
//                  Alarm On/Off: Turn alarm on and off by pressing Button 3.
//                  
//                  When Alarm Beeps: LEDS turn on, shake the accelerometer for 5 seconds to turn it off.
//
// Written by:      Onat Calik
// Last modified:   28 April 2017
//

#define _PLIB_DISABLE_LEGACY

#include <stdio.h>
#include <plib.h>
#include <time.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "PmodOLED.h"
#include "OledChar.h"
#include "OledGrph.h"
#include "delay.h"
#include "myDebug.h"
#include "myBoardConfigFall2016.h"
#include "myAccelerometer.h"

// Use preprocessor definitions for program constants
// The use of these definitions makes your code more readable!
#define NUMBER_OF_MILLISECONDS_PER_OLED_UPDATE 1000
#define BUTTON1 (1 << 6)
#define BUTTON1BIT 6
#define BUTTON2 (1 << 7)
#define BUTTON2BIT 7
#define BUTTON3 (1 << 0)
#define LEDS_MASK (0xf<<12)
#define LED1_MASK (1<<12)
#define LED2_MASK (1<<13)
#define LED3_MASK (1<<14)
#define LED4_MASK (1<<15)


#define INT1_PORT_MASK_BIT  (1 << 7)

#define DAY_LETTER 3 //Used for the day variable's array size
#define MAX_SECOND 60
#define MAX_MINUTE 60
#define MAX_HOUR 24
#define MAX_DAY_31 31
#define MAX_DAY_30 30
#define MAX_DAY_FEB 28
#define MAX_MONTH 12
#define MAX_DAY_IN_WEEK 7
#define FEBRUARY 2

#define FIVE_SECONDS 7


// Global variable to count number of times in timer2 ISR
volatile unsigned int timer2_ms_value = 0;
const SpiChannel ch = SPI_CHANNEL3;
enum tState {DAYOFWEEK, MONTH, DATE, HOUR, MINUTE, SECOND, END}; //state machine for setTime()
int tracker = 0;     //switches to 1 if accelerometer is shaken for 5 seconds
int count = 0;       //counts the time accelerometer was shaken for


//time struct for convenience
// contains the time variables
struct time{
    int mode; //0 off 1 on   
    char day[DAY_LETTER]; //used to track day of the week as letters
    int dday; //used to track day of the week as an integer
    int month;
    int date;
    int hour;     
    int minute; 
    int second;
};



// The interrupt handler for timer2
// IPL4 medium interrupt priority
void __ISR(_TIMER_2_VECTOR, IPL4AUTO) _Timer2Handler(void) {
    timer2_ms_value++; // Increment the millisecond counter.
    INTClearFlag(INT_T2); // Acknowledge the interrupt source by clearing its flag.
}

void __ISR(_EXTERNAL_1_VECTOR, IPL7AUTO) _EXTERNAL1HANDLER(void) 
{ 
    BYTE AccINTSources = getAccelReg(ch, INT_SOURCE_REGISTER);
    int five_sec = FIVE_SECONDS;
    
    if (AccINTSources & INACT_BIT_MASK) //if inactive for 3 seconds
    {
        count = 0;
    }
    
    if (count == five_sec) //if active for five seconds
    {
        count = 0;
        tracker = 1;
        PORTGCLR = LEDS_MASK;
    }
    
    INTClearFlag(INT_INT1);
}


// Initialize timer2 and set up the interrupts
void initTimer2() {
    // Configure Timer 2 to request a real-time interrupt once per millisecond.
    // The period of Timer 2 is (16 * 625)/(10 MHz) = 1ms.
    OpenTimer2(T2_ON | T2_IDLE_CON | T2_SOURCE_INT | T2_PS_1_16 | T2_GATE_OFF, 624);
    
    // Setup Timer 2 interrupts
    INTSetVectorPriority(INT_TIMER_2_VECTOR, INT_PRIORITY_LEVEL_4);
    INTClearFlag(INT_T2);
    INTEnable(INT_T2, INT_ENABLED);
}

void initINT()
{
    // This is a multi-vector setup
    INTConfigureSystem(INT_SYSTEM_CONFIG_MULT_VECTOR);
    
    // Let the interrupts happen
    INTEnableInterrupts();
}

void initExternalInterrupt()
{
    
    // Configure the external interrupt pin as digital input  
    // This is the pin INT1 of PIC32 that is connected to INT2 of ADXL345
    TRISESET = INT1_PORT_MASK_BIT;
    
    INTSetVectorPriority(INT_EXTERNAL_1_VECTOR, EXT_INT_PRI_4);
    
    //configure interrupt as rising edge
    //mINT1SetEdgeMode(1); //this does not work for everyone
    INTCONCLR = (1 << _INTCON_INT1EP_POSITION); 
    INTCONSET = ((1) << _INTCON_INT1EP_POSITION);
    
    INTClearFlag(INT_INT1);
    INTEnable(INT_INT1, INT_ENABLED);
    
    //clear the possible pending interrupt on the ADXL345 side
    getAccelReg(ch, INT_SOURCE_REGISTER);
}

// All the hardware initializations 
void initALL(){
    
    // Initialize GPIO for LEDs
    TRISGSET = BUTTON1 | BUTTON2;
    TRISASET = BUTTON3;
    TRISGCLR = LEDS_MASK; // For LEDs: configure PortG pins for output
    ODCGCLR = LEDS_MASK; // For LEDs: configure as normal output (not open drain)
    PORTGCLR = LEDS_MASK; // Turn all LEDs off

    // Initialize Timer1 and OLED for display
    DelayInit();
    OledInit();

    // Initial Timer2 and ADC
    initTimer2();
    
    // Initial interrupt controller
    initINT();
    
    initAccMasterSPI(ch);
    initAccelerometer(ch);
    initExternalInterrupt();  
}

//Prints the clock and alarm on Oled
void printTime(struct time alarm, struct time clock);

//Increments the time by 1 second
void incrementTime(struct time *t);

//Determines the day of the week
void dayOfWeek(struct time *t);

//Sets the time for either alarm or clock
void setTime(struct time *t);

//determines which month it is to setup the days accordingly
int whichMonth(struct time *t);

//compares two time structs, returns 1 if they match
int compareTime(struct time a, struct time b);

int main() {
    
    enum {BASE, SETCURRENT, SETALARM, WAKEUP} mode = BASE;
    initALL();
    unsigned int timer2_ms_value_save;
    unsigned int last_oled_update = 0;
    unsigned int ms_since_last_oled_update;

    OledClearBuffer();
    struct time clock;
    struct time alarm;
    clock.date = 1;
    snprintf(clock.day, 3, "%s", "SU");
    clock.mode = 1;
    clock.hour = 0;
    clock.month = 1;
    clock.minute = 0;
    clock.second = 0;
    clock.dday = 0;
    alarm.date = 1;
    snprintf(alarm.day, 3, "%s", "SU");
    alarm.mode = 0;
    alarm.hour = 0;
    alarm.month = 1;
    alarm.minute = 0;
    alarm.second = 0;
    alarm.dday = 0;
    
    int alarm_mode_save = alarm.mode;
    
    DDPCONbits.JTAGEN = 0; //extra code for Button 3
    
    while (1) 
    {

        switch (mode)
        {
            case BASE:
                if (PORTG & BUTTON1)
                {
                    mode = SETCURRENT;
                }
                
                if (PORTG & BUTTON2)
                {
                    mode = SETALARM;
                }
                
                if (PORTA & BUTTON3)
                {
                    alarm.mode = !alarm.mode;
                }
                
                if ((clock.mode == alarm.mode) && compareTime(clock, alarm))
                {
                    mode = WAKEUP;
                }
                ms_since_last_oled_update = timer2_ms_value - last_oled_update;
                if (ms_since_last_oled_update >= NUMBER_OF_MILLISECONDS_PER_OLED_UPDATE) 
                {
                    timer2_ms_value_save = timer2_ms_value;
                    last_oled_update = timer2_ms_value;

                    incrementTime(&clock);
                    
                }
                
                printTime(alarm, clock);
                
               
                break;
                
            case SETCURRENT:
                
                clock.mode = 0;
                printTime(alarm, clock);
                setTime(&clock);
                dayOfWeek(&clock);
                if (clock.mode == 1)
                {
                    mode = BASE;
                }
                break;
                
            case SETALARM:
                
                ms_since_last_oled_update = timer2_ms_value - last_oled_update;
                if (ms_since_last_oled_update >= NUMBER_OF_MILLISECONDS_PER_OLED_UPDATE) 
                {
                    timer2_ms_value_save = timer2_ms_value;
                    last_oled_update = timer2_ms_value;

                    incrementTime(&clock);
                    
                }
                alarm_mode_save = alarm.mode;
                printTime(alarm, clock);
                setTime(&alarm);
                dayOfWeek(&alarm); 
                
                if (alarm_mode_save != alarm.mode)
                {
                    alarm.mode = !alarm.mode;
                    mode = BASE;
                }
                break;
                
            case WAKEUP:
                
                PORTGSET = LEDS_MASK;
                ms_since_last_oled_update = timer2_ms_value - last_oled_update;
                if (ms_since_last_oled_update >= NUMBER_OF_MILLISECONDS_PER_OLED_UPDATE) 
                {
                    timer2_ms_value_save = timer2_ms_value;
                    last_oled_update = timer2_ms_value;

                    incrementTime(&clock);
                    count++;
                    
                }
                printTime(alarm, clock);
                
                if (tracker)
                {
                    tracker = 0;
                    mode = BASE;
                }
                break;
                
        }
        
        
    }         
    return (EXIT_SUCCESS);
}

void printTime(struct time alarm, struct time clock)
{
    char oledstring[100];

    OledSetCursor(0,0);
    OledPutString("ALARM ");
    OledSetCursor(6,0);
    snprintf(oledstring, 100, "%s %02d/%02d", alarm.day, alarm.month, alarm.date);
    OledPutString(oledstring);
    OledSetCursor(0,1);
    if (alarm.mode == 0)
    {
        OledPutString("OFF ");
    }
    else if (alarm.mode == 1)
    {
        OledPutString("ON ");
    }
    OledSetCursor(4,1);
    snprintf(oledstring, 100, "%02d:%02d:%02d", alarm.hour, alarm.minute, alarm.second);
    OledPutString(oledstring);
    
    OledSetCursor(0,2);
    OledPutString("CLOCK ");
    OledSetCursor(6,2);
    snprintf(oledstring, 100, "%s %02d/%02d", clock.day, clock.month, clock.date);
    OledPutString(oledstring);
    OledSetCursor(0,3);
    if (clock.mode == 0)
    {
        OledPutString("OFF ");
    }
    else if (clock.mode == 1)
    {
        OledPutString("ON ");
    }
    OledSetCursor(4,3);
    snprintf(oledstring, 100, "%02d:%02d:%02d", clock.hour, clock.minute, clock.second);
    OledPutString(oledstring);
    
    OledUpdate();
    
      
}

void incrementTime(struct time *t)
{
    int max_month = whichMonth(t);
    t->second++;
    if(t->second == MAX_SECOND)
    {
        t->minute++;
        t->second = 0;
        
        if(t->minute == MAX_MINUTE)
        {
            t->hour++;
            t->minute = 0;
            if (t->hour == MAX_HOUR)
            {
                t->dday++;
                t->date++;
                t->hour = 0;
                if (t->dday == MAX_DAY_IN_WEEK)
                {
                    t->dday = 0;
                }
                if (t->date == max_month)
                {
                    t->month++;
                    t->date = 1;
                    
                    if (t->month == MAX_MONTH)
                    {
                        t->month == 1;   
                    }
                }
                
            }
        }
    }
    dayOfWeek(t);
    
}

void dayOfWeek(struct time *t)
{
    if (t->dday == 0)
    {
        snprintf(t->day, 3, "%s", "SU");
    }
    
    if (t->dday == 1)
    {
        snprintf(t->day, 3, "%s", "MO");
    }
    
    if (t->dday == 2)
    {
        snprintf(t->day, 3, "%s", "TU");
    }
    
    if (t->dday == 3)
    {
        snprintf(t->day, 3, "%s", "WE");
    }
    
    if (t->dday == 4)
    {
        snprintf(t->day, 3, "%s", "TH");
    }
    
    if (t->dday == 5)
    {
        snprintf(t->day, 3, "%s", "FR");
    }
    
    if (t->dday == 6)
    {
        snprintf(t->day, 3, "%s", "SA");
    }
}

void setTime(struct time *t)
{
    static enum tState state = DAYOFWEEK;
    static int max_month = 0;
    
    switch (state)
    {
        case DAYOFWEEK:

            if (PORTG & BUTTON1)
            {
                t->dday++;
                if (t->dday >= MAX_DAY_IN_WEEK)
                {
                    t->dday = 0;
                }
            }
            if (PORTG & BUTTON2)
            {
                t->dday--;
                if (t->dday < 0)
                {
                    t->dday = MAX_DAY_IN_WEEK - 1;
                }
            }

            if(PORTA & BUTTON3)
            {
                state = MONTH;
            }
            dayOfWeek(t);
            break;
        case MONTH:
            if (PORTG & BUTTON1)
            {
                t->month++;
                if (t->month > MAX_MONTH)
                {
                    t->month = 1;
                }
            }
            if (PORTG & BUTTON2)
            {
                t->month--;
                if (t->month <= 0)
                {
                    t->month = MAX_MONTH;
                }

            }

            if(PORTA & BUTTON3)
            {
                state = DATE;
            }
            max_month = whichMonth(t);
            break;
            
        case DATE:
            if (PORTG & BUTTON1)
            {
                t->date++;
                if (t->date > max_month)
                {
                    t->date = 1;
                }
            }
            if (PORTG & BUTTON2)
            {
                t->date--;
                if (t->date <= 0)
                {
                    t->date = max_month;
                }

            }

            if(PORTA & BUTTON3)
            {
                state = HOUR;
            }
            break;
            
        case HOUR:
            if (PORTG & BUTTON1)
            {
                t->hour++;
                if (t->hour >= MAX_HOUR)
                {
                    t->hour = 0;
                }
            }
            if (PORTG & BUTTON2)
            {
                t->hour--;
                if (t->hour < 0)
                {
                    t->hour = MAX_HOUR - 1;
                }

            }

            if(PORTA & BUTTON3)
            {
                state = MINUTE;
            }
            break;
            
        case MINUTE:
            if (PORTG & BUTTON1)
            {
                t->minute++;
                if (t->minute >= MAX_MINUTE)
                {
                    t->minute = 0;
                }
            }
            if (PORTG & BUTTON2)
            {
                t->minute--;
                if (t->minute < 0)
                {
                    t->minute = MAX_MINUTE - 1;
                }

            }

            if(PORTA & BUTTON3)
            {
                state = SECOND;

            }
            break;
            
        case SECOND:
            if (PORTG & BUTTON1)
            {
                t->second++;
                if (t->second >= MAX_SECOND)
                {
                    t->second = 0;
                }
            }
            if (PORTG & BUTTON2)
            {
                t->second--;
                if (t->second < 0)
                {
                    t->second = MAX_SECOND - 1;
                }

            }

            if(PORTA & BUTTON3)
            {
                state = END;    
            }
            break;
            
        case END:
            t->mode = !(t->mode);
            state = DAYOFWEEK;
            break;
            
    }
}


int whichMonth(struct time *t)
{
    int max_month = 0;
    int month_31[7] = {1, 3, 5, 7, 8, 10, 12}; //months with 31 days
    
    if (t->month == FEBRUARY)
    {
        max_month = MAX_DAY_FEB;
    }
    
    else
    {
        max_month = MAX_DAY_30;
    }
    int i =0;
    for (i; i < 7; i++)
    {
        if (month_31[i] == t->month)
        {
            max_month = MAX_DAY_31;  
        }
    }
    
    return max_month;
}


int compareTime(struct time a, struct time b)
{
    if ((a.month == b.month) && (a.date == b.date) && (a.dday == b.dday) && (a.hour == b.hour) && (a.minute == b.minute) && (a.second == b.second))
    {
        return 1;
    }
    return 0;
}