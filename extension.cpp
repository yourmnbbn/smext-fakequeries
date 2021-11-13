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

#define A2S_INFO_PACKET "\xff\xff\xff\xff\x54\x53\x6f\x75\x72\x63\x65\x20\x45\x6e\x67\x69\x6e\x65\x20\x51\x75\x65\x72\x79\x00"
#define AS2_PLAYER_CHALLENGE_PACKET "\xff\xff\xff\xff\x55\xff\xff\xff\xff"

/**
 * @file extension.cpp
 * @brief Implement extension code here.
 */

FakeQuery g_FakeQuery;		/**< Global singleton for extension's main interface */
SMEXT_LINK(&g_FakeQuery);

void* g_pSteamSocketMgr = nullptr;
IGameConfig* g_pGameConfig;
int g_iSendToOffset;
SH_DECL_MANUALHOOK5(Hook_RecvFrom, 0, 0, 0, int, int, char*, int, int, sockaddr*)

ICvar* g_pCvar = nullptr;
IServer* g_pServer = nullptr;
ISDKTools* g_pSDKTools = nullptr;
IServerGameClients* serverClients = nullptr;
IServerTools *servertools = nullptr;
CGlobalVars* gpGlobals = nullptr;

ISteamGameServer *(*SteamAPI_SteamGameServer)();
bool (*SteamAPI_ISteamGameServer_BSecure)(ISteamGameServer *self);
SH_DECL_HOOK1_void(IServerGameDLL, GameServerSteamAPIActivated, SH_NOATTRIB, 0, bool);

bool g_bEnabled = false;
bool g_bSteamWorksAPIActivated = false;

ChallengeManager g_ChallengeManager;

int Hook_RecvFrom(int s, char* buf, int len, int flags, sockaddr* from)
{
    if(!g_bEnabled)
        RETURN_META_VALUE(MRES_IGNORED, NULL);
    
    //A2S_INFO was disabled by server
    if(g_pCvar->FindVar("host_info_show")->GetInt() < 1)
        RETURN_META_VALUE(MRES_IGNORED, NULL);
    
    int recvSize = META_RESULT_ORIG_RET(int);
    
    //A2S_INFO
    if(strncmp(buf, A2S_INFO_PACKET, recvSize) == 0)
    {
        //Build return message frame
        g_ReturnA2sInfo.BuildCommunicationFrame();
        
        //Send fake packet
        g_ReturnA2sInfo.SendTo(s, 0, from);
        
        RETURN_META_VALUE(MRES_SUPERCEDE, -1);
    }
    
    //A2S_PLAYER
    if(recvSize == 9 && buf[4] == 0x55)
    {
        //A2S_PLAYER Request challenge
        if(strncmp(buf, AS2_PLAYER_CHALLENGE_PACKET, recvSize) == 0)
        {
            g_ReturnA2sPlayer.BuildChallengeResponse();
            g_ReturnA2sPlayer.SendTo(s, 0, from);
            RETURN_META_VALUE(MRES_SUPERCEDE, -1);
        }
        
        //Request with bad challenge number
        if(!g_ReturnA2sPlayer.IsValidRequest(buf) && !g_ReturnA2sPlayer.IsOfficialRequest(buf))
            RETURN_META_VALUE(MRES_SUPERCEDE, -1);
        
        //Build return message frame
        g_ReturnA2sPlayer.BuildCommunicationFrame();
        
        //Send fake packet
        g_ReturnA2sPlayer.SendTo(s, 0, from);

        RETURN_META_VALUE(MRES_SUPERCEDE, -1);
    }
    
    RETURN_META_VALUE(MRES_IGNORED, NULL);
}

void FrameHook(bool simulating)
{
    g_ChallengeManager.RunFrame();
}

bool FakeQuery::SDK_OnLoad(char *error, size_t maxlen, bool late)
{
    ILibrary *pLibrary = libsys->OpenLibrary(
#if defined _WIN32
    "steam_api.dll"
#else
    "libsteam_api.so"
#endif
    , nullptr, 0);
    if (pLibrary != nullptr)
    {
        const char* pSteamGameServerFuncName = "SteamAPI_SteamGameServer_v013";
        const char* pBSecureFuncName = "SteamAPI_ISteamGameServer_BSecure";
    
        // bool (*SteamAPI_ISteamGameServer_BSecure)(ISteamGameServer *self);
        SteamAPI_SteamGameServer = reinterpret_cast<ISteamGameServer *(*)()>(pLibrary->GetSymbolAddress(pSteamGameServerFuncName));
        SteamAPI_ISteamGameServer_BSecure = reinterpret_cast<bool (*)(ISteamGameServer *self)>(pLibrary->GetSymbolAddress(pBSecureFuncName));

        if(SteamAPI_SteamGameServer == nullptr)
        {
            smutils->LogError(myself, "Failed to get %s function", pSteamGameServerFuncName);
        }
        
        if(SteamAPI_ISteamGameServer_BSecure == nullptr)
        {
            smutils->LogError(myself, "Failed to get %s function", pBSecureFuncName);
        }
        
        pLibrary->CloseLibrary();
    }

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
    
    SH_MANUALHOOK_RECONFIGURE(Hook_RecvFrom, offset, 0, 0);
    SH_ADD_MANUALHOOK(Hook_RecvFrom, g_pSteamSocketMgr, SH_STATIC(Hook_RecvFrom), true);
   
    if(!g_ReturnA2sInfo.ReadSteamINF())
    {
        snprintf(error, maxlen, "Error reading steam.inf");
        return false;
    }

    g_ChallengeManager.Init(5000);

    smutils->AddGameFrameHook(&FrameHook);
    sharesys->AddNatives(myself, g_ExtensionNatives);
    sharesys->RegisterLibrary(myself, "fakequeries");
    
    return true;
}

void FakeQuery::OnCoreMapStart(edict_t *pEdictList, int edictCount, int clientMax)
{
    g_ReturnA2sPlayer.InitResourceEntity();
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
    GET_V_IFACE_CURRENT(GetServerFactory, servertools, IServerTools, VSERVERTOOLS_INTERFACE_VERSION);
    
    gpGlobals = ismm->GetCGlobals();
    
    return true;
}

void FakeQuery::SDK_OnUnload()
{
    SH_REMOVE_MANUALHOOK(Hook_RecvFrom, g_pSteamSocketMgr, SH_STATIC(Hook_RecvFrom), true);
    gameconfs->CloseGameConfigFile(g_pGameConfig);
    smutils->RemoveGameFrameHook(&FrameHook);
    SH_REMOVE_HOOK(IServerGameDLL, GameServerSteamAPIActivated, gamedll, SH_MEMBER(this, &FakeQuery::Hook_GameServerSteamAPIActivated), true);
}

void FakeQuery::Hook_GameServerSteamAPIActivated(bool bActivated)
{
    if (bActivated && SteamAPI_SteamGameServer && SteamAPI_ISteamGameServer_BSecure)
    {
        g_bSteamWorksAPIActivated = true;
    }

    RETURN_META(MRES_IGNORED);
}


