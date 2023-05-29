#ifndef __PACKET__
#define __PACKET__

#include <tchar.h>

class CPacket {
protected:
	char* m_chpBuffer;
	char* m_chpFront, * m_chpRear;
	int m_iBufferSize;
	int m_iDataSize;

public:
	enum en_PACKET
	{
		eBUFFER_DEFAULT = 1400
	};

	CPacket();
	CPacket(int iBufferSize);
	virtual ~CPacket();

	void Release(void);
	void Clear(void);
	int GetBufferSize(void) { return m_iBufferSize; }
	int GetDataSize(void) { return m_iDataSize; }
	char* GetBufferPtr(void) { return m_chpBuffer; }
	char* GetFrontPtr(void) { return m_chpFront; }
	int MoveWritePos(int iSize);
	int MoveReadPos(int iSize);

	CPacket& operator=(CPacket& clSrcPacket);
	CPacket& operator<<(unsigned char byValue);
	CPacket& operator<<(char chValue);

	CPacket& operator<<(short shValue);
	CPacket& operator<<(unsigned short wValue);

	CPacket& operator<<(int iValue);
	CPacket& operator<<(long lValue);
	CPacket& operator<<(float fValue);
	CPacket& operator<<(DWORD dwValue);

	CPacket& operator<<(__int64 iValue);
	CPacket& operator<<(double dValue);

	CPacket& operator>>(BYTE& byValue);
	CPacket& operator>>(char& chValue);

	CPacket& operator>>(short& shValue);
	CPacket& operator>>(WORD& wValue);

	CPacket& operator>>(int& iValue);
	CPacket& operator>>(DWORD& dwValue);
	CPacket& operator>>(float& fValue);

	CPacket& operator>>(__int64& iValue);
	CPacket& operator>>(double& dValue);

	int GetData(char* chpDest, int iSize);
	int PutData(char* chpSrc, int iSrcSize);
};

#endif