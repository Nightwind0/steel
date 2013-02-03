#ifndef _STEEL_MUTEX_H_
#define _STEEL_MUTEX_H_


#ifdef WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

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
	int m_lock_count;
#ifdef WIN32
	CRITICAL_SECTION critical_section;
#else
	pthread_mutexattr_t attr;
	pthread_mutex_t handle;
#endif
  };

  class AutoLock { 
  public:
  AutoLock(Mutex &mutex):m_mutex(mutex){
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
