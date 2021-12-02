#if defined _fakequeries_included
  #endinput
#endif
#define _fakequeries_included

enum OS 
{
    OS_LINUX,
    OS_WINDOWS,
    OS_MAC,
}

enum ExtraData
{
    ExtraData_GamePort  = (1 << 7),     //The server's game port.
    ExtraData_Hltv      = (1 << 6),     //Spectator port for hltv and name of the hltv.
    ExtraData_ServerTag = (1 << 5),     //Tags that describe the game.
    ExtraData_GameID    = (1 << 0),     //The server's 64-bit app id, it is also 730 in csgo.
    ExtraData_SteamID   = (1 << 4),     //Server's 64-bit steam id.
};

/**
 * Enable or disable the extension.
 *
 * If the extension is enabled, all a2s_info and a2s_player queries will be handled by the extension,
 * otherwise they will be handled by the game engine.
 *
 * @param bEnable                   If enable extension?
 * @noreturn
 */
native void FQ_ToggleStatus(bool bEnable);

/**
 * Reset all changes to the a2s info response.
 *
 * @noreturn
 */
native void FQ_ResetA2sInfo();

/**
 * Set protocol version, csgo is using protocol version 17.
 * This is not often used, if not properly set may cause unexpected behavior.
 *
 * @param iVersion                  Version of the protocol.
 * @param bResetToDefault           If this is true, protocol version will be set to default.
 * @noreturn
 */
native void FQ_SetProtocolVersion(int iVersion, bool bResetToDefault = false);

/**
 * Set server name
 *
 * @param sBuf                      Name buffer, size is restricted under 256.
 * @param bResetToDefault           If this is true, server name will be set to default.
 * @noreturn
 */
native void FQ_SetServerName(const char[] sBuf, bool bResetToDefault = false);

/**
 * Set map name
 *
 * @param sBuf                      Name buffer, size is restricted under 256.
 * @param bResetToDefault           If this is true, map name will be set to default.
 * @noreturn
 */
native void FQ_SetMapName(const char[] sBuf, bool bResetToDefault = false);

/**
 * Set game folder name ,in csgo by default it's "csgo".
 *
 * @param sBuf                      Name buffer, size is restricted under 256.
 * @param bResetToDefault           If this is true, game folder name will be set to default.
 * @noreturn
 */
native void FQ_SetGameFolderName(const char[] sBuf, bool bResetToDefault = false);

/**
 * Set game discription, in csgo by default is "Counter-Strike: Global Offensive"
 *
 * @param sBuf                      Name buffer, size is restricted under 256.
 * @param bResetToDefault           If this is true, game discription will be set to default.
 * @noreturn
 */
native void FQ_SetGameDiscription(const char[] sBuf, bool bResetToDefault = false);

/**
 * Set app id, which is defined in steam.inf. Default app id of csgo is 730.
 * !!!!!!Note that incorrect appid will stop your server from being displayed in the community server list!!!!!!
 *
 * @param iAppID                    App id number.
 * @param bResetToDefault           If this is true, app id will be set to default.
 * @noreturn
 */
native void FQ_SetAppID(int iAppID, bool bResetToDefault = false);

/**
 * Set the total number of valid clients in server.
 *
 * @param iNumClients               Number of the clients.
 * @param bResetToDefault           If this is true, total number of clients will be set to default.
 * @noreturn
 */
native void FQ_SetNumClients(int iNumClients, bool bResetToDefault = false);

/**
 * Set the max number of the client slot.
 *
 * @param iMaxClients               Max number of the client slot.
 * @param bResetToDefault           If this is true, max number of clients will be set to default.
 * @noreturn
 */
native void FQ_SetMaxClients(int iMaxClients, bool bResetToDefault = false);

/**
 * Set the total number of fake clients in server, this is literally the number of bots.
 *
 * @param iNumFakeClients           Max number of the clients.
 * @param bResetToDefault           If this is true, total number of fake clients will be set to default.
 * @noreturn
 */
native void FQ_SetNumFakeClients(int iNumFakeClients, bool bResetToDefault = false);

/**
 * Set os of the server.
 *
 * @param os                        Os info.
 * @param bResetToDefault           If this is true, os will be set to default.
 * @return                          True if successfully changed the os, false if invalid os was passed.
 */
native bool FQ_SetOS(OS os, bool bResetToDefault = false);

/**
 * Set vac status of the server.
 *
 * @param bWindows                  True if set vac on, false otherwise.
 * @param bResetToDefault           If this is true, vac status will be set to default.
 * @noreturn
 */
native void FQ_SetVacStatus(bool bVacStatus, bool bResetToDefault = false);

/**
 * Set version of the server, format: xx.xx.xx.xx, can be found in steam.inf.
 *
 * @param sBuf                      Version buffer, size is restricted under 256..
 * @param bResetToDefault           If this is true, version will be set to default.
 * @noreturn
 */
native void FQ_SetGameVersion(const char[] sBuf, bool bResetToDefault = false);

/**
 * Set server tag, set sv_tag will do the same thing.
 *
 * @param sBuf                      Tag buffer, size is restricted under 256.
 * @param bResetToDefault           If this is true, server tag will be set to default.
 * @noreturn
 */
native void FQ_SetServerTag(const char[] sBuf, bool bResetToDefault = false);

/**
 * Set the extra data flag(EDF) in A2S_INFO response.
 * By default the EDF equals 'ExtraData_GamePort | ExtraData_Hltv | ExtraData_ServerTag | ExtraData_GameID | ExtraData_SteamID'
 * Note that if your server doesn't have hltv, even if you set ExtraData_Hltv it won't be added to the response.
 *
 * @param edf                       EDF byte value.
 * @param bResetToDefault           If this is true, EDF will be set to default.
 * @noreturn
 */
native void FQ_SetEDF(ExtraData edf, bool bResetToDefault = false);

/**
 * Set a2s player request challenge number.
 * If you don't know what this is, just skip it or see https://developer.valvesoftware.com/wiki/Server_queries
 *
 * @param iChallengeNumber          Challenge number, you can set it between 0x00000001 and 0xFFFFFFFE.
 * @param bResetToDefault           If this is true, challenge number will be set to default, by default it's random.
 * @return                          True if challenge number is set, false on passing invalid number 0 or 0xFFFFFFFF.
 */
native bool FQ_SetA2sPlayerChanllengeNumber(int iChallengeNumber, bool bResetToDefault = false);

/**
 * Add a fake player to A2S_PLAYER response
 *
 * @param iIndex                    Fake player index, you can duplicate this index for more than one fake players.
 * @param sName                     Fake player name.
 * @param iScore                    Fake player score
 * @param flTime                    Engine time when fake player is added to the response, use GetEngineTime().
 * @noreturn
 */
native void FQ_AddFakePlayer(int iIndex, const char[] sName, int iScore, float flTime);

/**
 * Remove a fake player by its index.
 * Note that if multiple fake players has the same index, they are all gonna be removed.
 *
 * @param iIndex                    Fake player index.
 * @return                          True if fake player of the specific index has been removed, false on failure or not found.
 */
native bool FQ_RemoveFakePlayer(int iIndex);

/**
 * Remove all fake players.
 *
 * @noreturn
 */
native void FQ_RemoveAllFakePlayer();

/**
 * Set the maximum number of fake players added to the response. By default all will be displayed.
 * Note that if you added less fake players than this, all will be displayed.
 *
 * @param iDisplayNum               Maximun fake players can be added to the response.
 * @noreturn
 */
native void FQ_SetFakePlayerDisplayNum(int iDisplayNum);



//Do not edit below this
public Extension __ext_fakequeries = 
{
	name = "fakequeries",
	file = "fakequeries.ext",
#if defined AUTOLOAD_EXTENSIONS
	autoload = 1,
#else
	autoload = 0,
#endif
#if defined REQUIRE_EXTENSIONS
	required = 1,
#else
	required = 0,
#endif
};

#if !defined REQUIRE_EXTENSIONS
public void __ext_fakequeries_SetNTVOptional()
{
    MarkNativeAsOptional("FQ_ToggleStatus");
    MarkNativeAsOptional("FQ_ResetA2sInfo");
    MarkNativeAsOptional("FQ_SetProtocolVersion");
    MarkNativeAsOptional("FQ_SetServerName");
    MarkNativeAsOptional("FQ_SetMapName");
    MarkNativeAsOptional("FQ_SetGameFolderName");
    MarkNativeAsOptional("FQ_SetGameDiscription");
    MarkNativeAsOptional("FQ_SetAppID");
    MarkNativeAsOptional("FQ_SetNumClients");
    MarkNativeAsOptional("FQ_SetMaxClients");
    MarkNativeAsOptional("FQ_SetNumFakeClients");
    MarkNativeAsOptional("FQ_SetOS");
    MarkNativeAsOptional("FQ_SetVacStatus");
    MarkNativeAsOptional("FQ_SetGameVersion");
    MarkNativeAsOptional("FQ_SetServerTag");
    MarkNativeAsOptional("FQ_SetA2sPlayerChanllengeNumber");
    MarkNativeAsOptional("FQ_AddFakePlayer");
    MarkNativeAsOptional("FQ_RemoveFakePlayer");
    MarkNativeAsOptional("FQ_RemoveAllFakePlayer");
    MarkNativeAsOptional("FQ_SetFakePlayerDisplayNum");
}
#endif
