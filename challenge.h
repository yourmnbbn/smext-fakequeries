#ifndef _INCLUDE_SOURCEMOD_EXTENSION_CHALLENGE_H_
#define _INCLUDE_SOURCEMOD_EXTENSION_CHALLENGE_H_

#include "extension.h"
#include <chrono>

using Milliseconds_t = std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>;

class ChallengeManager
{
public:
    void Init(uint32_t interval);
    void RunFrame();
    uint32_t GetInterval() { return m_unInterval; };
    bool SetInterval(uint32_t interval);

    uint32_t GetCurrentChallenge() { return m_unCurrentChallenge; };
    bool IsValidChallengeRequest(const char* challenge);

protected:
    uint32_t GenerateChallenge();
    Milliseconds_t GetTime();

private:
    Milliseconds_t m_TimeRecord;
    uint32_t m_unInterval;
    uint32_t m_unCurrentChallenge;
    uint32_t m_unLastChallenge;
};

extern ChallengeManager g_ChallengeManager;

#endif //_INCLUDE_SOURCEMOD_EXTENSION_CHALLENGE_H_