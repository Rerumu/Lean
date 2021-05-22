use crate::{
	codegen::baked::{
		LUA_INIT_CODE, LUA_INTERP_BOILERPLATE, LUA_IS_VARARG, LUA_MACRO_BOILERPLATE, LUA_NUM_PARAM,
		LUA_SETUP_BOILERPLATE,
	},
	common::types::{Inst, Opcode, Proto, Target, Value},
	dumper::dump_lua_module,
};
use std::io::{Result, Write};

enum OpType {
	Normal,
	Skip,
	Extra,
	Control,
	Closure,
}

const fn as_op_type(op: Opcode) -> OpType {
	match op {
		Opcode::Move
		| Opcode::LoadI
		| Opcode::LoadF
		| Opcode::LoadK
		| Opcode::LoadFalse
		| Opcode::LoadTrue
		| Opcode::LoadNil
		| Opcode::GetUpval
		| Opcode::SetUpval
		| Opcode::GetTabUp
		| Opcode::GetTable
		| Opcode::GetI
		| Opcode::GetField
		| Opcode::SetTabUp
		| Opcode::SetTable
		| Opcode::SetI
		| Opcode::SetField
		| Opcode::Method
		| Opcode::MmBin
		| Opcode::MmBinI
		| Opcode::MmBinK
		| Opcode::Unm
		| Opcode::Bnot
		| Opcode::Not
		| Opcode::Len
		| Opcode::Concat
		| Opcode::Close
		| Opcode::Tbc
		| Opcode::Call
		| Opcode::TailCall
		| Opcode::Return
		| Opcode::Return0
		| Opcode::Return1
		| Opcode::TForCall
		| Opcode::Vararg
		| Opcode::VarargPrep
		| Opcode::ExtraArg
		| Opcode::Invalid => OpType::Normal,
		Opcode::AddI
		| Opcode::AddK
		| Opcode::SubK
		| Opcode::MulK
		| Opcode::ModK
		| Opcode::PowK
		| Opcode::DivK
		| Opcode::IDivK
		| Opcode::BandK
		| Opcode::BorK
		| Opcode::BxorK
		| Opcode::ShlI
		| Opcode::ShrI
		| Opcode::Add
		| Opcode::Sub
		| Opcode::Mul
		| Opcode::Mod
		| Opcode::Pow
		| Opcode::Div
		| Opcode::IDiv
		| Opcode::Band
		| Opcode::Bor
		| Opcode::Bxor
		| Opcode::Shl
		| Opcode::Shr => OpType::Skip,
		Opcode::LoadKX | Opcode::NewTable | Opcode::SetList => OpType::Extra,
		Opcode::LFalseSkip
		| Opcode::Jmp
		| Opcode::Eq
		| Opcode::Lt
		| Opcode::Le
		| Opcode::EqK
		| Opcode::EqI
		| Opcode::LtI
		| Opcode::LeI
		| Opcode::GtI
		| Opcode::GeI
		| Opcode::Test
		| Opcode::TestSet
		| Opcode::ForLoop
		| Opcode::ForPrep
		| Opcode::TForPrep
		| Opcode::TForLoop => OpType::Control,
		Opcode::Closure => OpType::Closure,
	}
}

fn assume_label(target: &Target) -> u32 {
	match target {
		Target::Label(label) => *label,
		Target::Undefined(_) => {
			panic!("undefined jumps are not supported");
		}
	}
}

fn write_instruction(w: &mut dyn Write, inst: Inst, call: &str) -> Result<()> {
	write!(w, "{:?}({:#010x}{});", inst.opcode(), inst.inner, call)
}

fn write_const_list(w: &mut dyn Write, proto: &Proto) -> Result<()> {
	let len = proto.value_list.len();

	write!(w, "static TValue const ct_k[{}] = {{", len)?;

	for v in &proto.value_list {
		match v {
			Value::Nil | Value::NoString | Value::String(_) => {
				write!(w, "{{ {{ .i = 0 }}, LUA_VNIL }}")
			}
			Value::False => {
				write!(w, "{{ {{ .i = 0 }}, LUA_VFALSE }}")
			}
			Value::True => {
				write!(w, "{{ {{ .i = 0 }}, LUA_VTRUE }}")
			}
			Value::Integer(i) => {
				write!(w, "{{ {{ .i = {} }}, LUA_VNUMINT }}", i)
			}
			Value::Number(n) => {
				let mut ns = n.to_string();

				// nasty but eh
				if !ns.contains('.') {
					ns += ".0";
				}

				write!(w, "{{ {{ .n = {} }}, LUA_VNUMFLT }}", ns)
			}
		}?;

		write!(w, ",")?;
	}

	write!(w, "}};")
}

fn write_init(w: &mut dyn Write, proto: &Proto) -> Result<()> {
	let num_stack = proto.num_stack.to_string();

	write!(w, "{}", LUA_INIT_CODE.replace("`NUM_STACK`", &num_stack))?;
	write_const_list(w, proto)?;

	match (proto.is_vararg, proto.num_param) {
		(0, _) => write!(w, "int const param_offset = 0;"),
		(_, 0) => write!(w, "int const param_offset = cast_int(L->top - base);"),
		(_, num) => {
			let num_param = num.to_string();

			write!(w, "{}", LUA_IS_VARARG.replace("`NUM_PARAM`", &num_param))
		}
	}?;

	if proto.num_param != 0 {
		let num_param = proto.num_param.to_string();

		write!(w, "{}", LUA_NUM_PARAM.replace("`NUM_PARAM`", &num_param))?;
	}

	Ok(())
}

fn write_function(w: &mut dyn Write, index: &mut usize, proto: &Proto) -> Result<()> {
	let mut child_ref = Vec::with_capacity(proto.child_list.len());
	let saved = *index;

	for child in &proto.child_list {
		*index += 1;
		child_ref.push(*index);
		write_function(w, index, child)?;
	}

	write!(w, "int lua_func_{}(lua_State* L) {{", saved)?;
	write_init(w, proto)?;

	for (i, blk) in proto.block_list.iter().enumerate() {
		writeln!(w, "label_{}:", i)?;

		let mut iter = blk.code.iter();

		while let Some(inst) = iter.next() {
			let ci = match as_op_type(inst.opcode()) {
				OpType::Normal => "".to_string(),
				OpType::Extra if inst.opcode() == Opcode::SetList && !inst.k() => ", 0".to_string(),
				OpType::Extra => {
					let tail = iter.next().expect("trailing instruction not found");

					format!(", {}", tail.ax())
				}
				OpType::Skip => {
					let tail = iter.next().expect("trailing instruction not found");

					format!(", {:?}({:#010x})", tail.opcode(), tail.inner)
				}
				OpType::Control => {
					let lbl = assume_label(&blk.target);

					format!(", label_{}, label_{}", lbl, i + 1)
				}
				OpType::Closure => {
					let index = child_ref[inst.bx() as usize];

					format!(", lua_func_{}", index)
				}
			};

			write_instruction(w, *inst, &ci)?;
		}
	}

	writeln!(w, "}}")?;
	writeln!(w)
}

fn write_call_site(w: &mut dyn Write, proto: &Proto) -> Result<()> {
	let dumped = dump_lua_module(proto)?;
	let len = dumped.len().to_string();

	write!(w, "char const* BT_GLUE = \"")?;

	for v in dumped {
		write!(w, "\\x{:02X?}", v)?;
	}

	writeln!(w, "\";")?;
	writeln!(w)?;
	write!(w, "{}", LUA_SETUP_BOILERPLATE.replace("`LENGTH`", &len))
}

pub fn transpile(w: &mut dyn Write, proto: &Proto) -> Result<()> {
	let mut index = 0;

	writeln!(w, "{}", LUA_INTERP_BOILERPLATE)?;
	writeln!(w, "{}", LUA_MACRO_BOILERPLATE)?;

	write_function(w, &mut index, proto)?;
	write_call_site(w, proto)
}
