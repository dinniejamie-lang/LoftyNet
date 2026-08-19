#ifndef NNUE_SIMD_H
#define NNUE_SIMD_H

#include "nnue_types.h"
#include <cstdint>
#include <cstring>

// Include SIMD headers first before any potential conflicts
#if defined(__AVX2__)
#include <immintrin.h>
#elif defined(__SSE2__)
#include <emmintrin.h>
#endif

// SIMD-optimized operations for NNUE
// Auto-detects available instruction sets at compile time

namespace nnue {
namespace simd {

// Check if SIMD is available
#if defined(__AVX2__)
    #define NNUE_USE_AVX2 1
#elif defined(__SSE2__) || defined(_M_X64)
    #define NNUE_USE_SSE2 1
#else
    #define NNUE_USE_SCALAR 1
#endif

// AVX2 optimized ReLU activation
inline void relu_activation_avx2(int16_t* output, const int16_t* input, int size) {
    const int chunk_size = 16; // Process 16 int16_t at once with AVX2
    const int limit = (size / chunk_size) * chunk_size;
    
    __m256i zero = _mm256_setzero_si256();
    
    for (int i = 0; i < limit; i += chunk_size) {
        __m256i v_in = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&input[i]));
        __m256i v_relu = _mm256_max_epi16(v_in, zero);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(&output[i]), v_relu);
    }
    
    // Handle remainder
    for (int i = limit; i < size; ++i) {
        output[i] = (input[i] > 0) ? input[i] : 0;
    }
}

// AVX2 dot product
inline int32_t dot_product_avx2(const int16_t* a, const int16_t* b, int size) {
    const int chunk_size = 16;
    const int limit = (size / chunk_size) * chunk_size;
    
    __m256i acc_lo = _mm256_setzero_si256();
    __m256i acc_hi = _mm256_setzero_si256();
    
    for (int i = 0; i < limit; i += chunk_size) {
        __m256i va = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&a[i]));
        __m256i vb = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&b[i]));
        
        // Multiply and accumulate
        __m256i v_prod_lo = _mm256_madd_epi16(
            _mm256_unpacklo_epi16(va, vb),
            _mm256_unpackhi_epi16(va, vb)
        );
        
        acc_lo = _mm256_add_epi32(acc_lo, v_prod_lo);
    }
    
    // Horizontal sum
    __m256i acc_sum = _mm256_add_epi32(acc_lo, acc_hi);
    __m128i sum128 = _mm_add_epi32(_mm256_castsi256_si128(acc_sum),
                                    _mm256_extracti128_si256(acc_sum, 1));
    sum128 = _mm_add_epi32(sum128, _mm_shuffle_epi32(sum128, _MM_SHUFFLE(2, 3, 0, 1)));
    sum128 = _mm_add_epi32(sum128, _mm_shuffle_epi32(sum128, _MM_SHUFFLE(1, 0, 3, 2)));
    
    int32_t result = _mm_cvtsi128_si32(sum128);
    
    // Handle remainder
    for (int i = limit; i < size; ++i) {
        result += static_cast<int32_t>(a[i]) * static_cast<int32_t>(b[i]);
    }
    
    return result;
}

#elif defined(NNUE_USE_SSE2)
#include <emmintrin.h>

// SSE2 optimized ReLU activation
inline void relu_activation_sse2(int16_t* output, const int16_t* input, int size) {
    const int chunk_size = 8;
    const int limit = (size / chunk_size) * chunk_size;
    
    __m128i zero = _mm_setzero_si128();
    
    for (int i = 0; i < limit; i += chunk_size) {
        __m128i v_in = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&input[i]));
        __m128i v_relu = _mm_max_epi16(v_in, zero);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(&output[i]), v_relu);
    }
    
    for (int i = limit; i < size; ++i) {
        output[i] = (input[i] > 0) ? input[i] : 0;
    }
}

// SSE2 dot product
inline int32_t dot_product_sse2(const int16_t* a, const int16_t* b, int size) {
    const int chunk_size = 8;
    const int limit = (size / chunk_size) * chunk_size;
    
    __m128i acc = _mm_setzero_si128();
    
    for (int i = 0; i < limit; i += chunk_size) {
        __m128i va = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&a[i]));
        __m128i vb = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&b[i]));
        
        __m128i v_prod = _mm_madd_epi16(va, vb);
        acc = _mm_add_epi32(acc, v_prod);
    }
    
    // Horizontal sum
    acc = _mm_add_epi32(acc, _mm_shuffle_epi32(acc, _MM_SHUFFLE(2, 3, 0, 1)));
    acc = _mm_add_epi32(acc, _mm_shuffle_epi32(acc, _MM_SHUFFLE(1, 0, 3, 2)));
    
    int32_t result = _mm_cvtsi128_si32(acc);
    
    for (int i = limit; i < size; ++i) {
        result += static_cast<int32_t>(a[i]) * static_cast<int32_t>(b[i]);
    }
    
    return result;
}

#endif

// Generic (fallback) implementations
inline void relu_activation_scalar(int16_t* output, const int16_t* input, int size) {
    for (int i = 0; i < size; ++i) {
        output[i] = (input[i] > 0) ? input[i] : 0;
    }
}

inline int32_t dot_product_scalar(const int16_t* a, const int16_t* b, int size) {
    int32_t result = 0;
    for (int i = 0; i < size; ++i) {
        result += static_cast<int32_t>(a[i]) * static_cast<int32_t>(b[i]);
    }
    return result;
}

// Auto-dispatch functions
inline void relu_activation(int16_t* output, const int16_t* input, int size) {
#ifdef NNUE_USE_AVX2
    relu_activation_avx2(output, input, size);
#elif defined(NNUE_USE_SSE2)
    relu_activation_sse2(output, input, size);
#else
    relu_activation_scalar(output, input, size);
#endif
}

inline int32_t dot_product(const int16_t* a, const int16_t* b, int size) {
#ifdef NNUE_USE_AVX2
    return dot_product_avx2(a, b, size);
#elif defined(NNUE_USE_SSE2)
    return dot_product_sse2(a, b, size);
#else
    return dot_product_scalar(a, b, size);
#endif
}

// Vectorized add/subtract for accumulator updates
inline void vector_add(int16_t* dest, const int16_t* src, int size) {
#ifdef NNUE_USE_AVX2
    const int chunk_size = 16;
    const int limit = (size / chunk_size) * chunk_size;
    
    for (int i = 0; i < limit; i += chunk_size) {
        __m256i vd = _mm256_loadu_si256(reinterpret_cast<__m256i*>(&dest[i]));
        __m256i vs = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&src[i]));
        __m256i v_result = _mm256_add_epi16(vd, vs);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(&dest[i]), v_result);
    }
    
    for (int i = limit; i < size; ++i) {
        dest[i] += src[i];
    }
#elif defined(NNUE_USE_SSE2)
    const int chunk_size = 8;
    const int limit = (size / chunk_size) * chunk_size;
    
    for (int i = 0; i < limit; i += chunk_size) {
        __m128i vd = _mm_loadu_si128(reinterpret_cast<__m128i*>(&dest[i]));
        __m128i vs = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&src[i]));
        __m128i v_result = _mm_add_epi16(vd, vs);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(&dest[i]), v_result);
    }
    
    for (int i = limit; i < size; ++i) {
        dest[i] += src[i];
    }
#else
    for (int i = 0; i < size; ++i) {
        dest[i] += src[i];
    }
#endif
}

} // namespace simd
} // namespace nnue

#endif // NNUE_SIMD_H
