use crate::common::{
	number::{dump_unsigned, Serde},
	types::{
		Constant, Inst, Instruction, Integer, Number, Opcode, Proto, Upvalue, Value, LUA_DATA,
		LUA_INT, LUA_MAGIC, LUA_NUM,
	},
};
use std::{
	convert::TryFrom,
	io::{Result, Write},
	mem::size_of,
};

fn dump_size_of<T>(w: &mut dyn Write) -> Result<()>
where
	T: Sized,
{
	u8::try_from(size_of::<T>())
		.expect("size of type too large")
		.ser(w)
}

fn dump_lua_header(w: &mut dyn Write) -> Result<()> {
	w.write_all(LUA_MAGIC)?;
	w.write_all(LUA_DATA)?;
	dump_size_of::<Instruction>(w)?;
	dump_size_of::<Integer>(w)?;
	dump_size_of::<Number>(w)?;
	LUA_INT.ser(w)?;
	LUA_NUM.ser(w)?;

	Ok(())
}

fn dump_integer<T>(val: T, w: &mut dyn Write) -> Result<()>
where
	T: Into<u64>,
{
	dump_unsigned(val.into(), w)
}

fn dump_string(val: &str, w: &mut dyn Write) -> Result<()> {
	dump_integer(val.len() as u64 + 1, w)?;
	w.write_all(val.as_bytes())
}

fn dump_list<T, M>(list: &[T], dump: M, w: &mut dyn Write) -> Result<()>
where
	M: Fn(&T, &mut dyn Write) -> Result<()>,
{
	dump_integer(list.len() as u64, w)?;
	list.iter().try_for_each(|v| dump(v, w))
}

fn dump_dummy_inst(w: &mut dyn Write) -> Result<()> {
	let ret_inst = Inst::from(Opcode::Return0);

	dump_integer(1_u8, w)?;
	ret_inst.inner.ser(w)
}

fn dump_constant(value: &Value, w: &mut dyn Write) -> Result<()> {
	match value {
		Value::NoString => {
			u8::from(Constant::ShortString).ser(w)?;
			dump_integer(0_u8, w)
		}
		Value::String(s) => {
			if s.len() < 40 {
				u8::from(Constant::ShortString).ser(w)?;
			} else {
				u8::from(Constant::LongString).ser(w)?;
			}

			dump_string(s, w)
		}
		_ => u8::from(Constant::Nil).ser(w),
	}
}

fn dump_upval(value: &Upvalue, w: &mut dyn Write) -> Result<()> {
	let in_stack = u8::from(value.in_stack);
	let index = value.index;

	in_stack.ser(w)?;
	index.ser(w)?;
	0_u8.ser(w)?;

	Ok(())
}

fn dump_function(proto: &Proto, w: &mut dyn Write) -> Result<()> {
	dump_integer(0_u8, w)?;
	dump_integer(0_u8, w)?;
	dump_integer(0_u8, w)?;

	proto.num_param.ser(w)?;
	proto.is_vararg.ser(w)?;
	proto.num_stack.ser(w)?;

	dump_dummy_inst(w)?;
	dump_list(&proto.value_list, dump_constant, w)?;
	dump_list(&proto.upval_list, dump_upval, w)?;
	dump_list(&proto.child_list, dump_function, w)?;

	dump_integer(0_u8, w)?;
	dump_integer(0_u8, w)?;
	dump_integer(0_u8, w)?;
	dump_integer(0_u8, w)?;

	Ok(())
}

pub fn dump_lua_module(proto: &Proto) -> Result<Vec<u8>> {
	let mut vec = Vec::new();
	let len = proto.upval_list.len();
	let nup = u8::try_from(len).expect("main function too many upvalues (> 255)");

	dump_lua_header(&mut vec)?;
	nup.ser(&mut vec)?;
	dump_function(proto, &mut vec)?;

	Ok(vec)
}
