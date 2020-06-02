#ifndef CAFFE_UTIL_MKL_ALTERNATE_H_
#define CAFFE_UTIL_MKL_ALTERNATE_H_

#ifdef USE_TBB
#include <tbb/mutex.h>
#include <tbb/parallel_for.h>
#include <tbb/parallel_sort.h>

#define SORT(b, e, comp) tbb::parallel_sort(b, e, comp)

#define ATOMIC_UPDATE(mutex, operation)                                        \
  {                                                                            \
    tbb::mutex::scoped_lock lock(mutex);                                       \
    operation;                                                                 \
  }
#define FOR_LOOP_WITH_PREPARE(n, iname, operation, prepare)                    \
  tbb::parallel_for(                                                           \
      tbb::blocked_range<int>(0, n),                                           \
      [&](const tbb::blocked_range<int> &r) {                                  \
        prepare;                                                               \
        for (int iname = r.begin(); iname != r.end(); ++iname)                 \
          operation;                                                           \
      },                                                                       \
      tbb::auto_partitioner());
#define FOR_LOOP(n, iname, operation)                                          \
  FOR_LOOP_WITH_PREPARE(n, iname, operation, )
#elif defined(USE_OMP)
#ifdef _MSC_VER
#define OMP_PRAGMA_FOR __pragma(omp parallel for)
#define OMP_PRAGMA __pragma(omp)
#else
#define OMP_PRAGMA_FOR _Pragma("omp parallel for")
#define OMP_PRAGMA _Pragma(omp)
#endif

#include <algorithm>
#define SORT(b, e, comp) std::sort(b, e, comp)

#define ATOMIC_UPDATE(mutex, operation)                                        \
  _Pragma("omp critical(dataupdate)") { operation; }

#define FOR_LOOP_WITH_PREPARE(n, iname, operation, prepare)                    \
  OMP_PRAGMA_FOR                                                               \
  for (int iname = 0; iname < n; ++iname) {                                    \
    prepare;                                                                   \
    operation;                                                                 \
  }
#define FOR_LOOP(n, iname, operation)                                          \
  FOR_LOOP_WITH_PREPARE(n, iname, operation, )

#else
#include <algorithm>
#define SORT(b, e, comp) std::sort(b, e, comp)
#define ATOMIC_UPDATE(mutex, operation)                                        \
  { operation; }
#define FOR_LOOP_WITH_PREPARE(n, iname, operation, prepare)                    \
  prepare;                                                                     \
  for (int iname = 0; iname < n; ++iname)                                      \
    operation;
#define FOR_LOOP(n, iname, operation)                                          \
  FOR_LOOP_WITH_PREPARE(n, iname, operation, )
#endif

#ifdef USE_MKL

#include <mkl.h>

#else // If use MKL, simply include the MKL header

#ifdef USE_ACCELERATE
#include <Accelerate/Accelerate.h>
#else
extern "C" {
#include <cblas.h>
}
#endif // USE_ACCELERATE

#include <cmath>

// Functions that caffe uses but are not present if MKL is not linked.

// A simple way to define the vsl unary functions. The operation should
// be in the form e.g. y[i] = sqrt(a[i])
#define DEFINE_VSL_UNARY_FUNC(name, operation)                                 \
  template <typename Dtype>                                                    \
  void v##name(const int n, const Dtype *a, Dtype *y) {                        \
    CHECK_GT(n, 0);                                                            \
    CHECK(a);                                                                  \
    CHECK(y);                                                                  \
    FOR_LOOP(n, i, operation)                                                  \
  }                                                                            \
  inline void vs##name(const int n, const float *a, float *y) {                \
    v##name<float>(n, a, y);                                                   \
  }                                                                            \
  inline void vd##name(const int n, const double *a, double *y) {              \
    v##name<double>(n, a, y);                                                  \
  }

DEFINE_VSL_UNARY_FUNC(Sqr, y[i] = a[i] * a[i])
DEFINE_VSL_UNARY_FUNC(Sqrt, y[i] = sqrt(a[i]))
DEFINE_VSL_UNARY_FUNC(Exp, y[i] = exp(a[i]))
DEFINE_VSL_UNARY_FUNC(Ln, y[i] = log(a[i]))
DEFINE_VSL_UNARY_FUNC(Abs, y[i] = fabs(a[i]))

// A simple way to define the vsl unary functions with singular parameter b.
// The operation should be in the form e.g. y[i] = pow(a[i], b)
#define DEFINE_VSL_UNARY_FUNC_WITH_PARAM(name, operation)                      \
  template <typename Dtype>                                                    \
  void v##name(const int n, const Dtype *a, const Dtype b, Dtype *y) {         \
    CHECK_GT(n, 0);                                                            \
    CHECK(a);                                                                  \
    CHECK(y);                                                                  \
    FOR_LOOP(n, i, operation)                                                  \
  }                                                                            \
  inline void vs##name(const int n, const float *a, const float b, float *y) { \
    v##name<float>(n, a, b, y);                                                \
  }                                                                            \
  inline void vd##name(const int n, const double *a, const float b,            \
                       double *y) {                                            \
    v##name<double>(n, a, b, y);                                               \
  }

DEFINE_VSL_UNARY_FUNC_WITH_PARAM(Powx, y[i] = pow(a[i], b))

// A simple way to define the vsl binary functions. The operation should
// be in the form e.g. y[i] = a[i] + b[i]
#define DEFINE_VSL_BINARY_FUNC(name, operation)                                \
  template <typename Dtype>                                                    \
  void v##name(const int n, const Dtype *a, const Dtype *b, Dtype *y) {        \
    CHECK_GT(n, 0);                                                            \
    CHECK(a);                                                                  \
    CHECK(b);                                                                  \
    CHECK(y);                                                                  \
    FOR_LOOP(n, i, operation)                                                  \
  }                                                                            \
  inline void vs##name(const int n, const float *a, const float *b,            \
                       float *y) {                                             \
    v##name<float>(n, a, b, y);                                                \
  }                                                                            \
  inline void vd##name(const int n, const double *a, const double *b,          \
                       double *y) {                                            \
    v##name<double>(n, a, b, y);                                               \
  }

DEFINE_VSL_BINARY_FUNC(Add, y[i] = a[i] + b[i])
DEFINE_VSL_BINARY_FUNC(Sub, y[i] = a[i] - b[i])
DEFINE_VSL_BINARY_FUNC(Mul, y[i] = a[i] * b[i])
DEFINE_VSL_BINARY_FUNC(Div, y[i] = a[i] / b[i])

// In addition, MKL comes with an additional function axpby that is not present
// in standard blas. We will simply use a two-step (inefficient, of course) way
// to mimic that.
inline void cblas_saxpby(const int N, const float alpha, const float *X,
                         const int incX, const float beta, float *Y,
                         const int incY) {
  cblas_sscal(N, beta, Y, incY);
  cblas_saxpy(N, alpha, X, incX, Y, incY);
}
inline void cblas_daxpby(const int N, const double alpha, const double *X,
                         const int incX, const double beta, double *Y,
                         const int incY) {
  cblas_dscal(N, beta, Y, incY);
  cblas_daxpy(N, alpha, X, incX, Y, incY);
}

#endif // USE_MKL
#endif // CAFFE_UTIL_MKL_ALTERNATE_H_
