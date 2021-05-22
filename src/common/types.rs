use crate::{ext_operand, ext_s_operand};
use bit_field::BitField;
use nom::IResult;
use num_enum::{FromPrimitive, IntoPrimitive, TryFromPrimitive};
use std::convert::TryFrom;

pub type Res<'a, T> = IResult<&'a [u8], T>;

pub type Instruction = u32;
pub type Integer = i64;
pub type Number = f64;

pub const LUA_MAGIC: &[u8] = b"\x1BLua\x54\x00";
pub const LUA_DATA: &[u8] = b"\x19\x93\r\n\x1a\n";
pub const LUA_INT: Integer = 0x5678;
pub const LUA_NUM: Number = 370.5;

#[derive(Clone, Copy)]
pub struct Inst {
	pub inner: Instruction,
}

impl Inst {
	pub fn opcode(self) -> Opcode {
		Opcode::from(u8::try_from(self.inner.get_bits(0..7)).unwrap())
	}

	pub fn k(self) -> bool {
		self.inner.get_bit(15)
	}

	ext_operand!(ax, u32, 7..32);
	ext_operand!(bx, u32, 15..32);
	ext_s_operand!(sj, i32, 7..32);
}

impl From<Opcode> for Inst {
	fn from(op: Opcode) -> Self {
		let inner = u8::from(op).into();

		Self { inner }
	}
}

impl Default for Inst {
	fn default() -> Self {
		Self::from(Opcode::Invalid)
	}
}

#[derive(TryFromPrimitive, IntoPrimitive, PartialEq, Eq)]
#[repr(u8)]
pub enum Constant {
	Nil = 0b00000,
	False = 0b00001,
	True = 0b10001,
	Integer = 0b00011,
	Number = 0b10011,
	ShortString = 0b00100,
	LongString = 0b10100,
}

#[derive(FromPrimitive, IntoPrimitive, PartialEq, Eq, Clone, Copy, Debug)]
#[repr(u8)]
pub enum Opcode {
	Move = 0,
	LoadI,
	LoadF,
	LoadK,
	LoadKX,
	LoadFalse,
	LFalseSkip,
	LoadTrue,
	LoadNil,
	GetUpval,
	SetUpval,

	GetTabUp,
	GetTable,
	GetI,
	GetField,

	SetTabUp,
	SetTable,
	SetI,
	SetField,

	NewTable,

	Method,

	AddI,

	AddK,
	SubK,
	MulK,
	ModK,
	PowK,
	DivK,
	IDivK,

	BandK,
	BorK,
	BxorK,

	ShrI,
	ShlI,

	Add,
	Sub,
	Mul,
	Mod,
	Pow,
	Div,
	IDiv,

	Band,
	Bor,
	Bxor,
	Shl,
	Shr,

	MmBin,
	MmBinI,
	MmBinK,

	Unm,
	Bnot,
	Not,
	Len,

	Concat,

	Close,
	Tbc,
	Jmp,
	Eq,
	Lt,
	Le,

	EqK,
	EqI,
	LtI,
	LeI,
	GtI,
	GeI,

	Test,
	TestSet,

	Call,
	TailCall,

	Return,
	Return0,
	Return1,

	ForLoop,
	ForPrep,

	TForPrep,
	TForCall,
	TForLoop,

	SetList,

	Closure,

	Vararg,
	VarargPrep,

	ExtraArg,

	#[num_enum(default)]
	Invalid,
}

pub struct AbsLine {
	pub pc: u32,
	pub line: u32,
}

#[derive(Clone)]
pub enum Value {
	Nil,
	False,
	True,
	Integer(Integer),
	Number(Number),
	NoString,
	String(String),
}

pub struct Local {
	pub name: Option<String>,
	pub start_pc: u32,
	pub end_pc: u32,
}

pub struct Upvalue {
	pub name: Option<String>,
	pub in_stack: bool,
	pub index: u8,
}

pub enum Target {
	Label(u32),
	Undefined(i32),
}

pub struct Block {
	pub code: Vec<Inst>,
	pub target: Target,
}

pub struct Proto {
	pub source: Option<String>,
	pub is_vararg: u8,
	pub num_stack: u8,
	pub num_param: u8,
	pub line_defined: u32,
	pub last_line_defined: u32,
	pub value_list: Vec<Value>,
	pub block_list: Vec<Block>,
	pub child_list: Vec<Proto>,
	pub upval_list: Vec<Upvalue>,
	pub rel_line_list: Vec<i8>,
	pub abs_line_list: Vec<AbsLine>,
	pub local_list: Vec<Local>,
}
