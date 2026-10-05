// SPDX-License-Identifier: GPL-3.0-or-later
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "c2000/encoder.h"
#include "c2000/motoranalogcapture.h"
#include "params.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

bool testInterruptsDisabled = false;
void Param::Change(Param::PARAM_NUM) {}
static float angle = 0.0f;
static const float TwoPi = 6.28318530718f;

uint16_t c2000::MotorAnalogCapture::ResolverSine()
{
    return (uint16_t)(2048.0f + 1400.0f * sinf(angle));
}
uint16_t c2000::MotorAnalogCapture::ResolverCosine()
{
    return (uint16_t)(2048.0f + 1400.0f * cosf(angle));
}

static bool Tick(float rotorHz, uint16_t pwmHz)
{
    angle += TwoPi * rotorHz / pwmHz;
    if (angle > TwoPi) angle -= TwoPi;
    if (angle < -TwoPi) angle += TwoPi;
    c2000::Encoder::UpdateRotorAngle(1);
    return c2000::Encoder::UpdateRotorFrequency();
}

static void CheckWindow(uint16_t pwmHz, float rotorHz)
{
    const uint16_t window = ((uint32_t)pwmHz + 9U) / 10U;
    c2000::Encoder::SetPwmFrequency(pwmHz);
    assert(!testInterruptsDisabled);
    for (uint16_t i = 1; i < window; ++i)
    {
        assert(!Tick(rotorHz, pwmHz));
        assert(c2000::Encoder::GetLastSampleCount() == 0);
    }
    assert(Tick(rotorHz, pwmHz));
    assert(c2000::Encoder::GetLastSampleCount() == window);
    assert(fabsf(FP_TOFLOAT(c2000::Encoder::GetRotorFrequency()) -
                 fabsf(rotorHz)) <= 0.0625f);
    assert(c2000::Encoder::GetRotorDirection() == (rotorHz > 0 ? 1 : -1));
    const int expectedRpm = (int)(fabsf(rotorHz) * 30.0f);
    assert(abs(c2000::Encoder::GetSpeed() - expectedRpm) <= 2);

    // OFF mode still receives PWM ticks, but contributes no angle samples.
    for (uint16_t i = 0; i < window; ++i)
        c2000::Encoder::UpdateRotorFrequency();
    assert(c2000::Encoder::GetRotorFrequency() == 0);
    assert(c2000::Encoder::GetRotorDirection() == 0);
    assert(c2000::Encoder::GetLastSampleCount() == 0);
}

int main()
{
    Param::LoadDefaults();
    Param::SetInt(Param::polepairs, 2);
    Param::SetInt(Param::sincosofs, 2048);
    c2000::Encoder::Reset();
    c2000::Encoder::SetPwmFrequency(12207);
    for (unsigned i = 0; i < 4001; ++i) Tick(0, 12207);

    const uint16_t frequencies[] = { 6103, 12207, 24414 };
    for (unsigned i = 0; i < sizeof(frequencies) / sizeof(frequencies[0]); ++i)
    {
        CheckWindow(frequencies[i], 10.0f);
        CheckWindow(frequencies[i], -10.0f);
    }
    // At 100 Hz, assuming an exact 10 Hz reporting rate exceeds the tolerance.
    CheckWindow(6103, 100.0f);
    CheckWindow(6103, -100.0f);

    // A partial old window must not leak into a new PWM configuration.
    c2000::Encoder::SetPwmFrequency(6103);
    for (unsigned i = 0; i < 300; ++i) Tick(10, 6103);
    CheckWindow(24414, 10.0f);

    testInterruptsDisabled = true;
    c2000::Encoder::SetPwmFrequency(12207);
    assert(testInterruptsDisabled);
    testInterruptsDisabled = false;
    c2000::Encoder::SetPwmFrequency(0);
    for (unsigned i = 0; i < 5000; ++i) Tick(0, 12207);
    assert(c2000::Encoder::GetRotorFrequency() == 0);
    c2000::Encoder::SetPwmFrequency(65536);
    assert(c2000::Encoder::GetRotorFrequency() == 0);
    c2000::Encoder::Reset();
    assert(c2000::Encoder::GetLastSampleCount() == 0);
    assert(c2000::Encoder::GetLastAbsTurns() == 0);
    assert(c2000::Encoder::GetLastMaxSignedDiff() == 0);
    puts("C2000 resolver timing checks passed");
}
