#include "Compilation.h"

using namespace Steel;

Compilation::Compilation():m_next_idx(0){
    PushScope();
}
Compilation::~Compilation(){
    PopScope();
}

void Compilation::AddCode(const ByteCode& code){
    FunctionIndex fn = m_function_stack.front();
    m_code[fn].emplace_back(code);
}
void Compilation::PushScope(){
    m_identifier_map.push_back(std::map<identifier,uint16_t>());
    m_highwater_marks.push_back(m_next_idx);
    m_breaks.push_back(std::vector<CodePosition>());
}

void Compilation::PopScope(){
    m_identifier_map.pop_back();
    m_highwater_marks.pop_back();
    m_breaks.pop_back();
}

void Compilation::Using(const std::string& ns){
    m_imports.insert(ns);
}

void Compilation::alloc(Type type){
}

void Compilation::Import(const std::string& filename){
}

VariableIndex Compilation::NewVariable(const std::string& id, const std::string& ns, Steel::Type type){
    identifier name;
    name.m_name = id;
    name.m_ns = ns;
    name.m_const = false;

    m_identifier_map.back()[name] = m_next_idx;

    alloc(type);

    return m_next_idx++;
}

VariableIndex Compilation::NewConst(const std::string& id, const std::string& ns){
    identifier name;
    name.m_name = id;
    name.m_ns = ns;
    name.m_const = true;

    m_identifier_map.back()[name] = m_next_idx;

    alloc(Steel::Type::SCALAR);

    return m_next_idx++;
}

VariableIndex Compilation::Variable(const std::string& id, const std::string& ns){
    identifier _id;
    _id.m_name = id;
    _id.m_ns = ns;
    for(auto scope=m_identifier_map.rbegin(); scope != m_identifier_map.rend(); scope++){
	auto it = scope->find(_id);
	if(it != scope->end()){
	    return it->second;
	}
    }
    
    throw UnknownIdentifier(_id.m_name);
    return 0;
}


bool Compilation::IsConst(VariableIndex idx){
    for(auto scope=m_identifier_map.rbegin(); scope != m_identifier_map.rend(); scope++){
	for(auto it : *scope){
	  if(it.second == idx){
		return it.first.m_const;
	    }
	}
    }
    assert(0); // This means a variableindex wasn't found, the compiler is broken
}

VariableIndex Compilation::TempVariable(Type type){
    alloc(type);
    return m_next_idx++;
}

StringIndex Compilation::StringLiteral(const std::string& literal){
    auto it = m_stringdata_map.find(literal);
    if(it == m_stringdata_map.end()){
	size_t count = m_stringdata_map.size();
	m_stringdata_map[literal] = count;
	return count;
    }else{
	return it->second;
    }
}

CodePosition Compilation::CurrentPosition()const {
    auto cur_fun = m_function_stack.back();
    const auto it = m_code.find(cur_fun);
    return it->second.size();
}

CodePosition Compilation::PushLoopStart(CodePosition pos){
    // TODO: Should loop starts be indexed by FunctionIndex ? 
    m_loop_starts.emplace_back(pos);
    return pos; // wtf is the point?
}

void Compilation::BreakAt(CodePosition pos){
    m_breaks.back().push_back(pos);
}

CodePosition Compilation::GetLoopStart()const{
}

CodePosition Compilation::PopLoopStart()const{
}
FunctionIndex Compilation::FunctionStart(){
}

FunctionIndex Compilation::FunctionEnd(){
}

void Compilation::SetParam32(CodePosition pos, uint32_t param){
}

void Compilation::SetParam1(CodePosition pos, uint16_t param1){
}

void Compilation::SetParam2(CodePosition pos, uint16_t param2){
}

std::string Compilation::OutputByteCode()const{
}

void Compilation::SetBreaksTo(CodePosition end){
}
