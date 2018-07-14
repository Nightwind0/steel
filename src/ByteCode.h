
#ifndef _STEEL_BYTE_CODE_H
#define _STEEL_BYTE_CODE_H

#include <cstdint>
#include <string>

namespace Steel { 

  // Note: The stack will need to be SteelType* so that we can do lvalue stuff, true??
  // Or is it by value because we store them back to the DS!
  // But that will mean terrible performance because every change to an array will 
  // involve at least one copy of the whole thing, right? due to copy-on-write
  // So possibly pointers is better? But does that mean we're constantly
  // Passing everything by pointer, such as to functions? We don't want that....
  // Or.. keep things pointers, but when pushing for function calls go ahead
  // and make a copy. Arrays have copy-on-write anyway.... 
  // That way, within a function we will have access to the thing for
  // optimization reasons.
  // That does imply that we have to new things up all the time.
  // Maybe we should use unique_ptr ?
  class ByteCode {
  public:
    enum class Operation : unsigned char {
      NOP,
	HALT, // End execution
	LOAD, // Load from DS[param32] onto TOS.
	LOADIDX, // Load from data store at IDX register. Pushes onto TOS
	LOADEX, // Load external variable from interpreter environment, indicated by string-data[A]
	CREATE, // Creates an object of type param32 at DS[IDX]
	ALLOC, // Allocs one new variable onto DS of type param1
	DEALLOC, // Shrinks DS by one
	STORE, // Stores from TOS onto data store into data at lvalue register. Pops
	STOREEX, // Store to external variable indicated by string-data[param32]
	PUSHL, // Push  lvalue of data at DS[param32] onto lvalue stack
	POPL, // Pop lvalue stack
	LOADL, // Push the value in the LTOS (lvalue top-of-stack) to the stack (by value)
	SETIDX, // Set index register from TOS. Pops TOS
	SETIDXI, // Set the index register to param32
	CLEARIDX, // Clear the index register
	PUSHI, // Push literal (param32) as integer onto data store
	PUSHF, // Push float literal represented by (param32) onto data store
	PUSHS, // Push literal string from string-data onto data store specified by param1
	PUSHFN, // Push a functor onto the stack that is FunctionIndex param1
	POP, // Discards TOS
	ADD, // Pops two values from TOS and adds, pushing result to TOS
	SUB,
	MUL,
	DIV,
	POW,
	MOD,
	D, // Dice operator
	SHR, // Shift Right
	SHL, // Shift left
	BAND, // Binary AND
	BOR, // Binary OR
	BXOR, // Binary XOR
	BNOT,
	GT, // Pops two values from stack and pushes the results of the comparison to TOS
	LT,
	GTE,
	LTE,
	EQ,
	NEQ,
	NOT, // Pops TOS and performs logical NOT, pushes result to stack
	AND, // Logical AND
	OR, // Logical OR
	XOR, // Pop two, push 1 XOR 2
	JSR, // Jump to subroutine  at TOS. Does NOT pop TOS. Return needs to perform that
	RETURN, // Return from subroutine. Pops PC from stack. Pops TOS (The original functor that got us here)
	B, // Branch, unconditional to param32
	BZ, // Branch if TOS is zero to param32 code position
	BNZ, // Branch if TOS is non-zero to param32 code position
	CELEM, // Takes array from TOS and index specified by IDX and pushes that element onto TOS
	CELEML, // Takes array from TOS and index specfied by IDX and puts TOS[IDX] onto Lvalue
	ARES, // Reserve TOS number of spaces in array. Pops TOS
	APOP, // Pop's a value from array in lvalue and pushes onto stack
	APOPB, // Pops a value from the back of array in lvalue and pushes onto stack
	APUSH, // Pushes TOS onto array in lvalue
	APUSHB, // Pushes TOS onto back of array in lvalue
	CREM, //  From container (hash/array) A in lvalue, remove A[TOS], pops TOS
	HSET, // Add to hash in lvalue the TOS value (popped) at an index specified by IDX register
	};

    ByteCode(Operation op, uint16_t param1=0,uint16_t param2=0);
    ByteCode(Operation op, uint32_t param32);
    Operation op() const;
    uint16_t param1() const; 
    uint16_t param2() const;
    uint32_t param32() const;
  private:
    union Params {
      struct {
	uint16_t m_param1;
	uint16_t m_param2;
      };
      uint32_t m_param32;
    }m_params;

    Operation m_op;
  };




}




#endif
