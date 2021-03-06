#include <sourcemod>
#include <fakequeries>

public Plugin myinfo =
{
    name = "Example - Fake players",
    author = "yourmnbbn",
    description = "Create fake players by returning fake AS2_INFO and A2S_PLAYER response",
    version = "1.3.2",
    url = "URL"
};

//int g_iFakePlayerCount = 0;

public void OnPluginStart()
{
    FQ_ResetA2sInfo();
    FQ_RemoveAllFakePlayer();
    
    FQ_AddFakePlayer(1, "Test Player 1", 100, GetEngineTime());
    FQ_AddFakePlayer(2, "Test Player 3", 80, GetEngineTime());
    FQ_AddFakePlayer(3, "Test Player 3", 60, GetEngineTime());

    //deprecated, use FQ_InfoResponseAutoPlayerCount instead
    //g_iFakePlayerCount = 3;
    //FQ_SetNumClients(g_iFakePlayerCount + GetClientCount());
    
    //Player count in A2S_INFO response will be automatically set according to the fake players you add.
    FQ_InfoResponseAutoPlayerCount(true);

    //You can do like this to only add what you want in A2S_INFO extra data
    FQ_SetEDF(ExtraData_GamePort | ExtraData_ServerTag);
    
    FQ_ToggleStatus(true);
}


public void OnClientDisconnect_Post(int client)
{
    //deprecated use FQ_InfoResponseAutoPlayerCount instead
    //FQ_SetNumClients(g_iFakePlayerCount + GetClientCount());

    //As an example to make it more realistic, you can do as the following code, but you have to complete it yourself
    //if(GetRandomInt(0, 1))
    //{
        //FQ_RemoveFakePlayer(3);
    //}
    
}

public void OnClientPostAdminCheck(int client)
{
    //deprecated use FQ_InfoResponseAutoPlayerCount instead
    //FQ_SetNumClients(g_iFakePlayerCount + GetClientCount());

    //As an example to make it more realistic, you can do as the following code, but you have to complete it yourself
    //if(GetRandomInt(0, 1))
    //{
        //FQ_AddFakePlayer(4, "Test Player 4", 20, GetEngineTime());
    //}
}
