/* (c) Rajh, Redix and Sushi. */

#include <cstdio>

#include <engine/ghost.h>
#include <engine/textrender.h>
#include <engine/storage.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/generated/client_data.h>
#include <game/client/animstate.h>

#include "skins.h"
#include "menus.h"
#include "controls.h"
#include "ghost.h"

/*
Note:
Freezing fucks up the ghost
the ghost isnt really sync
don't really get the client tick system for prediction
can used PrevChar and PlayerChar and it would be fluent and accurate but won't be predicted
so it will be affected by lags
*/

// own ghost: ID = -1
CGhost::CGhost()
	: m_CurGhost(-1),
	m_StartRenderTick(-1),
	m_CurPos(0),
	m_Rendering(false),
	m_Recording(false),
	m_RaceState(RACE_NONE),
	m_BestTime(-1),
	m_NewRecord(false)
{ }

void CGhost::AddInfos(CGhostCharacter Player)
{
	if(m_Recording)
		m_CurGhost.m_Path.add(Player);
	if(GhostRecorder()->IsRecording())
		GhostRecorder()->WriteData(GHOSTDATA_TYPE_CHARACTER, (const char*)&Player, sizeof(Player));
}

void CGhost::OnRender()
{
	// only for race
	if(!m_pClient->m_IsRace || !g_Config.m_ClRaceGhost)
		return;

	// Check if the race line is crossed then start the render of the ghost if one
	int EnemyTeam = m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientID].m_Team^1;
	if(m_RaceState != RACE_STARTED && ((m_pClient->Collision()->GetCollisionRace(m_pClient->Collision()->GetIndex(m_pClient->m_PredictedPrevChar.m_Pos, m_pClient->m_LocalCharacterPos)) == TILE_BEGIN) ||
		(m_pClient->m_IsFastCap && m_pClient->m_aFlagPos[EnemyTeam] != vec2(-1, -1) && distance(m_pClient->m_LocalCharacterPos, m_pClient->m_aFlagPos[EnemyTeam]) < 32)))
	{
		m_RaceState = RACE_STARTED;
		StartRender();
		StartRecord();
	}

	if(m_RaceState == RACE_FINISHED)
	{
		if(m_NewRecord)
		{
			// search for own ghost
			array<CGhostItem>::range r = find_linear(m_lGhosts.all(), m_CurGhost);
			m_NewRecord = false;
			if(r.empty())
				m_lGhosts.add(m_CurGhost);
			else
				r.front() = m_CurGhost;

			bool Recording = GhostRecorder()->IsRecording();
			StopRecord(m_BestTime);
			Save(Recording);
		}
		else
			StopRecord();

		StopRender();
		OnReset();
	}

	CNetObj_Character Char;
	m_pClient->m_PredictedChar.Write(&Char);

	if(m_pClient->m_NewPredictedTick)
		AddInfos(CGhostTools::GetGhostCharacter(Char));

	// Play the ghost
	if(!m_Rendering)
		return;

	m_CurPos = Client()->PredGameTick()-m_StartRenderTick;

	if(m_lGhosts.size() == 0 || m_CurPos < 0)
	{
		StopRender();
		return;
	}

	for(int i = 0; i < m_lGhosts.size(); i++)
	{
		CGhostItem *pGhost = &m_lGhosts[i];
		if(m_CurPos >= pGhost->m_Path.size())
			continue;

		int PrevPos = (m_CurPos > 0) ? m_CurPos-1 : m_CurPos;
		CGhostCharacter Player = pGhost->m_Path[m_CurPos];
		CGhostCharacter Prev = pGhost->m_Path[PrevPos];

		RenderGhostHook(Player, Prev);
		RenderGhost(Player, Prev, &pGhost->m_RenderInfo);
		RenderGhostNamePlate(Player, Prev, pGhost->m_aOwner);
	}
}

void CGhost::RenderGhost(CGhostCharacter Player, CGhostCharacter Prev, CTeeRenderInfo *pRenderInfo)
{
	float IntraTick = Client()->PredIntraGameTick();

	float Angle = mix((float)Prev.m_Angle, (float)Player.m_Angle, IntraTick)/256.0f;
	vec2 Direction = GetDirection((int)(Angle*256.0f));
	vec2 Position = mix(vec2(Prev.m_X, Prev.m_Y), vec2(Player.m_X, Player.m_Y), IntraTick);
	//vec2 Vel = mix(vec2(Prev.m_VelX/256.0f, Prev.m_VelY/256.0f), vec2(Player.m_VelX/256.0f, Player.m_VelY/256.0f), IntraTick);
	float VelX = mix(Prev.m_VelX / 256.0f, Player.m_VelX / 256.0f, IntraTick);

	bool Stationary = Player.m_VelX <= 1 && Player.m_VelX >= -1;
	bool InAir = !Collision()->CheckPoint(Player.m_X, Player.m_Y+16);
	bool WantOtherDir = (Player.m_Direction == -1 && VelX > 0) || (Player.m_Direction == 1 && VelX < 0);

	float WalkTime = fmod(absolute(Position.x), 100.0f)/100.0f;
	CAnimState State;
	State.Set(&g_pData->m_aAnimations[ANIM_BASE], 0);

	if(InAir)
		State.Add(&g_pData->m_aAnimations[ANIM_INAIR], 0, 1.0f);
	else if(Stationary)
		State.Add(&g_pData->m_aAnimations[ANIM_IDLE], 0, 1.0f);
	else if(!WantOtherDir)
		State.Add(&g_pData->m_aAnimations[ANIM_WALK], WalkTime, 1.0f);

	if(Player.m_Weapon == WEAPON_GRENADE)
	{
		Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);
		Graphics()->QuadsBegin();
		Graphics()->QuadsSetRotation(State.GetAttach()->m_Angle*pi*2+Angle);
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.5f);

		// normal weapons
		int iw = clamp(Player.m_Weapon, 0, NUM_WEAPONS-1);
		RenderTools()->SelectSprite(g_pData->m_Weapons.m_aId[iw].m_pSpriteBody, Direction.x < 0 ? SPRITE_FLAG_FLIP_Y : 0);

		vec2 Dir = Direction;
		float Recoil = 0.0f;
		// TODO: is this correct?
		float a = (Client()->PredGameTick()-Player.m_AttackTick+IntraTick)/5.0f;
		if(a < 1)
			Recoil = sinf(a*pi);

		vec2 p = Position + Dir * g_pData->m_Weapons.m_aId[iw].m_Offsetx - Direction*Recoil*10.0f;
		p.y += g_pData->m_Weapons.m_aId[iw].m_Offsety;
		RenderTools()->DrawSprite(p.x, p.y, g_pData->m_Weapons.m_aId[iw].m_VisualSize);
		Graphics()->QuadsEnd();
	}

	// Render ghost
	RenderTools()->RenderTee(&State, pRenderInfo, 0, Direction, Position, true);
}

void CGhost::RenderGhostHook(CGhostCharacter Player, CGhostCharacter Prev)
{
	if (Prev.m_HookState<=0 || Player.m_HookState<=0)
		return;

	float IntraTick = Client()->PredIntraGameTick();
	vec2 Pos = mix(vec2(Prev.m_X, Prev.m_Y), vec2(Player.m_X, Player.m_Y), IntraTick);

	vec2 HookPos = mix(vec2(Prev.m_HookX, Prev.m_HookY), vec2(Player.m_HookX, Player.m_HookY), IntraTick);
	float d = distance(Pos, HookPos);
	vec2 Dir = normalize(Pos-HookPos);

	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);
	Graphics()->QuadsBegin();
	Graphics()->QuadsSetRotation(GetAngle(Dir)+pi);
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.5f);

	// render head
	RenderTools()->SelectSprite(SPRITE_HOOK_HEAD);
	IGraphics::CQuadItem QuadItem(HookPos.x, HookPos.y, 24, 16);
	Graphics()->QuadsDraw(&QuadItem, 1);

	// render chain
	RenderTools()->SelectSprite(SPRITE_HOOK_CHAIN);
	IGraphics::CQuadItem Array[1024];
	int j = 0;
	for(float f = 24; f < d && j < 1024; f += 24, j++)
	{
		vec2 p = HookPos + Dir*f;
		Array[j] = IGraphics::CQuadItem(p.x, p.y, 24, 16);
	}

	Graphics()->QuadsDraw(Array, j);
	Graphics()->QuadsSetRotation(0);
	Graphics()->QuadsEnd();
}

void CGhost::RenderGhostNamePlate(CGhostCharacter Player, CGhostCharacter Prev, const char *pName)
{
	if(!g_Config.m_ClGhostNamePlates)
		return;

	float IntraTick = Client()->PredIntraGameTick();

	vec2 Pos = mix(vec2(Prev.m_X, Prev.m_Y), vec2(Player.m_X, Player.m_Y), IntraTick);

	float FontSize = 18.0f + 20.0f * g_Config.m_ClNameplatesSize / 100.0f;

	// render name plate
	float a = 0.5f;
	if(g_Config.m_ClGhostNameplatesAlways == 0)
		a = clamp(0.5f-powf(distance(m_pClient->m_pControls->m_TargetPos, Pos)/200.0f,16.0f), 0.0f, 0.5f);

	float tw = TextRender()->TextWidth(0, FontSize, pName, -1);
	
	TextRender()->TextOutlineColor(0.0f, 0.0f, 0.0f, 0.5f*a);
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, a);
	TextRender()->Text(0, Pos.x-tw/2.0f, Pos.y-FontSize-38.0f, FontSize, pName, -1);

	// reset color;
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	TextRender()->TextOutlineColor(0.0f, 0.0f, 0.0f, 0.3f);
}

void CGhost::InitRenderInfos(CTeeRenderInfo *pRenderInfo, const char *pSkinName, int UseCustomColor, int ColorBody, int ColorFeet)
{
	int SkinId = m_pClient->m_pSkins->Find(pSkinName);
	if(SkinId < 0)
	{
		SkinId = m_pClient->m_pSkins->Find("default");
		if(SkinId < 0)
			SkinId = 0;
	}

	pRenderInfo->m_ColorBody = m_pClient->m_pSkins->GetColorV4(ColorBody);
	pRenderInfo->m_ColorFeet = m_pClient->m_pSkins->GetColorV4(ColorFeet);

	if(UseCustomColor)
		pRenderInfo->m_Texture = m_pClient->m_pSkins->Get(SkinId)->m_ColorTexture;
	else
	{
		pRenderInfo->m_Texture = m_pClient->m_pSkins->Get(SkinId)->m_OrgTexture;
		pRenderInfo->m_ColorBody = vec4(1,1,1,1);
		pRenderInfo->m_ColorFeet = vec4(1,1,1,1);
	}

	pRenderInfo->m_ColorBody.a = 0.5f;
	pRenderInfo->m_ColorFeet.a = 0.5f;
	pRenderInfo->m_Size = 64;
}

void CGhost::StartRecord()
{
	m_Recording = true;
	m_CurGhost.m_Path.clear();
	CGameClient::CClientData ClientData = m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientID];
	str_copy(m_CurGhost.m_aOwner, g_Config.m_PlayerName, sizeof(m_CurGhost.m_aOwner));
	InitRenderInfos(&m_CurGhost.m_RenderInfo, ClientData.m_aSkinName, ClientData.m_UseCustomColor, ClientData.m_ColorBody, ClientData.m_ColorFeet);

	if(g_Config.m_ClRaceSaveGhost)
	{
		Client()->GhostRecorder_Start();

		CGhostSkin Skin;
		StrToInts(&Skin.m_Skin0, 6, ClientData.m_aSkinName);
		Skin.m_UseCustomColor = ClientData.m_UseCustomColor;
		Skin.m_ColorBody = ClientData.m_ColorBody;
		Skin.m_ColorFeet = ClientData.m_ColorFeet;
		GhostRecorder()->WriteData(GHOSTDATA_TYPE_SKIN, (const char*)&Skin, sizeof(Skin));
	}
}

void CGhost::StopRecord(int Time)
{
	m_Recording = false;
	if(GhostRecorder()->IsRecording())
		GhostRecorder()->Stop(m_CurGhost.m_Path.size(), Time);
}

void CGhost::StartRender()
{
	m_CurPos = 0;
	m_Rendering = true;
	m_StartRenderTick = Client()->PredGameTick();
}

void CGhost::StopRender()
{
	m_Rendering = false;
}

void CGhost::Save(bool WasRecording)
{
	// remove old ghost from list (TODO: remove other ghosts?)
	if(m_pClient->m_pMenus->m_OwnGhost)
	{
		if(WasRecording)
			Storage()->RemoveFile(m_pClient->m_pMenus->m_OwnGhost->m_aFilename, IStorage::TYPE_SAVE);

		m_pClient->m_pMenus->m_lGhosts.remove(*m_pClient->m_pMenus->m_OwnGhost);
	}

	char aFilename[128] = {0};
	if(WasRecording)
	{
		char aOldFilename[128];

		// rename ghost
		Client()->Ghost_GetPath(aFilename, sizeof(aFilename), m_BestTime);
		Client()->Ghost_GetPath(aOldFilename, sizeof(aOldFilename));
		Storage()->RenameFile(aOldFilename, aFilename, IStorage::TYPE_SAVE);
	}

	// create ghost item
	CMenus::CGhostItem Item;
	str_copy(Item.m_aFilename, aFilename, sizeof(Item.m_aFilename));
	str_copy(Item.m_aPlayer, g_Config.m_PlayerName, sizeof(Item.m_aPlayer));
	Item.m_Time = m_BestTime;
	Item.m_Active = true;
	Item.m_ID = -1;

	// add item to list
	m_pClient->m_pMenus->m_lGhosts.add(Item);
	m_pClient->m_pMenus->m_OwnGhost = &find_linear(m_pClient->m_pMenus->m_lGhosts.all(), Item).front();
}

bool CGhost::Load(const char* pFilename, int ID)
{
	if(!Client()->GhostLoader_Load(pFilename))
		return false;

	// read header
	CGhostHeader Header = GhostLoader()->GetHeader();

	int NumTicks = GhostLoader()->GetTicks(Header);
	int Time = GhostLoader()->GetTime(Header);
	if(NumTicks <= 0 || Time <= 0)
	{
		GhostLoader()->Close();
		return false;
	}

	// create ghost
	CGhostItem Ghost;
	Ghost.m_ID = ID;
	Ghost.m_Path.clear();
	Ghost.m_Path.set_size(NumTicks);

	// read client info
	str_copy(Ghost.m_aOwner, Header.m_aOwner, sizeof(Ghost.m_aOwner));

	int Index = 0;
	bool FoundSkin = false;

	// read data
	int Type;
	while(GhostLoader()->ReadNextType(&Type))
	{
		if(Type == GHOSTDATA_TYPE_SKIN)
		{
			CGhostSkin Skin;
			if(GhostLoader()->ReadData(Type, (char*)&Skin, sizeof(Skin)) && !FoundSkin)
			{
				FoundSkin = true;
				char aSkinName[64];
				IntsToStr(&Skin.m_Skin0, 6, aSkinName);
				InitRenderInfos(&Ghost.m_RenderInfo, aSkinName, Skin.m_UseCustomColor, Skin.m_ColorBody, Skin.m_ColorFeet);
			}
		}
		else if(Type == GHOSTDATA_TYPE_CHARACTER)
		{
			CGhostCharacter Char;
			if(GhostLoader()->ReadData(Type, (char*)&Char, sizeof(Char)))
				Ghost.m_Path[Index++] = Char;
		}
	}

	GhostLoader()->Close();

	if(Index != NumTicks)
		return false;

	if(ID == -1)
		m_BestTime = Time;
	if(!FoundSkin)
		InitRenderInfos(&Ghost.m_RenderInfo, "default", 0, 0, 0);
	m_lGhosts.add(Ghost);
	return true;
}

void CGhost::Unload(int ID)
{
	CGhostItem Item(ID);
	m_lGhosts.remove_fast(Item);
}

void CGhost::ConGPlay(IConsole::IResult *pResult, void *pUserData)
{
	((CGhost *)pUserData)->StartRender();
}

void CGhost::OnConsoleInit()
{
	Console()->Register("gplay","", CFGFLAG_CLIENT, ConGPlay, this, "");
}

void CGhost::OnMessage(int MsgType, void *pRawMsg)
{
	// only for race
	if(!m_pClient->m_IsRace || !g_Config.m_ClRaceGhost || m_pClient->m_Snap.m_SpecInfo.m_Active)
		return;

	// check for messages from server
	if(MsgType == NETMSGTYPE_SV_KILLMSG)
	{
		CNetMsg_Sv_KillMsg *pMsg = (CNetMsg_Sv_KillMsg *)pRawMsg;
		if(pMsg->m_Victim == m_pClient->m_Snap.m_LocalClientID)
		{
			if(m_RaceState != RACE_FINISHED)
				OnReset();
		}
	}
	else if(MsgType == NETMSGTYPE_SV_CHAT)
	{
		CNetMsg_Sv_Chat *pMsg = (CNetMsg_Sv_Chat *)pRawMsg;
		if(pMsg->m_ClientID == -1 && m_RaceState == RACE_STARTED)
		{
			const char* pMessage = pMsg->m_pMessage;
			
			int Num = 0;
			while(str_comp_num(pMessage, " finished in: ", 14))
			{
				pMessage++;
				Num++;
				if(!pMessage[0])
					return;
			}
			
			// store the name
			char aName[64];
			str_copy(aName, pMsg->m_pMessage, Num+1);
			
			// prepare values and state for saving
			int Minutes, Seconds, MSec;
			if(!str_comp(aName, m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientID].m_aName) &&
				sscanf(pMessage, " finished in: %d minute(s) %d.%03d", &Minutes, &Seconds, &MSec) == 3)
			{
				m_RaceState = RACE_FINISHED;
				int CurTime = Minutes * 60 * 1000 + Seconds * 1000 + MSec;
				if(m_Recording && (CurTime < m_BestTime || m_BestTime == -1))
				{
					m_NewRecord = true;
					m_BestTime = CurTime;
				}
			}
		}
	}
}

void CGhost::OnReset()
{
	StopRecord();
	StopRender();
	m_RaceState = RACE_NONE;
	m_NewRecord = false;
	m_CurGhost.m_Path.clear();
	m_StartRenderTick = -1;

	char aFilename[512];
	Client()->Ghost_GetPath(aFilename, sizeof(aFilename));
	Storage()->RemoveFile(aFilename, IStorage::TYPE_SAVE);
}

void CGhost::OnShutdown()
{
	OnReset();
}

void CGhost::OnMapLoad()
{
	OnReset();
	m_BestTime = -1;
	m_lGhosts.clear();
	m_pClient->m_pMenus->GhostlistPopulate();
}
