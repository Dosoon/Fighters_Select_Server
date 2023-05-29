#include <iostream>
#include "CRingBuffer.h"

#define DEFAULT_BUFFER_SIZE 10000
#define MAX_BUFFER_SIZE 30000

CRingBuffer::CRingBuffer(void)
{
	_RingBuffer = (char*)malloc(sizeof(char) * DEFAULT_BUFFER_SIZE);
	if (_RingBuffer == nullptr)
		printf("Ringbuffermalloc ERROR\n");
	_front = 0;
	_rear = 0;
	_size = DEFAULT_BUFFER_SIZE;
}

CRingBuffer::CRingBuffer(int iBufferSize)
{
	_RingBuffer = (char*)malloc(sizeof(char) * iBufferSize);
	_front = 0;
	_rear = 0;
	_size = iBufferSize;
}

CRingBuffer::~CRingBuffer(void)
{
	//printf("Ringbufferfree\n");
	delete[] _RingBuffer;
}

void CRingBuffer::Resize(int size)
{

}

int CRingBuffer::GetBufferSize(void)
{
	return _size;
}

int CRingBuffer::GetSizeInUse(void)
{
	if (_front > _rear)
		return (_size - (_front - _rear));
	return (_rear - _front);
}

int CRingBuffer::GetFreeSize(void)
{
	return _size - GetSizeInUse() - 1;
}

int CRingBuffer::GetContinuousEnqueueSize(void)
{
	if (_front == _size - 1 && _front == _rear)
	{
		_front = 0;
		_rear = 0;
		return _size;
	}
	if ((_rear + 1) % _size == _front)
		return 0;
	if (_front > _rear)
		return (_front - _rear - 1);
	if (_rear == _size - 1)
		return 1;
	return (_size - _rear - 1);
}

int CRingBuffer::GetContinuousDequeueSize(void)
{
	if (_front == _rear)
		return 0;
	if (_front == _size - 1)
		return 1;
	if (_front > _rear)
		return (_size - _front - 1);
	return (_rear - _front);
}

int CRingBuffer::Enqueue(char* chpData, int iSize)
{
	if (GetFreeSize() == 0)
		return 0;
	int oldRear = _rear;
	for (int iCnt = 0; iCnt < iSize; iCnt++)
	{
		if ((_rear + 1) % _size == _front)
			break;
		_RingBuffer[_rear] = *chpData;
		_rear = (_rear + 1) % _size;
		chpData++;
	}
	return _rear - oldRear;
}

int CRingBuffer::Dequeue(char* chpDest, int iSize)
{
	if (GetSizeInUse() == 0)
		return 0;
	int oldFront = _front;
	for (int iCnt = 0; iCnt < iSize; iCnt++)
	{
		if (_front == _rear)
			break;
		*chpDest = _RingBuffer[_front];
		_front = (_front + 1) % _size;
		chpDest++;
	}
	return _front - oldFront;
}

int CRingBuffer::Peek(char* chpDest, int iSize)
{
	int value = 0;
	int tmp = _front;
	for (int iCnt = 0; iCnt < iSize; iCnt++)
	{
		if (tmp == _rear)
			break;
		*chpDest = _RingBuffer[tmp];
		tmp = (tmp + 1) % _size;
		chpDest++;
		value++;
	}
	return value;
}

void CRingBuffer::MoveRear(int iSize)
{
	_rear = (_rear + iSize) % _size;
}

void CRingBuffer::MoveFront(int iSize)
{
	int* p = nullptr;
	if (_front < 0)
		*p = 1;
	_front = (_front + iSize) % _size;
	if (_front < 0)
		*p = 1;
}

char* CRingBuffer::GetFrontBufferPtr(void)
{
	return _RingBuffer + (_front % _size);
}

char* CRingBuffer::GetRearBufferPtr(void)
{
	return _RingBuffer + (_rear % _size);
}

void CRingBuffer::ClearBuffer(void)
{
	_front = 0;
	_rear = 0;
}