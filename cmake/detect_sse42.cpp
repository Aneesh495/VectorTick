// SSE4.2 detection
#include <nmmintrin.h>
int main() {
    unsigned int crc = _mm_crc32_u32(0, 42);
    return static_cast<int>(crc);
}
