#include "PixelStudioApp.hpp"

#include <xmmintrin.h>
#include <pmmintrin.h>


int main(void)
{
	// activating High-Performance SSE/AVX Subnormal/Denormal Handling
	#if defined(__x86_64__) || defined(_M_X64)
	_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
	_MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
	#endif

	PixelStudio::PixelStudioApp app;
	return app.run();
}
