pub const LUA_INTERP_BOILERPLATE: &str = include_str!("./template/helper.c");

pub const LUA_MACRO_BOILERPLATE: &str = include_str!("./template/macro.c");

pub const LUA_SETUP_BOILERPLATE: &str = include_str!("./template/setup.c");

pub const LUA_INIT_CODE: &str = "
CallInfo *const ci = L->ci;
LClosure *const cl = lua_get_l_closure(ci);
TValue const *const rt_k = cl->p->k;

checkstackGCp(L, `NUM_STACK`, ci->func);

StkId base = ci->func + 1;
";

pub const LUA_NUM_PARAM: &str = "
StkId param_end = base + `NUM_PARAM`;

while (L->top < param_end) {
	setnilvalue(s2v(L->top));
	L->top++;
}
";

pub const LUA_NUM_VARARG: &str = "int const n_vararg = cast_int(L->top - base) - `NUM_PARAM`;";
