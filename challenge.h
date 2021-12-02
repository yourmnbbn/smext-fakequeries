#ifndef _INCLUDE_SOURCEMOD_EXTENSION_CHALLENGE_H_
#define _INCLUDE_SOURCEMOD_EXTENSION_CHALLENGE_H_

#include "extension.h"
#include <utlvector.h>
#include <netadr.h>

typedef struct
{
	netadr_s    adr;       // Address where challenge value was sent to.
	int			challenge; // To connect, adr IP address must respond with this #
	float		time;      // # is valid for only a short duration.
} challenge_t;


class ChallengeManager
{
public:
    ChallengeManager() : m_ServerQueryChallenges(0, 1024)
    {
    }

public:
    int GetChallenge(const netadr_s& adr);
    bool CheckChallenge(const netadr_s& adr, int nChallengeValue);
    bool IsValidA2sPlayerChallengeRequest(const char* challenge, const netadr_s& adr);
    bool IsValidA2sInfoChallengeRequest(const char* challenge, const netadr_s& adr);

private:
    CUtlVector<challenge_t> m_ServerQueryChallenges;
};

extern ChallengeManager g_ChallengeManager;

#endif //_INCLUDE_SOURCEMOD_EXTENSION_CHALLENGE_H_