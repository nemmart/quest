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

/* ---- the static link: uplevel access in a nested procedure (P39) ---------
 * A nested PL/I procedure receives the ENCLOSING procedure's frame pointer in
 * ac1; `WSAVS` saves it at `wp(fp, -6)`.  Every reference to a variable of the
 * enclosing procedure is therefore a DOUBLE INDIRECTION -- load the link into
 * a base register, then displace off it -- and it is a real cost the compiler
 * pays at every reference (R43: it is re-loaded in every block that needs it).
 * The source must make that visible, so an uplevel reference is spelled as an
 * ACCESSOR, never as a plain identifier.
 *
 *   UPLINK(P) __up   the link parameter: the FIRST parameter of a nested
 *                    procedure whose enclosing procedure is P
 *   UP(P, name)      a VARIABLE of the enclosing procedure P -- an lvalue;
 *                    `&UP(P, name)` is its address (XPEF / XPEFB)
 *   UPARG(P, k)      the enclosing procedure's k'th ARGUMENT.  The parent's
 *                    own parameters are by-reference, so this is a POINTER
 *                    and the value is `*UPARG(P, k)` -- a triple indirection
 *                    in the machine (link, then arg slot, then the datum:
 *                    `XNLDA 1,@[ac2+0xFFF4]`, FIRE.2 7016A479).
 *
 * P39 gate ruling (c), Sep 9 2026 (user).  Alternatives considered and
 * rejected: a `__enclosing` qualifier (makes an uplevel reference read like an
 * ordinary variable, hiding the cost -- fails the stated criterion), and
 * declaring the parent's frame as a struct the parent itself passes (the
 * better LONG-TERM shape, and the natural endpoint once the parents are being
 * reconstructed themselves, but it forces a struct on all 13 parents and
 * re-derives R3's slot allocation against member order -- too much, too early).
 *
 * The parent's frame layout lives in game/declarations.json under "frames".
 * RULE (P39, user ruling): a parent slot enters that table ONLY with a
 * recorded width and a NAMED WITNESS.  Sibling nested procedures must agree on
 * their common parent's layout independently; a disagreement is a finding, not
 * something to reconcile.  That is the difference between DERIVING the
 * parent's frame and FITTING it. */
#ifdef __cplusplus
  #define UPLINK(P)      struct P##__frame *
  #define UP(P, name)    (__up->name)
  #define UPARG(P, k)    (__up->__a##k)
#else
  #ifdef __TRANSLATOR__
    /* the translator recognises UP/UPARG by NAME and takes the displacement
     * from declarations.json; the link parameter is opaque to it. */
    #define UPLINK(P)    void *
    int32_t  UP();               /* UP(P, name)  -- an lvalue                */
    int32_t *UPARG();            /* UPARG(P, k)  -- the parent's k'th arg    */
  #else
    #define UPLINK(P)    struct P##__frame *
    #define UP(P, name)  (__up->name)
    #define UPARG(P, k)  (__up->__a##k)
  #endif
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

/* ---- PL/I BIT literals (P37) ---------------------------------------------
 * `'001'B` is NOT constant-folded by the 1986 compiler.  It is built at run
 * time, at every evaluation, by the runtime routine X.CB @7017E708:
 *     ac2 = the destination's WORD address (a frame temp)
 *     ac0 = a byte pointer to the CHARACTER form of the literal
 *     ac1 = its length
 * followed by an embedded, undecorated `LCALL [0x7017E708],0`.  Both of the
 * two direct game call sites agree (GET_INPUT 7016AA41 with "001";
 * 701703A6 with "1").  BITS("001") is that literal in the C subset; the
 * translator refuses anything but a string literal of '0'/'1'. */
void *BITS(const char *bits);

/* PL/I string concatenation a || b (CHAR temporaries; the translator's WSTB/WCMV shapes) */
const char *CAT(const char *a, const char *b);

/* ---- runtime calls actually used by the pilot routines ---- */
/* ?WRITE_SCREEN: channel, text[, row&, col&, options]; row/col are written back. */
void WRITE_SCREEN$2(const int32_t *chan, const void *text);
void WRITE_SCREEN$5(const int32_t *chan, const void *text, int16_t *row, int16_t *col, const int16_t *opts);
/* ?RANDOM_NUMBER: uniform in lo..hi, seed updated; returns in ac0 (RTConventions). */
int32_t RANDOM_NUMBER$3(const int32_t *lo, const int32_t *hi, int32_t *seed);
/* ?READ: channel, buffer, one, count&, options, flags — GET_INPUT's only call */
void READ$6(const int32_t *chan, void *buf, const int16_t *one, int16_t *count,
            const int16_t *opts, const void *flags);
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
