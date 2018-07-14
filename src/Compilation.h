#ifndef _STEEL_COMPILATION_H_
#define _STEEL_COMPILATION_H_

#include <map>
#include <string>

#include "ByteCode.h"
#include "SteelInterpreter.h"
#include "SteelException.h"


namespace Steel {


  enum class Type : uint32_t {
    SCALAR,
      ARRAY,
      HASH,
      FUNCTION,
      CLASS
  };

	class Compilation {
	public:
		Compilation();
		~Compilation();

		void AddCode(const ByteCode& code);
		void PushScope();
		void PopScope();
		void Using(const std::string& ns);
		void Import(const std::string& filename);
		VariableIndex NewVariable(const std::string& id, const std::string& ns = Steel::SteelInterpreter::kszGlobalNamespace, Type type = Type::SCALAR);
		VariableIndex NewConst(const std::string& id, const std::string& ns = Steel::SteelInterpreter::kszGlobalNamespace);
		VariableIndex Variable(const std::string& id, const std::string& ns = Steel::SteelInterpreter::kszUnspecifiedNamespace); // Returns 0 if not found
		bool IsConst(VariableIndex idx);
		VariableIndex TempVariable(Type type = Type::SCALAR); // Returns the index for a temp variable
		StringIndex StringLiteral(const std::string& literal);
		CodePosition CurrentPosition()const; // Give us a bytecode index for creating jumps, etc
		CodePosition PushLoopStart(CodePosition pos);
		void BreakAt(CodePosition pos);
		CodePosition GetLoopStart()const;
		CodePosition PopLoopStart()const; // sets all breaks to current pos
		FunctionIndex FunctionStart();
		FunctionIndex FunctionEnd();
		void SetParam32(CodePosition pos, uint32_t param);
		void SetParam1(CodePosition pos, uint16_t param1);
		void SetParam2(CodePosition pos, uint16_t param2);
		std::string OutputByteCode()const;
	private:
		void alloc(Type type);
		void SetBreaksTo(CodePosition end); // Goes through current breaks for this loop and sets their branch position
		struct identifier {
		  std::string m_name;
		  std::string m_ns;
		  bool m_const;
		  std::string name() const {
		    return m_ns + "::" + m_name;
		  }
		  bool operator<(const identifier& other) const {
		    return name() < other.name();		    
		  }
		};
		std::deque<CodePosition> m_scope_marks;
		std::deque<CodePosition> m_loop_starts;
		std::deque<std::map<identifier,uint16_t>> m_identifier_map;
		std::map<std::string,uint16_t> m_stringdata_map;
		std::map<uint16_t,std::vector<ByteCode>> m_code; // Map for functions, with main being at 0
		std::deque<FunctionIndex> m_function_stack;
		std::deque<std::vector<CodePosition>> m_breaks;
		std::set<std::string> m_imports;
		std::deque<VariableIndex> m_highwater_marks;
		VariableIndex m_next_idx;
		
	};


}



#endif
