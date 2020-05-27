#ifndef _H_WATCHDOGTIMER_H
#define _H_WATCHDOGTIMER_H
 
namespace WatchDogTimer {
    
    void init(float seconds = 6.0) {
        NRF_WDT->CONFIG = (WDT_CONFIG_SLEEP_Run << WDT_CONFIG_SLEEP_Pos);
        NRF_WDT->CRV = seconds * 32768; // 32k tick
        NRF_WDT->RREN = WDT_RREN_RR0_Enabled << WDT_RREN_RR0_Pos;
        NRF_WDT->TASKS_START = 1;
    }
    
    void kick() {
        NRF_WDT->RR[0] = WDT_RR_RR_Reload;
    }
};
 
#endif//_H_WATCHDOGTIMER_H