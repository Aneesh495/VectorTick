// AVX2 detection
#include <immintrin.h>
int main() {
    __m256i x = _mm256_setzero_si256();
    x = _mm256_add_epi64(x, x);
    return _mm256_extract_epi64(x, 0);
}
