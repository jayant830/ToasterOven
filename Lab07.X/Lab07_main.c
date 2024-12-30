// **** Include libraries here ****
// Standard libraries
#include <stdio.h>
#include <string.h>
//CSE13E Support Library
#include "BOARD.h"

//Lab specific libraries
#include "Leds.h"
#include "Adc.h"
#include "Ascii.h"
#include "Buttons.h"
#include "Oled.h"
#include "OledDriver.h"

// Microchip libraries
#include <xc.h>
#include <sys/attribs.h>



// **** Set any macros or preprocessor directives here ****
// Set a macro for resetting the timer, makes the code a little clearer.
#define TIMER_2HZ_RESET() (TMR1 = 0)


// **** Set any local typedefs here ****
typedef enum {
    SETUP, SELECTOR_CHANGE_PENDING, COOKING, RESET_PENDING, EXTRA_CREDIT
} OvenState;

typedef struct {
    OvenState state;
    //add more members to this struct
    uint16_t initialCookTime; 
    uint16_t timeRemaining; 
    uint16_t temperature; 
    uint16_t buttonPressTime; 
    uint8_t mode; 
} OvenData;


typedef enum {
    BAKE, TOAST, BROIL
} CookingStates;

static OvenData ovenData;
static uint16_t TIMER_TICK = 0;
static uint16_t extraTime = 0;
static uint8_t adcChange = 0;
static uint16_t storedextraTime = 0;
static uint8_t buttonEvent;
static uint16_t timeTickCounter = 0;
//static uint16_t adcValue;
static uint8_t editTemp = 0;
static uint16_t LEDSInterval;
static uint16_t remainder;
static char currentLEDS;
static uint8_t inverted = 0;
static uint16_t temp;
// **** Define any module-level, global, or external variables here ****
// constants defined below
#define SECOND 5
#define DEFAULT_TEMP 350
#define BROIL_TEMP 500
#define ALL_LEDS_ON 0xFF
#define CLEAR_LEDS 0x00
// **** Put any helper functions here ****
void handleSetupState(void);
void handleSelectorChangePendingState(void);
void handleCookingState(void);
void handleResetPendingState(void);
void handleExtraCreditState(void);
void updateAdcValue(void);
void handleButton3Down(void);
void handleButton4Down(void);
void handleButton3Up(void);
void toggleEditingTemp(void);
void updateModeAndTemperature(void);
void handleTimerTick(void);
void updateLEDS(void);
void handleCookingTimeEnd(void);
void decreaseCookingTime(void);
void resetOven(void);
void handleInvertedToggle(void);
int readAndProcessAdcValue(void);
void updateOvenData(int adcValue);

/*This function will update your OLED to reflect the state .*/
/*This function will update your OLED to reflect the state .*/
void updateOvenOLED(OvenData ovenData) {
    //update OLED here
    char toPrint[60] = "";
    char topOn[6], topOff[6], botOn[6], botOff[6];

    sprintf(topOn, "%s%s%s%s%s", OVEN_TOP_ON, OVEN_TOP_ON, OVEN_TOP_ON, OVEN_TOP_ON, OVEN_TOP_ON);
    sprintf(topOff, "%s%s%s%s%s", OVEN_TOP_OFF, OVEN_TOP_OFF, OVEN_TOP_OFF, OVEN_TOP_OFF, OVEN_TOP_OFF);
    sprintf(botOn, "%s%s%s%s%s", OVEN_BOTTOM_ON, OVEN_BOTTOM_ON, OVEN_BOTTOM_ON, OVEN_BOTTOM_ON, OVEN_BOTTOM_ON);
    sprintf(botOff, "%s%s%s%s%s", OVEN_BOTTOM_OFF, OVEN_BOTTOM_OFF, OVEN_BOTTOM_OFF, OVEN_BOTTOM_OFF, OVEN_BOTTOM_OFF);

    int timeMinutes, timeSeconds;
    if (ovenData.state == COOKING || ovenData.state == RESET_PENDING) {
        timeMinutes = ovenData.timeRemaining / 60;
        timeSeconds = ovenData.timeRemaining % 60;
    } else {
        timeMinutes = ovenData.initialCookTime / 60;
        timeSeconds = ovenData.initialCookTime % 60;
    }

    #define MAX_BUFFER_SIZE 100 

    switch (ovenData.mode) {
        case BAKE:
            snprintf(toPrint, MAX_BUFFER_SIZE, "|%s| Mode: Bake\n|     |  Time: %d:%02d\n|-----|  Temp: %d%sF\n|%s|",
                     topOff, timeMinutes, timeSeconds, ovenData.temperature, DEGREE_SYMBOL, botOff);
            break;
        case TOAST:
            snprintf(toPrint, MAX_BUFFER_SIZE, "|%s| Mode: Toast\n|     |  Time: %d:%02d\n|-----|\n|%s|",
                     topOff, timeMinutes, timeSeconds, (ovenData.state == COOKING || ovenData.state == RESET_PENDING) ? botOn : botOff);
            break;
        case BROIL:
            snprintf(toPrint, MAX_BUFFER_SIZE, "|%s| Mode: Broil\n|     |  Time: %d:%02d\n|-----|  Temp: 350%sF\n|%s|",
                     (ovenData.state == COOKING || ovenData.state == RESET_PENDING) ? topOn : topOff, timeMinutes, timeSeconds, DEGREE_SYMBOL, botOff);
            break;
    }

    OledClear(OLED_COLOR_BLACK);
    OledDrawString(toPrint);

    if (ovenData.state == EXTRA_CREDIT) {
        if (inverted) {
            OledSetDisplayNormal();
        } else {
            OledSetDisplayInverted();
        }
    }

    OledUpdate();

    OledUpdate();
}

void runOvenSM(void) {
    switch (ovenData.state) {
        case SETUP:
            handleSetupState();
            break;
        case SELECTOR_CHANGE_PENDING:
            handleSelectorChangePendingState();
            break;
        case COOKING:
            handleCookingState();
            break;
        case RESET_PENDING:
            handleResetPendingState();
            break;
        case EXTRA_CREDIT:
            handleExtraCreditState();
            break;
    }
}

void handleSetupState() {
    if (adcChange) {
        updateAdcValue();
    }
    if (buttonEvent & BUTTON_EVENT_3DOWN) {
        handleButton3Down();
    }
    if (buttonEvent & BUTTON_EVENT_4DOWN) {
        handleButton4Down();
    }
}

void updateAdcValue() {
    int adcValue = readAndProcessAdcValue();
    updateOvenData(adcValue);
    updateOvenOLED(ovenData);
}

int readAndProcessAdcValue() {
    int adcValue = AdcRead();
    return (adcValue & 0x03FC) >> 2;
}

void updateOvenData(int adcValue) {
    if (ovenData.mode == BAKE && editTemp) {
        ovenData.temperature = adcValue + 300;
    } else {
        ovenData.initialCookTime = adcValue + 1;
        ovenData.timeRemaining = ovenData.initialCookTime;
    }
}

void handleButton3Down() {
    ovenData.buttonPressTime = extraTime;
    ovenData.state = SELECTOR_CHANGE_PENDING;
}

void handleButton4Down() {
    switch (ovenData.state) {
        case SETUP:
            storedextraTime = extraTime;
            ovenData.state = COOKING;
            updateOvenOLED(ovenData);
            LEDS_SET(ALL_LEDS_ON);
            LEDSInterval = (ovenData.initialCookTime * 5) / 8;
            remainder = (ovenData.initialCookTime * 5) % 8;
            timeTickCounter = 0;
            break;
                default:
            break;
    }
}


void handleSelectorChangePendingState() {
    if (buttonEvent & BUTTON_EVENT_3UP) {
        handleButton3Up();
    }
}

void handleButton3Up() {
    uint16_t elapsedTime = extraTime - ovenData.buttonPressTime;
    if (elapsedTime >= SECOND) {
        toggleEditingTemp();
    } else {
        updateModeAndTemperature();
    }
    updateOvenOLED(ovenData);
    ovenData.state = SETUP;
}

void toggleEditingTemp() {
    if (ovenData.mode == BAKE) {
        editTemp = !editTemp;
    }
}

void updateModeAndTemperature() {
    if (ovenData.mode != BROIL) {
        ovenData.mode++;
    } else {
        ovenData.mode = BAKE;
    }

    switch (ovenData.mode) {
        case BROIL:
            temp = ovenData.temperature;
            ovenData.temperature = 500;
            break;
        case BAKE:
            ovenData.temperature = temp;
            break;
    }
}


void handleCookingState() {
    if (TIMER_TICK) {
        handleTimerTick();
    }
    if (buttonEvent & BUTTON_EVENT_4DOWN) {
        ovenData.state = RESET_PENDING;
        ovenData.buttonPressTime = extraTime;
    }
}

void handleTimerTick() {
    timeTickCounter++;
    if ((remainder > 0 && timeTickCounter == LEDSInterval + 1) || 
            (remainder == 0 && timeTickCounter == LEDSInterval)) {
        updateLEDS();
    }
    if (ovenData.timeRemaining == 0) {
        handleCookingTimeEnd();
    }
    if ((extraTime - storedextraTime) % 5 == 0) {
        decreaseCookingTime();
    }
}

void updateLEDS() {
    currentLEDS = LEDS_GET();
    timeTickCounter = 0;
    remainder--;
    LEDS_SET(currentLEDS << 1);
}

void handleCookingTimeEnd() {
    ovenData.timeRemaining = ovenData.initialCookTime;
    ovenData.state = EXTRA_CREDIT;
    updateOvenOLED(ovenData);
    LEDS_SET(CLEAR_LEDS);
}

void decreaseCookingTime() {
    ovenData.timeRemaining--;
    updateOvenOLED(ovenData);
}

void handleResetPendingState() {
    if (TIMER_TICK) {
        handleTimerTick();
    }
    if (extraTime - ovenData.buttonPressTime >= SECOND) {
        resetOven();
    }
    if (buttonEvent & BUTTON_EVENT_4UP && (extraTime - ovenData.buttonPressTime < SECOND)) {
        ovenData.state = COOKING;
    }
}

void resetOven() {
    ovenData.timeRemaining = ovenData.initialCookTime;
    ovenData.state = SETUP;
    updateOvenOLED(ovenData);
    LEDS_SET(CLEAR_LEDS);
}

void handleExtraCreditState() {
    if (TIMER_TICK) {
        handleInvertedToggle();
    }
    if (buttonEvent & BUTTON_EVENT_4UP) {
        inverted = 1;
        updateOvenOLED(ovenData);
        ovenData.state = SETUP;
        updateOvenOLED(ovenData);
    }
}

void handleInvertedToggle() {
    inverted = !inverted;
    updateOvenOLED(ovenData);
}



int main()
{
    BOARD_Init();

    //initalize timers and timer ISRs:
    // <editor-fold defaultstate="collapsed" desc="TIMER SETUP">
    
    // Configure Timer 2 using PBCLK as input. We configure it using a 1:16 prescalar, so each timer
    // tick is actually at F_PB / 16 Hz, so setting PR2 to F_PB / 16 / 100 yields a .01s timer.

       //initalize timers and timer ISRs:
    // <editor-fold defaultstate="collapsed" desc="TIMER SETUP">
    
    // Configure Timer 2 using PBCLK as input. We configure it using a 1:16 prescalar, so each timer
    // tick is actually at F_PB / 16 Hz, so setting PR2 to F_PB / 16 / 100 yields a .01s timer.

    T2CON = 0; // everything should be off
    T2CONbits.TCKPS = 0b100; // 1:16 prescaler
    PR2 = BOARD_GetPBClock() / 16 / 100; // interrupt at .5s intervals
    T2CONbits.ON = 1; // turn the timer on

    // Set up the timer interrupt with a priority of 4.
    IFS0bits.T2IF = 0; //clear the interrupt flag before configuring
    IPC2bits.T2IP = 4; // priority of  4
    IPC2bits.T2IS = 0; // subpriority of 0 arbitrarily 
    IEC0bits.T2IE = 1; // turn the interrupt on

    // Configure Timer 3 using PBCLK as input. We configure it using a 1:256 prescaler, so each timer
    // tick is actually at F_PB / 256 Hz, so setting PR3 to F_PB / 256 / 5 yields a .2s timer.

    T3CON = 0; // everything should be off
    T3CONbits.TCKPS = 0b111; // 1:256 prescaler
    PR3 = BOARD_GetPBClock() / 256 / 5; // interrupt at .5s intervals
    T3CONbits.ON = 1; // turn the timer on

    // Set up the timer interrupt with a priority of 4.
    IFS0bits.T3IF = 0; //clear the interrupt flag before configuring
    IPC3bits.T3IP = 4; // priority of  4
    IPC3bits.T3IS = 0; // subpriority of 0 arbitrarily 
    IEC0bits.T3IE = 1; // turn the interrupt on;

    
    // <editor-fold defaultstate="collapsed" desc="TIMER SETUP">
    
    // Configure Timer 2 using PBCLK as input. We configure it using a 1:16 prescalar, so each timer
    // tick is actually at F_PB / 16 Hz, so setting PR2 to F_PB / 16 / 100 yields a .01s timer.

    T2CON = 0; // everything should be off
    T2CONbits.TCKPS = 0b100; // 1:16 prescaler
    PR2 = BOARD_GetPBClock() / 16 / 100; // interrupt at .5s intervals
    T2CONbits.ON = 1; // turn the timer on

    // Set up the timer interrupt with a priority of 4.
    IFS0bits.T2IF = 0; //clear the interrupt flag before configuring
    IPC2bits.T2IP = 4; // priority of  4
    IPC2bits.T2IS = 0; // subpriority of 0 arbitrarily 
    IEC0bits.T2IE = 1; // turn the interrupt on

    // Configure Timer 3 using PBCLK as input. We configure it using a 1:256 prescaler, so each timer
    // tick is actually at F_PB / 256 Hz, so setting PR3 to F_PB / 256 / 5 yields a .2s timer.

    T3CON = 0; // everything should be off
    T3CONbits.TCKPS = 0b111; // 1:256 prescaler
    PR3 = BOARD_GetPBClock() / 256 / 5; // interrupt at .5s intervals
    T3CONbits.ON = 1; // turn the timer on

    // Set up the timer interrupt with a priority of 4.
    IFS0bits.T3IF = 0; //clear the interrupt flag before configuring
    IPC3bits.T3IP = 4; // priority of  4
    IPC3bits.T3IS = 0; // subpriority of 0 arbitrarily 
    IEC0bits.T3IE = 1; // turn the interrupt on;

    
    // <editor-fold defaultstate="collapsed" desc="TIMER SETUP">
    
    // Configure Timer 2 using PBCLK as input. We configure it using a 1:16 prescalar, so each timer
    // tick is actually at F_PB / 16 Hz, so setting PR2 to F_PB / 16 / 100 yields a .01s timer.

    T2CON = 0; // everything should be off
    T2CONbits.TCKPS = 0b100; // 1:16 prescaler
    PR2 = BOARD_GetPBClock() / 16 / 100; // interrupt at .5s intervals
    T2CONbits.ON = 1; // turn the timer on

    // Set up the timer interrupt with a priority of 4.
    IFS0bits.T2IF = 0; //clear the interrupt flag before configuring
    IPC2bits.T2IP = 4; // priority of  4
    IPC2bits.T2IS = 0; // subpriority of 0 arbitrarily 
    IEC0bits.T2IE = 1; // turn the interrupt on

    // Configure Timer 3 using PBCLK as input. We configure it using a 1:256 prescaler, so each timer
    // tick is actually at F_PB / 256 Hz, so setting PR3 to F_PB / 256 / 5 yields a .2s timer.

    T3CON = 0; // everything should be off
    T3CONbits.TCKPS = 0b111; // 1:256 prescaler
    PR3 = BOARD_GetPBClock() / 256 / 5; // interrupt at .5s intervals
    T3CONbits.ON = 1; // turn the timer on

    // Set up the timer interrupt with a priority of 4.
    IFS0bits.T3IF = 0; //clear the interrupt flag before configuring
    IPC3bits.T3IP = 4; // priority of  4
    IPC3bits.T3IS = 0; // subpriority of 0 arbitrarily 
    IEC0bits.T3IE = 1; // turn the interrupt on;

    // </editor-fold>
   
    printf("Welcome to jdharwad's Lab07 (Toaster Oven).  Compiled on %s %s.", __TIME__, __DATE__);

    //initialize state machine (and anything else you need to init) here
    
    OledInit();
    ButtonsInit();
    AdcInit();
    LEDS_INIT();
    
    updateOvenOLED(ovenData);
    while (1){
        // Add main loop code here:
        // check for events
        // on event, run runOvenSM()
        // clear event flags
        if (buttonEvent != BUTTON_EVENT_NONE || adcChange || TIMER_TICK) {
            runOvenSM();
            buttonEvent = 0;
            adcChange = 0;
            TIMER_TICK = 0;
        }
    };
}



//The 5hz timer is used to update the free-running timer and to generate TIMER_TICK events
void __ISR(_TIMER_3_VECTOR, ipl4auto) Timer3ISR(void)
{
    // Clear the interrupt flag.
    IFS0CLR = 1 << 12;

    // Add event-checking code here
    TIMER_TICK = 1;
    extraTime++;
}

void __ISR(_TIMER_2_VECTOR, ipl4auto) Timer2ISR(void)
{
    // Clear the interrupt flag.
    IFS0CLR = 1 << 8;

    // Add event-checking code here
    adcChange = AdcChanged();
    buttonEvent = ButtonsCheckEvents();
}