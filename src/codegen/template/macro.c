#define lua_save_top(L, ci) L->top = ci->top;

#define lua_update_base(ci) base = ci->func + 1;

#define lua_check_gc(L, c)                                                     \
  {                                                                            \
    luaC_condGC(L, { L->top = c; }, {});                                       \
    luai_threadyield(L);                                                       \
  }

#define lua_update_stack(ci)                                                   \
  lua_update_base(ci);                                                         \
  ra = RA(i)

#define lua_update_inst(baked)                                                 \
  Instruction const i = baked;                                                 \
  StkId ra = RA(i)

#define do_cond_jump(on_true, on_false)                                        \
  if (cond == GETARG_k(i))                                                     \
    goto on_false;                                                             \
  else                                                                         \
    goto on_true;

#define op_arithI(L, iop, fop, fallback)                                       \
  {                                                                            \
    TValue const *v1 = vRB(i);                                                 \
    int imm = GETARG_sC(i);                                                    \
    if (ttisinteger(v1)) {                                                     \
      lua_Integer iv1 = ivalue(v1);                                            \
      setivalue(s2v(ra), iop(L, iv1, imm));                                    \
    } else if (ttisfloat(v1)) {                                                \
      lua_Number nb = fltvalue(v1);                                            \
      lua_Number fimm = cast_num(imm);                                         \
      setfltvalue(s2v(ra), fop(L, nb, fimm));                                  \
    } else                                                                     \
      fallback                                                                 \
  }

#define op_arithf_aux(L, v1, v2, fop, fallback)                                \
  {                                                                            \
    lua_Number n1;                                                             \
    lua_Number n2;                                                             \
    if (tonumberns(v1, n1) && tonumberns(v2, n2)) {                            \
      setfltvalue(s2v(ra), fop(L, n1, n2));                                    \
    } else                                                                     \
      fallback                                                                 \
  }

#define op_arithf(L, fop, fallback)                                            \
  {                                                                            \
    TValue const *v1 = vRB(i);                                                 \
    TValue const *v2 = vRC(i);                                                 \
    op_arithf_aux(L, v1, v2, fop, fallback);                                   \
  }

#define op_arithfK(L, fop, fallback)                                           \
  {                                                                            \
    TValue const *v1 = vRB(i);                                                 \
    TValue const v2 = KC(i);                                                   \
    lua_assert(ttisnumber(&v2));                                               \
    op_arithf_aux(L, v1, &v2, fop, fallback);                                  \
  }

#define op_arith_aux(L, v1, v2, iop, fop, fallback)                            \
  {                                                                            \
    if (ttisinteger(v1) && ttisinteger(v2)) {                                  \
      lua_Integer i1 = ivalue(v1);                                             \
      lua_Integer i2 = ivalue(v2);                                             \
      setivalue(s2v(ra), iop(L, i1, i2));                                      \
    } else                                                                     \
      op_arithf_aux(L, v1, v2, fop, fallback);                                 \
  }

#define op_arith(L, iop, fop, fallback)                                        \
  {                                                                            \
    TValue *v1 = vRB(i);                                                       \
    TValue *v2 = vRC(i);                                                       \
    op_arith_aux(L, v1, v2, iop, fop, fallback);                               \
  }

#define op_arithK(L, iop, fop, fallback)                                       \
  {                                                                            \
    TValue *v1 = vRB(i);                                                       \
    TValue const v2 = KC(i);                                                   \
    lua_assert(ttisnumber(&v2));                                               \
    op_arith_aux(L, v1, &v2, iop, fop, fallback);                              \
  }

#define op_bitwiseK(L, op, fallback)                                           \
  {                                                                            \
    TValue *v1 = vRB(i);                                                       \
    TValue const v2 = KC(i);                                                   \
    lua_Integer i1;                                                            \
    lua_Integer i2 = ivalue(&v2);                                              \
    if (tointegerns(v1, &i1)) {                                                \
      setivalue(s2v(ra), op(i1, i2));                                          \
    } else                                                                     \
      fallback                                                                 \
  }

#define op_bitwise(L, op, fallback)                                            \
  {                                                                            \
    TValue *v1 = vRB(i);                                                       \
    TValue *v2 = vRC(i);                                                       \
    lua_Integer i1;                                                            \
    lua_Integer i2;                                                            \
    if (tointegerns(v1, &i1) && tointegerns(v2, &i2)) {                        \
      setivalue(s2v(ra), op(i1, i2));                                          \
    } else                                                                     \
      fallback                                                                 \
  }

#define op_order(L, opi, opn, other, on_true, on_false)                        \
  {                                                                            \
    int cond;                                                                  \
    TValue *rb = vRB(i);                                                       \
    if (ttisinteger(s2v(ra)) && ttisinteger(rb)) {                             \
      lua_Integer ia = ivalue(s2v(ra));                                        \
      lua_Integer ib = ivalue(rb);                                             \
      cond = opi(ia, ib);                                                      \
    } else if (ttisnumber(s2v(ra)) && ttisnumber(rb))                          \
      cond = opn(s2v(ra), rb);                                                 \
    else {                                                                     \
      lua_save_top(L, ci);                                                     \
      cond = other(L, s2v(ra), rb);                                            \
    }                                                                          \
    do_cond_jump(on_true, on_false);                                           \
  }

#define op_orderI(L, opi, opf, inv, tm, on_true, on_false)                     \
  {                                                                            \
    int cond;                                                                  \
    int im = GETARG_sB(i);                                                     \
    if (ttisinteger(s2v(ra)))                                                  \
      cond = opi(ivalue(s2v(ra)), im);                                         \
    else if (ttisfloat(s2v(ra))) {                                             \
      lua_Number fa = fltvalue(s2v(ra));                                       \
      lua_Number fim = cast_num(im);                                           \
      cond = opf(fa, fim);                                                     \
    } else {                                                                   \
      int isf = GETARG_C(i);                                                   \
      {                                                                        \
        lua_save_top(L, ci);                                                   \
        cond = luaT_callorderiTM(L, s2v(ra), im, inv, isf, tm);                \
      }                                                                        \
    }                                                                          \
    do_cond_jump(on_true, on_false);                                           \
  }

#define Move(baked)                                                            \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    setobjs2s(L, ra, RB(i));                                                   \
  }
#define LoadI(baked)                                                           \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    lua_Integer b = GETARG_sBx(i);                                             \
    setivalue(s2v(ra), b);                                                     \
  }
#define LoadF(baked)                                                           \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    int b = GETARG_sBx(i);                                                     \
    setfltvalue(s2v(ra), cast_num(b));                                         \
  }
#define LoadK(baked)                                                           \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    TValue const rb = FK(GETARG_Bx(i));                                        \
    setobj2s(L, ra, &rb);                                                      \
  }
#define LoadKX(baked, extra)                                                   \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    TValue const rb = FK(extra);                                               \
    setobj2s(L, ra, &rb);                                                      \
  }
#define LoadFalse(baked)                                                       \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    setbfvalue(s2v(ra));                                                       \
  }
#define LFalseSkip(baked, on_true, on_false)                                   \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    setbfvalue(s2v(ra));                                                       \
    goto on_true;                                                              \
  }
#define LoadTrue(baked)                                                        \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    setbtvalue(s2v(ra));                                                       \
  }
#define LoadNil(baked)                                                         \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    int b = GETARG_B(i);                                                       \
    do {                                                                       \
      setnilvalue(s2v(ra++));                                                  \
    } while (b--);                                                             \
  }

#define GetUpval(baked)                                                        \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    int b = GETARG_B(i);                                                       \
    setobj2s(L, ra, cl->upvals[b]->v);                                         \
  }

#define SetUpval(baked)                                                        \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    UpVal *uv = cl->upvals[GETARG_B(i)];                                       \
    setobj(L, uv->v, s2v(ra));                                                 \
    luaC_barrier(L, uv, s2v(ra));                                              \
  }

#define GetTabUp(baked)                                                        \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    const TValue *slot;                                                        \
    TValue *upval = cl->upvals[GETARG_B(i)]->v;                                \
    TValue rc = KC(i);                                                         \
    TString *key = tsvalue(&rc);                                               \
    if (luaV_fastget(L, upval, key, slot, luaH_getshortstr)) {                 \
      setobj2s(L, ra, slot);                                                   \
    } else {                                                                   \
      lua_save_top(L, ci);                                                     \
      luaV_finishget(L, upval, &rc, ra, slot);                                 \
    }                                                                          \
  }

#define GetTable(baked)                                                        \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    const TValue *slot;                                                        \
    TValue *rb = vRB(i);                                                       \
    TValue *rc = vRC(i);                                                       \
    lua_Unsigned n;                                                            \
    if (ttisinteger(rc)                                                        \
            ? (cast_void(n = ivalue(rc)), luaV_fastgeti(L, rb, n, slot))       \
            : luaV_fastget(L, rb, rc, slot, luaH_get)) {                       \
      setobj2s(L, ra, slot);                                                   \
    } else {                                                                   \
      lua_save_top(L, ci);                                                     \
      luaV_finishget(L, rb, rc, ra, slot);                                     \
    }                                                                          \
  }

#define GetI(baked)                                                            \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    const TValue *slot;                                                        \
    TValue *rb = vRB(i);                                                       \
    int c = GETARG_C(i);                                                       \
    if (luaV_fastgeti(L, rb, c, slot)) {                                       \
      setobj2s(L, ra, slot);                                                   \
    } else {                                                                   \
      TValue key;                                                              \
      setivalue(&key, c);                                                      \
      lua_save_top(L, ci);                                                     \
      luaV_finishget(L, rb, &key, ra, slot);                                   \
    }                                                                          \
  }

#define GetField(baked)                                                        \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    const TValue *slot;                                                        \
    TValue *rb = vRB(i);                                                       \
    TValue rc = KC(i);                                                         \
    TString *key = tsvalue(&rc);                                               \
    if (luaV_fastget(L, rb, key, slot, luaH_getshortstr)) {                    \
      setobj2s(L, ra, slot);                                                   \
    } else {                                                                   \
      lua_save_top(L, ci);                                                     \
      luaV_finishget(L, rb, &rc, ra, slot);                                    \
    }                                                                          \
  }

#define SetTabUp(baked)                                                        \
  {                                                                            \
    Instruction const i = baked;                                               \
    const TValue *slot;                                                        \
    TValue *upval = cl->upvals[GETARG_A(i)]->v;                                \
    TValue rb = KB(i);                                                         \
    TValue rc = RKC(i);                                                        \
    TString *key = tsvalue(&rb);                                               \
    if (luaV_fastget(L, upval, key, slot, luaH_getshortstr)) {                 \
      luaV_finishfastset(L, upval, slot, &rc);                                 \
    } else {                                                                   \
      lua_save_top(L, ci);                                                     \
      luaV_finishset(L, upval, &rb, &rc, slot);                                \
    }                                                                          \
  }

#define SetTable(baked)                                                        \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    const TValue *slot;                                                        \
    TValue *rb = vRB(i);                                                       \
    TValue rc = RKC(i);                                                        \
    lua_Unsigned n;                                                            \
    if (ttisinteger(rb)                                                        \
            ? (cast_void(n = ivalue(rb)), luaV_fastgeti(L, s2v(ra), n, slot))  \
            : luaV_fastget(L, s2v(ra), rb, slot, luaH_get)) {                  \
      luaV_finishfastset(L, s2v(ra), slot, &rc);                               \
    } else {                                                                   \
      lua_save_top(L, ci);                                                     \
      luaV_finishset(L, s2v(ra), rb, &rc, slot);                               \
    }                                                                          \
  }

#define SetI(baked)                                                            \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    const TValue *slot;                                                        \
    int c = GETARG_B(i);                                                       \
    TValue rc = RKC(i);                                                        \
    if (luaV_fastgeti(L, s2v(ra), c, slot)) {                                  \
      luaV_finishfastset(L, s2v(ra), slot, &rc);                               \
    } else {                                                                   \
      TValue key;                                                              \
      setivalue(&key, c);                                                      \
      lua_save_top(L, ci);                                                     \
      luaV_finishset(L, s2v(ra), &key, &rc, slot);                             \
    }                                                                          \
  }

#define SetField(baked)                                                        \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    const TValue *slot;                                                        \
    TValue rb = KB(i);                                                         \
    TValue rc = RKC(i);                                                        \
    TString *key = tsvalue(&rb);                                               \
    if (luaV_fastget(L, s2v(ra), key, slot, luaH_getshortstr)) {               \
      luaV_finishfastset(L, s2v(ra), slot, &rc);                               \
    } else {                                                                   \
      lua_save_top(L, ci);                                                     \
      luaV_finishset(L, s2v(ra), &rb, &rc, slot);                              \
    }                                                                          \
  }

#define NewTable(baked, extra)                                                 \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    int b = GETARG_B(i);                                                       \
    int c = GETARG_C(i);                                                       \
    Table *t;                                                                  \
    if (b > 0)                                                                 \
      b = 1 << (b - 1);                                                        \
                                                                               \
    lua_assert((!TESTARG_k(i)) == (GETARG_Ax(*pc) == 0));                      \
                                                                               \
    if (TESTARG_k(i))                                                          \
      c += extra * (MAXARG_C + 1);                                             \
                                                                               \
    L->top = ra + 1;                                                           \
    t = luaH_new(L);                                                           \
    sethvalue2s(L, ra, t);                                                     \
    if (b != 0 || c != 0)                                                      \
      luaH_resize(L, t, c, b);                                                 \
    lua_check_gc(L, ra + 1);                                                   \
  }

#define Method(baked)                                                          \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    const TValue *slot;                                                        \
    TValue *rb = vRB(i);                                                       \
    TValue rc = RKC(i);                                                        \
    TString *key = tsvalue(&rc);                                               \
    setobj2s(L, ra + 1, rb);                                                   \
    if (luaV_fastget(L, rb, key, slot, luaH_getstr)) {                         \
      setobj2s(L, ra, slot);                                                   \
    } else {                                                                   \
      lua_save_top(L, ci);                                                     \
      luaV_finishget(L, rb, &rc, ra, slot);                                    \
    }                                                                          \
  }

#define AddI(baked, fallback)                                                  \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    op_arithI(L, l_addi, luai_numadd, fallback);                               \
  }
#define AddK(baked, fallback)                                                  \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    op_arithK(L, l_addi, luai_numadd, fallback);                               \
  }
#define SubK(baked, fallback)                                                  \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    op_arithK(L, l_subi, luai_numsub, fallback);                               \
  }
#define MulK(baked, fallback)                                                  \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    op_arithK(L, l_muli, luai_nummul, fallback);                               \
  }
#define ModK(baked, fallback)                                                  \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    op_arithK(L, luaV_mod, luaV_modf, fallback);                               \
  }
#define PowK(baked, fallback)                                                  \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    op_arithfK(L, luai_numpow, fallback);                                      \
  }
#define DivK(baked, fallback)                                                  \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    op_arithfK(L, luai_numdiv, fallback);                                      \
  }
#define IDivK(baked, fallback)                                                 \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    op_arithK(L, luaV_idiv, luai_numidiv, fallback);                           \
  }
#define BandK(baked, fallback)                                                 \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    op_bitwiseK(L, l_band, fallback)                                           \
  };
#define BorK(baked, fallback)                                                  \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    op_bitwiseK(L, l_bor, fallback)                                            \
  };
#define BxorK(baked, fallback)                                                 \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    op_bitwiseK(L, l_bxor, fallback)                                           \
  };
#define ShrI(baked, fallback)                                                  \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    TValue *rb = vRB(i);                                                       \
    int ic = GETARG_sC(i);                                                     \
    lua_Integer ib;                                                            \
    if (tointegerns(rb, &ib)) {                                                \
      setivalue(s2v(ra), luaV_shiftl(ib, -ic));                                \
    } else                                                                     \
      fallback                                                                 \
  }

#define ShlI(baked, fallback)                                                  \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    TValue *rb = vRB(i);                                                       \
    int ic = GETARG_sC(i);                                                     \
    lua_Integer ib;                                                            \
    if (tointegerns(rb, &ib)) {                                                \
      setivalue(s2v(ra), luaV_shiftl(ic, ib));                                 \
    } else                                                                     \
      fallback                                                                 \
  }

#define Add(baked, fallback)                                                   \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    op_arith(L, l_addi, luai_numadd, fallback);                                \
  }
#define Sub(baked, fallback)                                                   \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    op_arith(L, l_subi, luai_numsub, fallback);                                \
  }
#define Mul(baked, fallback)                                                   \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    op_arith(L, l_muli, luai_nummul, fallback);                                \
  }
#define Mod(baked, fallback)                                                   \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    op_arith(L, luaV_mod, luaV_modf, fallback);                                \
  }
#define Pow(baked, fallback)                                                   \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    op_arithf(L, luai_numpow, fallback);                                       \
  }
#define Div(baked, fallback)                                                   \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    op_arithf(L, luai_numdiv, fallback);                                       \
  }
#define IDiv(baked, fallback)                                                  \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    op_arith(L, luaV_idiv, luai_numidiv, fallback);                            \
  }
#define Band(baked, fallback)                                                  \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    op_bitwise(L, l_band, fallback);                                           \
  }
#define Bor(baked, fallback)                                                   \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    op_bitwise(L, l_bor, fallback);                                            \
  }
#define Bxor(baked, fallback)                                                  \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    op_bitwise(L, l_bxor, fallback);                                           \
  }
#define Shl(baked, fallback)                                                   \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    op_bitwise(L, luaV_shiftr, fallback);                                      \
  }
#define Shr(baked, fallback)                                                   \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    op_bitwise(L, luaV_shiftl, fallback);                                      \
  }
#define MmBin(baked)                                                           \
  {                                                                            \
    Instruction const pi = i;                                                  \
    lua_update_inst(baked);                                                    \
    TValue *rb = vRB(i);                                                       \
    TMS tm = (TMS)GETARG_C(i);                                                 \
    StkId result = RA(pi);                                                     \
    lua_assert(OP_ADD <= GET_OPCODE(pi) && GET_OPCODE(pi) <= OP_SHR);          \
    lua_save_top(L, ci);                                                       \
    luaT_trybinTM(L, s2v(ra), rb, result, tm);                                 \
  }

#define MmBinI(baked)                                                          \
  {                                                                            \
    Instruction const pi = i;                                                  \
    lua_update_inst(baked);                                                    \
    int imm = GETARG_sB(i);                                                    \
    TMS tm = (TMS)GETARG_C(i);                                                 \
    int flip = GETARG_k(i);                                                    \
    StkId result = RA(pi);                                                     \
    lua_save_top(L, ci);                                                       \
    luaT_trybiniTM(L, s2v(ra), imm, flip, result, tm);                         \
  }

#define MmBinK(baked)                                                          \
  {                                                                            \
    Instruction const pi = i;                                                  \
    lua_update_inst(baked);                                                    \
    TValue const imm = KB(i);                                                  \
    TMS tm = (TMS)GETARG_C(i);                                                 \
    int flip = GETARG_k(i);                                                    \
    StkId result = RA(pi);                                                     \
    lua_save_top(L, ci);                                                       \
    luaT_trybinassocTM(L, s2v(ra), &imm, flip, result, tm);                    \
  }

#define Unm(baked)                                                             \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    TValue *rb = vRB(i);                                                       \
    lua_Number nb;                                                             \
    if (ttisinteger(rb)) {                                                     \
      lua_Integer ib = ivalue(rb);                                             \
      setivalue(s2v(ra), intop(-, 0, ib));                                     \
    } else if (tonumberns(rb, nb)) {                                           \
      setfltvalue(s2v(ra), luai_numunm(L, nb));                                \
    } else {                                                                   \
      lua_save_top(L, ci);                                                     \
      luaT_trybinTM(L, rb, rb, ra, TM_UNM);                                    \
    }                                                                          \
  }

#define Bnot(baked)                                                            \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    TValue *rb = vRB(i);                                                       \
    lua_Integer ib;                                                            \
    if (tointegerns(rb, &ib)) {                                                \
      setivalue(s2v(ra), intop(^, ~l_castS2U(0), ib));                         \
    } else {                                                                   \
      lua_save_top(L, ci);                                                     \
      luaT_trybinTM(L, rb, rb, ra, TM_BNOT);                                   \
    }                                                                          \
  }

#define Not(baked)                                                             \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    TValue const *rb = vRB(i);                                                 \
    if (l_isfalse(rb)) {                                                       \
      setbtvalue(s2v(ra));                                                     \
    } else {                                                                   \
      setbfvalue(s2v(ra));                                                     \
    }                                                                          \
  }

#define Len(baked)                                                             \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    lua_save_top(L, ci);                                                       \
    luaV_objlen(L, ra, vRB(i));                                                \
  }
#define Concat(baked)                                                          \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    int const n = GETARG_B(i);                                                 \
    L->top = ra + n;                                                           \
    luaV_concat(L, n);                                                         \
    lua_check_gc(L, L->top);                                                   \
  }

#define Close(baked)                                                           \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    lua_save_top(L, ci);                                                       \
    luaF_close(L, ra, LUA_OK, 1);                                              \
  }
#define Tbc(baked)                                                             \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    lua_save_top(L, ci);                                                       \
    luaF_newtbcupval(L, ra);                                                   \
  }
#define Jmp(baked, on_true, on_false) goto on_true
#define Eq(baked, on_true, on_false)                                           \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    TValue *rb = vRB(i);                                                       \
    lua_save_top(L, ci);                                                       \
    int const cond = luaV_equalobj(L, s2v(ra), rb);                            \
    do_cond_jump(on_true, on_false);                                           \
  }
#define Lt(baked, on_true, on_false)                                           \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    op_order(L, l_lti, LTnum, lessthanothers, on_true, on_false);              \
  }
#define Le(baked, on_true, on_false)                                           \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    op_order(L, l_lei, LEnum, lessequalothers, on_true, on_false);             \
  }
#define EqK(baked, on_true, on_false)                                          \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    TValue const rb = KB(i);                                                   \
    int cond = luaV_rawequalobj(s2v(ra), &rb);                                 \
    do_cond_jump(on_true, on_false);                                           \
  }
#define EqI(baked, on_true, on_false)                                          \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    int cond;                                                                  \
    int const im = GETARG_sB(i);                                               \
    if (ttisinteger(s2v(ra)))                                                  \
      cond = (ivalue(s2v(ra)) == im);                                          \
    else if (ttisfloat(s2v(ra)))                                               \
      cond = luai_numeq(fltvalue(s2v(ra)), cast_num(im));                      \
    else                                                                       \
      cond = 0;                                                                \
    do_cond_jump(on_true, on_false);                                           \
  }
#define LtI(baked, on_true, on_false)                                          \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    op_orderI(L, l_lti, luai_numlt, 0, TM_LT, on_true, on_false);              \
  }
#define LeI(baked, on_true, on_false)                                          \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    op_orderI(L, l_lei, luai_numle, 0, TM_LE, on_true, on_false);              \
  }
#define GtI(baked, on_true, on_false)                                          \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    op_orderI(L, l_gti, luai_numgt, 1, TM_LT, on_true, on_false);              \
  }
#define GeI(baked, on_true, on_false)                                          \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    op_orderI(L, l_gei, luai_numge, 1, TM_LE, on_true, on_false);              \
  }
#define Test(baked, on_true, on_false)                                         \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    int cond = !l_isfalse(s2v(ra));                                            \
    do_cond_jump(on_true, on_false);                                           \
  }
#define TestSet(baked, on_true, on_false)                                      \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    TValue *rb = vRB(i);                                                       \
    if (l_isfalse(rb) == GETARG_k(i)) {                                        \
      goto on_true;                                                            \
    } else {                                                                   \
      setobj2s(L, ra, rb);                                                     \
      goto on_false;                                                           \
    }                                                                          \
  }
#define Call(baked)                                                            \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    int b = GETARG_B(i);                                                       \
    int nresults = GETARG_C(i) - 1;                                            \
                                                                               \
    if (b != 0)                                                                \
      L->top = ra + b;                                                         \
                                                                               \
    CallInfo *post = luaD_precall(L, ra, nresults);                            \
                                                                               \
    if (post != NULL) {                                                        \
      post->callstatus = CIST_FRESH;                                           \
      luaV_execute(L, post);                                                   \
    }                                                                          \
                                                                               \
    lua_update_base(ci);                                                       \
  }

#define TailCall(baked)                                                        \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    int b = GETARG_B(i);                                                       \
                                                                               \
    if (b != 0)                                                                \
      L->top = ra + b;                                                         \
    else                                                                       \
      b = cast_int(L->top - ra);                                               \
                                                                               \
    if (TESTARG_k(i)) {                                                        \
      luaF_closeupval(L, base);                                                \
      lua_assert(L->tbclist < base);                                           \
      lua_assert(base == ci->func + 1);                                        \
    }                                                                          \
                                                                               \
    while (!ttisfunction(s2v(ra))) {                                           \
      luaD_tryfuncTM(L, ra);                                                   \
      b++;                                                                     \
      checkstackGCp(L, 1, ra);                                                 \
    }                                                                          \
                                                                               \
    if (GETARG_C(i)) {                                                         \
      ci->func -= param_offset;                                                \
    }                                                                          \
                                                                               \
    if (ttisLclosure(s2v(ra))) {                                               \
      L->nCcalls -= 1;                                                         \
      luaD_pretailcall(L, ci, ra, b);                                          \
      luaD_call(L, ci->func, LUA_MULTRET);                                     \
      L->nCcalls += 1;                                                         \
    } else {                                                                   \
      luaA_pretailcall(L, ci, ra, b);                                          \
      luaD_precall(L, ra, LUA_MULTRET);                                        \
    }                                                                          \
                                                                               \
    return LUA_MULTRET;                                                        \
  }

#define Return(baked)                                                          \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    int n = GETARG_B(i) - 1;                                                   \
    if (n < 0)                                                                 \
      n = cast_int(L->top - ra);                                               \
                                                                               \
    if (TESTARG_k(i)) {                                                        \
      if (L->top < ci->top)                                                    \
        L->top = ci->top;                                                      \
      luaF_close(L, base, CLOSEKTOP, 1);                                       \
      lua_update_stack(ci);                                                    \
    }                                                                          \
                                                                               \
    if (GETARG_C(i)) {                                                         \
      ci->func -= param_offset;                                                \
    }                                                                          \
                                                                               \
    L->top = ra + n;                                                           \
    luaD_poscall(L, ci, n);                                                    \
                                                                               \
    return n;                                                                  \
  }
#define Return0(baked)                                                         \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    if (L->hookmask) {                                                         \
      L->top = ra;                                                             \
      luaD_poscall(L, ci, 0);                                                  \
    } else {                                                                   \
      int nres;                                                                \
      L->ci = ci->previous;                                                    \
      L->top = base - 1;                                                       \
      for (nres = ci->nresults; nres > 0; nres--)                              \
        setnilvalue(s2v(L->top++));                                            \
    }                                                                          \
                                                                               \
    return 0;                                                                  \
  }
#define Return1(baked)                                                         \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    if (L->hookmask) {                                                         \
      L->top = ra + 1;                                                         \
      luaD_poscall(L, ci, 1);                                                  \
    } else {                                                                   \
      int nres = ci->nresults;                                                 \
      L->ci = ci->previous;                                                    \
      if (nres == 0)                                                           \
        L->top = base - 1;                                                     \
      else {                                                                   \
        setobjs2s(L, base - 1, ra);                                            \
        L->top = base;                                                         \
        for (; nres > 1; nres--)                                               \
          setnilvalue(s2v(L->top++));                                          \
      }                                                                        \
    }                                                                          \
                                                                               \
    return 1;                                                                  \
  }
#define ForLoop(baked, on_true, on_false)                                      \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    if (ttisinteger(s2v(ra + 2))) {                                            \
      lua_Unsigned count = l_castS2U(ivalue(s2v(ra + 1)));                     \
      if (count > 0) {                                                         \
        lua_Integer step = ivalue(s2v(ra + 2));                                \
        lua_Integer idx = ivalue(s2v(ra));                                     \
        chgivalue(s2v(ra + 1), count - 1);                                     \
        idx = intop(+, idx, step);                                             \
        chgivalue(s2v(ra), idx);                                               \
        setivalue(s2v(ra + 3), idx);                                           \
        goto on_true;                                                          \
      }                                                                        \
    } else if (floatforloop(ra))                                               \
      goto on_true;                                                            \
                                                                               \
    goto on_false;                                                             \
  }
#define ForPrep(baked, on_true, on_false)                                      \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    lua_save_top(L, ci);                                                       \
    if (forprep(L, ra))                                                        \
      goto on_true;                                                            \
    else                                                                       \
      goto on_false;                                                           \
  }
#define TForPrep(baked, on_true, on_false)                                     \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    lua_save_top(L, ci);                                                       \
    luaF_newtbcupval(L, ra + 3);                                               \
                                                                               \
    goto on_true;                                                              \
  }
#define TForCall(baked)                                                        \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    memcpy(ra + 4, ra, 3 * sizeof(*ra));                                       \
    L->top = ra + 4 + 3;                                                       \
    luaD_call(L, ra + 4, GETARG_C(i));                                         \
    lua_update_stack(ci);                                                      \
  }
#define TForLoop(baked, on_true, on_false)                                     \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    if (ttisnil(s2v(ra + 4))) {                                                \
      goto on_false;                                                           \
    } else {                                                                   \
      setobjs2s(L, ra + 2, ra + 4);                                            \
      goto on_true;                                                            \
    }                                                                          \
  }
#define SetList(baked, extra)                                                  \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    int n = GETARG_B(i);                                                       \
    unsigned int last = GETARG_C(i);                                           \
    Table *h = hvalue(s2v(ra));                                                \
    if (n == 0)                                                                \
      n = cast_int(L->top - ra) - 1;                                           \
    else                                                                       \
      L->top = ci->top;                                                        \
    last += n;                                                                 \
                                                                               \
    if (TESTARG_k(i)) {                                                        \
      last += extra * (MAXARG_C + 1);                                          \
    }                                                                          \
                                                                               \
    if (last > luaH_realasize(h))                                              \
      luaH_resizearray(L, h, last);                                            \
    for (; n > 0; n--) {                                                       \
      TValue *val = s2v(ra + n);                                               \
      setobj2t(L, &h->array[last - 1], val);                                   \
      last--;                                                                  \
      luaC_barrierback(L, obj2gco(h), val);                                    \
    }                                                                          \
  }

#define Closure(baked, native)                                                 \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    Proto *p = cl->p->p[GETARG_Bx(i)];                                         \
                                                                               \
    lua_save_top(L, ci);                                                       \
    pushclosure(L, p, cl->upvals, base, ra);                                   \
    luaA_wrap_closure(L, ra, native);                                          \
                                                                               \
    lua_check_gc(L, ra + 1);                                                   \
  }

#define Vararg(baked)                                                          \
  {                                                                            \
    lua_update_inst(baked);                                                    \
    int n = GETARG_C(i) - 1;                                                   \
    lua_save_top(L, ci);                                                       \
    luaT_getvarargs(L, ci, ra, n);                                             \
  }

#define VarargPrep(baked)                                                      \
  {                                                                            \
    luaA_adjustvarargs(L, GETARG_A(baked), ci, cl->p);                         \
    lua_update_base(ci);                                                       \
  }

#define ExtraArg(baked) lua_assert(0)
#define Invalid(baked) lua_assert(0)
