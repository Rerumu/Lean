#define lua_c
#define LUA_CORE

#include "lprefix.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"

#include "lauxlib.h"
#include "ldebug.h"
#include "lfunc.h"
#include "lgc.h"
#include "lobject.h"
#include "lopcodes.h"
#include "ltable.h"
#include "lualib.h"
#include "lvm.h"

/*
** 'l_intfitsf' checks whether a given integer is in the range that
** can be converted to a float without rounding. Used in comparisons.
*/

/* number of bits in the mantissa of a float */
#define NBM (l_floatatt(MANT_DIG))

/*
** Check whether some integers may not fit in a float, testing whether
** (maxinteger >> NBM) > 0. (That implies (1 << NBM) <= maxinteger.)
** (The shifts are done in parts, to avoid shifting by more than the size
** of an integer. In a worst case, NBM == 113 for long double and
** sizeof(long) == 32.)
*/
#if ((((LUA_MAXINTEGER >> (NBM / 4)) >> (NBM / 4)) >> (NBM / 4)) >>            \
     (NBM - (3 * (NBM / 4)))) > 0

/* limit for integers that fit in a float */
#define MAXINTFITSF ((lua_Unsigned)1 << NBM)

/* check whether 'i' is in the interval [-MAXINTFITSF, MAXINTFITSF] */
#define l_intfitsf(i) ((MAXINTFITSF + l_castS2U(i)) <= (2 * MAXINTFITSF))

#else /* all integers fit in a float precisely */

#define l_intfitsf(i) 1

#endif

/*
** Compare two strings 'ls' x 'rs', returning an integer less-equal-
** -greater than zero if 'ls' is less-equal-greater than 'rs'.
** The code is a little tricky because it allows '\0' in the strings
** and it uses 'strcoll' (to respect locales) for each segments
** of the strings.
*/
static int l_strcmp(const TString *ls, const TString *rs) {
  const char *l = getstr(ls);
  size_t ll = tsslen(ls);
  const char *r = getstr(rs);
  size_t lr = tsslen(rs);
  for (;;) { /* for each segment */
    int temp = strcoll(l, r);
    if (temp != 0)                  /* not equal? */
      return temp;                  /* done */
    else {                          /* strings are equal up to a '\0' */
      size_t len = strlen(l);       /* index of first '\0' in both strings */
      if (len == lr)                /* 'rs' is finished? */
        return (len == ll) ? 0 : 1; /* check 'ls' */
      else if (len == ll)           /* 'ls' is finished? */
        return -1; /* 'ls' is less than 'rs' ('rs' is not finished) */
      /* both strings longer than 'len'; go on comparing after the '\0' */
      len++;
      l += len;
      ll -= len;
      r += len;
      lr -= len;
    }
  }
}

/*
** Check whether integer 'i' is less than float 'f'. If 'i' has an
** exact representation as a float ('l_intfitsf'), compare numbers as
** floats. Otherwise, use the equivalence 'i < f <=> i < ceil(f)'.
** If 'ceil(f)' is out of integer range, either 'f' is greater than
** all integers or less than all integers.
** (The test with 'l_intfitsf' is only for performance; the else
** case is correct for all values, but it is slow due to the conversion
** from float to int.)
** When 'f' is NaN, comparisons must result in false.
*/
static int LTintfloat(lua_Integer i, lua_Number f) {
  if (l_intfitsf(i))
    return luai_numlt(cast_num(i), f); /* compare them as floats */
  else {                               /* i < f <=> i < ceil(f) */
    lua_Integer fi;
    if (luaV_flttointeger(f, &fi, F2Iceil)) /* fi = ceil(f) */
      return i < fi;                        /* compare them as integers */
    else            /* 'f' is either greater or less than all integers */
      return f > 0; /* greater? */
  }
}

/*
** Check whether integer 'i' is less than or equal to float 'f'.
** See comments on previous function.
*/
static int LEintfloat(lua_Integer i, lua_Number f) {
  if (l_intfitsf(i))
    return luai_numle(cast_num(i), f); /* compare them as floats */
  else {                               /* i <= f <=> i <= floor(f) */
    lua_Integer fi;
    if (luaV_flttointeger(f, &fi, F2Ifloor)) /* fi = floor(f) */
      return i <= fi;                        /* compare them as integers */
    else            /* 'f' is either greater or less than all integers */
      return f > 0; /* greater? */
  }
}

/*
** Check whether float 'f' is less than integer 'i'.
** See comments on previous function.
*/
static int LTfloatint(lua_Number f, lua_Integer i) {
  if (l_intfitsf(i))
    return luai_numlt(f, cast_num(i)); /* compare them as floats */
  else {                               /* f < i <=> floor(f) < i */
    lua_Integer fi;
    if (luaV_flttointeger(f, &fi, F2Ifloor)) /* fi = floor(f) */
      return fi < i;                         /* compare them as integers */
    else            /* 'f' is either greater or less than all integers */
      return f < 0; /* less? */
  }
}

/*
** Check whether float 'f' is less than or equal to integer 'i'.
** See comments on previous function.
*/
static int LEfloatint(lua_Number f, lua_Integer i) {
  if (l_intfitsf(i))
    return luai_numle(f, cast_num(i)); /* compare them as floats */
  else {                               /* f <= i <=> ceil(f) <= i */
    lua_Integer fi;
    if (luaV_flttointeger(f, &fi, F2Iceil)) /* fi = ceil(f) */
      return fi <= i;                       /* compare them as integers */
    else            /* 'f' is either greater or less than all integers */
      return f < 0; /* less? */
  }
}

/*
** Return 'l < r', for numbers.
*/
static int LTnum(const TValue *l, const TValue *r) {
  lua_assert(ttisnumber(l) && ttisnumber(r));
  if (ttisinteger(l)) {
    lua_Integer li = ivalue(l);
    if (ttisinteger(r))
      return li < ivalue(r);              /* both are integers */
    else                                  /* 'l' is int and 'r' is float */
      return LTintfloat(li, fltvalue(r)); /* l < r ? */
  } else {
    lua_Number lf = fltvalue(l); /* 'l' must be float */
    if (ttisfloat(r))
      return luai_numlt(lf, fltvalue(r)); /* both are float */
    else                                  /* 'l' is float and 'r' is int */
      return LTfloatint(lf, ivalue(r));
  }
}

/*
** Return 'l <= r', for numbers.
*/
static int LEnum(const TValue *l, const TValue *r) {
  lua_assert(ttisnumber(l) && ttisnumber(r));
  if (ttisinteger(l)) {
    lua_Integer li = ivalue(l);
    if (ttisinteger(r))
      return li <= ivalue(r);             /* both are integers */
    else                                  /* 'l' is int and 'r' is float */
      return LEintfloat(li, fltvalue(r)); /* l <= r ? */
  } else {
    lua_Number lf = fltvalue(l); /* 'l' must be float */
    if (ttisfloat(r))
      return luai_numle(lf, fltvalue(r)); /* both are float */
    else                                  /* 'l' is float and 'r' is int */
      return LEfloatint(lf, ivalue(r));
  }
}

/*
** return 'l < r' for non-numbers.
*/
static int lessthanothers(lua_State *L, const TValue *l, const TValue *r) {
  lua_assert(!ttisnumber(l) || !ttisnumber(r));
  if (ttisstring(l) && ttisstring(r)) /* both are strings? */
    return l_strcmp(tsvalue(l), tsvalue(r)) < 0;
  else
    return luaT_callorderTM(L, l, r, TM_LT);
}

/*
** return 'l <= r' for non-numbers.
*/
static int lessequalothers(lua_State *L, const TValue *l, const TValue *r) {
  lua_assert(!ttisnumber(l) || !ttisnumber(r));
  if (ttisstring(l) && ttisstring(r)) /* both are strings? */
    return l_strcmp(tsvalue(l), tsvalue(r)) <= 0;
  else
    return luaT_callorderTM(L, l, r, TM_LE);
}

/*
** Try to convert a 'for' limit to an integer, preserving the semantics
** of the loop. Return true if the loop must not run; otherwise, '*p'
** gets the integer limit.
** (The following explanation assumes a positive step; it is valid for
** negative steps mutatis mutandis.)
** If the limit is an integer or can be converted to an integer,
** rounding down, that is the limit.
** Otherwise, check whether the limit can be converted to a float. If
** the float is too large, clip it to LUA_MAXINTEGER.  If the float
** is too negative, the loop should not run, because any initial
** integer value is greater than such limit; so, the function returns
** true to signal that. (For this latter case, no integer limit would be
** correct; even a limit of LUA_MININTEGER would run the loop once for
** an initial value equal to LUA_MININTEGER.)
*/
static int forlimit(lua_State *L, lua_Integer init, const TValue *lim,
                    lua_Integer *p, lua_Integer step) {
  if (!luaV_tointeger(lim, p, (step < 0 ? F2Iceil : F2Ifloor))) {
    /* not coercible to in integer */
    lua_Number flim;           /* try to convert to float */
    if (!tonumber(lim, &flim)) /* cannot convert to float? */
      luaG_forerror(L, lim, "limit");
    /* else 'flim' is a float out of integer bounds */
    if (luai_numlt(0, flim)) { /* if it is positive, it is too large */
      if (step < 0)
        return 1;          /* initial value must be less than it */
      *p = LUA_MAXINTEGER; /* truncate */
    } else {               /* it is less than min integer */
      if (step > 0)
        return 1;          /* initial value must be greater than it */
      *p = LUA_MININTEGER; /* truncate */
    }
  }
  return (step > 0 ? init > *p : init < *p); /* not to run? */
}

/*
** Prepare a numerical for loop (opcode OP_FORPREP).
** Return true to skip the loop. Otherwise,
** after preparation, stack will be as follows:
**   ra : internal index (safe copy of the control variable)
**   ra + 1 : loop counter (integer loops) or limit (float loops)
**   ra + 2 : step
**   ra + 3 : control variable
*/
static int forprep(lua_State *L, StkId ra) {
  TValue *pinit = s2v(ra);
  TValue *plimit = s2v(ra + 1);
  TValue *pstep = s2v(ra + 2);
  if (ttisinteger(pinit) && ttisinteger(pstep)) { /* integer loop? */
    lua_Integer init = ivalue(pinit);
    lua_Integer step = ivalue(pstep);
    lua_Integer limit;
    if (step == 0)
      luaG_runerror(L, "'for' step is zero");
    setivalue(s2v(ra + 3), init); /* control variable */
    if (forlimit(L, init, plimit, &limit, step))
      return 1; /* skip the loop */
    else {      /* prepare loop counter */
      lua_Unsigned count;
      if (step > 0) { /* ascending loop? */
        count = l_castS2U(limit) - l_castS2U(init);
        if (step != 1) /* avoid division in the too common case */
          count /= l_castS2U(step);
      } else { /* step < 0; descending loop */
        count = l_castS2U(init) - l_castS2U(limit);
        /* 'step+1' avoids negating 'mininteger' */
        count /= l_castS2U(-(step + 1)) + 1u;
      }
      /* store the counter in place of the limit (which won't be
         needed anymore) */
      setivalue(plimit, l_castU2S(count));
    }
  } else { /* try making all values floats */
    lua_Number init;
    lua_Number limit;
    lua_Number step;
    if (!tonumber(plimit, &limit))
      luaG_forerror(L, plimit, "limit");
    if (!tonumber(pstep, &step))
      luaG_forerror(L, pstep, "step");
    if (!tonumber(pinit, &init))
      luaG_forerror(L, pinit, "initial value");
    if (step == 0)
      luaG_runerror(L, "'for' step is zero");
    if (luai_numlt(0, step) ? luai_numlt(limit, init) : luai_numlt(init, limit))
      return 1; /* skip the loop */
    else {
      /* make sure internal values are all floats */
      setfltvalue(plimit, limit);
      setfltvalue(pstep, step);
      setfltvalue(s2v(ra), init);     /* internal index */
      setfltvalue(s2v(ra + 3), init); /* control variable */
    }
  }
  return 0;
}

/*
** Execute a step of a float numerical for loop, returning
** true iff the loop must continue. (The integer case is
** written online with opcode OP_FORLOOP, for performance.)
*/
static int floatforloop(StkId ra) {
  lua_Number step = fltvalue(s2v(ra + 2));
  lua_Number limit = fltvalue(s2v(ra + 1));
  lua_Number idx = fltvalue(s2v(ra)); /* internal index */
  idx = luai_numadd(L, idx, step);    /* increment index */
  if (luai_numlt(0, step) ? luai_numle(idx, limit) : luai_numle(limit, idx)) {
    chgfltvalue(s2v(ra), idx);     /* update internal index */
    setfltvalue(s2v(ra + 3), idx); /* and control variable */
    return 1;                      /* jump back */
  } else
    return 0; /* finish the loop */
}

/*
** Shift left operation. (Shift right just negates 'y'.)
*/
#define luaV_shiftr(x, y) luaV_shiftl(x, -(y))

/*
** create a new Lua closure, push it in the stack, and initialize
** its upvalues.
*/
static void pushclosure(lua_State *L, Proto *p, UpVal **encup, StkId base,
                        StkId ra) {
  int nup = p->sizeupvalues;
  Upvaldesc *uv = p->upvalues;
  int i;
  LClosure *ncl = luaF_newLclosure(L, nup);
  ncl->p = p;
  setclLvalue2s(L, ra, ncl);  /* anchor new closure in stack */
  for (i = 0; i < nup; i++) { /* fill in its upvalues */
    if (uv[i].instack)        /* upvalue refers to local variable? */
      ncl->upvals[i] = luaF_findupval(L, base + uv[i].idx);
    else /* get upvalue from enclosing function */
      ncl->upvals[i] = encup[uv[i].idx];
    luaC_objbarrier(L, ncl, ncl->upvals[i]);
  }
}

/*
** {==================================================================
** Macros for arithmetic/bitwise/comparison opcodes in 'luaV_execute'
** ===================================================================
*/

#define l_addi(L, a, b) intop(+, a, b)
#define l_subi(L, a, b) intop(-, a, b)
#define l_muli(L, a, b) intop(*, a, b)
#define l_band(a, b) intop(&, a, b)
#define l_bor(a, b) intop(|, a, b)
#define l_bxor(a, b) intop(^, a, b)

#define l_lti(a, b) (a < b)
#define l_lei(a, b) (a <= b)
#define l_gti(a, b) (a > b)
#define l_gei(a, b) (a >= b)

#define FK(o) (ttisnil(ct_k + o) ? rt_k[o] : ct_k[o])
#define RA(i) (base + GETARG_A(i))
#define RB(i) (base + GETARG_B(i))
#define vRB(i) s2v(RB(i))
#define KB(i) FK(GETARG_B(i))
#define RC(i) (base + GETARG_C(i))
#define vRC(i) s2v(RC(i))
#define KC(i) FK(GETARG_C(i))
#define RKC(i) ((TESTARG_k(i)) ? KC(i) : *s2v(RC(i)))

LClosure *lua_get_l_closure(CallInfo *ci) {
  TValue func = clCvalue(s2v(ci->func))->upvalue[0];

  return clLvalue(&func);
}

// custom adjustment for C functions
void luaA_set_varargs(lua_State *L, CallInfo *ci, int param, int stack) {
  int actual = cast_int(L->top - ci->func) - 1;

  luaD_checkstack(L, stack + 1);
  setobjs2s(L, L->top++, ci->func);

  for (int i = 1; i <= param; i += 1) {
    setobjs2s(L, L->top++, ci->func + i);
    setnilvalue(s2v(ci->func + i));
  }

  ci->func += actual + 1;
  ci->top += actual + 1;
  lua_assert(L->top <= ci->top && ci->top <= L->stack_last);
}

void luaA_get_varargs(lua_State *L, CallInfo *ci, StkId where, int num,
                      int varg) {
  if (num < 0) {
    num = varg;                    /* get all extra arguments available */
    checkstackGCp(L, varg, where); /* ensure stack space */
    L->top = where + varg;         /* next instruction will need top */
  }

  int i;

  for (i = 0; i < num && i < varg; i++)
    setobjs2s(L, where + i, ci->func - varg + i);

  for (; i < num; i++) /* complete required results with nil */
    setnilvalue(s2v(where + i));
}

// custom tail call for C functions
void luaA_pretailcall(lua_State *L, CallInfo *ci, StkId func, int args) {
  for (int i = 0; i < args; i += 1) {
    setobjs2s(L, ci->func + i, func + i);
  }

  ci->top = func + args;
  L->top = ci->top;
}

// custom function wrapping for Lua functions
void luaA_wrap_closure(lua_State *L, StkId dummy, lua_CFunction native) {
  lua_lock(L);
  CClosure *cl = luaF_newCclosure(L, 1);

  cl->f = native;
  setobj2n(L, &cl->upvalue[0], s2v(dummy));
  setclCvalue(L, s2v(dummy), cl);

  luaC_checkGC(L);
  lua_unlock(L);
}
