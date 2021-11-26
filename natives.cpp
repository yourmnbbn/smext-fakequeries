#include "natives.h"

#define S2A_EXTRA_DATA_HAS_GAME_PORT (1 << 7)
#define S2A_EXTRA_DATA_HAS_GAMETAG_DATA (1 << 5)
#define S2A_EXTRA_DATA_GAMEID (1 << 0)
#define S2A_EXTRA_DATA_HAS_SPECTATOR_DATA (1 << 6)
#define S2A_EXTRA_DATA_HAS_STEAM_ID (1 << 4)

CReturnA2sInfo g_ReturnA2sInfo;
CReturnA2sPlayer g_ReturnA2sPlayer;

//https://github.com/alliedmodders/sourcemod/blob/1fbe5e1daaee9ba44164078fe7f59d862786e612/extensions/sdktools/vhelpers.cpp#L288
bool FindNestedDataTable(SendTable *pTable, const char *name)
{
    if (strcmp(pTable->GetName(), name) == 0)
    {
        return true;
    }

    int props = pTable->GetNumProps();
    SendProp *prop;

    for (int i=0; i<props; i++)
    {
        prop = pTable->GetProp(i);
        if (prop->GetDataTable())
        {
            if (FindNestedDataTable(prop->GetDataTable(), name))
            {
                return true;
            }
        }
    }

    return false;
}

//https://github.com/alliedmodders/sourcemod/blob/1fbe5e1daaee9ba44164078fe7f59d862786e612/extensions/sdktools/vglobals.cpp#L295
CBaseEntity* CReturnA2sPlayer::GetResourceEntity()
{
    if (gamehelpers->GetHandleEntity(m_ResourceEntity) != NULL)
        return gamehelpers->ReferenceToEntity(m_ResourceEntity.GetEntryIndex());
    else
        return nullptr;
}

void CReturnA2sPlayer::InitResourceEntity()
{
    if(!m_ResourceEntity.IsValid())
    {
        m_ResourceEntity.Term();
    
        int edictCount = gpGlobals->maxEntities;

        for (int i=0; i<edictCount; i++)
        {
            edict_t *pEdict;
            
            if (i >= 0 && i < gpGlobals->maxEntities)
                pEdict = (edict_t *)(gpGlobals->pEdicts + i);
            
            if (!pEdict || pEdict->IsFree())
                continue;

            if (!pEdict->GetNetworkable())
                continue;

            IHandleEntity *pHandleEnt = pEdict->GetNetworkable()->GetEntityHandle();
            
            if (!pHandleEnt)
                continue;

            ServerClass *pClass = pEdict->GetNetworkable()->GetServerClass();
            if (FindNestedDataTable(pClass->m_pTable, "DT_PlayerResource"))
            {
                m_ResourceEntity = pHandleEnt->GetRefEHandle();
                break;
            }
        }
    }
}

bool CReturnA2sPlayer::IsOfficialRequest(char* requestBuf)
{
    return *reinterpret_cast<uint32_t*>((uintptr_t)requestBuf + 5) == 0;
}

bool CReturnA2sPlayer::SetChallengeNumber(uint32_t number, bool bDefault)
{
    if(bDefault)
    {
        m_bDefaultChallengeNumber = true;
        return true;
    }
    
    if(number == 0xFFFFFFFF || number == 0)    
        return false;
    
    m_bDefaultChallengeNumber = false;
    m_ChallengeNumber = number;
    return true;
}

bool CReturnA2sPlayer::RemoveFakePlayer(uint8_t index)
{
    bool bFound = false;
    
    for(auto it = m_FakePlayers.begin(); it != m_FakePlayers.end();)
    {
        PlayerInfo_t& info = *it;
        
        if(info.index == index)
        {
            it = m_FakePlayers.erase(it);
            bFound = true;
        }
        else
        {
            it++;
        }
    }
    
    return bFound;
}

bool CReturnA2sPlayer::GetPlayerStatus(int iClientIndex, PlayerInfo_t& info)
{
    IGamePlayer* pPlayer = playerhelpers->GetGamePlayer(iClientIndex);
    
    if (!pPlayer)
        return false;
    
    if (!pPlayer->IsConnected())
        return false;
    
    info.index = iClientIndex;
    info.name = pPlayer->GetName();
    
    INetChannelInfo* pInfo = engine->GetPlayerNetInfo(iClientIndex);
    
    if(pInfo)
        info.playTime = pInfo->GetTimeConnected();
    else
        info.playTime = Plat_FloatTime();
    
    //Get player score
    CBaseEntity* pEntity = GetResourceEntity();
    
    if(!pEntity)
    {
        info.score = 0;
        return true;
    }
    
    static unsigned int offset = 0;

    if (offset == 0)
    {
        sm_sendprop_info_t info;
        gamehelpers->FindSendPropInfo("CCSPlayerResource", "m_iScore", &info);
        offset = info.actual_offset;
    }
    
    info.score  = *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(pEntity) + offset + iClientIndex*4);
    return true;
}


void CReturnA2sPlayer::BuildCommunicationFrame()
{
    int maxClients = g_pServer->GetNumClients();
    int iPlayerWritten = 0;
    
    m_replyPacket.Reset();
    m_TotalClientsCount = maxClients + (m_FakePlayers.size() > m_FakePlayerDisplayNum ? m_FakePlayerDisplayNum : m_FakePlayers.size());
    
    if(m_TotalClientsCount >= 64)
        m_TotalClientsCount = 64;

    m_replyPacket.WriteLong(-1);        //0xFFFFFFFF
    m_replyPacket.WriteByte(68);        //A2S_PLAYER response header
    m_replyPacket.WriteByte(m_TotalClientsCount > 64 ? 64 : m_TotalClientsCount);
    
    //Loop real clients
    for(int i = 1; i <= playerhelpers->GetMaxClients(); i++)
    {
        PlayerInfo_t info;
        
        if(!GetPlayerStatus(i, info))
            continue;
        
        m_replyPacket.WriteByte(info.index);
        m_replyPacket.WriteString(info.name.c_str());
        m_replyPacket.WriteLong(info.score);
        m_replyPacket.WriteFloat(info.playTime);
        
        iPlayerWritten++;
    }
    
    if(iPlayerWritten >= 64)
        return;
    
    int iFakePlayerCount = 0;
    
    //Loop fake clients
    for(PlayerInfo_t info : m_FakePlayers)
    {
        m_replyPacket.WriteByte(info.index);
        m_replyPacket.WriteString(info.name.c_str());
        m_replyPacket.WriteLong(info.score);
        m_replyPacket.WriteFloat(Plat_FloatTime() - info.playTime);

        iPlayerWritten++;
        iFakePlayerCount++;
        
        if(iPlayerWritten >= 64)
            break;
        
        if(iFakePlayerCount >= m_FakePlayerDisplayNum)
            break;
    }
}

void CReturnA2sPlayer::BuildEngineDefaultFrame()
{
    int maxClients = g_ReturnA2sInfo.GetMaxClients();

    m_replyPacket.Reset();

    m_replyPacket.WriteLong(-1);        //0xFFFFFFFF
    m_replyPacket.WriteByte(68);        //A2S_PLAYER response header
    m_replyPacket.WriteByte(1);
    m_replyPacket.WriteByte(0);
    m_replyPacket.WriteString("Max Players");
    m_replyPacket.WriteLong(maxClients);
    m_replyPacket.WriteFloat(Plat_FloatTime());
}

void CReturnA2sInfo::ResetA2sInfo()
{
    m_bDefaultPassWord = true;
    m_bDefaultProtocolVersion = true;
    m_bDefaultServerName = true;
    m_bDefaultMapName = true;
    m_bDefaultFolderName = true;
    m_bDefaultGameDescription = true;
    m_bDefaultAppID = true;
    m_bDefaultNumClients = true;
    m_bDefaultMaxClients = true;
    m_bDefaultNumFakeClients = true;
    m_bDefaultOS = true;
    m_bDefaultVacStatus = true;
    m_bDefaultGameVersion = true;
    m_bDefaultServerTag = true;
    m_bDefaultEDF = true;
}

bool CReturnA2sInfo::ReadSteamINF()
{
    FILE *pFile;
    char buff[512];
    g_pSM->BuildPath(Path_Game, buff, 512, "steam.inf");
    pFile = fopen(buff, "r");
    if (pFile == NULL)
    {
        return false;
    }
    
    long lSize;     
    char * buffer;
    size_t result;
    
    // obtain file size:
    fseek (pFile , 0 , SEEK_END);
    lSize = ftell (pFile);
    rewind (pFile);

    // allocate memory to contain the whole file:
    buffer = (char*) malloc (sizeof(char)*lSize);
    if (buffer == NULL)
        return false;

    // copy the file into the buffer:
    result = fread (buffer,1,lSize,pFile);
    
    char info [4][12];
        
    int j = 0;

    for(int i = 0; i < 4; i++)
    {
        while(buffer[j++] != '=')
        {
        }
        
        int k = 0;
        
        while(buffer[j++] != '\n')
        {
            info[i][k++] = buffer[j - 1];
        }
        
        if(info[i][k-1] == '\r') 
            k -= 1;
        
        info[i][k] = '\0';
    }    
    strcpy(m_RealGameVersion, info[2]);
    strcpy(m_RealGameDir, g_pSM->GetGameFolderName());
    m_RealAppID = 730;
    fclose (pFile);
    free (buffer);
    
    return true;
}

void CReturnA2sInfo::GameRealDescription()
{
    const char *desc = gamedll->GetGameDescription();
    strcpy(m_RealGameDesc, desc);        
}

void CReturnA2sInfo::InitRealInformation()
{
    m_RealPort = g_pServer->GetUDPPort();
}

void CReturnA2sInfo::BuildCommunicationFrame()
{
    GameRealDescription();
    InitRealInformation();
    
    m_replyPacket.Reset();
    
    m_replyPacket.WriteLong(-1);        //0xFFFFFFFF
    m_replyPacket.WriteByte(73);        //A2S_INFO response header
    m_replyPacket.WriteByte(GetProtocolVersion());  //This is hardcoded in the leaked code, 17
    
    m_replyPacket.WriteString(GetServerName());
    m_replyPacket.WriteString(GetMapName());
    m_replyPacket.WriteString(GetGameFolderName());
    m_replyPacket.WriteString(GetGameDiscription());
    m_replyPacket.WriteShort(GetAppID());
    m_replyPacket.WriteByte(GetNumClients());
    m_replyPacket.WriteByte(GetMaxClients());
    m_replyPacket.WriteByte(GetNumFakeClients());
    m_replyPacket.WriteByte(100);       //'d' for dedicated server and 'l' for listen server

    m_replyPacket.WriteByte(GetOS());   //'w' for windows, 'l' for linux and 'm' for macos

    m_replyPacket.WriteByte(GetPassWord());
    m_replyPacket.WriteByte(GetVacStatus());
    m_replyPacket.WriteString(GetGameVersion());

    uint8_t extraData = GetEDF();

    CHLTVServer* hltv = nullptr;
    for (CActiveHltvServerIterator it; it; it.Next())
    {
        hltv = it; 
        break;
    }

    if(!hltv && (extraData & S2A_EXTRA_DATA_HAS_SPECTATOR_DATA))
        extraData &= ~S2A_EXTRA_DATA_HAS_SPECTATOR_DATA;

    m_replyPacket.WriteByte(extraData);       //extra flags

    if(extraData & S2A_EXTRA_DATA_HAS_GAME_PORT)
        m_replyPacket.WriteShort(m_RealPort);

    if(extraData & S2A_EXTRA_DATA_HAS_STEAM_ID)
    {
        ISteamGameServer* pSteamClientGameServer = SteamAPI_SteamGameServer();
        uint64_t steamID = SteamAPI_ISteamGameServer_GetSteamID(pSteamClientGameServer);
        m_replyPacket.WriteLongLong(steamID);
    }

    if(extraData & S2A_EXTRA_DATA_HAS_SPECTATOR_DATA)
    {
        m_replyPacket.WriteShort(hltv->GetUDPPort());

        const char* tv_name = g_pCvar->FindVar("tv_name")->GetString();
        m_replyPacket.WriteString(tv_name[0] ? tv_name : GetServerName());
    }

    if(extraData & S2A_EXTRA_DATA_HAS_GAMETAG_DATA)
        m_replyPacket.WriteString(GetServerTag());
    
    if(extraData & S2A_EXTRA_DATA_GAMEID)
        m_replyPacket.WriteLongLong(GetAppID());
}

void CReturnA2sInfo::SetPassWord(bool bHavePassword, bool bDefault)
{
    if(bDefault)
    {
        m_bDefaultPassWord = true;
        return;
    }
    
    m_bDefaultPassWord = false;
    m_iPassWord = bHavePassword ? 1 : 0;
}

uint8_t CReturnA2sInfo::GetPassWord()
{
    if(m_bDefaultPassWord)
    {
        const char* sPassword = g_pServer->GetPassword();
        
        if(sPassword)
            return strcmp(sPassword, "") == 0 ? 0 : 1;
        else
            return 0;
    }
    else
        return m_iPassWord;
}

void CReturnA2sInfo::SetProtocolVersion(uint8_t iVersion, bool bDefault)
{
    if(bDefault)
    {
        m_bDefaultProtocolVersion = true;
        return;
    }
    
    m_bDefaultProtocolVersion = false;
    m_iProtocolVersion = iVersion;
}

uint8_t CReturnA2sInfo::GetProtocolVersion()
{
    if(m_bDefaultProtocolVersion)
        return DEFAULT_PROTO_VERSION;
    else
        return m_iProtocolVersion;
}

void CReturnA2sInfo::SetServerName(const char* sName, bool bDefault)
{
    if(bDefault)
    {
        m_bDefaultServerName = true;
        return;
    }
    
    m_bDefaultServerName = false;
    strncpy(m_ServerName, sName, sizeof(m_ServerName));
}

const char* CReturnA2sInfo::GetServerName()
{
    if(m_bDefaultServerName)
        return g_pServer->GetName();
    else
        return (const char*)m_ServerName;
}

void CReturnA2sInfo::SetMapName(const char* sName, bool bDefault)
{
    if(bDefault)
    {
        m_bDefaultMapName = true;
        return;
    }
    
    m_bDefaultMapName = false;
    strncpy(m_MapName, sName, sizeof(m_MapName));
}

const char* CReturnA2sInfo::GetMapName()
{
    if(m_bDefaultMapName)
        return g_pServer->GetMapName();
    else
        return (const char*)m_MapName;
}

void CReturnA2sInfo::SetGameFolderName(const char* sName, bool bDefault)
{
    if(bDefault)
    {
        m_bDefaultFolderName = true;
        return;
    }
    
    m_bDefaultFolderName = false;
    strncpy(m_FolderName, sName, sizeof(m_FolderName));
}

const char* CReturnA2sInfo::GetGameFolderName()
{
    if(m_bDefaultFolderName)
        return (const char*)m_RealGameDir;
    else
        return (const char*)m_FolderName;
}

void CReturnA2sInfo::SetGameDiscription(const char* sName, bool bDefault)
{
    if(bDefault)
    {
        m_bDefaultGameDescription = true;
        return;
    }
    
    m_bDefaultGameDescription = false;
    strncpy(m_GameDescription, sName, sizeof(m_GameDescription));
}

const char* CReturnA2sInfo::GetGameDiscription()
{
    if(m_bDefaultGameDescription)
        return (const char*)m_RealGameDesc;
    else
        return (const char*)m_GameDescription;
}

void CReturnA2sInfo::SetAppID(short iId, bool bDefault)
{
    if(bDefault)
    {
        m_bDefaultAppID = true;
        return;
    }
    
    m_bDefaultAppID = false;
    m_iAppID = iId;
}

short CReturnA2sInfo::GetAppID()
{
    if(m_bDefaultAppID)
        return m_RealAppID;
    else
        return m_iAppID;
}

void CReturnA2sInfo::SetNumClients(uint8_t iClientCount, bool bDefault)
{
    if(bDefault)
    {
        m_bDefaultNumClients = true;
        return;
    }
    
    m_bDefaultNumClients = false;
    m_iNumClients = iClientCount;
}

uint8_t CReturnA2sInfo::GetNumClients()
{
    if(m_bDefaultNumClients)
        return g_pServer->GetNumClients();
    else
        return m_iNumClients;
}

void CReturnA2sInfo::SetMaxClients(uint8_t iMaxClients, bool bDefault)
{
    if(bDefault)
    {
        m_bDefaultMaxClients = true;
        return;
    }
    
    m_bDefaultMaxClients = false;
    m_iMaxClients = iMaxClients;
}

uint8_t CReturnA2sInfo::GetMaxClients()
{
    if(m_bDefaultMaxClients)
    {
        int iSlot = serverClients->GetMaxHumanPlayers();
        
        if(iSlot == -1)
            iSlot = g_pServer->GetMaxClients();
        
        return iSlot;
    }
    else
        return m_iMaxClients;
}

void CReturnA2sInfo::SetNumFakeClients(uint8_t iNumFakeClients, bool bDefault)
{
    if(bDefault)
    {
        m_bDefaultNumFakeClients = true;
        return;
    }
    
    m_bDefaultNumFakeClients = false;
    m_iNumFakeClients = iNumFakeClients;
}

uint8_t CReturnA2sInfo::GetNumFakeClients()
{
    if(m_bDefaultNumFakeClients)
        return g_pServer->GetNumFakeClients();
    else
        return m_iNumFakeClients;
}

bool CReturnA2sInfo::SetOS(uint8_t os, bool bDefault)
{
    if(bDefault)
    {
        m_bDefaultOS = true;
        return false;
    }
    
    m_bDefaultOS = false;
    
    if(os == 0)
    {
        m_OS = 108;
        return true;
    }
    else if(os == 1)
    {
        m_OS = 119;
        return true;
    }
    else if(os == 2)
    {
        m_OS = 109;
        return true;
    }
    else
        return false;
}

uint8_t CReturnA2sInfo::GetOS()
{
    if(m_bDefaultOS)
    {
#ifdef _WIN32
        return 119;
#else
        return 108;
#endif
    }
    else
        return m_OS;
}

void CReturnA2sInfo::SetVacStatus(bool bVacStatus, bool bDefault)
{
    if(bDefault)
    {
        m_bDefaultVacStatus = true;
        return;
    }
    
    m_bDefaultVacStatus = false;
    m_iVacStatus = bVacStatus ? 1 : 0;
}

uint8_t CReturnA2sInfo::GetVacStatus()
{
    if(m_bDefaultVacStatus)
    {
        if(g_bSteamWorksAPIActivated)
        {
            ISteamGameServer* pSteamClientGameServer = SteamAPI_SteamGameServer();
            return SteamAPI_ISteamGameServer_BSecure(pSteamClientGameServer);
        }
        else
            return 1;
    }
    else
        return m_iVacStatus;
}

void CReturnA2sInfo::SetGameVersion(const char* sGameVersion, bool bDefault)
{
    if(bDefault)
    {
        m_bDefaultGameVersion = true;
        return;
    }
    
    m_bDefaultGameVersion = false;
    strncpy(m_GameVersion, sGameVersion, sizeof(m_GameVersion));
}

const char* CReturnA2sInfo::GetGameVersion()
{
    if(m_bDefaultGameVersion)
        return (const char*)m_RealGameVersion;
    else
        return (const char*)m_GameVersion;
}

void CReturnA2sInfo::SetServerTag(const char* sServerTag, bool bDefault)
{
    if(bDefault)
    {
        m_bDefaultServerTag = true;
        return;
    }
    
    m_bDefaultServerTag = false;
    strncpy(m_ServerTag, sServerTag, sizeof(m_ServerTag));
}

const char* CReturnA2sInfo::GetServerTag()
{
    if(m_bDefaultServerTag)
        return g_pCvar->FindVar("sv_tags")->GetString();
    else
        return (const char*)m_ServerTag;
}

void CReturnA2sInfo::SetEDF(uint8_t edf, bool bDefault)
{
    if(bDefault)
    {
        m_bDefaultEDF = true;
        return;
    }

    m_bDefaultEDF = false;
    m_EDF = edf;
}

uint8_t CReturnA2sInfo::GetEDF()
{
    if(m_bDefaultEDF)
        return S2A_EXTRA_DATA_GAMEID | S2A_EXTRA_DATA_HAS_GAME_PORT | S2A_EXTRA_DATA_HAS_GAMETAG_DATA | S2A_EXTRA_DATA_HAS_SPECTATOR_DATA | S2A_EXTRA_DATA_HAS_STEAM_ID;
    else
        return m_EDF;
}

static cell_t FQ_ToggleStatus(IPluginContext* pContext, const cell_t* params)
{
    g_bEnabled = static_cast<bool>(params[1]);
    
    return 0;
}

static cell_t FQ_ResetA2sInfo(IPluginContext* pContext, const cell_t* params)
{
    g_ReturnA2sInfo.ResetA2sInfo();
    
    return 0;
}

static cell_t FQ_SetProtocolVersion(IPluginContext* pContext, const cell_t* params)
{
    g_ReturnA2sInfo.SetProtocolVersion(params[1], static_cast<bool>(params[2]));
    
    return 0;
}

static cell_t FQ_SetServerName(IPluginContext* pContext, const cell_t* params)
{
    char* nativeString;
    pContext->LocalToString(params[1], &nativeString);
    
    g_ReturnA2sInfo.SetServerName(nativeString, static_cast<bool>(params[2]));
    
    return 0;
}

static cell_t FQ_SetMapName(IPluginContext* pContext, const cell_t* params)
{
    char* nativeString;
    pContext->LocalToString(params[1], &nativeString);
    
    g_ReturnA2sInfo.SetMapName(nativeString, static_cast<bool>(params[2]));
    
    return 0;
}

static cell_t FQ_SetGameFolderName(IPluginContext* pContext, const cell_t* params)
{
    char* nativeString;
    pContext->LocalToString(params[1], &nativeString);
    
    g_ReturnA2sInfo.SetGameFolderName(nativeString, static_cast<bool>(params[2]));
    
    return 0;
}

static cell_t FQ_SetGameDiscription(IPluginContext* pContext, const cell_t* params)
{
    char* nativeString;
    pContext->LocalToString(params[1], &nativeString);
    
    g_ReturnA2sInfo.SetGameDiscription(nativeString, static_cast<bool>(params[2]));
    
    return 0;
}

static cell_t FQ_SetAppID(IPluginContext* pContext, const cell_t* params)
{
    g_ReturnA2sInfo.SetAppID(params[1], static_cast<bool>(params[2]));
    
    return 0;
}

static cell_t FQ_SetNumClients(IPluginContext* pContext, const cell_t* params)
{
    g_ReturnA2sInfo.SetNumClients(params[1], static_cast<bool>(params[2]));
    
    return 0;
}

static cell_t FQ_SetMaxClients(IPluginContext* pContext, const cell_t* params)
{
    g_ReturnA2sInfo.SetMaxClients(params[1], static_cast<bool>(params[2]));
    
    return 0;
}

static cell_t FQ_SetNumFakeClients(IPluginContext* pContext, const cell_t* params)
{
    g_ReturnA2sInfo.SetNumFakeClients(params[1], static_cast<bool>(params[2]));
    
    return 0;
}

static cell_t FQ_SetOS(IPluginContext* pContext, const cell_t* params)
{
    return g_ReturnA2sInfo.SetOS(params[1], static_cast<bool>(params[2]));
}

static cell_t FQ_SetVacStatus(IPluginContext* pContext, const cell_t* params)
{
    g_ReturnA2sInfo.SetVacStatus(params[1], static_cast<bool>(params[2]));
    
    return 0;
}

static cell_t FQ_SetGameVersion(IPluginContext* pContext, const cell_t* params)
{
    char* nativeString;
    pContext->LocalToString(params[1], &nativeString);
    
    g_ReturnA2sInfo.SetGameVersion(nativeString, static_cast<bool>(params[2]));
    
    return 0;
}

static cell_t FQ_SetServerTag(IPluginContext* pContext, const cell_t* params)
{
    char* nativeString;
    pContext->LocalToString(params[1], &nativeString);
    
    g_ReturnA2sInfo.SetServerTag(nativeString, static_cast<bool>(params[2]));
    
    return 0;
}
static cell_t FQ_SetEDF(IPluginContext* pContext, const cell_t* params)
{
    g_ReturnA2sInfo.SetEDF(params[1], static_cast<bool>(params[2]));
    return 0;
}

static cell_t FQ_AddFakePlayer(IPluginContext* pContext, const cell_t* params)
{
    char* nativeString;
    pContext->LocalToString(params[2], &nativeString);
    
    g_ReturnA2sPlayer.InsertFakePlayer(params[1], nativeString, params[3], sp_ctof(params[4]));

    return 0;
}

static cell_t FQ_RemoveAllFakePlayer(IPluginContext* pContext, const cell_t* params)
{
    g_ReturnA2sPlayer.ClearAllFakePlayer();
    
    return 0;
}

static cell_t FQ_RemoveFakePlayer(IPluginContext* pContext, const cell_t* params)
{
    return g_ReturnA2sPlayer.RemoveFakePlayer(params[1]);
}

static cell_t FQ_SetFakePlayerDisplayNum(IPluginContext* pContext, const cell_t* params)
{
    g_ReturnA2sPlayer.SetFakePlayerDisplayNum(params[1]);
    
    return 0;
}

static cell_t FQ_SetA2sPlayerChanllengeNumber(IPluginContext* pContext, const cell_t* params)
{
    return g_ReturnA2sPlayer.SetChallengeNumber(params[1], params[2]);
}

const sp_nativeinfo_t g_ExtensionNatives[] =
{
    { "FQ_ToggleStatus",                    FQ_ToggleStatus },
    { "FQ_ResetA2sInfo",                    FQ_ResetA2sInfo },
    { "FQ_SetProtocolVersion",              FQ_SetProtocolVersion },
    { "FQ_SetServerName",                   FQ_SetServerName },
    { "FQ_SetMapName",                      FQ_SetMapName },
    { "FQ_SetGameFolderName",               FQ_SetGameFolderName },
    { "FQ_SetGameDiscription",              FQ_SetGameDiscription },
    { "FQ_SetAppID",                        FQ_SetAppID },
    { "FQ_SetNumClients",                   FQ_SetNumClients },
    { "FQ_SetMaxClients",                   FQ_SetMaxClients },
    { "FQ_SetNumFakeClients",               FQ_SetNumFakeClients },
    { "FQ_SetOS",                           FQ_SetOS },
    { "FQ_SetVacStatus",                    FQ_SetVacStatus },
    { "FQ_SetGameVersion",                  FQ_SetGameVersion },
    { "FQ_SetServerTag",                    FQ_SetServerTag },
    { "FQ_SetEDF",                          FQ_SetEDF },
    { "FQ_AddFakePlayer",                   FQ_AddFakePlayer },
    { "FQ_RemoveAllFakePlayer",             FQ_RemoveAllFakePlayer },
    { "FQ_RemoveFakePlayer",                FQ_RemoveFakePlayer },
    { "FQ_SetFakePlayerDisplayNum",         FQ_SetFakePlayerDisplayNum },
    { "FQ_SetA2sPlayerChanllengeNumber",    FQ_SetA2sPlayerChanllengeNumber },
    { nullptr,                              nullptr }
};