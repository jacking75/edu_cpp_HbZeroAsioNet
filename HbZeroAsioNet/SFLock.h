#pragma once

#include <mutex>

#include "SFNetwork_Def.h"

#pragma warning( disable : 4512 )

namespace HbZeroAsioNet 
{
	class ILock 
	{
	public:
		virtual void Lock() = 0;
		virtual void UnLock() = 0;
	};

#ifdef WIN32	
	class WinSpinCriticalSection : public ILock
	{
	public:
		WinSpinCriticalSection()
		{
			InitializeCriticalSectionAndSpinCount(&m_lock, MAX_SPIN_LOCK_COUNT); 
		}

		virtual ~WinSpinCriticalSection()
		{
			DeleteCriticalSection(&m_lock); 
		}

		virtual void Lock() override 
		{
			EnterCriticalSection(&m_lock);
		}

		virtual void UnLock() override 
		{
			LeaveCriticalSection(&m_lock);
		}

	private:
		CRITICAL_SECTION m_lock;
	};
#endif
	
	
	class StandardCppCriticalSection : public ILock 
	{
	public:
		virtual void Lock() override 
		{
			m_lock.lock();
		}

		virtual void UnLock() override 
		{
			m_lock.unlock();
		}

	private:
		std::mutex m_lock;
	};
	
	
	
	class __SCOPED_LOCK__
	{
	public:
		__SCOPED_LOCK__( ILock &LockObj )
			: m_LockObj(LockObj) 
		{ 
			m_LockObj.Lock(); 
		}

		~__SCOPED_LOCK__() 
		{ 
			m_LockObj.UnLock(); 
		}

	private:
		ILock &m_LockObj;
	};
}
