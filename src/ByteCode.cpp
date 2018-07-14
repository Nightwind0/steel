#include "ByteCode.h"

using namespace Steel;

ByteCode::ByteCode(Operation op, uint16_t param1,uint16_t param2):m_op(op){
  m_params.m_param1 = param1;
  m_params.m_param2 = param2;
}
ByteCode::ByteCode(Operation op, uint32_t param32):m_op(op){
  m_params.m_param32 = param32;
}
ByteCode::Operation ByteCode::op() const{
  return m_op;
}
uint16_t ByteCode::param1() const {
  return m_params.m_param1;
}

uint16_t ByteCode::param2() const {
  return m_params.m_param2;
}

uint32_t ByteCode::param32() const{
  return m_params.m_param32;
}
