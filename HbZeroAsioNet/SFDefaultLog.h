#pragma once

#include <iostream>
#include "SFInterface.h"


namespace HbZeroAsioNet
{
	class ConsoleLog : public ILogger   
	{
	public:
		ConsoleLog() {}
		virtual ~ConsoleLog() {}

				
	protected:
		virtual void Error( const char* pText ) override 
		{
			__SCOPED_LOCK__ LOCK( m_Lock );
			std::cout << "[ERROR] | " << pText << std::endl;
		}

		virtual void Warn( const char* pText ) override 
		{
			__SCOPED_LOCK__ LOCK( m_Lock );
			std::cout << "[WARN] | " << pText << std::endl;
		}

		virtual void Debug( const char* pText ) override 
		{
			__SCOPED_LOCK__ LOCK( m_Lock );
			std::cout << "[DEBUG] | " << pText << std::endl;
		}

		virtual void Trace( const char* pText ) override 
		{
			__SCOPED_LOCK__ LOCK( m_Lock );
			std::cout << "[TRACE] | " << pText << std::endl;
		}

		virtual void Info( const char* pText ) override 
		{
			__SCOPED_LOCK__ LOCK( m_Lock );
			std::cout << "[INFO] | " << pText << std::endl;
		}
		

#ifdef WIN32 
		WinSpinCriticalSection m_Lock;
#else 
		StandardCppCriticalSection m_Lock;
#endif
	};
}

