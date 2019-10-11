#ifndef __MOCK_LOG_H__
#define __MOCK_LOG_H__

#include "SFILog.h" 

using namespace  SFNetworkLib;

class MockLog : public ILog 
{
public:
	virtual void Error( const char* pText ) override 
	{
	}

	virtual void Warn( const char* pText ) override 
	{
	}

	virtual void Debug( const char* pText ) override 
	{
	}

	virtual void Info( const char* pText ) override 
	{
	}
};



#endif