// =============================================================================
// Component Name: ATMega168 "DUMMY" peripheral.
//
// This is still preliminary code for the ATMega168 "DUMMY" peripheral, to 
// show how to implement special registers functionality. This could be valid
// also for Mega48 and Mega88, with minor modifications.
//
// Copyright (C) 2009 Advanced MicroControllers Tools (http://www.amctools.com/)
// Copyright (C) 2009-2010 Wojciech Stryjewski <thvortex@gmail.com>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation; either version 2.1 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//
#include <windows.h>
#include <commctrl.h>
#pragma hdrstop
#include <stdio.h>

#define IS_DUMMY_PERIPHERAL           // To distinguish from a normal user component
#include "blackbox.h"
#include "useravr.h"

int WINAPI DllEntryPoint(HINSTANCE, unsigned long, void*) {return 1;} // is DLL
// =============================================================================

// Maximum value allowed in CLKPR register CLKPS field
#define MAX_PRESCALER_INDEX 8

DECLARE_PINS
   // No pins allowed in "dummy" peripheral. Leave it empty
END_PINS

DECLARE_INTERRUPTS  // Keep same order as in .INI "Interrupt_map"
   INT0, INT1, IOCH0, IOCH1, IOCH2, SPMR
END_INTERRUPTS              

// Involved registers. Use same order as in .INI file "Special_register_map" FIX, FIX, FIX !!!
// These are IDs or indexes. The real data is stored in  a hidden WORD8 
// type array. (see "blackbox.h" for details). The WORD8 data is referred
// with the REG(xxx) macro. 
//
DECLARE_REGISTERS  // Always before DECLARE_VAR
   PCMSK0, PCMSK1, PCMSK2, EICRA, CLKPR, PRR, SMCR, GTCCR, SPMCSR,
   OSCCAL, GPIOR0, GPIOR1, GPIOR2, MCUCR, MCUSR
END_REGISTERS

DECLARE_VAR
   int Prescaler; // Current prescaler index (in case CLKPS bits have UNKNOWN value)
END_VAR

bool Started;          // True if simulation started and interface functions work

// Action codes to be used with REMIND_ME2() -> On_remind_me(),
enum {AUTOCLEAR_SELFPRGEN, AUTOCLEAR_CLKPCE, AUTOCLEAR_IVCE};

const double Clock_presc_table[] = {1, 2, 4, 8, 16, 32, 64, 128, 256}; // For CLKPR

// Component names passed to NOTIFY() for use with Power Reduction Register
// (PRR). Must be in the same order as the bits in the register. Not implemented
// for internal VMLAB coded peripherals Names must be as defined in the .INI
// file 'Peripheral_list' The arbitray integer nr. must be recognized at
// On_notify( ) at the corresponding peripheral
const char *PRR_Names[] = {
   "ADC", "UART", "SPI", "TIMER1", NULL, "TIMER0", "TIMER2", "TWI"
};

USE_WINDOW(WINDOW_USER_1); // Window to display registers, etc. See .RC file

REGISTERS_VIEW
//          ID     res ID      b7           .     ..    bit names ...                          b0
//
   DISPLAY(PCMSK0, GADGET1,  PCINT7,   PCINT6,  PCINT5,  PCINT4,  PCINT3,  PCINT2,  PCINT1,  PCINT0)
   DISPLAY(PCMSK1, GADGET2,  *, PCINT14, PCINT13, PCINT12, PCINT11, PCINT10,  PCINT9,  PCINT8)
   DISPLAY(PCMSK2, GADGET3,  PCINT23, PCINT22, PCINT21, PCINT20, PCINT19, PCINT18, PCINT17, PCINT16)
   DISPLAY(EICRA,  GADGET4,  *, *, *, *, ISC11, ISC10, ISC01, ISC00)
   DISPLAY(CLKPR,  GADGET5,  CLKPCE, *, *, *, CLKPS3, CLKPS2, CLKPS1, CLKPS0)
   DISPLAY(PRR,    GADGET6,  PRTWI, PRTIM2, PRTIM0, *, PRTIM1, PRSPI, PRUSART0, PRADC)
   DISPLAY(SMCR,   GADGET7,  *, *, *, *, SM2, SM1, SM0, SE)
   DISPLAY(GTCCR,  GADGET8,  TSM, *, *, *, *, *, PSRASY, PSRSYNC)
   DISPLAY(SPMCSR, GADGET9,  SPMIE, RWWSB, *, RWWSRE, BLBSET, PGWRT, PGERS, SELFPRGEN)
   DISPLAY(OSCCAL, GADGET10, CAL7, CAL6, CAL5, CAL4, CAL3, CAL2, CAL1, CAL0)
   DISPLAY(MCUCR,  GADGET11, *, *, *, PUD, *, *, IVSEL, IVCE)
   DISPLAY(MCUSR,  GADGET12, *, *, *, *, WDRF, BORF, EXTRF, PORF)
   DISPLAY(GPIOR0, GADGET16, *, *, *, *, *, *, *, *);
   DISPLAY(GPIOR1, GADGET17, *, *, *, *, *, *, *, *);
   DISPLAY(GPIOR2, GADGET18, *, *, *, *, *, *, *, *);

   // If a register is not included here (no DISPLAY() nor HIDDEN()), it will be 
   // considered as plain RAM, and no On_xxx (read/write) will be taken into
   // accout for it. A warning will be given.

END_VIEW

const char *On_create()            //
{
   return NULL;
}

void On_destroy()                  //  Leave them like this, if no action required
{
}

void Change_clock(int pPrescaler)
//***********************
// Helper function to adjust the clock speed from the current prescaler index
// stored in VAR(Prescaler) to the new prescaler index in "pPrescaler". This
// function "undoes" the effects of the previous presclaer (by multiplying the
// current clock by the prescaler value) and then applies the new prescaler
// by dividing the clock by it.
{
   double clock = GET_CLOCK();
   clock *= Clock_presc_table[VAR(Prescaler)];
   clock /= Clock_presc_table[pPrescaler];

   if(!SET_CLOCK(clock)) {
      char strBuffer[128];
      sprintf(strBuffer, "Requested clock speed out of range: %.1fMhz",
         clock * 1.0e-6);
      WARNING(strBuffer, CAT_CPU, WARN_MISC);
   } else {
      VAR(Prescaler) = pPrescaler;
   }
}

void On_simulation_begin()
//***********************
// Handle fuse clock options. Could override the .CLOCK directive
// This part of the code was previously handled at On_reset()
{
   int resetDelay = 8; // 8 cycles, just an example. There could be an option

   double myClock = GET_CLOCK();  // The one coming in Project File .CLOCK directive

   // Reset delay selection fuse. This is just an initial example
   // The real approach is a bit more sophisticated, depending also of CKSEL
   //
   switch(GET_FUSE("SUT")) {
      case 0x0: resetDelay = 1000;   break;   // Just a test example,
      case 0x1: resetDelay = 4000;   break;   // not really like this....
      case 0x2: resetDelay = 8236;   break;
      case 0x3: resetDelay = 16534;  break;
   }
   char myBuffer[64];
   ::sprintf(myBuffer, "Default clock = %5.1f MHz", myClock * 1.0e-6);
   PRINT(myBuffer);
   ::sprintf(myBuffer, "Selected reset delay = %d clock cycles", resetDelay);
   PRINT(myBuffer);

   // Clock selection fuse
   //
   switch(GET_FUSE("CKSEL")) {

      case 0x2: // Calibrated Internal RC Oscillator
         PRINT("Selected 8MHz calibrated internal RC oscillator (CKSEL fuses = 0010)");
         myClock = 8.0e6;
         break;

      case 0x3: // Internal 128 kHz
         PRINT("Selected 128KHz internal RC Oscillator (CKSEL fuses = 0011)");
         myClock = 128.0e3;
         break;
   }

   // Set clock (if overriden by internal oscillators) and reset delays. If
   // clock out of range (i.e. it's not at least 32Khz and is more than the
   // max value specified in the .ini file for the micro), then a warning
   // message will be printed inside On_reset().
   SET_CLOCK(myClock, resetDelay);
   
   // If fuse CKDIV8=0 (programmed) then On_reset() will divide the system
   // clock by 8 and will assign the initial value to the CLKPR register.
   if(GET_FUSE("CKDIV8") == 0) { 
      PRINT("System clock divided by 8 (fuse CKDIV8=0)");
   }
   VAR(Prescaler) = 0;
   
   // TODO: Print the actual clock value used
   
   Started = true;
   
   //TRACE(true);   // Uncomment for tracing
}
/* >>> Improvements: a frequency vs. VDD check could be performed to see if the
       combination is within specs.
*/


void On_simulation_end()
//**********************
{
   FOREACH_REGISTER(j){
      REG(j) = WORD8(0,0); // Leave all bits unknown (X)
   }

   Started = false;
}

WORD8 *On_register_read(REGISTER_ID pId)
//*************************************
// A NULL return means no special action: the normal register address will 
// be read; same if the function is not present. Really not needed here... it could be deleted
// 
{
   return NULL; 
}

void On_register_write(REGISTER_ID pId, WORD8 pData)
//**************************************************
// The micro is writing into the given register. This notification
// allows to perform all the derived operations. pId contains
// the register ID and pData the data to be written (see at the end)
{
   int zeroMask = 0xFF;    // To implement read-only bits
   BOOL bitIVSEL = 0;

// Macros to integrate the read-only mask and X bits checking for each register
//
#define case_register(r, m) \
   case r: { \
      zeroMask = m; \
      if((pData.x() & m) != m) \
         WARNING("Unknown bits (X) written into "#r" register", CAT_MEMORY, WARN_MEMORY_WRITE_X_IO);

#define end_register } break;


   switch(pId) {

      // TODO: If EICRA modified and level triggered interrupts selected, then
      // trigger interrupts immediately if GET_LOGIC shows low for interrupt
      // pins.

      case_register(MCUSR, 0x0F)  // MCU Status Register
      //-------------------------------------------------
      // R/W bits = WDRF BORF EXTRF PORF (mask = 0x0F)     
         
         // Setting or clearing WDRF affects the watchdog timer bit WDE.
         if(pData[3] != UNKNOWN) {
            NOTIFY("WDOG", pData[3] ? NTF_WDRF1 : NTF_WDRF0);
         }         
      end_register
            
      case_register(MCUCR, 0x13)  // MCU Control Register
      //-------------------------------------------------
      // R/W bits = PUD, IVSEL,IVCE (mask = 0x13)

         switch (pData.get_field(1, 0)) {     // Get IVSEL, IVCE bits group (1, 0)
            case 2:
               bitIVSEL = 1; // no 'break' here
            case 0:
               if(REG(MCUCR)[0] == 1) {       // bit 0, IVCE valid only during 4 cycles
                  SET_INTERRUPT_VECTORS(bitIVSEL ? IV_BOOT_RESET : IV_STANDARD_RESET);
               }
               // TODO: Warn about changing IVSEL while IVCE is disabled
               break;
            case 1:
               REMIND_ME2(4, AUTOCLEAR_IVCE); // Set auto-clear after 4 cycles. Interrupts must be
               break;                         // disabled during this time ! (not implemented yet)
            case 3:
               WARNING("MCUCR: forbidden IVCE/IVSEL write sequence", CAT_CPU, WARN_MISC);
               break;
         }
      end_register

      case_register(CLKPR, 0x8F)   // Clock Prescale Register
      //-----------------------------------------------------
      // R/W bits = CLKPCE and CLKPS3..0 (mask = 0x8F)

         int oldPrescaler = REG(CLKPR).get_field(3, 0);
         int newPrescaler = pData.get_field(3, 0);
         
         // Can only set CLKPCE=1 if all CLKPS fields are zero. CLKPCE will be
         // auto cleared after 4 cycles but only if it wasn't already set.
         // TODO: Need a separate tick signature for each autoclear
         if(pData[7] == 1) {
            if(newPrescaler == 0) {
               if(REG(CLKPR)[7] != 1) {
                  REMIND_ME2(4, AUTOCLEAR_CLKPCE);
               }
            } else {
               WARNING("CLKPR: Must write CLKPSx=0 if writing CLKPCE=1",
                  CAT_CPU, WARN_MISC);
               pData = REG(CLKPR);  // Trick to disable any register update
            }
         }
         
         // Clock prescaler can be changed only if writing CLKPCE=0 and CLKPCE
         // was previously set in the register.
         else if(newPrescaler != oldPrescaler) {
            if(REG(CLKPR)[7] != 1) {
               WARNING("CLKPR: Cannot change CLKPSx if CLKPCE not already set",
                  CAT_CPU, WARN_MISC);
            } else if(pData[7] != 0) {
               WARNING("CLKPR: Must write CLKPCE=0 if changing CLKPSx",
                  CAT_CPU, WARN_MISC);
            } else {
               if(newPrescaler > MAX_PRESCALER_INDEX) {
                  WARNING("CLKPR: Selecting a reserved prescaling factor",
                     CAT_CPU, WARN_PARAM_RESERVED);
               }
               if(newPrescaler >= 0 && newPrescaler <= MAX_PRESCALER_INDEX) {
                  Change_clock(newPrescaler);
               }
               break; // Allow register assignment
            }
         }
         
         // Fall thru case: allow only writing CLKPS=X or CLKPS=0
         REG(CLKPR).set_bit(7, pData[7]);
         pData = REG(CLKPR);
         
      end_register

      case_register(PRR, 0xEF) // Power Reduction Register
      //-------------------------------------------------
      // All bits r/w except #4, mask = 0xEF

          // Check each bit in the register and send out notification only
          // if the new value is different from the old one.
          for(int i = 0; i < 8; i++) {
             if(pData[i] != REG(PRR)[i] && PRR_Names[i]) {
                NOTIFY(PRR_Names[i], pData[i] == 1 ? NTF_PRR1 : NTF_PRR0);
             }
          }
      end_register

      case_register(SMCR, 0x0F) // Sleep Mode Control Register
      //-----------------------------------------------------
      // Only r/w bits 0-3; mask = 0x0F. No special action

      end_register

      case_register(SPMCSR, 0x9F) // Store Program Memory Control and Status Register
      //----------------------------------------------------------------------------
      // All bits r/w except 5, 6. Mask = 0x9F

         SET_INTERRUPT_ENABLE(SPMR, (BOOL)pData[7]);   // Bit 7 = SPMIE
         if(pData[0] == 1) {                           // Bit 0: SELFPRGEN
            REMIND_ME2(4, AUTOCLEAR_SELFPRGEN);        // Clear after 4 cycles
         }
         // TODO: Eventually need to handle read-while-write sections here
      end_register

      case_register(OSCCAL, 0xFF)  // Oscillator Calibration Register
      //-------------------------------------------------------------
      // All bits r/w, Mask = 0xFF
      // Could simulate here a small internal clock change.... not implemented

      end_register

      case_register(GTCCR, 0x83) // General Timer/Counter Control Register
      //--------------------------------------------------------------------
      // Valid bits: 7, 1, 0. Mask = 0x83

         // If bit 7, TSM == 1, PSRSYNC/PSRASY stay set
         if(pData[7] == 1) {

            // If PSRSYNC/PSRASY set while in TSM mode, the prescaler is halted because
            // its reset signal remains continuously asserted
            if(pData[0] == 1 && REG(GTCCR)[0] == 0) { // PSRSYNC
               NOTIFY("TIMER0", NTF_TSM);
               NOTIFY("TIMER1", NTF_TSM);
            }
            if(pData[1] == 1 && REG(GTCCR)[1] == 0) { // PSRASY
               NOTIFY("TIMER2", NTF_TSM);
            }
            
            // If PSRSYNC/PSRASY manually cleared, reset/restart prescalers
            if(pData[0] == 0 && REG(GTCCR)[0] == 1) { // PSRSYNC
               NOTIFY("TIMER0", NTF_PSR);
               NOTIFY("TIMER1", NTF_PSR);
            }
            if(pData[1] == 0 && REG(GTCCR)[1] == 1) { // PSRASY
               NOTIFY("TIMER2", NTF_PSR);
            }
                     
         // If bit 7, TSM == 0, PSRSYNC/PSRASY cleared by hardware
         } else {              
            zeroMask = 0x80;    // All will be cleared except bit 7
            
            // If PSRSYNC/PSRASY bits are written or already set, reset/restart prescalers
            if(pData[0] == 1 || REG(GTCCR)[0] == 1) { // PSRSYNC
               NOTIFY("TIMER0", NTF_PSR);
               NOTIFY("TIMER1", NTF_PSR);
            }
            if(pData[1] == 1 || REG(GTCCR)[1] == 1) { // PSRASY
               NOTIFY("TIMER2", NTF_PSR);
            }
         }
      end_register
   }
   REG(pId) = pData & zeroMask;  // Put arriving data into the register; zero read-only bits
}

void On_remind_me(double pTime, int pAux)
//***************************************
// Response to REMIND_ME2() used to implement the 4 cycles 
// time window in some functions. After such time flags are cleared.
{
   switch(pAux) {
      case AUTOCLEAR_SELFPRGEN:
         REG(SPMCSR).set_bit(0, 0);  // Clear bit 0: SELFPRGEN
         break;
         
      case AUTOCLEAR_CLKPCE:
         if(REG(CLKPR)[7] != 0) {
            WARNING("CLKPR: CLKPCE cleared by hardware; previously set 4 cycles ago",
               CAT_CPU, WARN_MISC);
         }      
         REG(CLKPR).set_bit(7, 0);   // Clear bit 7: CLKPCE
         break;
         
      case AUTOCLEAR_IVCE:
         REG(MCUCR).set_bit(0, 0);   // Clear bit 0: IVCE
         break;
   }
}

int On_instruction(int pCode)
//**************************
// Treatment of special instructions. Handle this callback function to
// code instructions whose behaviour depends on some register contents, 
// which can be different among AVR models. The return value gives the
// action to be done. Instructions that need special handling 
// are SLEEP, SPM and LPM and WDR, identified in pCode
{
   int retValue = 0; // The return value

   switch(pCode) {

      case INSTR_WDR:
      //-------------
         NOTIFY("WDOG", NTF_WDR);           // a reset notification should be sent 
         break;                             // to the watchdog peripheral

      case INSTR_SLEEP:
      //--------------
      {
         if(REG(SMCR)[0] == 1) {                        // Check Sleep Enable bit 0, SE
            int sleepMode = REG(SMCR).get_field(3, 1);  // Extract field 3 - 1: SM2, SM1, SM0
            switch(sleepMode) {
               case 0:  retValue = SLEEP_IDLE; break;
               case 1:  retValue = SLEEP_NOISE_REDUCTION; break;
               case 2:  retValue = SLEEP_POWERDOWN; break;
               case 3:  retValue = SLEEP_POWERSAVE; break;
               case 6:  retValue = SLEEP_STANDBY; break;
               case 4: case 5: case 7: // Reserved cases
                  retValue = SLEEP_DENIED;
                  WARNING("SLEEP: using a reserved mode in SMx bits", CAT_CPU, WARN_MISC);
                  break;
               default:
                  retValue = SLEEP_DENIED;
                  WARNING("SLEEP: some SMx bits are X in SMCR", CAT_CPU, WARN_MISC);
            }
         } else {
            WARNING("SLEEP: bit 0 (SE) on SMCR is not 1", CAT_CPU, WARN_MISC);
            retValue = SLEEP_DENIED;
         }
      } break; // INSTR_SLEEP

      case INSTR_SPM:
      //-------------
      // Get bits PGWRT, PGERS, SELFPRGEN, bit field (2, 0), to determine
      // the action to be returned
      {
         if(REG(SPMCSR)[0] == 0) { // First, check bit 0, SELFPRGEN
            WARNING("SPM: bit SELFPRGEN is not 1 at SPMCSR", CAT_CPU, WARN_MISC);
            retValue = SPM_DENIED;
            break;
         }
         int spmMode = REG(SPMCSR).get_field(2, 0);
         switch(spmMode) {
            case 0x01:                       // Write temporary buffer
               retValue = SPM_WRITE_BUFFER;
               break;
            case 0x03:                       // Erase selected page
               retValue = SPM_ERASE_PAGE;
               break;
            case 0x05:                       // Write selected page
               retValue = SPM_WRITE_PAGE;
               break;
            case 0x09:                       // Write memory lock bits
               retValue = SPM_DENIED;
               WARNING("SPM: writing lock bits not implemented", CAT_CPU, WARN_MISC);
               // TODO: Lock bits could be treated like fuses
               break;
            case -1:                         // If some undetermined value
               WARNING("SPM: some X bits at SPMCSR", CAT_CPU, WARN_MISC);
               retValue = SPM_DENIED;
               break;
            default:
               WARNING("SPM: invalid bit combination at SPMCSR", CAT_CPU, WARN_MISC);
               retValue = SPM_DENIED;
         }
      } break; // INSTR_SPM

      case INSTR_LPM:
      //------------
      // Still to be implemented. Just will follow standard behaviour
         break; // INSTR_LPM
   }
   return retValue;
}

void On_reset(int pCause)
//***********************
// Initialize registers to the desired value. Use the FOREACH macro for
// a general initialization
{
   // Set the corresponding bit in the MCUSR according to the cause
   // Follow page #44 manual
   //
   FOREACH_REGISTER(j) {
      if(j != MCUSR) REG(j)= 0;  // Set all zero except MCUSR, treated below
   }
   switch(pCause) {
      case RESET_POWERON:  REG(MCUSR) = 1; break; // PORF bit 0, clear the rest
      case RESET_EXTERNAL: REG(MCUSR).set_bit(1, 1); break; // EXTRF bit 1
      case RESET_BROWNOUT: REG(MCUSR).set_bit(2, 1); break; // BORF bit 2
      case RESET_WATCHDOG: REG(MCUSR).set_bit(3, 1); break; // WDRF bit 3
   }
   REG(OSCCAL) = 0x3A;  // Load an arbitrary calibration value. This can be improved !!!
   
   // If fuse CKDIV8=0 then set initial prescaler factor to 8 (at index 3)
   // and call SET_CLOCK() to adjust the clock speed accordingly
   if(GET_FUSE("CKDIV8") == 0) {
      REG(CLKPR) = 3;
      Change_clock(3);
   } else {
      Change_clock(0);
   }
   
   // TODO: Check for low-level INT and FLAG_LOCK
   // Since both INT0 and INT1 are level triggered afet reset, if either pin
   // is low then immediately schedule an interrupt.   
}
 

void On_port_edge(const char *pPortName, int pBit, EDGE pEdge, double pTime)
//*************************************************************************
// Handle external interrupts. Parameter pPortName contains the involved port
// name "PA", "PB", etc, as defined in .INI file. pBit is the bit nr, 0 - 7
{
   // TODO: Delete this later
   //char buf[128];
   //sprintf(buf, "%s%d = %d", pPortName, pBit, pEdge);
   //PRINT(buf);

   switch (pPortName[1]) { // See the 2nd letter, A, B, C, etc
      case 'B':
      //-------                             // Port "PB": PCINT0 to PCINT7
         if(REG(PCMSK0)[pBit] == 1)         // Check if register's mask bit is 1
            SET_INTERRUPT_FLAG(IOCH0, FLAG_SET);            
         break;

      case 'C':
      //-------                             // Port "PC": PCINT8 to PCINT15
         if(REG(PCMSK1)[pBit] == 1)         // Check if register's mask bit is 1
            SET_INTERRUPT_FLAG(IOCH1, FLAG_SET);            
         break;

      // TODO: edge INTx interrupts only work if not in sleep (IDLE is ok)
      case 'D':                             // Port "PD": PCINT16 to PCINT23 and INT0, INT1
      //-------                             
         if(pBit == 2) {                           //  INT0 handling, shared by PD2
            switch(REG(EICRA).get_field(1, 0)) {   //  Check edge selection bits 1 - 0 -> ISC01, ISC00
               case 0:   // Low level
                  if(pEdge == FALL) {
                     SET_INTERRUPT_FLAG(INT0, FLAG_LOCK);   // Lock interrupt till called again
                  } else if(pEdge == RISE) {
                     SET_INTERRUPT_FLAG(INT0, FLAG_UNLOCK); // Unlock interrupt
                  }
                  break;
               case 1:  // Any edge
                  SET_INTERRUPT_FLAG(INT0, FLAG_SET); 
                  break;
               case 2:  // Falling edge
                  if(pEdge == FALL)
                     SET_INTERRUPT_FLAG(INT0, FLAG_SET); 
                  break;
               case 3:  // Rising  edge 
                  if(pEdge == RISE)
                     SET_INTERRUPT_FLAG(INT0, FLAG_SET); 
                  break;               
               default: // -1: some bit undefined. Flag error
                  BREAK("INT0: undefined bits in EICRA");
                  break;
            }

         } else if(pBit == 3) {                   // INT1 handling, shared by PD3
            switch(REG(EICRA).get_field(3, 2)) {  // Check edge: bits 3 - 2, ISC11, ISC10 
               case 0:  // Low level
                  if(pEdge == FALL) {
                     SET_INTERRUPT_FLAG(INT1, FLAG_LOCK);   // Lock interrupt till called again
                  } else if(pEdge == RISE) {
                     SET_INTERRUPT_FLAG(INT1, FLAG_UNLOCK); // Unlock interrupt
                  }
                  break;
               case 1:  // Any edge 
                  SET_INTERRUPT_FLAG(INT1, FLAG_SET); 
                  break;
               case 2:  // Falling edge 
                  if(pEdge == FALL)
                     SET_INTERRUPT_FLAG(INT1, FLAG_SET); 
                  break;
               case 3:  // Rising  edge 
                  if(pEdge == RISE)
                     SET_INTERRUPT_FLAG(INT1, FLAG_SET);
                  break;
               default: // -1: some bit is undefined. Flag error
                  BREAK("INT1: undefined bits in EICRA");
                  break;
            }
         }
         if(REG(PCMSK2)[pBit] == 1)      
            SET_INTERRUPT_FLAG(IOCH2, FLAG_SET);  //  Both the INT1 and IOCH2 can be set in parallel          
         break;
   }
}

void On_gadget_notify(GADGET pGadget, int pCode)
//**********************************************
// This is an example of a possible use of the dummy peripheral
// for adding some debug goodies, which can be tuned to the
// micro being modelled, in this case to force different types of reset
{
   if(Started) { // Cannot call RESET() until simulation has started 
      if(pCode == BN_CLICKED) { // Response to button click 
         switch(pGadget) {
            case GADGET13: RESET(RESET_EXTERNAL); break;
            case GADGET14: RESET(RESET_BROWNOUT); break;
            case GADGET15: RESET(RESET_WATCHDOG); break;
            // The POWER ON code is given at simulation start
         }
      }
   }
}
