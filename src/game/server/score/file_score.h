/* copyright (c) 2008 rajh and gregwar. Score stuff */

#ifndef GAME_SERVER_FILESCORE_H
#define GAME_SERVER_FILESCORE_H

#include <base/tl/sorted_array.h>
#include "../score.h"

class CFileScore : public IScore
{
	CGameContext *m_pGameServer;
	IServer *m_pServer;
	
	class CPlayerScore
	{
	public:
		char m_aName[MAX_NAME_LENGTH];
		int m_Time;
		char m_aIP[16];
		int m_aCpTime[NUM_CHECKPOINTS];
		
		CPlayerScore() {};
		CPlayerScore(const char *pName, int Time, const char *pIP, int *apCpTime);
		
		bool operator<(const CPlayerScore& other) const { return (this->m_Time < other.m_Time); }
	};
	
	sorted_array<CPlayerScore> m_Top;
	
	CGameContext *GameServer() { return m_pGameServer; }
	IServer *Server() { return m_pServer; }
	
	CPlayerScore *SearchScoreByID(int ID, int *pPosition=0);
	CPlayerScore *SearchScoreByName(const char *pName, int *pPosition, bool ExactMatch);
	
	void Init();
	void Save();
	static void SaveScoreThread(void *pUser);

	void WriteEntry(IOHANDLE File, const CPlayerScore *pEntry) const;
	IOHANDLE OpenFile(int Flags) const;
	
public:
	CFileScore(CGameContext *pGameServer);
	~CFileScore();
	
	void LoadScore(int ClientID, bool PrintRank);
	void SaveScore(int ClientID, int Time, int *pCpTime, bool NewRecord);
	
	void ShowTop5(int ClientID, int Debut=1);
	void ShowRank(int ClientID, const char *pName, bool Search=false);
};

#endif
