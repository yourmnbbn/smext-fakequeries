#ifndef _INCLUDE_FAKEQUERIES_HLTVHELPER_H_
#define _INCLUDE_FAKEQUERIES_HLTVHELPER_H_

enum { HLTV_SERVER_MAX_COUNT = 2 };

class CHLTVServer
{
public:
	int GetUDPPort(){ return GetIServer()->GetUDPPort(); }
	bool IsActive(){ return GetIServer()->IsActive(); }
	
private:
	CHLTVServer(){}
	IServer* GetIServer(){ return (IServer*)&m_pCBaseServerVtptr; }

#ifdef _WIN32
	DWORD m_DontUse[2];
#else
	DWORD m_DontUse;
#endif

	void* m_pCBaseServerVtptr;
};
extern CHLTVServer** g_pHltvServer;

//This iterator is taken from the leaked code
template <bool bActiveOnly >
class THltvServerIterator
{
protected:
	int m_nIndex;
public:
	THltvServerIterator( ) { m_nIndex = -1; Next(); }

	operator CHLTVServer* ( )
	{
		return m_nIndex < HLTV_SERVER_MAX_COUNT ? g_pHltvServer[ m_nIndex ] : NULL;
	}
	CHLTVServer *operator->( )
	{
		return m_nIndex < HLTV_SERVER_MAX_COUNT ? g_pHltvServer[ m_nIndex ] : NULL;
	}
	bool Next()
	{
		if ( m_nIndex >= HLTV_SERVER_MAX_COUNT )
			return false;
		while ( ++m_nIndex < HLTV_SERVER_MAX_COUNT )
		{
			CHLTVServer *hltv = g_pHltvServer[ m_nIndex ];
			if ( hltv )
			{
				if ( bActiveOnly )
				{
					if ( hltv->IsActive() )
						return true;
				}
				else
				{
					return true;
				}
			}
		}
		return false; // no more active HLTV instances
	}
	uint GetIndex()const { return m_nIndex; }
protected:
};

typedef THltvServerIterator<true> CActiveHltvServerIterator;
typedef THltvServerIterator<false> CHltvServerIterator;

#endif // _INCLUDE_FAKEQUERIES_HLTVHELPER_H_