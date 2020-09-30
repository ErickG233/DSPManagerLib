/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "FIR16.h"

#include <string.h>


FIR16::FIR16()
    : mIndex(0)
{
    memset(mState, 0, sizeof(mState));
    memset(mCoeff, 0, sizeof(mCoeff));
}

FIR16::~FIR16()
{
}

void FIR16::setParameters(double coeff[16])
{
    for (int32_t i = 0; i < 16; i ++) {
        mCoeff[i] = double(coeff[i] * 4294967296.0);
    }
}

double FIR16::process(double x0)
{
    mIndex --;
    mState[mIndex & 0xf] = x0;

    double y = 0.0;
    for (int32_t i = 0; i < 16; i ++) {
        y += mCoeff[i] * mState[(i + mIndex) & 0xf];
    }

    return (y / 4294967296.0);
}
