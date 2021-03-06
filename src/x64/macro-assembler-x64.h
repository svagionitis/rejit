// Copyright (C) 2013 Alexandre Rames <alexandre@coreperf.com>
// rejit is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef REJIT_X64_MACRO_ASSEMBLER_X64_H_
#define REJIT_X64_MACRO_ASSEMBLER_X64_H_

#include "assembler-x64.h"
#include "assembler-x64-inl.h"
#include "macro-assembler-base.h"

namespace rejit {
namespace internal {


const Register result_matches = rbx;
const Register ring_index = r12;

const Register string_base = r13;
const Register string_pointer = r14;
// Points past the last character of the string not including '\0'. Ie. points
// to '\0' for '\0' terminated strings or just after the last character.
// Memory at and after string_end should not be accessed.
const Register string_end = r15;
const Operand current_char = Operand(string_pointer, 0);
const Operand next_char = Operand(string_pointer, kCharSize);
const Operand previous_char = Operand(string_pointer, -kCharSize);
const Operand current_chars = Operand(string_pointer, 0);

static inline int CalleeSavedRegsSize() {
  if (FLAG_emit_debug_code) {
    return kCalleeSavedRegsSize;
  } else {
    return kUsedCalleeSavedRegsSize;
  }
}

const int kStateInfoSize = 5 * kPointerSize;
// Next starting position for fast forwarding.
const Operand ff_position    (rbp, -CalleeSavedRegsSize() - 1 * kPointerSize);
// State from which FF thinks there may be a potential match.
const Operand ff_found_state (rbp, -CalleeSavedRegsSize() - 2 * kPointerSize);
// Position of the end of the match when matching the regexp backward from the
// ff_element.
const Operand backward_match (rbp, -CalleeSavedRegsSize() - 3 * kPointerSize);
// Position of the end of the match when matching the regexp forward from the
// ff_element.
const Operand forward_match  (rbp, -CalleeSavedRegsSize() - 4 * kPointerSize);
// Used when looking for multiple matches (kMatchAll) to indicate the end of the
// previous match.
const Operand last_match_end (rbp, -CalleeSavedRegsSize() - 5 * kPointerSize);

const Register mscratch = r8;
const Register scratch = r9;
const Register scratch1 = r9;
const Register scratch2 = r10;
const Register scratch3 = r11;


class MacroAssembler : public MacroAssemblerBase {
 public:
  MacroAssembler();
  // TODO(ajr): Check destructor need.

  inline void PushRegisters(RegList regs);
  inline void PopRegisters(RegList regs);

  // Currently following the System V ABI only.
  inline void PushCallerSavedRegisters();
  inline void PopCallerSavedRegisters();
  void PushCalleeSavedRegisters();
  void PopCalleeSavedRegisters();

  inline void PushAllRegisters();
  inline void PopAllRegisters();
  inline void PushAllRegistersAndFlags();
  inline void PopAllRegistersAndFlags();

  void CallCppPrepareStack();
  void CallCpp(Address address);

  void Move(Register dst, uint64_t value);
  inline void Move(Register dst, Register src);

  inline void MoveCharsFrom(Register dst, unsigned n, const char* chars);
  inline void LoadCharsFrom(Register dst, unsigned n, const Operand& src);

  inline void LoadCurrentChar(Register r);

  void MaskFirstChars(unsigned n_chars, Register dst);

  // Variable width helpers.
  void mov(unsigned width, Register dst, const Operand&);
  void mov_truncated(unsigned width, Register dst, const Operand&);

  // Variable width compare.
  // Accesses 2^log2(width) bytes at dst.
  template <class Type>
    void cmp_truncated_(unsigned width, Register dst, Type src) {
      if (width == 0) return;
      if (width == 1) {
        cmpb(dst, src);
      } else if (width < 4) {
        cmpw(dst, src);
      } else if (width < 8) {
        cmpl(dst, src);
      } else {
        cmpq(dst, src);
      }
    }
  inline void cmp_truncated(unsigned width, Register dst, Register src) {
    return cmp_truncated_<Register>(width, dst, src);
  }
  inline void cmp_truncated(unsigned width, Register dst, const Operand& src) {
    return cmp_truncated_<const Operand&>(width, dst, src);
  }
  void cmp_truncated(unsigned width, const Operand& dst, int64_t src);
  // Variable width compare.
  // Guarantees that no memory accesses from [dst + width] will occur.
  void cmp_safe(unsigned width, Condition cond, Operand dst, int64_t src,
                Label* on_no_match = NULL);
  void cmp_safe(unsigned width, Condition cond, Operand dst, Register src,
                Label* on_no_match = NULL);
  // Variable width compare.
  // Accesses 2^ceiling(log2(width)) bytes at dst.
  void cmp(unsigned width, const Operand& dst, int64_t src);
  void cmp(unsigned width, const Operand& dst, Register src);

  void movdq(XMMRegister dst, uint64_t high, uint64_t low);
  void movdqp(XMMRegister dst, const char* chars, unsigned n_chars);

  // Clear a memory region.
  // The start and end (and size) must be 8-bytes aligned.
  // start must point at a strictly lower address than the end register.
  enum MemZeroOutputStatus {
    // On output, the start and end register point to the higher address.
    AtLowAddress,
    // On output, the start and end register point to the lower address.
    AtHighAddress,
  };
  // Hint about the size of the copy.
  enum MemZeroSizeHint {
    Small,
    Big,
  };
  void MemZero(Register start, Register end, Register zero = no_reg,
               MemZeroOutputStatus = AtHighAddress, MemZeroSizeHint = Small);
  inline void MemZero(const Operand& start, size_t size, Register zero = no_reg,
               MemZeroOutputStatus = AtHighAddress, MemZeroSizeHint = Small);
  void MemZero(Register start, size_t size, Register zero = no_reg,
               MemZeroOutputStatus = AtHighAddress, MemZeroSizeHint = Small);

  // Increment or decrement by the size of a character.
  inline void inc_c(Register dst);
  inline void dec_c(Register dst);

  // Advance the register by a number of characters in the given direction.
  inline void Advance(unsigned, Direction, Register);
  inline void AdvanceToEOS();

  // Debug helpers ---------------------------------------------------

  // All debug helpers must preserve registers and flags.

  // Print a message.
  void msg(const char* message);
  inline void debug_msg(const char* message);
  void debug_msg(Condition cond, const char* message);

  // Assert from assembly.
  // Define a macro for convenience.
  void asm_assert_(Condition cond, const char* file, int line, const char* description);

  // Print a message and stop.
  void stop(const char* messgae);
  void stop(Condition cond, const char* messgae);
#define asm_assert(cond, description) asm_assert_(cond, __FILE__, __LINE__, description)
};


} }  // namespace rejit::internal

#endif  // REJIT_X64_MACRO_ASSEMBLER_X64_H_

