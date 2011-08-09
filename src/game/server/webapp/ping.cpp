#if defined(CONF_TEERACE)

#include <game/stream.h>
#include <game/server/webapp.h>
#include <engine/external/json/reader.h>
#include <engine/external/json/writer.h>

#include "ping.h"

int CWebPing::Ping(void *pUserData)
{
	CParam *pData = (CParam*)pUserData;
	CServerWebapp *pWebapp = (CServerWebapp*)pData->m_pWebapp;
	bool CrcCheck = pData->m_CrcCheck;
	
	if(!pWebapp->Connect())
	{
		delete pData;
		return 0;
	}
	
	Json::Value Data;
	Json::FastWriter Writer;
	
	int Num = 0;
	for(int i = 0; i < pData->m_lName.size(); i++)
	{
		int User = (pData->m_lUserID[i] > 0);
		if(User)
		{
			// TODO: send clan
			char aBuf[16];
			str_format(aBuf, sizeof(aBuf), "%d", pData->m_lUserID[i]);
			Data["users"][aBuf] = pData->m_lName[i];
		}
		else
		{
			Data["anonymous"][Num] = pData->m_lName[i];
			Num++;
		}		
	}
	Data["map"] = pWebapp->MapName();
	
	std::string Json = Writer.write(Data);
	delete pData;
	
	char aBuf[1024];
	str_format(aBuf, sizeof(aBuf), CServerWebapp::POST, pWebapp->ApiPath(), "ping/", pWebapp->ServerIP(), pWebapp->ApiKey(), Json.length(), Json.c_str());
	CBufferStream Buf;
	int Check = pWebapp->SendRequest(aBuf, &Buf);
	pWebapp->Disconnect();
	
	if(!Check)
	{
		dbg_msg("webapp", "error (ping)");
		return 0;
	}
	
	bool Online = str_comp(Buf.GetData(), "\"PONG\"") == 0;
	
	pWebapp->AddOutput(new COut(Online, CrcCheck));
	return Online;
}

#endif
