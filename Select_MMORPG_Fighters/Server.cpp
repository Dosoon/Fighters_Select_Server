#pragma comment(lib, "ws2_32")
#pragma comment(lib, "winmm.lib")

#include <iostream>
#include <tchar.h>
#include <WinSock2.h>
#include <list>
#include <unordered_map>
#include "CRingBuffer.h"
#include "PacketDefine.h"
#include <time.h>
#include <WS2tcpip.h>
#include "CPacket.h"
#include <conio.h>
#include <map>
#include <DbgHelp.h>
#include "CCrushDump.hpp"
#include <thread>
#include <memory>

using namespace std;

/////////////////////////////////////////////////
// define 매크로
/////////////////////////////////////////////////

#define SERVERIP "127.0.0.1"
#define SERVERPORT 10404

#define dfRANGE_MOVE_TOP	0
#define dfRANGE_MOVE_LEFT	0
#define dfRANGE_MOVE_RIGHT	6400
#define dfRANGE_MOVE_BOTTOM	6400

#define dfERROR_RANGE 50
#define dfNETWORK_PACKET_CODE 0x89;

#define dfATTACK1_RANGE_X		80
#define dfATTACK2_RANGE_X		90
#define dfATTACK3_RANGE_X		100
#define dfATTACK1_RANGE_Y		10
#define dfATTACK2_RANGE_Y		10
#define dfATTACK3_RANGE_Y		20

#define dfATTACK1_DAMAGE        3
#define dfATTACK2_DAMAGE        6
#define dfATTACK3_DAMAGE        9

#define dfLOG_LEVEL_DEBUG       0
#define dfLOG_LEVEL_ERROR       2
#define dfLOG_LEVEL_SYSTEM      1

#define dfSECTOR_MAX_X          64
#define dfSECTOR_MAX_Y          64

#define dfNETWORK_PACKET_RECV_TIMEOUT	30000


#define _LOG(LogLevel, fmt, ...)                    \
do {                                                \
    if (g_iLogLevel <= LogLevel)                    \
    {                                               \
        wsprintf(g_szLogBuff, fmt, ##__VA_ARGS__);  \
        Log(g_szLogBuff, LogLevel);                 \
    }                                               \
} while (0)                                         \


void Log(WCHAR* szString, int iLogLevel);

/////////////////////////////////////////////////
// 구조체
/////////////////////////////////////////////////

struct stSESSION {
    DWORD ID = 0;
    CRingBuffer RecvQ;
    CRingBuffer SendQ;
    u_long IP = 0;
    u_short PORT = 0;
    SOCKET Socket = 0;
    DWORD dwLastRecvTime = 0;       // 타임아웃 처리용
};

struct stSECTOR_POS {
    int iX, iY;
};

struct stSECTOR_AROUND {
    int iCount;
    stSECTOR_POS Around[9];
};

struct stPLAYER {
    stSESSION* Session;
    DWORD ID;
    short X, Y;
    BYTE Dir = dfPACKET_MOVE_DIR_LL;
    char HP;
    BYTE Action;
    stSECTOR_POS CurSector;
    stSECTOR_POS OldSector;
};


#pragma pack(push, 1)

struct stPACKET_HEADER {
    BYTE byCode = 0x89;
    BYTE bySize = 0;
    BYTE byType = 0;
};
#pragma pack(pop)

/////////////////////////////////////////////////
// 전역변수
/////////////////////////////////////////////////

FD_SET g_ReadSet, g_WriteSet;
SOCKET g_ListenSocket = INVALID_SOCKET;

unordered_map<SOCKET, stSESSION*> g_SessionHashmap, g_DeleteSessionHashmap;
list<stPLAYER*> g_Sector[dfSECTOR_MAX_Y][dfSECTOR_MAX_X];
unordered_map<int, stPLAYER*> g_PlayerMap;

timeval g_tv = { 0, 0 };
DWORD g_ID = 0;
DWORD g_timer, g_frameTimer;
DWORD g_packetCnt = 0;
DWORD g_packetMoveStartCnt = 0,
g_packetMoveStopCnt = 0,
g_packetAttack1Cnt = 0,
g_packetAttack2Cnt = 0,
g_packetAttack3Cnt = 0,
g_packetEchoCnt = 0;
bool g_bShutdown = false, g_frameLog = false;

LARGE_INTEGER timer, start1, start2, end1, end2;
float deltaTime;

int g_iLogLevel = dfLOG_LEVEL_SYSTEM;
DWORD g_frame = 0;
WCHAR g_szLogBuff[1024];

SYSTEMTIME stNowTime;
WCHAR g_fileName[256];
FILE* g_logFile;

void Disconnect(stSESSION* p);
void SendPacket_SectorOne(int iSectorX, int iSectorY, CPacket* pPacket, stSESSION* pExceptSession);
void SendPacket_Around(stSESSION* pSession, CPacket* pPacket, bool bSendMe = false);
bool Sector_UpdateCharacter(stPLAYER* pPlayer);
void CharacterSectorUpdatePacket(stPLAYER* pPlayer);
void GetSectorAround(int iSectorX, int iSectorY, stSECTOR_AROUND* pSectorAround);
void Crash(void);

stSESSION* FindSession(SOCKET socket)
{
    return g_SessionHashmap[socket];
}

stPLAYER* FindPlayer(int ID)
{
    return g_PlayerMap[ID];
}

stSESSION* CreateSession(SOCKET socket)
{
    stSESSION* newSession = new stSESSION;
    if (newSession == nullptr)
    {
        _LOG(dfLOG_LEVEL_ERROR, L"CreateSession :: fail to allocate (newSession == nullptr)");
    }

    newSession->ID = g_ID++;
    newSession->Socket = socket;
    newSession->dwLastRecvTime = timeGetTime();

    g_SessionHashmap.insert(make_pair(socket, newSession));

    return newSession;
}

stPLAYER* CreatePlayer(SOCKET socket)
{
    stPLAYER* newPlayer = (stPLAYER*)malloc(sizeof(stPLAYER));
    if (newPlayer == nullptr)
    {
        _LOG(dfLOG_LEVEL_ERROR, L"CreatePlayer :: fail to allocate (newPlayer == nullptr)");
    }
    newPlayer->Session = FindSession(socket);
    newPlayer->ID = newPlayer->Session->ID;
    newPlayer->HP = 100;
    newPlayer->Dir = dfPACKET_MOVE_DIR_LL;
    newPlayer->Action = 8;
    newPlayer->X = rand() % 6300 + 50;
    newPlayer->Y = rand() % 6300 + 50;
    newPlayer->CurSector.iX = newPlayer->X / (dfRANGE_MOVE_RIGHT / dfSECTOR_MAX_X);
    newPlayer->CurSector.iY = newPlayer->Y / (dfRANGE_MOVE_BOTTOM / dfSECTOR_MAX_Y);
    newPlayer->OldSector.iX = newPlayer->CurSector.iX;
    newPlayer->OldSector.iY = newPlayer->CurSector.iY;

    g_PlayerMap.insert(make_pair(newPlayer->ID, newPlayer));
    g_Sector[newPlayer->CurSector.iY][newPlayer->CurSector.iX].push_back(newPlayer);

    return newPlayer;
}

int SendMsg_Unicast(stSESSION* p, CPacket* msg, int iSize)
{
    int enqueueRet;
    enqueueRet = p->SendQ.Enqueue(msg->GetBufferPtr(), msg->GetDataSize());
    if (enqueueRet < iSize)
        return -1; // 연결도 끊어야 함!
}

void SendMsg_Broadcast(stSESSION* p, CPacket* msg, int iSize)
{
    for (unordered_map<SOCKET, stSESSION*>::iterator iter = g_SessionHashmap.begin(); iter != g_SessionHashmap.end(); ++iter)
    {
        if ((*iter).second != p)
            SendMsg_Unicast((*iter).second, msg, iSize);
    }
}

void mpCreateOtherCharacter(CPacket* pPacket, DWORD ID, BYTE dir, WORD X, WORD Y, BYTE HP)
{
    stPACKET_HEADER pHeader;
    pHeader.byCode = dfNETWORK_PACKET_CODE;
    pHeader.bySize = 10;
    pHeader.byType = dfPACKET_SC_CREATE_OTHER_CHARACTER;

    pPacket->Clear();
    pPacket->PutData((char*)&pHeader, sizeof(stPACKET_HEADER));
    *pPacket << ID << dir << X << Y << HP;
}

void mpCreateMyCharacter(CPacket* pPacket, DWORD ID, BYTE dir, WORD X, WORD Y, BYTE HP)
{
    stPACKET_HEADER pHeader;
    pHeader.byCode = dfNETWORK_PACKET_CODE;
    pHeader.bySize = 10;
    pHeader.byType = dfPACKET_SC_CREATE_MY_CHARACTER;

    pPacket->Clear();
    pPacket->PutData((char*)&pHeader, sizeof(stPACKET_HEADER));
    *pPacket << ID << dir << X << Y << HP;;
}

void mpDeleteCharacter(CPacket* pPacket, DWORD ID)
{
    stPACKET_HEADER pHeader;
    pHeader.byCode = dfNETWORK_PACKET_CODE;
    pHeader.bySize = 4;
    pHeader.byType = dfPACKET_SC_DELETE_CHARACTER;

    pPacket->Clear();
    pPacket->PutData((char*)&pHeader, sizeof(stPACKET_HEADER));
    *pPacket << ID;
}

void mpMoveStart(CPacket* pPacket, DWORD ID, BYTE dir, WORD X, WORD Y)
{
    stPACKET_HEADER pHeader;
    pHeader.byCode = dfNETWORK_PACKET_CODE;
    pHeader.bySize = 9;
    pHeader.byType = dfPACKET_SC_MOVE_START;

    pPacket->Clear();
    pPacket->PutData((char*)&pHeader, sizeof(stPACKET_HEADER));
    *pPacket << ID;
    *pPacket << dir;
    *pPacket << X;
    *pPacket << Y;
}
void mpMoveStop(CPacket* pPacket, DWORD ID, BYTE dir, WORD X, WORD Y)
{
    stPACKET_HEADER pHeader;
    pHeader.byCode = dfNETWORK_PACKET_CODE;
    pHeader.bySize = 9;
    pHeader.byType = dfPACKET_SC_MOVE_STOP;

    pPacket->Clear();
    pPacket->PutData((char*)&pHeader, sizeof(stPACKET_HEADER));
    *pPacket << ID;
    *pPacket << dir;
    *pPacket << X;
    *pPacket << Y;
}

void mpSync(CPacket* pPacket, DWORD ID, WORD X, WORD Y) {
    stPACKET_HEADER pHeader;
    pHeader.byCode = dfNETWORK_PACKET_CODE;
    pHeader.bySize = 8;
    pHeader.byType = dfPACKET_SC_SYNC;

    pPacket->Clear();
    pPacket->PutData((char*)&pHeader, sizeof(stPACKET_HEADER));
    *pPacket << ID;
    *pPacket << X;
    *pPacket << Y;
}

void mpAttack1(CPacket* pPacket, DWORD ID, BYTE dir, WORD X, WORD Y)
{
    stPACKET_HEADER pHeader;
    pHeader.byCode = dfNETWORK_PACKET_CODE;
    pHeader.bySize = 9;
    pHeader.byType = dfPACKET_SC_ATTACK1;

    pPacket->Clear();
    pPacket->PutData((char*)&pHeader, sizeof(stPACKET_HEADER));
    *pPacket << ID;
    *pPacket << dir;
    *pPacket << X;
    *pPacket << Y;
}

void mpAttack2(CPacket* pPacket, DWORD ID, BYTE dir, WORD X, WORD Y)
{
    stPACKET_HEADER pHeader;
    pHeader.byCode = dfNETWORK_PACKET_CODE;
    pHeader.bySize = 9;
    pHeader.byType = dfPACKET_SC_ATTACK2;

    pPacket->Clear();
    pPacket->PutData((char*)&pHeader, sizeof(stPACKET_HEADER));
    *pPacket << ID;
    *pPacket << dir;
    *pPacket << X;
    *pPacket << Y;
}

void mpAttack3(CPacket* pPacket, DWORD ID, BYTE dir, WORD X, WORD Y)
{
    stPACKET_HEADER pHeader;
    pHeader.byCode = dfNETWORK_PACKET_CODE;
    pHeader.bySize = 9;
    pHeader.byType = dfPACKET_SC_ATTACK3;

    pPacket->Clear();
    pPacket->PutData((char*)&pHeader, sizeof(stPACKET_HEADER));
    *pPacket << ID;
    *pPacket << dir;
    *pPacket << X;
    *pPacket << Y;
}

void mpDamage(CPacket* pPacket, DWORD AttackID, DWORD DamageID, BYTE Damage)
{
    stPACKET_HEADER pHeader;
    pHeader.byCode = dfNETWORK_PACKET_CODE;
    pHeader.bySize = 9;
    pHeader.byType = dfPACKET_SC_DAMAGE;

    pPacket->Clear();
    pPacket->PutData((char*)&pHeader, sizeof(stPACKET_HEADER));
    *pPacket << AttackID;
    *pPacket << DamageID;
    *pPacket << Damage;
}

void Crash()
{
    int* p = nullptr;
    *p = 0;
}

void Disconnect(stSESSION* p)
{
    CPacket sendDeleteCharacter;
    mpDeleteCharacter(&sendDeleteCharacter, p->ID);
    SendPacket_Around(p, &sendDeleteCharacter, false);
    stSESSION* pSession = g_SessionHashmap[p->Socket];
    stPLAYER* pPlayer = g_PlayerMap[p->ID];
    closesocket(p->Socket);

    stPLAYER* Player = FindPlayer(p->ID);
    if (Player == NULL)
    {
        _LOG(dfLOG_LEVEL_ERROR, L"Disconnect :: fail to Find Player(Player == nullptr)");
    }
    int SecX = Player->CurSector.iX;
    int SecY = Player->CurSector.iY;

    g_SessionHashmap.erase(p->Socket);
    g_Sector[SecY][SecX].remove(Player);
    g_PlayerMap.erase(Player->ID);

    pSession->SendQ.~CRingBuffer();
    pSession->RecvQ.~CRingBuffer();
    free(pSession);
    free(pPlayer);
}

bool netPacketProc_Echo(stSESSION* p, CPacket* Packet)
{
    DWORD t;
    stPACKET_HEADER pHeader;
    pHeader.byCode = dfNETWORK_PACKET_CODE;
    pHeader.bySize = 4;
    pHeader.byType = dfPACKET_SC_ECHO;

    *Packet >> t;

    Packet->Clear();
    Packet->PutData((char*)&pHeader, sizeof(stPACKET_HEADER));
    *Packet << t;

    SendMsg_Unicast(p, Packet, Packet->GetDataSize());
    return true;
}

bool netPacketProc_MoveStart(stSESSION* p, CPacket* Packet)
{
    BYTE dir;
    WORD X, Y;
    int ID;
    stPACKET_HEADER header;

    *Packet >> dir >> X >> Y;

    //-----------------------------------------------
    // ID로 캐릭터 검색
    //-----------------------------------------------

    stPLAYER* currPlayer = FindPlayer(p->ID);

    if (currPlayer == NULL)
    {
        _LOG(dfLOG_LEVEL_ERROR, L"netPacketProc_MoveStart :: fail to Find Player(currPlayer == nullptr)");
        return false;
    }

    //-----------------------------------------------
    // 좌표 차이가 너무 많이 난다면 싱크 패킷 전송
    //-----------------------------------------------
    if (abs(currPlayer->X - X) > dfERROR_RANGE || abs(currPlayer->Y - Y) > dfERROR_RANGE)
    {
        mpSync(Packet, currPlayer->ID, currPlayer->X, currPlayer->Y);

        SendPacket_Around(currPlayer->Session, Packet, true);
        _LOG(dfLOG_LEVEL_SYSTEM, L"SYNC :: Frame(%d) / SessionID(%d) / X [%d] Y [%d] -> X [%d] Y [%d]", g_frame, currPlayer->ID, X, Y, currPlayer->X, currPlayer->Y);

        X = currPlayer->X;
        Y = currPlayer->Y;

    }

    currPlayer->Action = dir;

    switch (dir)
    {
    case dfPACKET_MOVE_DIR_RR:
    case dfPACKET_MOVE_DIR_RU:
    case dfPACKET_MOVE_DIR_RD:
        currPlayer->Dir = dfPACKET_MOVE_DIR_RR;
        break;
    case dfPACKET_MOVE_DIR_LU:
    case dfPACKET_MOVE_DIR_LL:
    case dfPACKET_MOVE_DIR_LD:
        currPlayer->Dir = dfPACKET_MOVE_DIR_LL;
        break;
    }

    currPlayer->X = X;
    currPlayer->Y = Y;


    //------------------------------------------------------
    // 좌표 변경이 있을 경우 섹터 업데이트
    //------------------------------------------------------
    if (Sector_UpdateCharacter(currPlayer))
    {
        CharacterSectorUpdatePacket(currPlayer);
    }

    mpMoveStart(Packet, p->ID, dir, currPlayer->X, currPlayer->Y);
    SendPacket_Around(p, Packet);
    return true;
}

bool netPacketProc_MoveStop(stSESSION* p, CPacket* Packet)
{
    BYTE dir;
    WORD X, Y;
    int ID;
    stPACKET_HEADER header;

    *Packet >> dir >> X >> Y;

    //-----------------------------------------------
    // ID로 캐릭터 검색
    //-----------------------------------------------

    stPLAYER* currPlayer = FindPlayer(p->ID);

    if (currPlayer == NULL)
    {
        _LOG(dfLOG_LEVEL_ERROR, L"netPacketProc_MoveStop :: fail to Find Player(currPlayer == nullptr)");
        return false;
    }

    //-----------------------------------------------
    // 좌표 차이가 너무 많이 난다면 싱크 패킷 전송
    //-----------------------------------------------
    if (abs(currPlayer->X - X) > dfERROR_RANGE || abs(currPlayer->Y - Y) > dfERROR_RANGE)
    {
        mpSync(Packet, currPlayer->ID, currPlayer->X, currPlayer->Y);
        SendPacket_Around(currPlayer->Session, Packet, true);

        _LOG(dfLOG_LEVEL_SYSTEM, L"SYNC :: Frame(%d) / SessionID(%d) / X [%d] Y [%d] -> X [%d] Y [%d]", g_frame, currPlayer->ID, X, Y, currPlayer->X, currPlayer->Y);

        X = currPlayer->X;
        Y = currPlayer->Y;
    }

    currPlayer->Action = 8;

    switch (dir)
    {
    case dfPACKET_MOVE_DIR_RR:
    case dfPACKET_MOVE_DIR_RU:
    case dfPACKET_MOVE_DIR_RD:
        currPlayer->Dir = dfPACKET_MOVE_DIR_RR;
        break;
    case dfPACKET_MOVE_DIR_LU:
    case dfPACKET_MOVE_DIR_LL:
    case dfPACKET_MOVE_DIR_LD:
        currPlayer->Dir = dfPACKET_MOVE_DIR_LL;
        break;
    }

    currPlayer->X = X;
    currPlayer->Y = Y;

    //------------------------------------------------------
    // 좌표 변경이 있을 경우 섹터 업데이트
    //------------------------------------------------------
    if (Sector_UpdateCharacter(currPlayer))
    {
        CharacterSectorUpdatePacket(currPlayer);
    }

    mpMoveStop(Packet, p->ID, dir, currPlayer->X, currPlayer->Y);
    SendPacket_Around(p, Packet);

    return true;
}

bool netPacketProc_Attack1(stSESSION* p, CPacket* Packet)
{
    int iCnt;
    stSECTOR_AROUND curAround;
    list<stPLAYER*>* pSectorList;
    list<stPLAYER*>::iterator ListIter;
    BYTE dir;
    WORD X, Y;

    *Packet >> dir >> X >> Y;

    //-----------------------------------------------
    // ID로 캐릭터 검색,
    // 같은 섹터 내의 플레이어들에게 공격모션 패킷 전송
    //-----------------------------------------------

    stPLAYER* currPlayer = FindPlayer(p->ID);

    if (currPlayer == NULL)
    {
        _LOG(dfLOG_LEVEL_ERROR, L"netPacketProc_Attack1 :: fail to Find Player(currPlayer == nullptr)");
        return false;
    }

    mpAttack1(Packet, p->ID, dir, X, Y);
    SendPacket_Around(p, Packet);


    //-----------------------------------------------
    // 데미지 판정 // 섹터 내에서...
    //-----------------------------------------------
    GetSectorAround(currPlayer->CurSector.iX, currPlayer->CurSector.iY, &curAround);

    for (iCnt = 0; iCnt < curAround.iCount; iCnt++) {
        pSectorList = &g_Sector[curAround.Around[iCnt].iY][curAround.Around[iCnt].iX];

        for (ListIter = pSectorList->begin(); ListIter != pSectorList->end(); ListIter++)
        {
            if ((*ListIter)->Session == p)
                continue;
            if (abs((*ListIter)->X - X) < dfATTACK1_RANGE_X && abs((*ListIter)->Y - Y) < dfATTACK1_RANGE_Y)
            {
                (*ListIter)->HP -= dfATTACK1_DAMAGE;
                CPacket damagePacket;
                mpDamage(&damagePacket, p->ID, (*ListIter)->ID, (*ListIter)->HP);

                SendPacket_Around((*ListIter)->Session, &damagePacket, true);
            }
        }
    }

    return true;
}

bool netPacketProc_Attack2(stSESSION* p, CPacket* Packet)
{
    int iCnt;
    stSECTOR_AROUND curAround;
    list<stPLAYER*>* pSectorList;
    list<stPLAYER*>::iterator ListIter;
    BYTE dir;
    WORD X, Y;

    *Packet >> dir >> X >> Y;

    stPLAYER* currPlayer = FindPlayer(p->ID);

    if (currPlayer == NULL)
    {
        _LOG(dfLOG_LEVEL_ERROR, L"netPacketProc_Attack2 :: fail to Find Player(currPlayer == nullptr)");
        return false;
    }

    mpAttack2(Packet, p->ID, dir, X, Y);
    SendPacket_Around(p, Packet);
    
    //-----------------------------------------------
    // 데미지 판정, 섹터 내에서
    //-----------------------------------------------
    GetSectorAround(currPlayer->CurSector.iX, currPlayer->CurSector.iY, &curAround);

    for (iCnt = 0; iCnt < curAround.iCount; iCnt++) {
        pSectorList = &g_Sector[curAround.Around[iCnt].iY][curAround.Around[iCnt].iX];

        for (ListIter = pSectorList->begin(); ListIter != pSectorList->end(); ListIter++)
        {
            if ((*ListIter)->Session == p)
                continue;
            if (abs((*ListIter)->X - X) < dfATTACK2_RANGE_X && abs((*ListIter)->Y - Y) < dfATTACK2_RANGE_Y)
            {
                (*ListIter)->HP -= dfATTACK2_DAMAGE;
                CPacket damagePacket;
                mpDamage(&damagePacket, p->ID, (*ListIter)->ID, (*ListIter)->HP);

                SendPacket_Around((*ListIter)->Session, &damagePacket, true);
            }
        }
    }

    return true;
}

bool netPacketProc_Attack3(stSESSION* p, CPacket* Packet)
{
    int iCnt;
    stSECTOR_AROUND curAround;
    list<stPLAYER*>* pSectorList;
    list<stPLAYER*>::iterator ListIter;
    BYTE dir;
    WORD X, Y;

    *Packet >> dir >> X >> Y;

    //-----------------------------------------------
    // ID로 캐릭터 검색,
    // 같은 섹터 내의 플레이어들에게 공격모션 패킷 전송
    //-----------------------------------------------

    stPLAYER* currPlayer = FindPlayer(p->ID);

    if (currPlayer == NULL)
    {
        _LOG(dfLOG_LEVEL_ERROR, L"netPacketProc_Attack3 :: fail to Find Player(currPlayer == nullptr)");
        return false;
    }

    mpAttack3(Packet, p->ID, dir, X, Y);
    SendPacket_Around(p, Packet);

    //-----------------------------------------------
    // 데미지 판정, 섹터 내에서
    //-----------------------------------------------
    GetSectorAround(currPlayer->CurSector.iX, currPlayer->CurSector.iY, &curAround);

    for (iCnt = 0; iCnt < curAround.iCount; iCnt++) {
        pSectorList = &g_Sector[curAround.Around[iCnt].iY][curAround.Around[iCnt].iX];

        for (ListIter = pSectorList->begin(); ListIter != pSectorList->end(); ListIter++)
        {
            if ((*ListIter)->Session == p)
                continue;
            if (abs((*ListIter)->X - X) < dfATTACK3_RANGE_X && abs((*ListIter)->Y - Y) < dfATTACK3_RANGE_Y)
            {
                (*ListIter)->HP -= dfATTACK3_DAMAGE;
                CPacket damagePacket;
                mpDamage(&damagePacket, p->ID, (*ListIter)->ID, (*ListIter)->HP);

                SendPacket_Around((*ListIter)->Session, &damagePacket, true);
            }
        }
    }

    return true;
}

bool PacketProc(stSESSION* p, BYTE PacketType, CPacket* Packet)
{
    switch (PacketType)
    {
    case dfPACKET_CS_MOVE_START:
        ++g_packetMoveStartCnt;
        ++g_packetCnt;
        return netPacketProc_MoveStart(p, Packet);
        break;
    case dfPACKET_CS_MOVE_STOP:
        ++g_packetMoveStopCnt;
        ++g_packetCnt;
        return netPacketProc_MoveStop(p, Packet);
        break;
    case dfPACKET_CS_ATTACK1:
        ++g_packetAttack1Cnt;
        ++g_packetCnt;
        return netPacketProc_Attack1(p, Packet);
        break;
    case dfPACKET_CS_ATTACK2:
        ++g_packetAttack2Cnt;
        ++g_packetCnt;
        return netPacketProc_Attack2(p, Packet);
        break;
    case dfPACKET_CS_ATTACK3:
        ++g_packetAttack3Cnt;
        ++g_packetCnt;
        return netPacketProc_Attack3(p, Packet);
        break;
    case dfPACKET_CS_ECHO:
        ++g_packetEchoCnt;
        ++g_packetCnt;
        return netPacketProc_Echo(p, Packet);
        break;
    }
    return 1;
}


void netProc_Recv(stSESSION* p)
{
    if (p == nullptr)
        return;
    char buf[10000], packet[10];
    int recvRet, enqueueRet, peekRet, dequeueRet;

    p->dwLastRecvTime = timeGetTime();

    recvRet = recv(p->Socket, p->RecvQ.GetRearBufferPtr(), p->RecvQ.GetContinuousEnqueueSize(), 0);
    if (recvRet == SOCKET_ERROR || recvRet == 0)
    {
        int error = WSAGetLastError();
        if (p->RecvQ.GetContinuousEnqueueSize() == 0)
            Crash();
        if (error != WSAEWOULDBLOCK)
        {
            if (error != WSAECONNRESET)
                _LOG(dfLOG_LEVEL_ERROR, L"netProc_Recv :: SessionID(%d) recv()(%d) error(%d)", p->ID, recvRet, error);
            g_DeleteSessionHashmap.insert(make_pair(p->Socket, p));
            return;
        }
    }
    if (recvRet > 0)
    {
        p->RecvQ.MoveRear(recvRet);
    }

    while (1)
    {
        if (p->RecvQ.GetSizeInUse() <= sizeof(stPACKET_HEADER))
            break;

        stPACKET_HEADER header;
        CPacket pack;

        peekRet = p->RecvQ.Peek((char*)&header, sizeof(header));
        if (header.byCode != 0x89)
            break;
        if (p->RecvQ.GetSizeInUse() < sizeof(header) + header.bySize)
            break;
        p->RecvQ.MoveFront(peekRet);
        dequeueRet = p->RecvQ.Dequeue(packet, header.bySize);
        pack.PutData(packet, header.bySize);
        PacketProc(p, header.byType, &pack);
    }

}

int netProc_Send(stSESSION* p)
{
    if (p == nullptr)
        return -1;

    int peekRet, sendRet, error;
    bool gotErr = false;
    int backupSend;

    sendRet = send(p->Socket, p->SendQ.GetFrontBufferPtr(), p->SendQ.GetContinuousDequeueSize(), 0);
    backupSend = sendRet;

    if (sendRet == SOCKET_ERROR || sendRet == 0)
    {
        error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK)
        {
            if (error != WSAECONNRESET)
            {
                _LOG(dfLOG_LEVEL_ERROR, L"netProc_Send :: SessionID(%d) send() error(%d)", p->ID, error);
                _LOG(dfLOG_LEVEL_ERROR, L"SizeInUse :: %d", p->SendQ.GetSizeInUse());
                _LOG(dfLOG_LEVEL_ERROR, L"GCDeQSize :: %d", p->SendQ.GetContinuousDequeueSize());
            }
            g_DeleteSessionHashmap.insert(make_pair(p->Socket, p));
            return -1;
        }
    }
    p->SendQ.MoveFront(sendRet);
    return sendRet;
}

void netProc_Accept()
{
    SOCKET acceptedSock;
    SOCKADDR_IN acceptedAddr;
    char clientIP[16];

    int addrLen = sizeof(acceptedAddr);

    acceptedSock = accept(g_ListenSocket, (SOCKADDR*)&acceptedAddr, &addrLen);
    if (acceptedSock == INVALID_SOCKET)
    {
        int e = GetLastError();
        if (e != WSAEWOULDBLOCK)
        {
            _LOG(dfLOG_LEVEL_ERROR, L"netProc_Accept :: accept() error(%d)", e);
            return;
        }
    }

    //----------------------------------------------------------
    // 세션 생성 및 등록 후
    // 해당 소켓으로 캐릭터도 생성
    //----------------------------------------------------------

    CreateSession(acceptedSock);
    CreatePlayer(acceptedSock);

    //----------------------------------------------------------
    // IP와 PORT 정보를 세션 구조체에 세팅
    //----------------------------------------------------------

    stSESSION* newSession = FindSession(acceptedSock);
    if (newSession == NULL)
    {
        _LOG(dfLOG_LEVEL_ERROR, L"netProc_Accept :: fail to Find Session(newSession == nullptr)");
    }

    stPLAYER* newPlayer = FindPlayer(newSession->ID);
    if (newPlayer == NULL)
    {
        _LOG(dfLOG_LEVEL_ERROR, L"netProc_Accept :: fail to Find Player(newPlayer == nullptr)");
    }
    inet_ntop(AF_INET, &acceptedAddr.sin_addr, clientIP, sizeof(clientIP));
    newSession->IP = acceptedAddr.sin_addr.s_addr;
    newSession->PORT = acceptedAddr.sin_port;

    _LOG(dfLOG_LEVEL_DEBUG, L"netProc_Accept :: Connect :: IP: %s / SessionID: %d", clientIP, FindSession(acceptedSock)->ID);

    //----------------------------------------------------------
    // 지금 새로 추가된 세션의 캐릭터에 대한
    // 자기 자신 생성 패킷 & 타인 생성 패킷 전송
    //----------------------------------------------------------

    CPacket createMyCharacter;
    mpCreateMyCharacter(&createMyCharacter, newPlayer->ID, newPlayer->Dir, newPlayer->X, newPlayer->Y, newPlayer->HP);
    SendMsg_Unicast(newSession, &createMyCharacter, createMyCharacter.GetDataSize());

    CPacket createOtherCharacter;
    mpCreateOtherCharacter(&createOtherCharacter, newSession->ID, newPlayer->Dir, newPlayer->X, newPlayer->Y, newPlayer->HP);
    SendPacket_Around(newSession, &createOtherCharacter, false);

    int iCnt;
    stSECTOR_AROUND AroundSector;
    list<stPLAYER*>* pSectorList;
    list<stPLAYER*>::iterator SectorIter;

    GetSectorAround(newPlayer->CurSector.iX, newPlayer->CurSector.iY, &AroundSector);


    for (iCnt = 0; iCnt < AroundSector.iCount; iCnt++)
    {
        pSectorList = &g_Sector[AroundSector.Around[iCnt].iY][AroundSector.Around[iCnt].iX];

        for (SectorIter = pSectorList->begin(); SectorIter != pSectorList->end(); SectorIter++)
        {
            //----------------------------------------------------------
            // 이번 iter가 지금 새로 추가된 세션이 아니라면
            //----------------------------------------------------------

            if (((*SectorIter)->ID == newPlayer->ID))
                continue;

            //----------------------------------------------------------
            // 해당 세션의 캐릭터에 대한 생성 패킷을 새 세션 쪽에 전송
            //----------------------------------------------------------

            //cnt++;
            CPacket createMyCharacterToOther;
            stPLAYER* currPlayer = FindPlayer((*SectorIter)->ID);
            if (currPlayer == nullptr)
                continue;
            mpCreateOtherCharacter(&createMyCharacterToOther, currPlayer->ID, currPlayer->Dir, currPlayer->X, currPlayer->Y, currPlayer->HP);
            SendMsg_Unicast(newSession, &createMyCharacterToOther, createMyCharacterToOther.GetDataSize());

            //----------------------------------------------------------
            // 이번 캐릭터가 움직이고 있는 중이라면, 이동 패킷까지 전송
            //----------------------------------------------------------

            if (currPlayer->Action < 8)
            {
                CPacket moveStart;
                mpMoveStart(&moveStart, currPlayer->ID, currPlayer->Action, currPlayer->X, currPlayer->Y);
                SendMsg_Unicast(newSession, &moveStart, moveStart.GetDataSize());
            }
        }
    }

    _LOG(dfLOG_LEVEL_DEBUG, L"netProc_Accept :: Create Character :: IP: %s / SessionID: %d / X: %d / Y: %d", clientIP, newSession->ID, newPlayer->X, newPlayer->Y);
}

void netSelectSocket(SOCKET* sockTable, FD_SET* rset, FD_SET* wset)
{
    int selectRet;
    bool bProcFlag;

    /////////////////////////////////////////////////
    // Select 호출 및 후작업
    /////////////////////////////////////////////////

    selectRet = select(0, &g_ReadSet, &g_WriteSet, NULL, &g_tv);


    if (selectRet > 0) {
        for (int i = 0; i < FD_SETSIZE && selectRet > 0; ++i)
        {
            bProcFlag = true;

            if (sockTable[i] == INVALID_SOCKET)
                continue;

            if (FD_ISSET(sockTable[i], wset))
            {
                --selectRet;
                bProcFlag = netProc_Send(FindSession(sockTable[i]));
            }

            if (FD_ISSET(sockTable[i], rset))
            {
                --selectRet;
                if (bProcFlag)
                {
                    if (sockTable[i] == g_ListenSocket)
                        netProc_Accept();
                    else
                        netProc_Recv(FindSession(sockTable[i]));
                }
            }
        }
    }
    else if (selectRet == SOCKET_ERROR)
    {
        int e = WSAGetLastError();
        if (e != WSAEWOULDBLOCK)
        {
            _LOG(dfLOG_LEVEL_ERROR, L"netSelectSocket :: fail to select(%d)", e);
            Crash();
        }
    }
}

void Network()
{
    //----------------------------------------------
    // 세션을 돌면서, enable이 false가 된 세션은
    // 연결을 끊는다.
    //----------------------------------------------

    unordered_map<SOCKET, stSESSION*>::iterator iter = g_DeleteSessionHashmap.begin();
    while (iter != g_DeleteSessionHashmap.end())
    {
        _LOG(dfLOG_LEVEL_DEBUG, L"Network :: Disconnect (Session ID : %d)", (*iter).second->ID);
        Disconnect((*iter).second);
        iter++;
    }

    g_DeleteSessionHashmap.clear();

    int selectRet;

    SOCKET UserTable_SOCKET[FD_SETSIZE] = { INVALID_SOCKET, };
    int iSocketCount = 0;

    /////////////////////////////////////////////////
    // FD_SET 세팅, listen socket 세팅
    /////////////////////////////////////////////////

    iter = g_SessionHashmap.begin();
    FD_ZERO(&g_ReadSet);
    FD_ZERO(&g_WriteSet);

    FD_SET(g_ListenSocket, &g_ReadSet);
    UserTable_SOCKET[iSocketCount++] = g_ListenSocket;


    //----------------------------------------------
    // 전체 해시맵을 돌면서 세션을 순차적으로 select
    //----------------------------------------------

    for (unordered_map<SOCKET, stSESSION*>::iterator iter = g_SessionHashmap.begin(); iter != g_SessionHashmap.end();)
    {
        UserTable_SOCKET[iSocketCount++] = (*iter).second->Socket;
        /////////////////////////////////////////////////
        // 플레이어 소켓을 ReadSet에 세팅
        /////////////////////////////////////////////////

        FD_SET((*iter).second->Socket, &g_ReadSet);

        /////////////////////////////////////////////////
        // SendQ에 데이터가 있다면
        // 서버가 보낼 데이터가 있다는 뜻이므로
        // WriteSet에 세팅
        /////////////////////////////////////////////////

        if ((*iter).second->SendQ.GetSizeInUse() > 0)
            FD_SET((*iter).second->Socket, &g_WriteSet);
        iter++;


        //----------------------------------------------
        // FD_SETSIZE에 도달했다면 일단 한 번 select
        //----------------------------------------------

        if (FD_SETSIZE <= iSocketCount)
        {
            netSelectSocket(UserTable_SOCKET, &g_ReadSet, &g_WriteSet);
            FD_ZERO(&g_ReadSet);
            FD_ZERO(&g_WriteSet);


            //----------------------------------------------
            // FD_SET에 등록된 목록인 UserTable 초기화
            //----------------------------------------------
            memset(UserTable_SOCKET, INVALID_SOCKET, sizeof(SOCKET) * FD_SETSIZE);

            //----------------------------------------------
            // 리슨소켓 재설정
            //----------------------------------------------
            FD_SET(g_ListenSocket, &g_ReadSet);
            UserTable_SOCKET[0] = g_ListenSocket;
            iSocketCount = 1;
        }
    }

    if (iSocketCount > 0)
        netSelectSocket(UserTable_SOCKET, &g_ReadSet, &g_WriteSet);
}

inline bool Check(short X, short Y)
{
    if (X > dfRANGE_MOVE_LEFT && X < dfRANGE_MOVE_RIGHT && Y > dfRANGE_MOVE_TOP && Y < dfRANGE_MOVE_BOTTOM)
        return true;
    return false;
}


void Logic()
{
    DWORD currTime = timeGetTime();

    if (currTime - g_timer < 40) {
        return;
    }

    if (currTime - g_frameTimer > 1000)
    {
        g_frameTimer = currTime;
        if (g_frameLog)
        {
            printf("\n-----------------------------------------------------------\nFRAME        -- %dfps\nTOTAL PACKET -- %d\n-----------------------------------------------------------\nPACKET COUNT -- MoveStart ::\t%d\n                MoveStop ::\t%d\n                Attack1 ::\t%d\n                Attack2 ::\t%d\n                Attack3 ::\t%d\n                Echo ::\t\t%d\n-----------------------------------------------------------\n",
                g_frame, g_packetCnt, g_packetMoveStartCnt, g_packetMoveStopCnt, g_packetAttack1Cnt, g_packetAttack2Cnt, g_packetAttack3Cnt, g_packetEchoCnt);
            g_frameTimer = currTime;
            g_frame = 0;
            g_packetCnt = 0;
            g_packetMoveStartCnt = 0;
            g_packetMoveStopCnt = 0;
            g_packetMoveStopCnt = 0;
            g_packetAttack1Cnt = 0;
            g_packetAttack2Cnt = 0;
            g_packetAttack3Cnt = 0;
            g_packetEchoCnt = 0;
        }

    }

    g_timer = currTime;
    ++g_frame;

    for (unordered_map<SOCKET, stSESSION*>::iterator iter = g_SessionHashmap.begin(); iter != g_SessionHashmap.end(); ++iter)
    {
        stPLAYER* currPlayer = FindPlayer((*iter).second->ID);
        if (currPlayer == NULL)
        {
            _LOG(dfLOG_LEVEL_ERROR, L"Logic :: fail to Find Player(currPlayer == nullptr)");
            continue;
        }

        if (currPlayer->HP <= 0 || currPlayer->HP > 100)
        {
            _LOG(dfLOG_LEVEL_DEBUG, L"Logic :: Dead User (SessionID : %d / HP : %d)", currPlayer->ID, currPlayer->HP);
            g_DeleteSessionHashmap.insert(make_pair((*iter).first, (*iter).second));
        }
        else
        {
            DWORD timeoutCheck = (currTime - (*iter).second->dwLastRecvTime);
            if (timeoutCheck > dfNETWORK_PACKET_RECV_TIMEOUT)
            {
                g_DeleteSessionHashmap.insert(make_pair((*iter).first, (*iter).second));
                continue;
            }

            switch (currPlayer->Action)
            {
            case dfPACKET_MOVE_DIR_LL:
                if (Check(currPlayer->X - 6, currPlayer->Y))
                {
                    currPlayer->X -= 6;
                }
                break;
            case dfPACKET_MOVE_DIR_LU:
                if (Check(currPlayer->X - 6, currPlayer->Y - 4))
                {
                    currPlayer->X -= 6;
                    currPlayer->Y -= 4;
                }
                break;
            case dfPACKET_MOVE_DIR_UU:
                if (Check(currPlayer->X, currPlayer->Y - 4))
                {
                    currPlayer->Y -= 4;
                }
                break;
            case dfPACKET_MOVE_DIR_RU:
                if (Check(currPlayer->X + 6, currPlayer->Y - 4))
                {
                    currPlayer->X += 6;
                    currPlayer->Y -= 4;
                }
                break;
            case dfPACKET_MOVE_DIR_RR:
                if (Check(currPlayer->X + 6, currPlayer->Y))
                {
                    currPlayer->X += 6;
                }
                break;
            case dfPACKET_MOVE_DIR_RD:
                if (Check(currPlayer->X + 6, currPlayer->Y + 4))
                {
                    currPlayer->X += 6;
                    currPlayer->Y += 4;
                }
                break;
            case dfPACKET_MOVE_DIR_DD:
                if (Check(currPlayer->X, currPlayer->Y + 4))
                {
                    currPlayer->Y += 4;
                }
                break;
            case dfPACKET_MOVE_DIR_LD:
                if (Check(currPlayer->X - 6, currPlayer->Y + 4))
                {
                    currPlayer->X -= 6;
                    currPlayer->Y += 4;
                }
                break;
            }

            if (currPlayer->Action < 8)
                //------------------------------------------------------
                // 좌표 변경이 있을 경우 섹터 업데이트
                //------------------------------------------------------
            {
                if (Sector_UpdateCharacter(currPlayer))
                {
                    CharacterSectorUpdatePacket(currPlayer);
                }
            }
        }
    }
}

bool netStartUp() {

    int nonblockRet, optRet, lingeroptRet, bindRet, listenRet;

    /////////////////////////////////////////////////
    // 서버 초기 세팅
    /////////////////////////////////////////////////

    WSADATA wsa;

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        _LOG(dfLOG_LEVEL_ERROR, L"netStartUp :: WSAStartup Error");
        return false;
    }

    cout << ">> WSAStartup #" << endl;

    g_ListenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (g_ListenSocket == INVALID_SOCKET) {
        _LOG(dfLOG_LEVEL_ERROR, L"netStartUp :: fail to create Listen Socket");
        return false;
    }

    SOCKADDR_IN serveraddr;
    ZeroMemory(&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(SERVERPORT);

    u_long nonBlock = 1;
    nonblockRet = ioctlsocket(g_ListenSocket, FIONBIO, &nonBlock);
    if (nonblockRet == SOCKET_ERROR) {
        _LOG(dfLOG_LEVEL_ERROR, L"netStartUp :: fail to set ioctlsocket(%d)", WSAGetLastError());
        //cout << ">> ioctlsocket Error : " << WSAGetLastError() << endl;
        return false;
    }

    /*bool offNagle = false;
    optRet = setsockopt(g_ListenSocket, IPPROTO_TCP, TCP_NODELAY, (char*)&offNagle, sizeof(offNagle));
    if (optRet == SOCKET_ERROR) {
        cout << ">> Nagle Off Error : " << WSAGetLastError() << endl;
        return false;
    }*/

    linger optval;
    optval.l_onoff = 1;
    optval.l_linger = 0;
    lingeroptRet = setsockopt(g_ListenSocket, SOL_SOCKET, SO_LINGER, (char*)&optval, sizeof(optval));
    if (lingeroptRet == SOCKET_ERROR) {
        _LOG(dfLOG_LEVEL_ERROR, L"netStartUp :: fail to set Linger Option(%d)", WSAGetLastError());
        return false;
    }

    bindRet = bind(g_ListenSocket, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
    if (bindRet == SOCKET_ERROR) {
        _LOG(dfLOG_LEVEL_ERROR, L"netStartUp :: fail to Bind(%d)", WSAGetLastError());
        return false;
    }

    cout << ">> Bind OK # Port : " << SERVERPORT << endl;

    listenRet = listen(g_ListenSocket, SOMAXCONN_HINT(5000));
    if (listenRet == SOCKET_ERROR) {
        _LOG(dfLOG_LEVEL_ERROR, L"netStartUp :: fail to Listen(%d)", WSAGetLastError());
        return false;
    }

    cout << ">> Listen OK #" << endl;

    return true;
}

void ServerControl()
{
    static bool bControlMode = false;

    //---------------------------------------------------------
    // L : 컨트롤 Lock / U : 컨트롤 Unlock / Q : 서버 종료
    //---------------------------------------------------------
    if (_kbhit())
    {
        WCHAR ControlKey = _getwch();

        if (!bControlMode && L'u' == ControlKey || L'U' == ControlKey)
        {
            bControlMode = true;
            wprintf(L"\nControl Mode : Press Q - Quit\n");
            wprintf(L"Control Mode : Press F - Enable/Disable Frame Log\n");
            wprintf(L"               Current Log Level : %d\n", g_iLogLevel + 1);
            wprintf(L"Control Mode : Press 1 - Log Level 1 (DEBUG)\n");
            wprintf(L"Control Mode : Press 2 - Log Level 2 (SYSTEM)\n");
            wprintf(L"Control Mode : Press 3 - Log Level 3 (ERROR)\n");
            wprintf(L"Control Mode : Press L - Key Lock\n");
        }

        if (bControlMode && (L'l' == ControlKey || L'L' == ControlKey))
        {
            wprintf(L"Control Lock! Press U - Control Unlock\n");
            bControlMode = false;
        }

        if (bControlMode && (L'q' == ControlKey || L'Q' == ControlKey))
        {
            g_bShutdown = true;
        }

        if (bControlMode && (L'f' == ControlKey || L'F' == ControlKey))
        {
            g_frameLog = !g_frameLog;
        }

        if (bControlMode && (L'1' == ControlKey))
        {
            g_iLogLevel = dfLOG_LEVEL_DEBUG;
            wprintf(L"               Current Log Level : %d\n", g_iLogLevel + 1);
        }

        if (bControlMode && (L'2' == ControlKey))
        {
            g_iLogLevel = dfLOG_LEVEL_SYSTEM;
            wprintf(L"               Current Log Level : %d\n", g_iLogLevel + 1);
        }

        if (bControlMode && (L'3' == ControlKey))
        {
            g_iLogLevel = dfLOG_LEVEL_ERROR;
            wprintf(L"               Current Log Level : %d\n", g_iLogLevel + 1);
        }
    }
}
inline bool Sector_UpdateCharacter(stPLAYER* pPlayer) {

    int curSectorX = pPlayer->X / (dfRANGE_MOVE_RIGHT / dfSECTOR_MAX_X);
    int curSectorY = pPlayer->Y / (dfRANGE_MOVE_BOTTOM / dfSECTOR_MAX_Y);

    if (pPlayer->CurSector.iX == curSectorX && pPlayer->CurSector.iY == curSectorY)
        return false;

    pPlayer->OldSector.iX = pPlayer->CurSector.iX;
    pPlayer->OldSector.iY = pPlayer->CurSector.iY;

    g_Sector[pPlayer->CurSector.iY][pPlayer->CurSector.iX].remove(pPlayer);
    pPlayer->CurSector.iX = curSectorX;
    pPlayer->CurSector.iY = curSectorY;
    g_Sector[pPlayer->CurSector.iY][pPlayer->CurSector.iX].push_back(pPlayer);

    return true;
}

void GetSectorAround(int iSectorX, int iSectorY, stSECTOR_AROUND* pSectorAround) {
    int iCntX, iCntY;

    iSectorX--;
    iSectorY--;

    pSectorAround->iCount = 0;

    for (iCntY = 0; iCntY < 3; iCntY++) {
        if ((iSectorY + iCntY < 0) || (iSectorY + iCntY >= dfSECTOR_MAX_Y))
            continue;
        for (iCntX = 0; iCntX < 3; iCntX++) {
            if ((iSectorX + iCntX < 0) || (iSectorX + iCntX >= dfSECTOR_MAX_X))
                continue;

            pSectorAround->Around[pSectorAround->iCount].iX = iSectorX + iCntX;
            pSectorAround->Around[pSectorAround->iCount].iY = iSectorY + iCntY;
            pSectorAround->iCount++;

        }
    }
}

void GetUpdateSectorAround(stPLAYER* pPlayer, stSECTOR_AROUND* pRemoveSector, stSECTOR_AROUND* pAddSector) {
    int iCntOld, iCntCur;
    bool bFind;
    stSECTOR_AROUND OldSectorAround, CurSectorAround;

    OldSectorAround.iCount = 0;
    CurSectorAround.iCount = 0;
    pRemoveSector->iCount = 0;
    pAddSector->iCount = 0;

    GetSectorAround(pPlayer->OldSector.iX, pPlayer->OldSector.iY, &OldSectorAround);
    GetSectorAround(pPlayer->CurSector.iX, pPlayer->CurSector.iY, &CurSectorAround);

    for (iCntOld = 0; iCntOld < OldSectorAround.iCount; iCntOld++) {
        bFind = false;
        for (iCntCur = 0; iCntCur < CurSectorAround.iCount; iCntCur++)
        {
            if (OldSectorAround.Around[iCntOld].iX == CurSectorAround.Around[iCntCur].iX
                && OldSectorAround.Around[iCntOld].iY == CurSectorAround.Around[iCntCur].iY)
            {
                bFind = true;
                break;
            }
        }

        if (!bFind)
        {
            pRemoveSector->Around[pRemoveSector->iCount] = OldSectorAround.Around[iCntOld];
            pRemoveSector->iCount++;
        }
    }

    for (iCntCur = 0; iCntCur < CurSectorAround.iCount; iCntCur++)
    {
        bFind = false;
        for (iCntOld = 0; iCntOld < OldSectorAround.iCount; iCntOld++)
        {
            if (OldSectorAround.Around[iCntOld].iX == CurSectorAround.Around[iCntCur].iX
                && OldSectorAround.Around[iCntOld].iY == CurSectorAround.Around[iCntCur].iY)
            {
                bFind = true;
                break;
            }
        }

        if (!bFind)
        {
            pAddSector->Around[pAddSector->iCount] = CurSectorAround.Around[iCntCur];
            pAddSector->iCount++;
        }
    }
}

void CharacterSectorUpdatePacket(stPLAYER* pPlayer)
{
    _LOG(dfLOG_LEVEL_DEBUG, L"CharacterSectorUpdatePacket :: Sector Updated (SessionID: %d / New Sector: [%d][%d])", pPlayer->ID, pPlayer->CurSector.iY, pPlayer->CurSector.iX);

    stSECTOR_AROUND RemoveSector, AddSector;
    stPLAYER* pExistPlayer;

    list<stPLAYER*>* pSectorList;
    list<stPLAYER*>::iterator ListIter;

    CPacket Packet, Packet2, Packet3, Packet4;
    int iCnt;

    GetUpdateSectorAround(pPlayer, &RemoveSector, &AddSector);

    mpDeleteCharacter(&Packet, pPlayer->ID);

    for (iCnt = 0; iCnt < RemoveSector.iCount; iCnt++) {
        SendPacket_SectorOne(RemoveSector.Around[iCnt].iX, RemoveSector.Around[iCnt].iY, &Packet, NULL);
        pSectorList = &g_Sector[RemoveSector.Around[iCnt].iY][RemoveSector.Around[iCnt].iX];

        for (ListIter = pSectorList->begin(); ListIter != pSectorList->end(); ListIter++)
        {
            mpDeleteCharacter(&Packet2, (*ListIter)->ID);

            SendMsg_Unicast(pPlayer->Session, &Packet2, Packet2.GetDataSize());
        }
    }

    mpCreateOtherCharacter(&Packet, pPlayer->ID, pPlayer->Dir, pPlayer->X, pPlayer->Y, pPlayer->HP);
    mpMoveStart(&Packet2, pPlayer->ID, pPlayer->Action, pPlayer->X, pPlayer->Y);

    for (iCnt = 0; iCnt < AddSector.iCount; iCnt++)
    {
        SendPacket_SectorOne(AddSector.Around[iCnt].iX, AddSector.Around[iCnt].iY, &Packet, NULL);
        SendPacket_SectorOne(AddSector.Around[iCnt].iX, AddSector.Around[iCnt].iY, &Packet2, NULL);

        pSectorList = &g_Sector[AddSector.Around[iCnt].iY][AddSector.Around[iCnt].iX];

        for (ListIter = pSectorList->begin(); ListIter != pSectorList->end(); ListIter++)
        {
            pExistPlayer = *ListIter;

            if (pExistPlayer != pPlayer) {
                mpCreateOtherCharacter(&Packet3, pExistPlayer->ID, pExistPlayer->Dir, pExistPlayer->X, pExistPlayer->Y, pExistPlayer->HP);

                SendMsg_Unicast(pPlayer->Session, &Packet3, Packet3.GetDataSize());

                if (pExistPlayer->Action < 8)
                {
                    mpMoveStart(&Packet4, pExistPlayer->ID, pExistPlayer->Action, pExistPlayer->X, pExistPlayer->Y);
                    SendMsg_Unicast(pPlayer->Session, &Packet4, Packet4.GetDataSize());
                }
            }
        }
    }
}

void SendPacket_SectorOne(int iSectorX, int iSectorY, CPacket* pPacket, stSESSION* pExceptSession)
{
    list<stPLAYER*>* pSectorList = &g_Sector[iSectorY][iSectorX];
    list<stPLAYER*>::iterator SectorIter;
    for (SectorIter = pSectorList->begin(); SectorIter != pSectorList->end(); SectorIter++)
    {
        if ((*SectorIter)->Session != pExceptSession)
        {
            SendMsg_Unicast((*SectorIter)->Session, pPacket, pPacket->GetDataSize());
            g_packetCnt++;
        }
    }
}

void SendPacket_Around(stSESSION* pSession, CPacket* pPacket, bool bSendMe)
{
    int iCnt;
    stSECTOR_AROUND AroundSector;
    list<stPLAYER*>* pSectorList;
    list<stPLAYER*>::iterator SectorIter;

    stPLAYER* pPlayer = FindPlayer(pSession->ID);
    if (pPlayer == NULL)
    {
        _LOG(dfLOG_LEVEL_ERROR, L"SendPacket_Around :: fail to Find Player(pPlayer == nullptr)");
    }

    GetSectorAround(pPlayer->CurSector.iX, pPlayer->CurSector.iY, &AroundSector);

    for (iCnt = 0; iCnt < AroundSector.iCount; iCnt++)
    {
        pSectorList = &g_Sector[AroundSector.Around[iCnt].iY][AroundSector.Around[iCnt].iX];

        for (SectorIter = pSectorList->begin(); SectorIter != pSectorList->end(); SectorIter++)
        {
            if (((*SectorIter)->ID == pPlayer->ID) && !bSendMe)
                continue;
            SendMsg_Unicast((*SectorIter)->Session, pPacket, pPacket->GetDataSize());
            g_packetCnt++;
        }
    }
}

void Log(WCHAR* szString, int iLogLevel)
{
    time_t timer = time(NULL);
    struct tm localTimer;
    localtime_s(&localTimer, &timer);

    if (_wfopen_s(&g_logFile, g_fileName, L"a, ccs=UNICODE") != 0)
    {
        cout << ">> Log :: Failed to Log\n" << endl;
        return;
    }

    fseek(g_logFile, 0, SEEK_END);
    fwprintf(g_logFile, L"[%d%02d%02d_%02d:%02d:%02d] %s\n", localTimer.tm_year + 1900, localTimer.tm_mon + 1, localTimer.tm_mday,
        localTimer.tm_hour, localTimer.tm_min, localTimer.tm_sec, szString);
    fclose(g_logFile);
}

void netCleanUp()
{
    WSACleanup();
}

int _tmain(int argc, _TCHAR* argv[])
{
    CCrashDump();
    GetLocalTime(&stNowTime);
    wsprintf(g_fileName, L"Log_%d%02d%02d_%02d.%02d.%02d.txt", stNowTime.wYear, stNowTime.wMonth, stNowTime.wDay,
        stNowTime.wHour, stNowTime.wMinute, stNowTime.wSecond);


    timeBeginPeriod(1);
    srand(time(NULL));

    if (!netStartUp())
        return -1;

    g_timer = timeGetTime();
    g_frameTimer = g_timer;
    g_tv.tv_sec = 0;
    g_tv.tv_usec = 0;
    while (!g_bShutdown)
    {
        Network();
        Logic();
        ServerControl();
    }

    netCleanUp();
    return 0;
}