class CRingBuffer
{
private:
	char* _RingBuffer;
	int _front, _rear;
	int _size;
public:
	CRingBuffer(void);
	CRingBuffer(int iBufferSize);
	~CRingBuffer(void);

	void Resize(int size);
	int GetBufferSize(void);
	int GetSizeInUse(void);
	int GetFreeSize(void);
	int GetContinuousEnqueueSize(void);
	int GetContinuousDequeueSize(void);

	int Enqueue(char* chpData, int iSize);
	int Dequeue(char* chpDest, int iSize);
	int Peek(char* chpDest, int iSize);

	void MoveRear(int iSize);
	void MoveFront(int iSize);
	char* GetFrontBufferPtr(void);
	char* GetRearBufferPtr(void);
	void ClearBuffer(void);
};