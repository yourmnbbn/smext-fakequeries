#include "challenge.h"

void ChallengeManager::Init(uint32_t interval)
{
    if(interval < 1000)
        m_unInterval = 1000;
    else
        m_unInterval = interval;
    
    m_unCurrentChallenge = GenerateChallenge();
    m_unLastChallenge = m_unCurrentChallenge;
}

void ChallengeManager::RunFrame()
{
    Milliseconds_t time = GetTime();
    if ((time - m_TimeRecord).count() >= m_unInterval)
    {
        m_TimeRecord = time;

        m_unLastChallenge = m_unCurrentChallenge;
        m_unCurrentChallenge = GenerateChallenge();
    }
}

bool ChallengeManager::SetInterval(uint32_t interval)
{
    if(interval < 1000)
        return false;
    
    m_unInterval = interval;
    return true;
}

Milliseconds_t ChallengeManager::GetTime()
{
    return std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
}

uint32_t ChallengeManager::GenerateChallenge()
{
    srand(smutils->GetAdjustedTime());
    uint32_t unChallenge = rand();
    
    if(unChallenge == 0xFFFFFFFF || unChallenge == 0)
        unChallenge = 0x5AA5EFC2;
    
    return unChallenge;
}

bool ChallengeManager::IsValidA2sPlayerChallengeRequest(const char* challenge)
{
    uint32_t challengeNum = *reinterpret_cast<uint32_t*>((uintptr_t)challenge + 5);
    return challengeNum == m_unCurrentChallenge || challengeNum == m_unLastChallenge;
}

bool ChallengeManager::IsValidA2sInfoChallengeRequest(const char* challenge)
{
    uint32_t challengeNum = *reinterpret_cast<uint32_t*>((uintptr_t)challenge + 25);
    return challengeNum == m_unCurrentChallenge || challengeNum == m_unLastChallenge;
}