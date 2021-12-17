#ifndef _INCLUDE_SOURCEMOD_EXTENSION_SYMBOLHELPER_H_
#define _INCLUDE_SOURCEMOD_EXTENSION_SYMBOLHELPER_H_

#include <ITextParsers.h>

using namespace SourceMod; 

#ifdef _WIN32
    #define PLATFORM_STRING "windows"
#else
    #define PLATFORM_STRING "linux"
#endif

//https://github.com/alliedmodders/sourcemod/blob/1fbe5e1daaee9ba44164078fe7f59d862786e612/core/logic/stringutil.cpp#L273
size_t UTIL_DecodeHexString(unsigned char *buffer, size_t maxlength, const char *hexstr)
{
    size_t written = 0;
    size_t length = strlen(hexstr);

    for (size_t i = 0; i < length; i++)
    {
        if (written >= maxlength)
            break;
        buffer[written++] = hexstr[i];
        if (hexstr[i] == '\\' && hexstr[i + 1] == 'x')
        {
            if (i + 3 >= length)
                continue;
            /* Get the hex part. */
            char s_byte[3];
            int r_byte;
            s_byte[0] = hexstr[i + 2];
            s_byte[1] = hexstr[i + 3];
            s_byte[2] = '\0';
            /* Read it as an integer */
            sscanf(s_byte, "%x", &r_byte);
            /* Save the value */
            buffer[written - 1] = r_byte;
            /* Adjust index */
            i += 3;
        }
    }

    return written;
}

class CSymbolHelper : public ITextListener_SMC
{
    enum MyParseState
    {
        MyParseState_None,
        MyParseState_ValidateChallengeFunc,
        MyParseState_SendPacketFunc,
    };

public:
    CSymbolHelper() : m_State(MyParseState_None), m_pValidateChallengeFunc(nullptr), m_pSendPacketFunc(nullptr)
    {
    }

public:
    bool ParseFile(const char* filePath, char* error, size_t maxlen)
    {
        char path[PLATFORM_MAX_PATH];
        g_pSM->BuildPath(Path_SM, path, sizeof(path), "gamedata/%s.txt", filePath);
        return textparsers->ParseSMCFile(path, this, nullptr, error, maxlen) == SMCError_Okay;
    }

    void* GetValidateChallengeFuncPtr() { return m_pValidateChallengeFunc; }
    void* GetSendPacketFuncPtr()  { return m_pSendPacketFunc; }

public://symbols
    void* FindPatternFromSteamClientBinary(const char* textValue)
    {
        char sig[512];
        size_t len = 0;

        len = UTIL_DecodeHexString((unsigned char*)sig, sizeof(sig), textValue);
        if(len < 1)
            return nullptr;
        
        //vtable pointer points to the steamclient library, so don't need to get the base address of that module
        return memutils->FindPattern(*(void**)SteamGameServer(), sig, len);
    }

public: //ITextListener_SMC
    virtual SMCResult ReadSMC_NewSection(const SMCStates *states, const char *name)
    {
        if(strcmp(name, "ValidateChallengeFunc") == 0)
            m_State = MyParseState_ValidateChallengeFunc;
        else if(strcmp(name, "SendPacketFunc") == 0)
            m_State = MyParseState_SendPacketFunc;

        return SMCResult_Continue;
    }

    virtual SMCResult ReadSMC_KeyValue(const SMCStates *states, const char *key, const char *value)
    {
        switch(m_State)
        {
            case MyParseState_ValidateChallengeFunc:
                {
                    if(strcmp(key, PLATFORM_STRING) != 0)
                        break;
                    
                    m_pValidateChallengeFunc = FindPatternFromSteamClientBinary(value);
                    break;
                }
            case MyParseState_SendPacketFunc:
                {
                    if(strcmp(key, PLATFORM_STRING) != 0)
                        break;
                    
                    m_pSendPacketFunc = FindPatternFromSteamClientBinary(value);
                    break;
                }
            default : break;
        }
        return SMCResult_Continue;
    }

    virtual SMCResult ReadSMC_LeavingSection(const SMCStates *states)
    {
        m_State = MyParseState_None;
        return SMCResult_Continue;
    }

private:
    MyParseState m_State;

    void* m_pValidateChallengeFunc;
    void* m_pSendPacketFunc;
};

#endif //_INCLUDE_SOURCEMOD_EXTENSION_SYMBOLHELPER_H_
