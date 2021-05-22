#define lua_c
#define LUA_CORE

#include "lprefix.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"

#include "lauxlib.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lualib.h"
#include "lvm.h"

/* limit for table tag-method chains (to avoid infinite loops) */
#define MAXTAGLOOP 2000

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

// custom adjustment for C functions
void luaA_adjustvarargs(lua_State *L, int num, CallInfo *ci, const Proto *p) {
  int actual = cast_int(L->top - ci->func) - 1;

  luaD_checkstack(L, p->maxstacksize + 1);
  setobjs2s(L, L->top++, ci->func);

  for (int i = 1; i <= num; i += 1) {
    setobjs2s(L, L->top++, ci->func + i);
    setnilvalue(s2v(ci->func + i));
  }

  ci->func += actual + 1;
  ci->top += actual + 1;
  lua_assert(L->top <= ci->top && ci->top <= L->stack_last);
}

// custom tail call for C functions
void luaA_pretailcall(lua_State *L, CallInfo *ci, StkId func, int nargs) {
  for (int i = 0; i < nargs; i += 1) {
    setobjs2s(L, ci->func + i, func + i);
  }

  ci->top = ci->func + nargs;
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

LClosure *lua_get_l_closure(CallInfo *ci) {
  TValue func = clCvalue(s2v(ci->func))->upvalue[0];

  return clLvalue(&func);
}
