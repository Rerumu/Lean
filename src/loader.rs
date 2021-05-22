use crate::{
	common::{
		number::{load_unsigned, Serde},
		types::{
			AbsLine, Constant, Inst, Instruction, Integer, Local, Number, Proto, Res, Upvalue,
			Value, LUA_DATA, LUA_INT, LUA_MAGIC, LUA_NUM,
		},
	},
	splitter::Splitter,
};
use nom::{
	bytes::complete::{tag, take},
	combinator::{map, map_res, verify},
	multi::length_count,
	number::complete::u8,
};
use std::{convert::TryFrom, mem::size_of};

pub fn verify_size_of<T>(input: &[u8]) -> Res<u8> {
	verify(u8, |&v| size_of::<T>() == usize::from(v))(input)
}

fn verify_lua_header(input: &[u8]) -> Res<()> {
	let (input, _) = tag(LUA_MAGIC)(input)?;
	let (input, _) = tag(LUA_DATA)(input)?;
	let (input, _) = verify_size_of::<Instruction>(input)?;
	let (input, _) = verify_size_of::<Integer>(input)?;
	let (input, _) = verify_size_of::<Number>(input)?;
	let (input, _) = verify(Integer::deser, |&v| v == LUA_INT)(input)?;
	let (input, _) = verify(Number::deser, |&v| v == LUA_NUM)(input)?;

	Ok((input, ()))
}

fn load_t<T>(input: &[u8]) -> Res<T>
where
	T: TryFrom<u64>,
{
	map_res(load_unsigned, T::try_from)(input)
}

fn load_string_opt(input: &[u8]) -> Res<Option<String>> {
	let (input, len) = load_t::<u32>(input)?;

	if len == 0 {
		return Ok((input, None));
	}

	let (input, inner) = take(len - 1)(input)?;
	let value = String::from_utf8_lossy(inner).to_string();

	Ok((input, Some(value)))
}

fn load_string(input: &[u8]) -> Res<Value> {
	map(load_string_opt, |s| match s {
		Some(s) => Value::String(s),
		None => Value::NoString,
	})(input)
}

fn load_list<T, F>(func: F) -> impl Fn(&[u8]) -> Res<Vec<T>>
where
	F: Fn(&[u8]) -> Res<T> + Copy,
{
	move |input| length_count(load_t::<u32>, func)(input)
}

fn load_instruction(input: &[u8]) -> Res<Inst> {
	map(Instruction::deser, |inner| Inst { inner })(input)
}

fn load_constant(input: &[u8]) -> Res<Value> {
	let (input, tag) = map_res(u8, Constant::try_from)(input)?;
	let (input, value) = match tag {
		Constant::Nil => (input, Value::Nil),
		Constant::False => (input, Value::False),
		Constant::True => (input, Value::True),
		Constant::Integer => map(Integer::deser, Value::Integer)(input)?,
		Constant::Number => map(Number::deser, Value::Number)(input)?,
		Constant::ShortString | Constant::LongString => load_string(input)?,
	};

	Ok((input, value))
}

fn load_upvalue(input: &[u8]) -> Res<Upvalue> {
	let (input, in_stack) = u8(input)?;
	let (input, index) = u8(input)?;
	let (input, _) = u8(input)?; // kind is unused
	let result = Upvalue {
		name: None,
		in_stack: in_stack != 0,
		index,
	};

	Ok((input, result))
}

fn load_abs_line_info(input: &[u8]) -> Res<AbsLine> {
	let (input, pc) = load_t::<u32>(input)?;
	let (input, line) = load_t::<u32>(input)?;
	let result = AbsLine { pc, line };

	Ok((input, result))
}

fn load_local(input: &[u8]) -> Res<Local> {
	let (input, name) = load_string_opt(input)?;
	let (input, start_pc) = load_t::<u32>(input)?;
	let (input, end_pc) = load_t::<u32>(input)?;
	let result = Local {
		name,
		start_pc,
		end_pc,
	};

	Ok((input, result))
}

fn load_function(input: &[u8]) -> Res<Proto> {
	let (input, source) = load_string_opt(input)?;
	let (input, line_defined) = load_t::<u32>(input)?;
	let (input, last_line_defined) = load_t::<u32>(input)?;

	// metadata
	let (input, num_param) = u8(input)?;
	let (input, is_vararg) = u8(input)?;
	let (input, num_stack) = u8(input)?;

	// essential
	let (input, inst_list) = load_list(load_instruction)(input)?;
	let (input, value_list) = load_list(load_constant)(input)?;
	let (input, mut upval_list) = load_list(load_upvalue)(input)?;
	let (input, child_list) = load_list(load_function)(input)?;

	// debug
	let (input, rel_line_list) = load_list(i8::deser)(input)?;
	let (input, abs_line_list) = load_list(load_abs_line_info)(input)?;
	let (input, local_list) = load_list(load_local)(input)?;
	let (input, name_list) = load_list(load_string_opt)(input)?;

	let block_list = Splitter::new().split(inst_list);

	for (upv, name) in upval_list.iter_mut().zip(name_list) {
		upv.name = name;
	}

	let result = Proto {
		source,
		is_vararg,
		num_stack,
		num_param,
		line_defined,
		last_line_defined,
		value_list,
		block_list,
		child_list,
		upval_list,
		rel_line_list,
		abs_line_list,
		local_list,
	};

	Ok((input, result))
}

pub fn load_lua_module(input: &[u8]) -> Res<Proto> {
	let (input, _) = verify_lua_header(input)?;
	let (input, _) = u8(input)?; // upvalues :)?

	load_function(input)
}
