/* quest_rt.h — the runtime header for the reconstructed Quest source.
 * SCAFFOLDING (Sep 8 2026): shapes only; filled in by P35.
 *
 * Same header, two views:
 *   C   (pycparser / the IR translator): typedefs the translator recognises
 *   C++ (native build): real objects with operator[] and bounds checks
 */
#ifndef QUEST_RT_H
#define QUEST_RT_H
#include <stdint.h>

/* PL/I CHAR(n) VARYING: length word + data. Capacity is part of the type. */
#define VARYING(n) struct { int16_t len; char data[n]; }

/* 1-based array of records: the source indexes foo[i] with PL/I's i;
 * the translator folds (i-1)*stride + origin into the compiler's constant;
 * natively operator[] subtracts one and checks bounds. */
#ifdef __cplusplus
  template <typename T, int N> struct array1 { T e[N]; T &operator[](int i); };
  #define ARRAY1(T, N) array1<T, N>
#else
  #define ARRAY1(T, N) T   /* translator: element type + N from the declaration */
#endif

/* PL/I subscript check (DERR 17): assert(0 < i && i <= n) */
#define SUB(i, n) (i)

/* Runtime calls: arity in the name; const = the callee only reads it.
 * (One per shape actually used; see Project28/RTConventions.md.) */
void WRITE_SCREEN$2(const uint32_t *chan, const void *msg);
void WRITE_SCREEN$5(const uint32_t *chan, const void *msg, int16_t *row, int16_t *col, const uint32_t *opts);
void UNSIGNED_TO_CHAR$1(void *out);           /* writes a varying at the given address */
uint32_t RANDOM_NUMBER$3(uint32_t *seed, const int32_t *hi, const int32_t *lo);

#endif
