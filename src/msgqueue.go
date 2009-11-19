package msgqueue

import(
	"container/heap";
	//"fmt"
)

type Message struct {
	priority int;
	message []byte;
}

func (m *Message) Bytes() []byte {
	return m.message;
}

type MsgQueue struct {
	messages []*Message;
	used int;
}

/* Satisfy the sort interface */

func (mq *MsgQueue) Len() int {
	return mq.used;
}

func (mq *MsgQueue) Less(i, j int) bool {
	return mq.messages[i].priority < mq.messages[j].priority
}

func (mq *MsgQueue) Swap(i, j int) {
	mq.messages[i], mq.messages[j] = mq.messages[j], mq.messages[i]
}

/* Satisfy the heap interface */

func (mq *MsgQueue) Push(x interface{}) {
	if mq.used == len(mq.messages) {
		newSlice := make([]*Message, (1+len(mq.messages))*2);
		for i, c := range mq.messages {
			newSlice[i] = c
		}
		mq.messages = newSlice;
	}
	mq.messages[mq.used] = x.(*Message);
	mq.used += 1;
}

func (mq *MsgQueue) Pop() interface{} {
	if mq.used == 0 {
		return nil;
	}
	m := mq.messages[mq.used-1];
	mq.messages = mq.messages[0:mq.used-1];
	mq.used -= 1;
	return m;
}

/* Public-facing functions (you'll use these) */

func NewMsgQueue(size int) *MsgQueue {
	return &MsgQueue{messages: make([]*Message, size), used: 0};
}

func (mq *MsgQueue) Give(m *Message) {
	heap.Push(mq, m);
}

func (mq *MsgQueue) Take() *Message {
	return heap.Pop(mq).(*Message);
}

