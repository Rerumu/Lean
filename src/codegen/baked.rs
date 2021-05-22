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

pub const LUA_IS_VARARG: &str = "
int param_offset = cast_int(L->top - base);

if (`NUM_PARAM` != 0 && param_offset < `NUM_PARAM`) {
	param_offset = `NUM_PARAM`;
}
";

pub const LUA_NUM_PARAM: &str = "
for (int i = cast_int(L->top - base); i < `NUM_PARAM`; i += 1) {
	setnilvalue(s2v(base + i));
}
";
