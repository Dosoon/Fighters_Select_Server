#include <iostream>
#include <Windows.h>
#include "CPacket.h"

using namespace std;

CPacket::CPacket() {
	m_chpBuffer = (char*)malloc(eBUFFER_DEFAULT);
	ZeroMemory(m_chpBuffer, eBUFFER_DEFAULT);
	m_chpFront = m_chpBuffer;
	m_chpRear = m_chpBuffer;
	m_iBufferSize = eBUFFER_DEFAULT;
	m_iDataSize = 0;
}

CPacket::CPacket(int iBufferSize) {
	m_chpBuffer = (char*)malloc(iBufferSize);
	ZeroMemory(m_chpBuffer, eBUFFER_DEFAULT);
	m_chpFront = m_chpBuffer;
	m_chpRear = m_chpBuffer;
	m_iBufferSize = iBufferSize;
	m_iDataSize = 0;
}

CPacket::~CPacket() {
	free(m_chpBuffer);
}

void CPacket::Release(void) {
	m_chpFront = m_chpBuffer;
	m_chpRear = m_chpFront;
}

void CPacket::Clear(void) {
	m_chpFront = m_chpBuffer;
	m_chpRear = m_chpFront;
	m_iDataSize = 0;
}

int CPacket::MoveWritePos(int iSize) {
	if (m_iBufferSize - m_iDataSize >= iSize)
	{
		m_chpRear += iSize;
		return iSize;
	}
	return 0;
}

int CPacket::MoveReadPos(int iSize) {
	if (m_iBufferSize - m_iDataSize >= iSize)
	{
		m_chpFront += iSize;
		return iSize;
	}
	return 0;
}

CPacket& CPacket::operator=(CPacket& clSrcPacket) {
	for (int iCnt = 0; iCnt < clSrcPacket.GetDataSize(); iCnt++)
	{
		*m_chpBuffer++ = (*clSrcPacket.GetBufferPtr())++;
	}

	this->m_iDataSize = clSrcPacket.m_iDataSize;
	this->m_chpFront = m_chpBuffer;
	this->m_chpRear = m_chpFront + m_iDataSize;
	return *this;
}

CPacket& CPacket::operator<<(unsigned char byValue) {
	*(unsigned char*)m_chpRear = byValue;
	m_chpRear += sizeof(unsigned char);
	m_iDataSize += sizeof(unsigned char);

	return *this;
}

CPacket& CPacket::operator<<(char chValue) {
	*(char*)m_chpRear = chValue;
	m_chpRear += sizeof(char);
	m_iDataSize += sizeof(char);

	return *this;
}

CPacket& CPacket::operator<<(short shValue) {
	*(short*)m_chpRear = shValue;
	m_chpRear += sizeof(short);
	m_iDataSize += sizeof(short);

	return *this;
}

CPacket& CPacket::operator<<(unsigned short wValue) {
	*(unsigned short*)m_chpRear = wValue;
	m_chpRear += sizeof(unsigned short);
	m_iDataSize += sizeof(unsigned short);

	return *this;
}

CPacket& CPacket::operator<<(int iValue) {
	*(int*)m_chpRear = iValue;
	m_chpRear += sizeof(int);
	m_iDataSize += sizeof(int);

	return *this;
}

CPacket& CPacket::operator<<(long lValue) {
	*(long*)m_chpRear = lValue;
	m_chpRear += sizeof(long);
	m_iDataSize += sizeof(long);

	return *this;
}

CPacket& CPacket::operator<<(DWORD dwValue) {
	*(int*)m_chpRear = dwValue;
	m_chpRear += sizeof(DWORD);
	m_iDataSize += sizeof(int);

	return *this;
}

CPacket& CPacket::operator<<(float fValue) {
	*(float*)m_chpRear = fValue;
	m_chpRear += sizeof(float);
	m_iDataSize += sizeof(float);

	return *this;
}

CPacket& CPacket::operator<<(__int64 iValue) {
	*(__int64*)m_chpRear = iValue;
	m_chpRear += sizeof(__int64);
	m_iDataSize += sizeof(__int64);

	return *this;
}

CPacket& CPacket::operator<<(double dValue) {
	*(double*)m_chpRear = dValue;
	m_chpRear += sizeof(double);
	m_iDataSize += sizeof(double);

	return *this;
}

CPacket& CPacket::operator>>(BYTE& byValue) {
	byValue = *(BYTE*)m_chpFront;
	m_chpFront += sizeof(BYTE);
	m_iDataSize -= sizeof(BYTE);

	return *this;
}

CPacket& CPacket::operator>>(char& chValue) {
	chValue = *(char*)m_chpFront;
	m_chpFront += sizeof(char);
	m_iDataSize -= sizeof(char);

	return *this;
}

CPacket& CPacket::operator>>(short& shValue) {
	shValue = *(short*)m_chpFront;
	m_chpFront += sizeof(short);
	m_iDataSize -= sizeof(short);

	return *this;

}

CPacket& CPacket::operator>>(WORD& wValue) {
	wValue = *(WORD*)m_chpFront;
	m_chpFront += sizeof(WORD);
	m_iDataSize -= sizeof(WORD);

	return *this;

}

CPacket& CPacket::operator>>(int& iValue) {
	iValue = *(int*)m_chpFront;
	m_chpFront += sizeof(int);
	m_iDataSize -= sizeof(int);

	return *this;

}

CPacket& CPacket::operator>>(DWORD& dwValue) {
	dwValue = *(DWORD*)m_chpFront;
	m_chpFront += sizeof(DWORD);
	m_iDataSize -= sizeof(DWORD);

	return *this;

}

CPacket& CPacket::operator>>(float& fValue) {
	fValue = *(float*)m_chpFront;
	m_chpFront += sizeof(float);
	m_iDataSize -= sizeof(float);

	return *this;

}

CPacket& CPacket::operator>>(__int64& iValue) {
	iValue = *(__int64*)m_chpFront;
	m_chpFront += sizeof(__int64);
	m_iDataSize -= sizeof(__int64);

	return *this;
}

CPacket& CPacket::operator>>(double& dValue) {
	dValue = *(double*)m_chpFront;
	m_chpFront += sizeof(double);
	m_iDataSize -= sizeof(double);

	return *this;

}

int CPacket::GetData(char* chpDest, int iSize) {
	if (m_iDataSize < iSize)
		return 0;
	int value = 0;
	for (int iCnt = 0; iCnt < iSize; iCnt++)
	{
		*chpDest = *m_chpFront;
		m_chpFront++;
		chpDest++;
		value++;
	}
	m_iDataSize -= iSize;
	return value;
}

int CPacket::PutData(char* chpSrc, int iSrcSize) {
	if (m_iBufferSize - m_iDataSize < iSrcSize)
		return 0;

	int value = 0;
	for (int iCnt = 0; iCnt < iSrcSize; iCnt++)
	{
		*m_chpRear = *chpSrc;
		m_chpRear++;
		chpSrc++;
		value++;
	}
	m_iDataSize += iSrcSize;
	return value;
}