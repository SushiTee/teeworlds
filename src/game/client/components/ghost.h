/* (c) Rajh, Redix and Sushi. */

#ifndef GAME_CLIENT_COMPONENTS_GHOST_H
#define GAME_CLIENT_COMPONENTS_GHOST_H

#include <base/tl/array.h>

#include <game/client/component.h>
#include <game/ghost.h>

#include "menus.h"

class CGhost : public CComponent
{
private:
	enum
	{
		MAX_ACTIVE_GHOSTS = 16,
	};

	class CGhostPath
	{
		int m_ChunkSize;
		int m_NumItems;

		array<CGhostCharacter*> m_lChunks;

		void Copy(const CGhostPath &other);

	public:
		CGhostPath() { Reset(); }
		~CGhostPath() { Reset(); }
		CGhostPath(const CGhostPath &Other) { Copy(Other); };
		CGhostPath &operator = (const CGhostPath &Other) { Copy(Other); return *this; };

		void Reset(int ChunkSize = 25 * 60); // one minute with default snap rate
		void SetSize(int Items);
		int Size() const { return m_NumItems; }

		void Add(CGhostCharacter Char);
		CGhostCharacter *Get(int Index);
	};

	class CGhostItem
	{
	public:
		CTeeRenderInfo m_RenderInfo;
		CGhostSkin m_Skin;
		CGhostPath m_Path;
		int m_StartTick;
		char m_aPlayer[MAX_NAME_LENGTH];
		int m_PlaybackPos;
		bool m_Mirror;
		int m_Team;

		CGhostItem() { Reset(); }

		void AutoMirror(int Team) { m_Mirror = Team != TEAM_SPECTATORS && m_Team == (Team ^ 1); }
		bool Empty() const { return m_Path.Size() == 0; }
		void Reset()
		{
			m_Path.Reset();
			m_StartTick = -1;
			m_PlaybackPos = -1;
			m_Mirror = false;
			m_Team = -1;
		}
	};

	static const char *ms_pGhostDir;

	class IGhostLoader *m_pGhostLoader;
	class IGhostRecorder *m_pGhostRecorder;

	CGhostItem m_aActiveGhosts[MAX_ACTIVE_GHOSTS];
	CGhostItem m_CurGhost;

	char m_aTmpFilename[128];

	int m_NewRenderTick;
	int m_StartRenderTick;
	int m_LastDeathTick;
	bool m_Recording;
	bool m_Rendering;

	bool m_SymmetricMap;
	bool m_AllowRestart;

	void GetPath(char *pBuf, int Size, const char *pPlayerName, int Time = -1) const;

	void AddInfos(const CNetObj_Character *pChar);
	int GetSlot() const;

	void MirrorChar(CNetObj_Character *pChar, int Middle);

	void StartRecord(int Tick);
	void StopRecord(int Time = -1);
	void StartRender(int Tick);
	void StopRender();

	void RenderGhostNamePlate(const CNetObj_Character *pPrev, const CNetObj_Character *pPlayer, float IntraTick, const char *pName);
	void InitRenderInfos(CGhostItem *pGhost);
	bool AutoMirroring() const;

	static void ConGPlay(IConsole::IResult *pResult, void *pUserData);

public:
	CGhost();

	virtual void OnRender();
	virtual void OnConsoleInit();
	virtual void OnReset();
	virtual void OnMessage(int MsgType, void *pRawMsg);
	virtual void OnMapLoad();

	void OnNewSnapshot(bool Predicted = false);
	void OnTeamJoin(int Team);

	int FreeSlot() const;
	int Load(const char *pFilename);
	void Unload(int Slot);
	void UnloadAll();

	void SaveGhost(CMenus::CGhostItem *pItem);

	const char *GetGhostDir() const { return ms_pGhostDir; }

	void ToggleMirror(int Slot) { m_aActiveGhosts[Slot].m_Mirror = !m_aActiveGhosts[Slot].m_Mirror; };
	bool IsMirrored(int Slot) const { return m_aActiveGhosts[Slot].m_Mirror; };

	bool IsMapSymmetric() const { return m_SymmetricMap; }

	class IGhostLoader *GhostLoader() const { return m_pGhostLoader; }
	class IGhostRecorder *GhostRecorder() const { return m_pGhostRecorder; }
};

#endif
