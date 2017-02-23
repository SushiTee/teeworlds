/* CSqlScore class by Sushi */
#if defined(CONF_SQL)

#include <engine/shared/config.h>

#include <game/teerace.h>

#include "../gamecontext.h"
#include "sql_score.h"

// TODO: rework this

static LOCK gs_SqlLock = 0;

CSqlScore::CSqlScore(CGameContext *pGameServer)
: m_pGameServer(pGameServer),
  m_pServer(pGameServer->Server())
{
	str_copy(m_SqlConfig.m_aDatabase, g_Config.m_SvSqlDatabase, sizeof(m_SqlConfig.m_aDatabase));
	str_copy(m_SqlConfig.m_aUser, g_Config.m_SvSqlUser, sizeof(m_SqlConfig.m_aUser));
	str_copy(m_SqlConfig.m_aPass, g_Config.m_SvSqlPw, sizeof(m_SqlConfig.m_aPass));
	str_copy(m_SqlConfig.m_aIp, g_Config.m_SvSqlIp, sizeof(m_SqlConfig.m_aIp));
	m_SqlConfig.m_Port = g_Config.m_SvSqlPort;

	str_copy(m_aPrefix, g_Config.m_SvSqlPrefix, sizeof(m_aPrefix));

	m_aMap[0] = 0;
	
	if(gs_SqlLock == 0)
		gs_SqlLock = lock_create();
}

CSqlScore::~CSqlScore()
{
	lock_wait(gs_SqlLock);
	lock_unlock(gs_SqlLock);
}

bool CSqlScore::Connect()
{
	try 
	{
		// Create connection
		m_pDriver = get_driver_instance();
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "tcp://%s:%d", m_SqlConfig.m_aIp, m_SqlConfig.m_Port);
		m_pConnection = m_pDriver->connect(aBuf, m_SqlConfig.m_aUser, m_SqlConfig.m_aPass);
		
		// Create Statement
		m_pStatement = m_pConnection->createStatement();
		
		// Create database if not exists
		str_format(aBuf, sizeof(aBuf), "CREATE DATABASE IF NOT EXISTS %s", m_SqlConfig.m_aDatabase);
		m_pStatement->execute(aBuf);
		
		// Connect to specific database
		m_pConnection->setSchema(m_SqlConfig.m_aDatabase);
		dbg_msg("SQL", "SQL connection established");
		return true;
	} 
	catch (sql::SQLException &e)
	{
		dbg_msg("SQL", "ERROR: SQL connection failed");
		return false;
	}
	return false;
}

void CSqlScore::Disconnect()
{
	try
	{
		delete m_pConnection;
		dbg_msg("SQL", "SQL connection disconnected");
	}
	catch (sql::SQLException &e)
	{
		dbg_msg("SQL", "ERROR: No SQL connection");
	}
}

// create tables... should be done only once
void CSqlScore::OnMapLoad()
{
	IScore::OnMapLoad();

	str_copy(m_aMap, m_pServer->GetMapName(), sizeof(m_aMap));
	ClearString(m_aMap, sizeof(m_aMap));

	// create connection
	if(Connect())
	{
		try
		{
			// create tables
			char aBuf[768];
			str_format(aBuf, sizeof(aBuf), "CREATE TABLE IF NOT EXISTS %s_%s_race (Name VARCHAR(31) NOT NULL, Time INTEGER DEFAULT 0, IP VARCHAR(16) DEFAULT '0.0.0.0', cp1 INTEGER DEFAULT 0, cp2 INTEGER DEFAULT 0, cp3 INTEGER DEFAULT 0, cp4 INTEGER DEFAULT 0, cp5 INTEGER DEFAULT 0, cp6 INTEGER DEFAULT 0, cp7 INTEGER DEFAULT 0, cp8 INTEGER DEFAULT 0, cp9 INTEGER DEFAULT 0, cp10 INTEGER DEFAULT 0, cp11 INTEGER DEFAULT 0, cp12 INTEGER DEFAULT 0, cp13 INTEGER DEFAULT 0, cp14 INTEGER DEFAULT 0, cp15 INTEGER DEFAULT 0, cp16 INTEGER DEFAULT 0, cp17 INTEGER DEFAULT 0, cp18 INTEGER DEFAULT 0, cp19 INTEGER DEFAULT 0, cp20 INTEGER DEFAULT 0, cp21 INTEGER DEFAULT 0, cp22 INTEGER DEFAULT 0, cp23 INTEGER DEFAULT 0, cp24 INTEGER DEFAULT 0, cp25 INTEGER DEFAULT 0);", m_aPrefix, m_aMap);
			m_pStatement->execute(aBuf);
			dbg_msg("SQL", "Tables were created successfully");
			
			// get the best time
			str_format(aBuf, sizeof(aBuf), "SELECT Time FROM %s_%s_race ORDER BY `Time` ASC LIMIT 0, 1;", m_aPrefix, m_aMap);
			m_pResults = m_pStatement->executeQuery(aBuf);
			
			if(m_pResults->next())
			{
				UpdateRecord(m_pResults->getInt("Time"));
				dbg_msg("SQL", "Getting best time on server done");
			
				// delete results
				delete m_pResults;
			}
				
			// delete statement
			delete m_pStatement;
		}
		catch (sql::SQLException &e)
		{
			dbg_msg("SQL", "ERROR: Tables were NOT created");
		}

		// disconnect from database
		Disconnect();
	}
}

// update stuff
void CSqlScore::LoadScoreThread(void *pUser)
{
	lock_wait(gs_SqlLock);
	
	CSqlScoreData *pData = (CSqlScoreData *)pUser;
	CSqlScore *pScore = pData->m_pSqlData;
	const CSqlConfig *pSqlConfig = &pScore->m_SqlConfig;
	
	// Connect to database
	if(pScore->Connect())
	{
		try
		{
			// check strings
			pScore->ClearString(pData->m_aName, sizeof(pData->m_aName));
			
			char aBuf[512];
			// check if there is an entry with the same ip
			if(g_Config.m_SvScoreIP)
			{
				str_format(aBuf, sizeof(aBuf), "SELECT * FROM %s_%s_race WHERE IP='%s';", pScore->m_aPrefix, pScore->m_aMap, pData->m_aIP);
				pScore->m_pResults = pScore->m_pStatement->executeQuery(aBuf);
				
				if(pScore->m_pResults->next())
				{
					// get the best time
					int Time = pScore->m_pResults->getInt("Time");
					pScore->m_aPlayerData[pData->m_ClientID].m_Time = Time;
					if(g_Config.m_SvCheckpointSave)
					{
						char aColumn[8];
						for(int i = 0; i < NUM_CHECKPOINTS; i++)
						{
							str_format(aColumn, sizeof(aColumn), "cp%d", i+1);
							pScore->m_aPlayerData[pData->m_ClientID].m_aCpTime[i] = pScore->m_pResults->getInt(aColumn);
						}
					}

					if(g_Config.m_SvShowBest)
					{
						pScore->m_aPlayerData[pData->m_ClientID].UpdateCurTime(Time);
						int SendTo = g_Config.m_SvShowTimes ? -1 : pData->m_ClientID;
						pScore->GameServer()->SendPlayerTime(SendTo, Time, pData->m_ClientID);
					}
					
					dbg_msg("SQL", "Getting best time done");
				
					// delete statement and results
					delete pScore->m_pStatement;
					delete pScore->m_pResults;
				
					// disconnect from database
					pScore->Disconnect();
					
					delete pData;

					lock_unlock(gs_SqlLock);
					
					return;
				}
				
			}
		
			str_format(aBuf, sizeof(aBuf), "SELECT * FROM %s_%s_race WHERE Name='%s';", pScore->m_aPrefix, pScore->m_aMap, pData->m_aName);
			pScore->m_pResults = pScore->m_pStatement->executeQuery(aBuf);
			if(pScore->m_pResults->next())
			{
				// check if IP differs
				const char *pIP = pScore->m_pResults->getString("IP").c_str();
				if(str_comp(pIP, pData->m_aIP) != 0)
				{
					// set the new ip
					str_format(aBuf, sizeof(aBuf), "UPDATE %s_%s_race SET IP='%s' WHERE Name='%s';", pScore->m_aPrefix, pScore->m_aMap, pData->m_aIP, pData->m_aName);
					pScore->m_pStatement->execute(aBuf);
				}
				
				// get the best time
				int Time = pScore->m_pResults->getInt("Time");
				pScore->m_aPlayerData[pData->m_ClientID].m_Time = Time;
				if(g_Config.m_SvCheckpointSave)
				{
					char aColumn[8];
					for(int i = 0; i < NUM_CHECKPOINTS; i++)
					{
						str_format(aColumn, sizeof(aColumn), "cp%d", i+1);
						pScore->m_aPlayerData[pData->m_ClientID].m_aCpTime[i] = pScore->m_pResults->getInt(aColumn);
					}
				}

				if(g_Config.m_SvShowBest)
				{
					pScore->m_aPlayerData[pData->m_ClientID].UpdateCurTime(Time);
					int SendTo = g_Config.m_SvShowTimes ? -1 : pData->m_ClientID;
					pScore->GameServer()->SendPlayerTime(SendTo, Time, pData->m_ClientID);
				}
			}
			
			dbg_msg("SQL", "Getting best time done");
			
			// delete statement and results
			delete pScore->m_pStatement;
			delete pScore->m_pResults;
		}
		catch (sql::SQLException &e)
		{
			dbg_msg("SQL", "ERROR: Could not update account");
		}
		
		// disconnect from database
		pScore->Disconnect();
	}
	
	delete pData;

	lock_unlock(gs_SqlLock);
}

void CSqlScore::OnPlayerInit(int ClientID, bool PrintRank)
{
	m_aPlayerData[ClientID].Reset();

	CSqlScoreData *Tmp = new CSqlScoreData();
	Tmp->m_ClientID = ClientID;
	str_copy(Tmp->m_aName, Server()->ClientName(ClientID), sizeof(Tmp->m_aName));
	Server()->GetClientAddr(ClientID, Tmp->m_aIP, sizeof(Tmp->m_aIP));
	Tmp->m_pSqlData = this;
	
	void *LoadThread = thread_init(LoadScoreThread, Tmp);
	thread_detach(LoadThread);
}

void CSqlScore::SaveScoreThread(void *pUser)
{
	lock_wait(gs_SqlLock);
	
	CSqlScoreData *pData = (CSqlScoreData *)pUser;
	CSqlScore *pScore = pData->m_pSqlData;
	const CSqlConfig *pSqlConfig = &pScore->m_SqlConfig;
	
	// Connect to database
	if(pScore->Connect())
	{
		try
		{
			// check strings
			pScore->ClearString(pData->m_aName, sizeof(pData->m_aName));
			
			char aBuf[768];
			
			// fisrt check for IP
			str_format(aBuf, sizeof(aBuf), "SELECT * FROM %s_%s_race WHERE IP='%s';", pScore->m_aPrefix, pScore->m_aMap, pData->m_aIP);
			pScore->m_pResults = pScore->m_pStatement->executeQuery(aBuf);
			
			// if ip found...
			if(pScore->m_pResults->next())
			{
				// update time
				if(g_Config.m_SvCheckpointSave)
					str_format(aBuf, sizeof(aBuf), "UPDATE %s_%s_race SET Name='%s', Time='%d', cp1='%d', cp2='%d', cp3='%d', cp4='%d', cp5='%d', cp6='%d', cp7='%d', cp8='%d', cp9='%d', cp10='%d', cp11='%d', cp12='%d', cp13='%d', cp14='%d', cp15='%d', cp16='%d', cp17='%d', cp18='%d', cp19='%d', cp20='%d', cp21='%d', cp22='%d', cp23='%d', cp24='%d', cp25='%d' WHERE IP='%s';", pScore->m_aPrefix, pScore->m_aMap, pData->m_aName, pData->m_Time, pData->m_aCpCurrent[0], pData->m_aCpCurrent[1], pData->m_aCpCurrent[2], pData->m_aCpCurrent[3], pData->m_aCpCurrent[4], pData->m_aCpCurrent[5], pData->m_aCpCurrent[6], pData->m_aCpCurrent[7], pData->m_aCpCurrent[8], pData->m_aCpCurrent[9], pData->m_aCpCurrent[10], pData->m_aCpCurrent[11], pData->m_aCpCurrent[12], pData->m_aCpCurrent[13], pData->m_aCpCurrent[14], pData->m_aCpCurrent[15], pData->m_aCpCurrent[16], pData->m_aCpCurrent[17], pData->m_aCpCurrent[18], pData->m_aCpCurrent[19], pData->m_aCpCurrent[20], pData->m_aCpCurrent[21], pData->m_aCpCurrent[22], pData->m_aCpCurrent[23], pData->m_aCpCurrent[24], pData->m_aIP);
				else
					str_format(aBuf, sizeof(aBuf), "UPDATE %s_%s_race SET Name='%s', Time='%d' WHERE IP='%s';", pScore->m_aPrefix, pScore->m_aMap, pData->m_aName, pData->m_Time, pData->m_aIP);
				pScore->m_pStatement->execute(aBuf);
				
				dbg_msg("SQL", "Updating time done");
				
				// delete results statement
				delete pScore->m_pResults;
				delete pScore->m_pStatement;
				
				// disconnect from database
				pScore->Disconnect();
				
				delete pData;
				
				lock_unlock(gs_SqlLock);
				
				return;
			}
			
			// TODO: search for name
			
			// if no entry found... create a new one
			str_format(aBuf, sizeof(aBuf), "INSERT IGNORE INTO %s_%s_race(Name, IP, Time, cp1, cp2, cp3, cp4, cp5, cp6, cp7, cp8, cp9, cp10, cp11, cp12, cp13, cp14, cp15, cp16, cp17, cp18, cp19, cp20, cp21, cp22, cp23, cp24, cp25) VALUES ('%s', '%s', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d');", pScore->m_aPrefix, pScore->m_aMap, pData->m_aName, pData->m_aIP, pData->m_Time, pData->m_aCpCurrent[0], pData->m_aCpCurrent[1], pData->m_aCpCurrent[2], pData->m_aCpCurrent[3], pData->m_aCpCurrent[4], pData->m_aCpCurrent[5], pData->m_aCpCurrent[6], pData->m_aCpCurrent[7], pData->m_aCpCurrent[8], pData->m_aCpCurrent[9], pData->m_aCpCurrent[10], pData->m_aCpCurrent[11], pData->m_aCpCurrent[12], pData->m_aCpCurrent[13], pData->m_aCpCurrent[14], pData->m_aCpCurrent[15], pData->m_aCpCurrent[16], pData->m_aCpCurrent[17], pData->m_aCpCurrent[18], pData->m_aCpCurrent[19], pData->m_aCpCurrent[20], pData->m_aCpCurrent[21], pData->m_aCpCurrent[22], pData->m_aCpCurrent[23], pData->m_aCpCurrent[24]);
			pScore->m_pStatement->execute(aBuf);
			
			dbg_msg("SQL", "Updateing time done");
			
			// delete results statement
			delete pScore->m_pResults;
			delete pScore->m_pStatement;
		}
		catch (sql::SQLException &e)
		{
			dbg_msg("SQL", "ERROR: Could not update time");
		}
		
		// disconnect from database
		pScore->Disconnect();
	}
	
	delete pData;

	lock_unlock(gs_SqlLock);
}

void CSqlScore::OnPlayerFinish(int ClientID, int Time, int *pCpTime)
{
	bool NewPlayerRecord = m_aPlayerData[ClientID].UpdateTime(Time, pCpTime);
	if(UpdateRecord(Time) && g_Config.m_SvShowTimes)
		GameServer()->SendRecord(-1);

	if(!NewPlayerRecord)
		return;

	CSqlScoreData *Tmp = new CSqlScoreData();
	Tmp->m_ClientID = ClientID;
	Tmp->m_Time = Time;
	mem_copy(Tmp->m_aCpCurrent, pCpTime, sizeof(Tmp->m_aCpCurrent));
	str_copy(Tmp->m_aName, Server()->ClientName(ClientID), sizeof(Tmp->m_aName));
	Server()->GetClientAddr(ClientID, Tmp->m_aIP, sizeof(Tmp->m_aIP));
	Tmp->m_pSqlData = this;
	
	void *SaveThread = thread_init(SaveScoreThread, Tmp);
	thread_detach(SaveThread);
}

void CSqlScore::ShowRankThread(void *pUser)
{
	lock_wait(gs_SqlLock);
	
	CSqlScoreData *pData = (CSqlScoreData *)pUser;
	CSqlScore *pScore = pData->m_pSqlData;
	const CSqlConfig *pSqlConfig = &pScore->m_SqlConfig;
	
	// Connect to database
	if(pScore->Connect())
	{
		try
		{
			// check strings
			pScore->ClearString(pData->m_aName, sizeof(pData->m_aName));
			
			// TODO: use sql for searching

			// check sort methode
			char aBuf[512];
			str_format(aBuf, sizeof(aBuf), "SELECT Name, IP, Time FROM %s_%s_race ORDER BY `Time` ASC;", pScore->m_aPrefix, pScore->m_aMap);
			pScore->m_pResults = pScore->m_pStatement->executeQuery(aBuf);
			int RowCount = 0;
			bool Found = false;
			while(pScore->m_pResults->next())
			{
				RowCount++;
				
				if(pData->m_Search)
				{
					if(str_find_nocase(pScore->m_pResults->getString("Name").c_str(), pData->m_aName))
					{
						Found = true;
						break;
					}
				}
				else if(!str_comp(pScore->m_pResults->getString("IP").c_str(), pData->m_aIP))
				{
					Found = true;
					break;
				}
			}

			bool Public = false;

			if(Found)
			{
				Public = g_Config.m_SvShowTimes;
				char aTime[64];
				IRace::FormatTimeLong(aTime, sizeof(aTime), pScore->m_pResults->getInt("Time"));
				if(!Public)
					str_format(aBuf, sizeof(aBuf), "Your time: %s", aTime);
				else
					str_format(aBuf, sizeof(aBuf), "%d. %s Time: %s",
						RowCount, pScore->m_pResults->getString("Name").c_str(), aTime);
				if(pData->m_Search)
					str_append(aBuf, pData->m_aRequestingPlayer, sizeof(aBuf));
			}
			else
				str_format(aBuf, sizeof(aBuf), "%s is not ranked", pData->m_aName);

			if(Public)
				pScore->GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
			else
				pScore->GameServer()->SendChatTarget(pData->m_ClientID, aBuf);
			
			dbg_msg("SQL", "Showing rank done");
			
			// delete results and statement
			delete pScore->m_pResults;	
			delete pScore->m_pStatement;
		}
		catch (sql::SQLException &e)
		{
			dbg_msg("SQL", "ERROR: Could not show rank");
		}
		
		// disconnect from database
		pScore->Disconnect();
	}
	
	delete pData;

	lock_unlock(gs_SqlLock);
}

void CSqlScore::ShowRank(int ClientID, const char *pName, bool Search)
{
	CSqlScoreData *Tmp = new CSqlScoreData();
	Tmp->m_ClientID = ClientID;
	str_copy(Tmp->m_aName, pName, sizeof(Tmp->m_aName));
	Server()->GetClientAddr(ClientID, Tmp->m_aIP, sizeof(Tmp->m_aIP));
	Tmp->m_Search = Search;
	str_format(Tmp->m_aRequestingPlayer, sizeof(Tmp->m_aRequestingPlayer), " (%s)", Server()->ClientName(ClientID));
	Tmp->m_pSqlData = this;
	
	void *RankThread = thread_init(ShowRankThread, Tmp);
	thread_detach(RankThread);
}

void CSqlScore::ShowTop5Thread(void *pUser)
{
	lock_wait(gs_SqlLock);
	
	CSqlScoreData *pData = (CSqlScoreData *)pUser;
	CSqlScore *pScore = pData->m_pSqlData;
	const CSqlConfig *pSqlConfig = &pScore->m_SqlConfig;
	
	// Connect to database
	if(pScore->Connect())
	{
		try
		{
			// check sort methode
			char aBuf[512];
			str_format(aBuf, sizeof(aBuf), "SELECT Name, Time FROM %s_%s_race ORDER BY `Time` ASC LIMIT %d, 5;", pScore->m_aPrefix, pScore->m_aMap, pData->m_Num-1);
			pScore->m_pResults = pScore->m_pStatement->executeQuery(aBuf);
			
			// show top5
			pScore->GameServer()->SendChatTarget(pData->m_ClientID, "----------- Top 5 -----------");
			
			int Rank = pData->m_Num;
			while(pScore->m_pResults->next())
			{
				char aTime[64];
				IRace::FormatTimeLong(aTime, sizeof(aTime), pScore->m_pResults->getInt("Time"));
				str_format(aBuf, sizeof(aBuf), "%d. %s Time: %s",
					Rank, pScore->m_pResults->getString("Name").c_str(), aTime);
				pScore->GameServer()->SendChatTarget(pData->m_ClientID, aBuf);
				Rank++;
			}
			pScore->GameServer()->SendChatTarget(pData->m_ClientID, "------------------------------");
			
			dbg_msg("SQL", "Showing top5 done");
			
			// delete results and statement
			delete pScore->m_pResults;
			delete pScore->m_pStatement;
		}
		catch (sql::SQLException &e)
		{
			dbg_msg("SQL", "ERROR: Could not show top5");
		}
		
		// disconnect from database
		pScore->Disconnect();
	}
	
	delete pData;

	lock_unlock(gs_SqlLock);
}

void CSqlScore::ShowTop5(int ClientID, int Debut)
{
	CSqlScoreData *Tmp = new CSqlScoreData();
	Tmp->m_Num = Debut;
	Tmp->m_ClientID = ClientID;
	Tmp->m_pSqlData = this;
	
	void *Top5Thread = thread_init(ShowTop5Thread, Tmp);
	thread_detach(Top5Thread);
}

// anti SQL injection
void CSqlScore::ClearString(char *pString, int Size)
{
	// check if the string is long enough to escape something
	if(Size <= 2)
	{
		if(pString[0] == '\'' || pString[0] == '\\' || pString[0] == ';')
			pString[0] = '_';
		return;
	}
	
	// replace ' ' ' with ' \' ' and remove '\'
	for(int i = 0; i < str_length(pString); i++)
	{
		// replace '-' with '_'
		if(pString[i] == '-')
		{
			pString[i] = '_';
			continue;
		}
		
		// escape ', \ and ;
		if(pString[i] == '\'' || pString[i] == '\\' || pString[i] == ';')
		{
			for(int j = Size-2; j > i; j--)
				pString[j] = pString[j-1];
			pString[i] = '\\';
			i++; // so we dont double escape
			continue;
		}
	}

	// aaand remove spaces and \ at the end xD
	for(int i = str_length(pString)-1; i >= 0; i--)
	{
		if(pString[i] == ' ' || pString[i] == '\\')
			pString[i] = 0;
		else
			break;
	}
}

#endif
