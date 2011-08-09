#if defined(CONF_TEERACE)

#include <game/stream.h>
#include <game/server/webapp.h>
#include <engine/external/json/reader.h>
#include <engine/external/json/writer.h>

#include "user.h"


int CWebUser::AuthToken(void *pUserData)
{
	CParam *pData = (CParam*)pUserData;
	CServerWebapp *pWebapp = (CServerWebapp*)pData->m_pWebapp;
	int ClientID = pData->m_ClientID;
	CUnpacker Unpacker = pData->m_Unpacker;
	
	if(!pWebapp->Connect())
	{
		pWebapp->AddOutput(new COut(WEB_USER_AUTH, ClientID));
		delete pData;
		return 0;
	}
	
	Json::Value Userdata;
	Json::FastWriter Writer;
	
	Userdata["api_token"] = pData->m_aToken;
	
	std::string Json = Writer.write(Userdata);
	delete pData;
	
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), CServerWebapp::POST, pWebapp->ApiPath(), "users/auth_token/", pWebapp->ServerIP(), pWebapp->ApiKey(), Json.length(), Json.c_str());
	CBufferStream Buf;
	bool Check = pWebapp->SendRequest(aBuf, &Buf);
	pWebapp->Disconnect();
	
	if(!Check)
	{
		dbg_msg("webapp", "error (user auth token)");
		pWebapp->AddOutput(new COut(WEB_USER_AUTH, ClientID));
		return 0;
	}
	
	if(str_comp(Buf.GetData(), "false") == 0)
	{
		pWebapp->AddOutput(new COut(WEB_USER_AUTH, ClientID));
		return 0;
	}
	
	Json::Value User;
	Json::Reader Reader;
	if(!Reader.parse(Buf.GetData(), Buf.GetData()+Buf.Size(), User))
	{
		pWebapp->AddOutput(new COut(WEB_USER_AUTH, ClientID));
		return 0;
	}
	
	COut *pOut = new COut(WEB_USER_AUTH, ClientID);
	pOut->m_UserID = User["id"].asInt();
	pOut->m_IsStaff = User["is_staff"].asBool();
	pOut->m_Unpacker = Unpacker;
	str_copy(pOut->m_aUsername, User["username"].asCString(), sizeof(pOut->m_aUsername));
	pWebapp->AddOutput(pOut);
	return 1;
}

// TODO: rework this
float HueToRgb(float v1, float v2, float h)
{
   if(h < 0.0f) h += 1;
   if(h > 1.0f) h -= 1;
   if((6.0f * h) < 1.0f) return v1 + (v2 - v1) * 6.0f * h;
   if((2.0f * h) < 1.0f) return v2;
   if((3.0f * h) < 2.0f) return v1 + (v2 - v1) * ((2.0f/3.0f) - h) * 6.0f;
   return v1;
}

int HslToRgb(int v)
{
	vec3 HSL = vec3(((v>>16)&0xff)/255.0f, ((v>>8)&0xff)/255.0f, 0.5f+(v&0xff)/255.0f*0.5f);
	vec3 RGB;
	if(HSL.s == 0.0f)
		RGB = vec3(HSL.l, HSL.l, HSL.l);
	else
	{
		float v2 = HSL.l < 0.5f ? HSL.l * (1.0f + HSL.s) : (HSL.l+HSL.s) - (HSL.s*HSL.l);
		float v1 = 2.0f * HSL.l - v2;

		RGB = vec3(HueToRgb(v1, v2, HSL.h + (1.0f/3.0f)), HueToRgb(v1, v2, HSL.h), HueToRgb(v1, v2, HSL.h - (1.0f/3.0f)));
	}
	
	RGB = RGB*255;
	return (((int)RGB.r)<<16)|(((int)RGB.g)<<8)|(int)RGB.b;
}

int CWebUser::UpdateSkin(void *pUserData)
{
	CParam *pData = (CParam*)pUserData;
	CServerWebapp *pWebapp = (CServerWebapp*)pData->m_pWebapp;
	
	if(!pWebapp->Connect())
	{
		delete pData;
		return 0;
	}
	
	Json::Value Userdata;
	Json::FastWriter Writer;
	
	Userdata["skin_name"] = pData->m_SkinName;
	if(pData->m_UseCustomColor)
	{
		Userdata["body_color"] = HslToRgb(pData->m_ColorBody);
		Userdata["feet_color"] = HslToRgb(pData->m_ColorFeet);
	}
	
	std::string Json = Writer.write(Userdata);
	
	char aBuf[512];
	char aURL[128];
	str_format(aURL, sizeof(aURL), "users/skin/%d/", pData->m_UserID);
	str_format(aBuf, sizeof(aBuf), CServerWebapp::PUT, pWebapp->ApiPath(), aURL, pWebapp->ServerIP(), pWebapp->ApiKey(), Json.length(), Json.c_str());
	CBufferStream Buf;
	bool Check = pWebapp->SendRequest(aBuf, &Buf);
	pWebapp->Disconnect();
	
	delete pData;
	
	if(!Check)
	{
		dbg_msg("webapp", "error (skin update)");
		return 0;
	}
	
	return 1;
}

int CWebUser::GetRank(void *pUserData) // TODO: get clan here too
{
	CParam *pData = (CParam*)pUserData;
	CServerWebapp *pWebapp = (CServerWebapp*)pData->m_pWebapp;
	int UserID = pData->m_UserID;
	int ClientID = pData->m_ClientID;
	bool PrintRank = pData->m_PrintRank;
	bool GetBestRun = pData->m_GetBestRun;
	char aName[64];
	str_copy(aName, pData->m_aName, sizeof(aName));
	delete pData;
	
	bool Check = 0;
	
	int GlobalRank = 0;
	int MapRank = 0;
	
	if(!pWebapp->Connect())
		return 0;
	
	CBufferStream Buf;
	char aBuf[512];
	char aURL[128];
	
	// get userid if there is none
	if(!UserID)
	{
		Json::Value PostUser;
		Json::FastWriter Writer;

		PostUser["username"] = aName;

		std::string Json = Writer.write(PostUser);
	
		str_format(aURL, sizeof(aURL), "users/get_by_name/");
		str_format(aBuf, sizeof(aBuf), CServerWebapp::POST, pWebapp->ApiPath(), aURL, pWebapp->ServerIP(), pWebapp->ApiKey(), Json.length(), Json.c_str());
		Check = pWebapp->SendRequest(aBuf, &Buf);
		pWebapp->Disconnect();
		
		if(!Check)
		{
			dbg_msg("webapp", "error (user global rank)");
			return 0;
		}
		
		Json::Value User;
		Json::Reader Reader;
		if(!Reader.parse(Buf.GetData(), Buf.GetData()+Buf.Size(), User))
			return 0;
		
		UserID = User["id"].asInt();
		// no user found
		if(!UserID)
		{
			COut *pOut = new COut(WEB_USER_RANK, ClientID);
			pOut->m_PrintRank = PrintRank;
			pOut->m_MatchFound = 0;
			str_copy(pOut->m_aUsername, aName, sizeof(pOut->m_aUsername));
			pWebapp->AddOutput(pOut);
			return 1;
		}
		str_copy(aName, User["username"].asCString(), sizeof(aName));
		
		if(!pWebapp->Connect())
			return 0;
	}
	
	// global rank
	str_format(aURL, sizeof(aURL), "users/rank/%d/", UserID);
	str_format(aBuf, sizeof(aBuf), CServerWebapp::GET, pWebapp->ApiPath(), aURL, pWebapp->ServerIP(), pWebapp->ApiKey());
	Buf.Clear();
	Check = pWebapp->SendRequest(aBuf, &Buf);
	pWebapp->Disconnect();
	
	if(!Check)
	{
		dbg_msg("webapp", "error (user global rank)");
		return 0;
	}
	
	GlobalRank = str_toint(Buf.GetData());
	
	if(!pWebapp->Connect())
		return 0;
	
	str_format(aURL, sizeof(aURL), "users/map_rank/%d/%d/", UserID, pWebapp->CurrentMap()->m_ID);
	str_format(aBuf, sizeof(aBuf), CServerWebapp::GET, pWebapp->ApiPath(), aURL, pWebapp->ServerIP(), pWebapp->ApiKey());
	Buf.Clear();
	Check = pWebapp->SendRequest(aBuf, &Buf);
	pWebapp->Disconnect();
	
	if(!Check)
	{
		dbg_msg("webapp", "error (user map rank)");
		return 0;
	}
	
	Json::Value Rank;
	Json::Reader Reader;
	if(!Reader.parse(Buf.GetData(), Buf.GetData()+Buf.Size(), Rank))
		return 0;
		
	MapRank = Rank["position"].asInt();
	CPlayerData Run;
	if(MapRank)
	{
		// getting times
		if(!pWebapp->DefaultScoring())
		{
			float Time = str_tofloat(Rank["bestrun"]["time"].asCString());
			float aCheckpointTimes[25] = {0.0f};
			Json::Value Checkpoint = Rank["bestrun"]["checkpoints_list"];
			for(unsigned int i = 0; i < Checkpoint.size(); i++)
				aCheckpointTimes[i] = str_tofloat(Checkpoint[i].asCString());
			Run.Set(Time, aCheckpointTimes);
		}
	}
	
	COut *pOut = new COut(WEB_USER_RANK, ClientID);
	pOut->m_GlobalRank = GlobalRank;
	pOut->m_MapRank = MapRank;
	pOut->m_BestRun = Run;
	pOut->m_UserID = UserID;
	pOut->m_PrintRank = PrintRank;
	pOut->m_GetBestRun = GetBestRun;
	str_copy(pOut->m_aUsername, aName, sizeof(pOut->m_aUsername));
	pWebapp->AddOutput(pOut);
	return 1;
}

int CWebUser::PlayTime(void *pUserData) // TODO: get clan here too
{
	CParam *pData = (CParam*)pUserData;
	CServerWebapp *pWebapp = (CServerWebapp*)pData->m_pWebapp;
	int UserID = pData->m_UserID;
	int Time = pData->m_PlayTime;
	delete pData;
	
	if(!pWebapp->Connect())
		return 0;
	
	char aBuf[512];
	char aURL[128];
	
	Json::Value Post;
	Json::FastWriter Writer;

	Post["seconds"] = Time;
	
	std::string Json = Writer.write(Post);
	
	str_format(aURL, sizeof(aURL), "users/playtime/%d/", UserID);
	str_format(aBuf, sizeof(aBuf), CServerWebapp::PUT, pWebapp->ApiPath(), aURL, pWebapp->ServerIP(), pWebapp->ApiKey(), Json.length(), Json.c_str());
	CBufferStream Buf;
	bool Check = pWebapp->SendRequest(aBuf, &Buf);
	pWebapp->Disconnect();
		
	if(!Check)
	{
		dbg_msg("webapp", "error (user playtime)");
		return 0;
	}

	pWebapp->Disconnect();
	return 1;
}

#endif
