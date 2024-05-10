#ifndef STK_NGP_TEST_NGPTESTDEVICEMACROS_HPP
#define STK_NGP_TEST_NGPTESTDEVICEMACROS_HPP

#include <Kokkos_Core.hpp>

#define NGP_TEST_FUNCTION KOKKOS_FUNCTION
#define NGP_TEST_INLINE KOKKOS_INLINE_FUNCTION

#ifdef STK_ENABLE_GPU
#define NGP_TEST_DEVICE_ONLY __device__
#else
#define NGP_TEST_DEVICE_ONLY
#endif

#endif
