// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef TEST_C2000_DRIVERLIB_H
#define TEST_C2000_DRIVERLIB_H

// Host boundary for the ADC declaration and interrupt-mask operations only.
typedef unsigned ADC_Trigger;
extern bool testInterruptsDisabled;
inline bool Interrupt_disableMaster()
{
    const bool previous = testInterruptsDisabled;
    testInterruptsDisabled = true;
    return previous;
}
inline void Interrupt_enableMaster() { testInterruptsDisabled = false; }

#endif
