#ifndef _STEEL_MUTEX_H_
#define _STEEL_MUTEX_H_


#include <mutex>

#define USE_MUTEXES 1


namespace Steel  {
  class Mutex{
  public:
    Mutex();
    ~Mutex();
    void enable();
    void disable();
    bool lock();
    bool unlock();
  private:
    bool m_enabled;
    std::recursive_mutex m_mutex;
  };

  class AutoLock { 
  public:
  AutoLock(Mutex &mutex):m_mutex(mutex){
      m_success = m_mutex.lock();
    }
  AutoLock(Mutex* pMutex):m_mutex(*pMutex){
      m_success = m_mutex.lock();
    }
    ~AutoLock(){
      if(m_success)
	m_mutex.unlock();
    }
  private:
    Mutex& m_mutex;
    bool m_success;
  };
};


#endif
