use crate::common::types::Res;
use nom::{
	bytes::complete::take_while_m_n,
	combinator::{map, verify},
	number::{complete, Endianness},
};
use std::{
	convert::TryFrom,
	io::{Result, Write},
	iter::once,
	mem::size_of,
};

pub type Unsigned = u64;

const ENDIANNESS: Endianness = Endianness::Little;
const TAIL_LEN: usize = size_of::<Unsigned>() * 8 / 7 - 1;
const UNSG_LEN: usize = TAIL_LEN + 1;

macro_rules! impl_serde {
	($t:ty, $func:expr) => {
		impl Serde for $t {
			fn ser(self, w: &mut dyn Write) -> Result<()> {
				match ENDIANNESS {
					Endianness::Big => w.write_all(&self.to_be_bytes()),
					Endianness::Little => w.write_all(&self.to_le_bytes()),
					Endianness::Native => w.write_all(&self.to_ne_bytes()),
				}
			}

			fn deser(input: &[u8]) -> Res<Self> {
				$func(ENDIANNESS)(input)
			}
		}
	};
}

pub fn dump_unsigned(mut val: Unsigned, w: &mut dyn Write) -> Result<()> {
	let mut result = [0x80; UNSG_LEN];

	for v in result.iter_mut().rev() {
		*v = u8::try_from(val & 0x7F).unwrap();
		val >>= 7;

		if val == 0 {
			break;
		}
	}

	let start = result.iter().position(|v| v & 0x80 == 0).unwrap();

	result[UNSG_LEN - 1] |= 0x80;

	w.write_all(&result[start..])
}

pub fn load_unsigned(input: &[u8]) -> Res<Unsigned> {
	let (input, tail) = take_while_m_n(0, TAIL_LEN, |v| v & 0x80 == 0)(input)?;
	let (input, head) = map(verify(u8::deser, |v| v & 0x80 != 0), |v| v & 0x7F)(input)?;
	let result = tail
		.iter()
		.copied()
		.chain(once(head))
		.fold(0, |acc, x| acc << 7 | Unsigned::from(x));

	Ok((input, result))
}

pub trait Serde
where
	Self: Sized,
{
	fn ser(self, w: &mut dyn Write) -> Result<()>;
	fn deser(input: &[u8]) -> Res<Self>;
}

impl Serde for i8 {
	fn ser(self, w: &mut dyn Write) -> Result<()> {
		w.write_all(&self.to_le_bytes())
	}

	fn deser(input: &[u8]) -> Res<Self> {
		complete::i8(input)
	}
}

impl Serde for u8 {
	fn ser(self, w: &mut dyn Write) -> Result<()> {
		w.write_all(&self.to_le_bytes())
	}

	fn deser(input: &[u8]) -> Res<Self> {
		complete::u8(input)
	}
}

impl_serde!(f32, complete::f32);
impl_serde!(f64, complete::f64);
impl_serde!(i128, complete::i128);
impl_serde!(i16, complete::i16);
impl_serde!(i32, complete::i32);
impl_serde!(i64, complete::i64);
impl_serde!(u128, complete::u128);
impl_serde!(u16, complete::u16);
impl_serde!(u32, complete::u32);
impl_serde!(u64, complete::u64);
