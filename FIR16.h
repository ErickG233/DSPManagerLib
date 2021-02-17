#pragma once

#include <stdint.h>

class FIR16 {
	double mCoeff[16];
	double mState[16];
	int32_t mIndex;

	public:
	FIR16();
	~FIR16();
	void setParameters(double coeff[16]);
	double process(double x0);
};
