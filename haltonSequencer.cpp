#include "precomp.h"
#include "haltonSequencer.h"


// [Credit] Ray Tracing Gems II
void HaltonSequencer::generateSamples(const int HALTON_SAMPLES, const int HALTON_BASE_PRIME, float samples[])
{
	int n{ 0 };
	int d{ 1 };

	for (int i = 0; i < HALTON_SAMPLES; ++i)
	{
		int x{ d - n };

		if (x == 1)
		{
			n = 1;
			d *= HALTON_BASE_PRIME;
		}
		else
		{
			int y = d / HALTON_BASE_PRIME;

			while (x <= y)
			{
				y /= HALTON_BASE_PRIME;
			}

			n = (HALTON_BASE_PRIME + 1) * y - x;
		}

		samples[i] = static_cast<float>(n) / static_cast<float>(d);
	}
}