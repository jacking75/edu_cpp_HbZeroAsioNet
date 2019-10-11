#include "stdafx.h"
#include "CppUnitTest.h"

#include "MockLog.h"
#include "SFRingBuffer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace UnitTestSFNetworkLib
{
	const INT32 MAX_BUFFER_SIZE = 320;
	const INT32 MAX_PACKET_SIZE = 25;

	TEST_CLASS(TestRingBuffer)
	{
	public:
		
		TEST_METHOD_INITIALIZE(Initialize)
		{
			m_pLog			= new MockLog;
			m_pRingBuffer	=  new HbZeroAsioNet::RingBuffer(m_pLog);

			m_pRingBuffer->Allocate( MAX_BUFFER_SIZE, MAX_PACKET_SIZE );
		}

		TEST_METHOD_CLEANUP(Cleanup) 
		{ 
			delete m_pRingBuffer;
			delete m_pLog;
		}

		TEST_METHOD(Clear)
		{
			m_pRingBuffer->Clear();

			Assert::AreEqual(0, m_pRingBuffer->GetReadOffset());
			Assert::AreEqual(0, m_pRingBuffer->GetWriteOffset());
		}


		void Setup()
		{
		}

		HbZeroAsioNet::RingBuffer* m_pRingBuffer;
		MockLog* m_pLog;
	};
}