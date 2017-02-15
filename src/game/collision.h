/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_COLLISION_H
#define GAME_COLLISION_H

#include <base/vmath.h>

class CCollision
{
	class CTile *m_pTiles;
	class CTeleTile *m_pTele;
	class CSpeedupTile *m_pSpeedup;
	int m_Width;
	int m_Height;
	class CLayers *m_pLayers;

	vec2 *m_pTeleporter;
	bool m_MainTiles;
	bool m_StopTiles;

	void InitTeleporter();

	bool IsTileSolid(int x, int y);
	int GetTile(int x, int y);

	bool IsRaceTile(int TilePos, int Mask);

public:
	enum
	{
		COLFLAG_SOLID=1,
		COLFLAG_DEATH=2,
		COLFLAG_NOHOOK=4,

		RACECHECK_TILES_MAIN=1,
		RACECHECK_TILES_STOP=2,
		RACECHECK_TELE=4,
		RACECHECK_SPEEDUP=8,
	};

	CCollision();
	virtual ~CCollision();
	void Init(class CLayers *pLayers);
	bool CheckPoint(float x, float y) { return IsTileSolid(round_to_int(x), round_to_int(y)); }
	bool CheckPoint(vec2 Pos) { return CheckPoint(Pos.x, Pos.y); }
	int GetCollisionAt(float x, float y) { return GetTile(round_to_int(x), round_to_int(y)); }
	int GetWidth() { return m_Width; };
	int GetHeight() { return m_Height; };
	int IntersectLine(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision);
	void MovePoint(vec2 *pInoutPos, vec2 *pInoutVel, float Elasticity, int *pBounces);
	void MoveBox(vec2 *pInoutPos, vec2 *pInoutVel, vec2 Size, float Elasticity);
	bool TestBox(vec2 Pos, vec2 Size);
	
	// race
	int GetTilePos(vec2 Pos);
	vec2 GetPos(int TilePos);
	int GetIndex(vec2 Pos);
	int GetIndex(int TilePos);

	int CheckRaceTile(vec2 PrevPos, vec2 Pos, int Mask);

	int CheckCheckpoint(int TilePos);
	int CheckSpeedup(int TilePos);
	void GetSpeedup(int SpeedupPos, vec2 *Dir, int *Force);
	int CheckTeleport(int TilePos);
	vec2 GetTeleportDestination(int Number);
};

#endif
