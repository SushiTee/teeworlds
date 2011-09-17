/* CClientWebapp Class by Sushi and Redix*/
#include <engine/shared/config.h>

#include <game/http/response.h>

#include "gameclient.h"
#include "webapp.h"

CClientWebapp::CClientWebapp(CGameClient *pGameClient)
: IWebapp(pGameClient->Storage()),
  m_pClient(pGameClient)
{
	m_ApiTokenError = false;
	m_ApiTokenRequested = false;
}

void CClientWebapp::OnResponse(CHttpConnection *pCon)
{
	int Type = pCon->Type();
	CResponse *pData = pCon->Response();
	bool Error = pCon->Error() || pCon->Response()->StatusCode() != 200;

	// TODO: add event listener (server and client)
	if(Type == WEB_API_TOKEN)
	{
		// TODO: better error messages
		if(Error || str_comp(pData->GetBody(), "false") == 0 || pData->Size() != 24+2)
			m_ApiTokenError = true;
		else
		{
			m_ApiTokenError = false;
			str_copy(g_Config.m_WaApiToken, pData->GetBody()+1, 24+1);
		}
		m_ApiTokenRequested = false;
	}
}
