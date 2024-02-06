// your PA3 BoundedBuffer.cpp code here
#include "BoundedBuffer.h"
#include <assert.h>

using namespace std;


BoundedBuffer::BoundedBuffer (int _cap) : cap(_cap) {
    // modify as needed
}

BoundedBuffer::~BoundedBuffer () {
    // modify as needed
}

void BoundedBuffer::push (char* msg, int size) {
    // 1. Convert the incoming byte sequence given by msg and size into a vector<char>
    vector<char> data(msg, msg + size); // This tells us the start and end address of the char
    // 2. Wait until there is room in the queue (i.e., queue lengh is less than cap)
    //      waiting on slot available
    unique_lock<mutex> lck(mtx);
    data_ready.wait(lck, [this] { return (int)q.size() < cap; });
    // 3. Then push the vector at the end of the queue
    q.push(data);
    lck.unlock();
    // 4. Wake up threads that were waiting for push
    //      notifiying data available
    slot_ready.notify_one();
}

int BoundedBuffer::pop (char* msg, int size) {
    // 1. Wait until the queue has at least 1 item
    //      waiting on data available
    unique_lock<mutex> lck(mtx);
    slot_ready.wait(lck, [this] { return (int)q.size() > 0; });
    // 2. Pop the front item of the queue. The popped item is a vector<char>
    vector<char> data = q.front();
    q.pop();
    lck.unlock();
    // 3. Convert the popped vector<char> into a char*, copy that into msg; assert that the vector<char>'s length is <= size
    assert(data.size() <= static_cast<size_t>(size));
    copy(data.begin(), data.end(), msg);
    // 4. Wake up threads that were waiting for pop
    //      notifiying slot available
    data_ready.notify_one();
    // 5. Return the vector's length to the caller so that they know how many bytes were popped
    return data.size();
}

size_t BoundedBuffer::size () {
    return q.size();
}