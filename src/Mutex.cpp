#include "Mutex.h"
#include <iostream>
#include <cassert>
#include "SteelException.h"

#ifndef WIN32
#include <pthread.h>
extern "C"
{
#if defined(__APPLE__) || defined (__FreeBSD__) || defined(__OpenBSD__)
	int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int kind);
#else
	int pthread_mutexattr_setkind_np(pthread_mutexattr_t *attr, int kind);
#endif
}
#else
#endif

namespace Steel { 

	Mutex::Mutex():m_lock_count(0)
#ifdef WIN32
  {
	InitializeCriticalSection(&critical_section);
#else
  {
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	#if defined(__FreeBSD__) || defined(__APPLE__)
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	#else
	/* or PTHREAD_MUTEX_RECURSIVE_NP */
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);	
	#endif
	pthread_mutex_init(&m_handle, &attr);
	pthread_mutexattr_destroy(&attr);
#endif
	m_enabled = true;
  }

  Mutex::~Mutex(){
#ifdef WIN32
	DeleteCriticalSection(&critical_section);
#else
	pthread_mutex_destroy(&m_handle);
#endif
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
		++m_lock_count;
#ifdef WIN32
	EnterCriticalSection(&critical_section);
	return true;
#else
     return 0 == pthread_mutex_lock(&m_handle);
#endif
    }
	return true;
  }

  bool Mutex::unlock(){
    if(m_enabled && m_lock_count--){
#ifdef WIN32
	LeaveCriticalSection(&critical_section);
	return true;
#else
	return 0 == pthread_mutex_unlock(&m_handle);
#endif
    }
	return true;
  }

}
