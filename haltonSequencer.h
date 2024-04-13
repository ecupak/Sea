#pragma once


class HaltonSequencer
{
public:
	HaltonSequencer() = default;

	void generateSamples(const int HALTON_SAMPLES, const int HALTON_BASE_PRIME, float samples[]);
};

