#pragma once

#include "SFInterface.h"


namespace HbZeroAsioNet
{
	class BasicFactoryNetworkComponent : public IFactoryNetworkComponent 
	{
	public:
		BasicFactoryNetworkComponent() {}
		virtual ~BasicFactoryNetworkComponent() {}


		void SetComponent(NetworkConfig netConfig, IPacketHandler* pPacketHandler)
		{			
			m_pPacketHandler = pPacketHandler;
			m_pLogger = pPacketHandler->GetLogger();
		}

		virtual IPacketHandler* GetPacketHandler() override
		{
			return m_pPacketHandler;
		}

		virtual ILogger* GetLogger() override
		{
			return m_pLogger;
		}
						

		IPacketHandler* m_pPacketHandler;
		ILogger* m_pLogger;
	};
}

