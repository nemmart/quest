/* quest_rt.h — the runtime header for the reconstructed Quest source.
 * Project 35 (Sep 8 2026): filled in for the C subset of the four pilot
 * routines (PICK_X_Y, UPDATE_SCREENS, REFRESH_SCREEN, DIED).
 *
 * Same header, two views:
 *   C   (pycparser / compiler/translate.py): plain declarations the
 *       translator recognises by NAME; the numbers live in declarations.json
 *   C++ (native compile check, later the native build): real objects
 *
 * Conventions (compiler/README.md, Sep 8):
 *   - runtime calls are arity-in-name: NAME$N takes exactly N arguments,
 *     all by reference (PL/I); `const` = the callee only reads through it
 *     (docs/Project28/RTConventions.md).  A write through a const pointer
 *     is a native compile error and a translator refusal.
 *   - TMP(e): a by-reference argument that is an expression — the DG
 *     compiler materialises it in a frame temporary (PL/I dummy argument).
 *   - SUB(i, n): PL/I subscript check, i in 1..n, else DERR 17.
 *   - two-arity game routines: one body with a leading `int arg_count`
 *     (the frame marker word), plus NAME$N call sites.
 */
#ifndef QUEST_RT_H
#define QUEST_RT_H
#include <stdint.h>
#include <stddef.h>

/* a parameter the (partial) source does not use yet — staged routines only */
#ifdef __TRANSLATOR__
  #define UNUSED
#else
  #define UNUSED __attribute__((unused))
#endif

/* PL/I CHAR(n) VARYING: length word + data. Capacity is part of the type. */
#define VARYING(n) struct { int16_t len; char data[n]; }

#ifdef __cplusplus
  /* ---- C++ view ---- */
  template <typename T, int N> struct array1 {
      T *e;                              /* bound to the world image at run time */
      T &operator[](int i) { return e[i - 1]; }   /* 1-based; bounds check = SUB() */
  };
  #define ARRAY1(T, N) array1<T, N>
  template <typename T> struct based_ptr { T *p; T *operator->() { return p; } };
  /* TMP(e): a dummy argument; its TYPE is the callee parameter's (PL/I), so
   * it converts to whatever pointer the parameter wants and lives to the end
   * of the full expression (the native runtime will give it the parameter's
   * width; this is the compile-check view). */
  struct tmp_arg {
      union { int32_t w; int16_t h; } u;
      operator int32_t *() { return &u.w; }
      operator const int32_t *() { return &u.w; }
      operator int16_t *() { u.h = (int16_t)u.w; return &u.h; }
      operator const int16_t *() { u.h = (int16_t)u.w; return &u.h; }
      operator const void *() { return &u.w; }
  };
  inline tmp_arg TMP(int32_t v) { tmp_arg t; t.u.w = v; return t; }
  #define SUB(i, n) (i)
  inline int32_t ABS(int32_t v) { return v < 0 ? -v : v; }   /* PL/I ABS builtin */
  extern "C" {
#else
  /* ---- C view (the translator's) ---- */
  #define ARRAY1(T, N) T           /* element type; N from the declaration */
  void *TMP(int32_t e);            /* translator: frame temporary of the parameter's width holding e */
  int32_t ABS(int32_t v);          /* PL/I ABS builtin: the WSGE/WNEG diamond */
  #ifdef __TRANSLATOR__
    int32_t SUB(int32_t i, int32_t n); /* translator: assert(0 < i && i <= n), "DERR 17" */
  #else
    #define SUB(i, n) (i)
  #endif
#endif

/* ---- conversions: EXPLICIT, never implicit (P36 ruling 1, Sep 8 2026) ----
 * The DG compiler inserts a CHECKED convert when a 32-bit value reaches a
 * 16-bit destination.  The source of record says so: a bare `int16 = int32`
 * is a translator REFUSAL.  Each maps 1:1 to the IR op of the same name
 * (IR.md §5.1/§5.5): cvwn is EFFECTFUL (sets OVR if the value did not fit
 * int16), sx16/trunc16 are pure. */
#ifdef __cplusplus
  inline int32_t cvwn(int32_t v) { return (int32_t)(int16_t)(v & 0xFFFF); }
  inline int32_t sx16(int32_t v) { return (int32_t)(int16_t)(v & 0xFFFF); }
  inline int32_t trunc16(int32_t v) { return (int32_t)(v & 0xFFFF); }
#else
  int32_t cvwn(int32_t v);      /* CVWN: sx16(v & 0xFFFF), ovr |= did not fit */
  int32_t sx16(int32_t v);      /* sign-extend bits 15:0 */
  int32_t trunc16(int32_t v);   /* v & 0xFFFF */
#endif

/* ---- PL/I bit strings (P36) ----------------------------------------------
 * A bit reference is `16 * <word displacement> + n` computed into a register
 * and applied to a base pointer the instruction resolves indirectly
 * (WSZB / WBTO / WBTZ, EagleCompute.cpp:261/272/283).  `n` is numbered from
 * the MSB (0 = bit 15 of the word), which is the DG convention.
 * The word argument is the record/static word itself, as an lvalue; the
 * translator decomposes its address into base + scaled subscript + K and
 * folds `16*K + n` into the one WNADI constant the compiler emits. */
int  BIT(int16_t word, int n);              /* rvalue: 0/1  (WSZB)          */
void BIT_SET(int16_t word, int n);          /* statement    (WBTO)          */
void BIT_CLR(int16_t word, int n);          /* statement    (WBTZ)          */
void BIT_PUT(int16_t word, int n, int e);   /* statement: WBTO; test; WBTZ  */

/* PL/I LENGTH() of a CHAR VARYING passed by reference: the length word AT the
 * argument's address, read sign-extended (XNLDA; IR.md §5.8 -- 0xFFFF is the
 * count -1 and the master runs it).  `sx16(M16[R[ac3 + -(10+2n)]])`.
 *
 * P36 FINDING: the arena twins' claim arithmetic needs NO sizing intrinsic.
 * A claim count is ceil(bytes/4) -- the compiler's `+3; lsh -1; lsh -1` -- over
 * the CAT chain's running byte length, with +2 more for the final varying's
 * length word (`+5; lsh -1; lsh -1`).  Every constant in DIED's two groups
 * falls out of the literal piece lengths: group 70166144 `+3, +0x17` = 3 and
 * 23 bytes, group 701661AE `+3, +0x4C` = 3 and 76.  The only thing the C
 * cannot say without help is the length of a VARYING parameter, which is what
 * LEN() is for.  NOT yet in the translator's subset -- it REFUSES here. */
int LEN(const void *varying);

/* PL/I string concatenation a || b (CHAR temporaries; the translator's WSTB/WCMV shapes) */
const char *CAT(const char *a, const char *b);

/* ---- runtime calls actually used by the pilot routines ---- */
/* ?WRITE_SCREEN: channel, text[, row&, col&, options]; row/col are written back. */
void WRITE_SCREEN$2(const int32_t *chan, const void *text);
void WRITE_SCREEN$5(const int32_t *chan, const void *text, int16_t *row, int16_t *col, const int16_t *opts);
/* ?RANDOM_NUMBER: uniform in lo..hi, seed updated; returns in ac0 (RTConventions). */
int32_t RANDOM_NUMBER$3(const int32_t *lo, const int32_t *hi, int32_t *seed);
/* ?UNSIGNED_TO_CHAR: writes a varying at the given address (register argument ac2) */
void UNSIGNED_TO_CHAR$1(void *out);

/* ---- string statements (docs/Project29/StringsDesign.md §2; IR.md §5.8) ---- */
/* [@dst, n varying] = piece      PL/I assignment to CHAR(n) VARYING */
void assign_varying(void *dst, int n, const char *lit);
/* [@dst, n] = piece              PL/I assignment to CHAR(n) (fixed, blank padded) */
void assign_fixed(void *dst, int n, const char *lit);
/* ac1 = cmp(a, b)                PL/I comparison, blank padded; -1/0/+1 */
int pad_equal(const void *a, const void *b);
/* words(@d, k) = words(@s, k)    WBLM word fill */
void words_copy(void *dst, const void *src, int k);

/* ---- game routines the pilot routines call (game→game `call`) ---- */
void UPDATE_SCREENS(const int16_t *x, const int16_t *y, const int32_t *cell);
void HIT_ANY_CHAR(void);
void REPOSITION(const int16_t *who);
void REFRESH_SCREEN(int arg_count, const int16_t *flag);
void REFRESH_SCREEN$0(void);
void REFRESH_SCREEN$1(const int16_t *flag);
void DISPLAY_SCREEN$1(const int16_t *who);
void DISPLAY_INVENTORY$1(const int16_t *who);

#ifdef __cplusplus
  }
#endif
#endif
