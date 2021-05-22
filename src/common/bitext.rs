#[macro_export]
macro_rules! ext_operand {
	($read:ident, $ret:ty, $range:expr) => {
		pub fn $read(self) -> $ret {
			use bit_field::BitField;

			self.inner.get_bits($range) as $ret
		}
	};
}

#[macro_export]
macro_rules! ext_s_operand {
	($read:ident, $ret:ty, $range:expr) => {
		pub fn $read(self) -> $ret {
			static HALF: i32 = (1 << $range.end - $range.start) - 1 >> 1;

			(self.inner.get_bits($range) as i32 - HALF) as $ret
		}
	};
}
