#pragma once

using namespace HbZeroAsioNet;


// 네트웍 패킷 인덱스
enum NETWORK_MESSAGE_ID : UINT16  
{
	CHAT			= 101
};

const INT32 MAX_CHAT_MESSAGE_LEN = 32;
struct ChatPacket : public BasicPacketHandler::PACKET_HEADER
{
	char szMessage[MAX_CHAT_MESSAGE_LEN];
};