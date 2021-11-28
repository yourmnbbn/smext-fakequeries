#include "challenge.h"
#include <random.h>

#define MAX_CHALLENGES 16384
#define MAX_INTERVAL 5.f

bool ChallengeManager::CheckChallenge(const netadr_s& adr, int nChallengeValue)
{
    float net_time = Plat_FloatTime();

    for (int i=0 ; i<m_ServerQueryChallenges.Count() ; i++)
    {
        if ( adr.CompareAdr(m_ServerQueryChallenges[i].adr, true)) // base adr only
        {
            if (nChallengeValue != m_ServerQueryChallenges[i].challenge)
                return false;

            if (net_time > (m_ServerQueryChallenges[i].time + MAX_INTERVAL))
            {
                m_ServerQueryChallenges.FastRemove(i);
                return false;
            }

            return true;
        }

        // clean up any old entries
        if (net_time > (m_ServerQueryChallenges[i].time + MAX_INTERVAL)) 
        {
            m_ServerQueryChallenges.FastRemove(i);
            i--; // backup one as we just shifted the whole vector back by the deleted element
        }
    }

    return false;
}

int ChallengeManager::GetChallenge(const netadr_s& adr)
{
    int		oldest = 0;
    float	oldestTime = FLT_MAX;
    float net_time = Plat_FloatTime();
        
    // see if we already have a challenge for this ip
    for (int i = 0 ; i < m_ServerQueryChallenges.Count() ; i++)
    {
        if (adr.CompareAdr(m_ServerQueryChallenges[i].adr, true))
        {
            if(net_time > m_ServerQueryChallenges[i].time + MAX_INTERVAL)
            {
                m_ServerQueryChallenges.FastRemove(i);
                break;
            }
            else
            {
                return m_ServerQueryChallenges[i].challenge;
            }
        }
        
        if (m_ServerQueryChallenges[i].time < oldestTime)
        {
            // remember oldest challenge
            oldestTime = m_ServerQueryChallenges[i].time;
            oldest = i;
        }
    }

    if (m_ServerQueryChallenges.Count() > MAX_CHALLENGES)
    {
        m_ServerQueryChallenges.FastRemove( oldest );	
    }

    int newEntry = m_ServerQueryChallenges.AddToTail();

    m_ServerQueryChallenges[newEntry].challenge = (RandomInt(0,0x0FFF) << 16) | RandomInt(0,0xFFFF);
    m_ServerQueryChallenges[newEntry].adr = adr;
    m_ServerQueryChallenges[newEntry].time = net_time;
    return m_ServerQueryChallenges[newEntry].challenge;
}

bool ChallengeManager::IsValidA2sPlayerChallengeRequest(const char* challenge, const netadr_s& adr)
{
    int challengeNum = *reinterpret_cast<uint32_t*>((uintptr_t)challenge + 5);
    return CheckChallenge(adr, challengeNum);
}

bool ChallengeManager::IsValidA2sInfoChallengeRequest(const char* challenge, const netadr_s& adr)
{
    int challengeNum = *reinterpret_cast<uint32_t*>((uintptr_t)challenge + 25);
    return CheckChallenge(adr, challengeNum);
}