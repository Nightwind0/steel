#include "Mutex.h"
#include <iostream>
#include <cassert>
#include "SteelException.h"


namespace Steel { 

  Mutex::Mutex()

  {
	m_enabled = true;
  }

  Mutex::~Mutex(){
  }

  void Mutex::enable()
  {
    m_enabled = true;
  }

  void Mutex::disable()
  {
    m_enabled = false;
  }

  bool Mutex::lock(){
    if(m_enabled){
      m_mutex.lock();
    }
    return true;
  }

  bool Mutex::unlock(){
    if(m_enabled){
      m_mutex.unlock();
    }
    return true;
  }

}
