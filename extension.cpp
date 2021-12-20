/**
 * vim: set ts=4 :
 * =============================================================================
 * SourceMod Sample Extension
 * Copyright (C) 2004-2008 AlliedModders LLC.  All rights reserved.
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * As a special exception, AlliedModders LLC gives you permission to link the
 * code of this program (as well as its derivative works) to "Half-Life 2," the
 * "Source Engine," the "SourcePawn JIT," and any Game MODs that run on software
 * by the Valve Corporation.  You must obey the GNU General Public License in
 * all respects for all other code used.  Additionally, AlliedModders LLC grants
 * this exception to all derivative works.  AlliedModders LLC defines further
 * exceptions, found in LICENSE.txt (as of this writing, version JULY-31-2007),
 * or <http://www.sourcemod.net/license.php>.
 *
 * Version: $Id$
 */

#include "extension.h"
#include "natives.h"
#include "challenge.h"
#include <ISDKTools.h>
#include "CDetour/detours.h"
#include "symbolhelper.h"

#define A2S_INFO_PACKET "\xff\xff\xff\xff\x54\x53\x6f\x75\x72\x63\x65\x20\x45\x6e\x67\x69\x6e\x65\x20\x51\x75\x65\x72\x79\x00"

/**
 * @file extension.cpp
 * @brief Implement extension code here.
 */

FakeQuery g_FakeQuery;		/**< Global singleton for extension's main interface */
SMEXT_LINK(&g_FakeQuery);

void* g_pSteamSocketMgr = nullptr;
IGameConfig* g_pGameConfig;
int g_iSendToOffset;
SH_DECL_MANUALHOOK5(Hook_RecvFrom, 0, 0, 0, int, int, char*, int, int, netadr_s*)
SH_DECL_HOOK1_void(IServerGameDLL, GameServerSteamAPIActivated, SH_NOATTRIB, 0, bool);

ICvar* g_pCvar = nullptr;
IServer* g_pServer = nullptr;
ISDKTools* g_pSDKTools = nullptr;
IServerGameClients* serverClients = nullptr;

void (CALLING_CONVENTION *SendPacket)(void*, const char*, int, uint32_t*);
bool (CALLING_CONVENTION *Filter_ShouldDiscard)(netadr_s*);

bool g_bEnabled = false;

ChallengeManager g_ChallengeManager;
CHLTVServer** g_pHltvServer = nullptr;

CDetour* g_pDetourFunc = nullptr;
CSymbolHelper g_SymbolHelper;

DETOUR_DECL_MEMBER2(DetourFunc, bool, uint32_t*, adr, int, challenge)
{
    if(!g_bEnabled)
        return DETOUR_MEMBER_CALL(DetourFunc)(adr, challenge);
    
    netadr_s addr(*adr, 0);
    if(g_ChallengeManager.CheckChallenge(addr, challenge))
        return true;

    char sBuf[128];
    bf_write buf(sBuf, 128);
    buf.WriteLong(-1);
    buf.WriteByte(0x41);
    buf.WriteLong(g_ChallengeManager.GetChallenge(addr));

    SendPacket(this, (const char*)buf.GetData(), buf.GetNumBytesWritten(), adr);
    return false;
}

int Hook_RecvFrom(int s, char* buf, int len, int flags, netadr_s* from)
{
    if(!g_bEnabled)
        RETURN_META_VALUE(MRES_IGNORED, 0);
    
    if(!SteamGameServer())  //Necessary interface is not ready yet
        RETURN_META_VALUE(MRES_IGNORED, 0);
    
    int recvSize = META_RESULT_ORIG_RET(int);
    if(recvSize < 4)
        RETURN_META_VALUE(MRES_IGNORED, 0);

    if(*(int*)buf != -1)    //Only accept connection-less packet
        RETURN_META_VALUE(MRES_IGNORED, 0);

    //A2S_INFO
    if(recvSize >= 25 && strncmp(buf, A2S_INFO_PACKET, recvSize) == 0)
    {
        //This ip got banned by the server, we let the server handle it
        if(Filter_ShouldDiscard(from))
        {
            RETURN_META_VALUE(MRES_IGNORED, 0);
        }
        
        switch(g_pCvar->FindVar("host_info_show")->GetInt())
        {
            case 2: //host_info_show 2 need challenge when requesting A2S_INFO
                {
                    if(recvSize == 25 || !g_ReturnA2sInfo.IsValidRequest(buf, from))
                    {
                        g_ReturnA2sInfo.BuildChallengeResponse(from);
                        g_ReturnA2sInfo.SendTo(s, 0, from);
                        break;
                    }

                    g_ReturnA2sInfo.BuildCommunicationFrame();
                    g_ReturnA2sInfo.SendTo(s, 0, from);
                    break;
                }
            case 1:
                {
                    g_ReturnA2sInfo.BuildCommunicationFrame();
                    g_ReturnA2sInfo.SendTo(s, 0, from);
                    break;
                }
            default: break;
        }

        RETURN_META_VALUE(MRES_SUPERCEDE, -1);
    }
    
    //A2S_PLAYER
    if(recvSize == 9 && buf[4] == 0x55)
    {
        //This ip got banned by the server, we let the server handle it
        if(Filter_ShouldDiscard(from))
        {
            RETURN_META_VALUE(MRES_IGNORED, 0);
        }

        switch(g_pCvar->FindVar("host_players_show")->GetInt())
        {
            case 2:
                {
                    //Bad challenge number, resend back valid challenge
                    if(!g_ReturnA2sPlayer.IsValidRequest(buf, from))
                    {
                        g_ReturnA2sPlayer.BuildChallengeResponse(from);
                        g_ReturnA2sPlayer.SendTo(s, 0, from);
                        break;
                    }
                    
                    g_ReturnA2sPlayer.BuildCommunicationFrame();
                    g_ReturnA2sPlayer.SendTo(s, 0, from);
                    break;
                }
            case 1:
                {
                    g_ReturnA2sPlayer.BuildEngineDefaultFrame();
                    g_ReturnA2sPlayer.SendTo(s, 0, from);
                    break;
                }
            default : break;
        }

        RETURN_META_VALUE(MRES_SUPERCEDE, -1);
    }
    
    RETURN_META_VALUE(MRES_IGNORED, 0);
}

bool FakeQuery::SDK_OnLoad(char *error, size_t maxlen, bool late)
{
    SH_ADD_HOOK(IServerGameDLL, GameServerSteamAPIActivated, gamedll, SH_MEMBER(this, &FakeQuery::Hook_GameServerSteamAPIActivated), true);

    char sConfError[256];
    if(!gameconfs->LoadGameConfigFile("fakequeries.games", &g_pGameConfig, sConfError, sizeof(sConfError)))
    {
        snprintf(error, maxlen, "Could not read fakequeries.games : %s", sConfError);
        return false;
    }

    g_pGameConfig->GetAddress("g_pSteamSocketMgr", &g_pSteamSocketMgr);
    if(!g_pSteamSocketMgr)
    {
        snprintf(error, maxlen, "Failed to get address of g_pSteamSocketMgr");
        return false;
    }
    
    int offset;
    if(!g_pGameConfig->GetOffset("CSteamSocketMgr::recvfrom", &offset))
    {
        snprintf(error, maxlen, "Failed to get CSteamSocketMgr::recvfrom offset");
        return false;
    }
    
    if(!g_pGameConfig->GetOffset("CSteamSocketMgr::sendto", &g_iSendToOffset))
    {
        snprintf(error, maxlen, "Failed to get CSteamSocketMgr::sendto offset");
        return false;
    }
    
    g_pGameConfig->GetAddress("g_pHltvServer", (void**)&g_pHltvServer);
    if(!g_pHltvServer)
    {
        snprintf(error, maxlen, "Failed to get address of g_pHltvServer");
        return false;
    }

    g_pGameConfig->GetMemSig("Filter_ShouldDiscard", (void**)&Filter_ShouldDiscard);
    if(!Filter_ShouldDiscard)
    {
        snprintf(error, maxlen, "Failed to look up Filter_ShouldDiscard signature");
        return false;
    }

    SH_MANUALHOOK_RECONFIGURE(Hook_RecvFrom, offset, 0, 0);
    SH_ADD_MANUALHOOK(Hook_RecvFrom, g_pSteamSocketMgr, SH_STATIC(Hook_RecvFrom), true);
   
    if(!g_ReturnA2sInfo.ReadSteamINF())
    {
        snprintf(error, maxlen, "Error reading steam.inf");
        return false;
    }

    sharesys->AddNatives(myself, g_ExtensionNatives);
    sharesys->RegisterLibrary(myself, "fakequeries");
    
    return true;
}

void FakeQuery::SDK_OnAllLoaded()
{
    SM_GET_LATE_IFACE(SDKTOOLS, g_pSDKTools);    
    g_pServer = g_pSDKTools->GetIServer();
    
    g_ReturnA2sInfo.InitRealInformation();
}

bool FakeQuery::SDK_OnMetamodLoad(ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
    GET_V_IFACE_CURRENT(GetEngineFactory, g_pCvar, ICvar, CVAR_INTERFACE_VERSION);
    GET_V_IFACE_ANY(GetServerFactory, serverClients, IServerGameClients, INTERFACEVERSION_SERVERGAMECLIENTS);
    
    return true;
}

void FakeQuery::SDK_OnUnload()
{
    SH_REMOVE_MANUALHOOK(Hook_RecvFrom, g_pSteamSocketMgr, SH_STATIC(Hook_RecvFrom), true);
    gameconfs->CloseGameConfigFile(g_pGameConfig);
    SH_REMOVE_HOOK(IServerGameDLL, GameServerSteamAPIActivated, gamedll, SH_MEMBER(this, &FakeQuery::Hook_GameServerSteamAPIActivated), true);

    if(g_pDetourFunc)
        g_pDetourFunc->DisableDetour();
}

void FakeQuery::Hook_GameServerSteamAPIActivated(bool bActivated)
{
    if (!bActivated || !SteamGameServer())
    {
        smutils->LogError(myself, "Steam API is not active");
        RETURN_META(MRES_IGNORED);
    }

    char error[256];
    if(!g_SymbolHelper.ParseFile("fakequeries.games", error, sizeof(error)))
    {
        smutils->LogError(myself, "Fail to parse fakequeries.games : %s", error);
        RETURN_META(MRES_IGNORED);
    }
    
    void* pFunc = g_SymbolHelper.GetValidateChallengeFuncPtr();
    if(!pFunc)
    {
        smutils->LogError(myself, "Look up ValidateChallengeFunc signature failed");
        RETURN_META(MRES_IGNORED);
    }

    *(void**)&SendPacket = g_SymbolHelper.GetSendPacketFuncPtr();
    if(!SendPacket)
    {
        smutils->LogError(myself, "Look up SendPacketFunc signature failed");
        RETURN_META(MRES_IGNORED);
    }

    CDetourManager::Init(smutils->GetScriptingEngine(), g_pGameConfig);
    g_pDetourFunc = DETOUR_CREATE_MEMBER(DetourFunc, pFunc);
    g_pDetourFunc->EnableDetour();

    RETURN_META(MRES_IGNORED);
}


