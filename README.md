# sm-ext-fakequeries (CSGO)
 Return fake AS2_INFO and A2S_PLAYER response. Currently only tested on csgo. And I think it can be ported to any other games as well. With this extension you can pretty much change anything in the a2s_info and a2s_player response. This has been a private experimental project of mine for a long time and now I decide to make it public.

## Feature
Allows you to change the following part in A2S_INFO response.  
- A2s protocol version  
- Server name.
- Map name.
- Game folder name.
- Game description.
- App ID
- Number of clients.
- Max number of clients.
- Number of fake clients.
- OS information.
- VAC status of the server.
- Game version.
- Server tag.
- Extra data flag(EDF)  

Allows you to change the following part in A2S_PLAYER response. 
- Challenge number.
- Add or remove fake players. You can set the index, name, score and playtime of a fake player. (The fake player only exist in the response and is not an actual client or fakeclient in game.)

If you are unfamiliar with some of the things listed above, [here is the answer.](https://developer.valvesoftware.com/wiki/Server_queries)

## Known issue
- Some 3rd party query tools will firstly request for a challenge number, then use it to request A2S_INFO, A2S_RULES etc at the same time. Tools with this kind of implementation is not supported when using this extension.
  