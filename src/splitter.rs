use crate::common::types::{Block, Inst, Opcode, Target};
use std::collections::{BTreeSet, HashMap};

// breaks a stream of instructions into basic blocks
// `label_set` contains a set of where a jump starts or ends
// `target_map` contains a set of where a jump targets and its offset
pub struct Splitter {
	label_set: BTreeSet<usize>,
	target_map: HashMap<usize, i32>,
}

impl Splitter {
	// after start/end positions are decided, we just split the original
	// code based on those indices and add a bit of useful information
	// like the next label we jump to, or an undefined offset if the jump
	// is not well formed
	pub fn new() -> Self {
		Self {
			label_set: BTreeSet::new(),
			target_map: HashMap::new(),
		}
	}

	pub fn split(mut self, code: Vec<Inst>) -> Vec<Block> {
		self.find_edges(&code);
		self.split_at_edges(code)
	}

	fn add_target(&mut self, pc: usize, offset: i32) {
		let dest = pc as i32 + offset + 1;

		self.label_set.insert(dest as usize);
		self.target_map.insert(pc, offset);
	}

	fn find_edges(&mut self, code: &[Inst]) {
		self.label_set.insert(0);

		for (pc, inst) in code.iter().enumerate() {
			match inst.opcode() {
				Opcode::LFalseSkip
				| Opcode::Test
				| Opcode::TestSet
				| Opcode::Eq
				| Opcode::Lt
				| Opcode::Le
				| Opcode::EqK
				| Opcode::EqI
				| Opcode::LtI
				| Opcode::LeI
				| Opcode::GtI
				| Opcode::GeI => {
					self.add_target(pc, 1);
				}
				Opcode::Jmp => {
					self.add_target(pc, inst.sj());
				}
				Opcode::ForPrep => {
					self.add_target(pc, inst.bx() as i32 + 1);
				}
				Opcode::TForPrep => {
					self.add_target(pc, inst.bx() as i32);
				}
				Opcode::ForLoop | Opcode::TForLoop => {
					let sbx = inst.bx() as i32;

					self.add_target(pc, -sbx);
				}
				Opcode::Return | Opcode::Return0 | Opcode::Return1 => {}
				_ => {
					continue;
				}
			}

			self.label_set.insert(pc + 1);
		}

		self.label_set.retain(|&v| v <= code.len());
	}

	fn get_offset_target(&self, post: usize, offset: i32) -> Target {
		// nasty; we count the index of the label before our destination
		let dest = post as i32 + offset;
		let index = self.label_set.range(0..=dest as usize).count();

		// an out of bounds destination means the jump was either
		// past the last instruction or before the first one
		if index > self.label_set.len() {
			Target::Undefined(offset)
		} else {
			Target::Label(index as u32 - 1)
		}
	}

	fn split_at_edges(&self, code: Vec<Inst>) -> Vec<Block> {
		let mut iter = code.into_iter();
		let mut list = Vec::new();
		let mut prev = 0;

		for pc in self.label_set.iter().skip(1).map(|v| v - 1) {
			let offset = self.target_map.get(&pc).copied().unwrap_or_default();
			let post = pc + 1;

			let code = iter.by_ref().take(post - prev).collect();
			let target = self.get_offset_target(post, offset);

			prev = post;
			list.push(Block { code, target });
		}

		list
	}
}
