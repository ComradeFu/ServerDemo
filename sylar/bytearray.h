#ifndef __SYLAR_BYTEARRAY_H__
#define __SYLAR_BYTEARRAY_H__

#include <memory>
#include <string>
#include <stdint.h>
#include <sys/types.h>
#include <vector>
#include <sys/socket.h>

namespace sylar
{

class ByteArray
{
public:
    typedef std::shared_ptr<ByteArray> ptr;

    //做成链表的形式
    //越收越大的时候，用小的内存合起来，会比数组好得多。不然数组要遇到resize，以及分配大内存的问题。
    struct Node
    {
        Node(size_t s);
        Node();
        ~Node();

        char* ptr;
        size_t size;

        Node* next;
    };

    //每个链表长度默认4k
    ByteArray(size_t base_size = 4096);
    ~ByteArray();

    //write，类似pb，可以压缩。比如 int 是10以下还是可以压缩的
    //提供两种，原样大小写入和压缩写入，F is short for fixed，固定长度
    
    //int 提供 1个字节到8个字节的
    void writeFint8(const int8_t value);
    void writeFuint8(const uint8_t value);
    void writeFint16(int16_t value);
    void writeFuint16(uint16_t value);
    void writeFint32(int32_t value);
    void writeFuint32(uint32_t value);
    void writeFint64(int64_t value);
    void writeFuint64(uint64_t value);
    //可变长度
    // void writeInt8(const int8_t& value); //一个字节的压缩没意义
    // void writeUint8(const uint8_t& value);
    // void writeInt16(const int16_t& value);
    // void writeUint16(const uint16_t& value);
    void writeInt32(int32_t value);
    void writeUint32(uint32_t value);
    void writeInt64(int64_t value);
    void writeUint64(uint64_t value);

    void writeFloat(const float value);
    void writeDouble(const double value);
    //length:int16 , data
    void writeStringF16(const std::string& value);
    //length:int32 , data
    void writeStringF32(const std::string& value);
    //length:int64 , data
    void writeStringF64(const std::string& value);
    //压缩可变的长度 //length:varint , data
    void writeStringVint(const std::string& value);
    //没有指定长度，只有 data（就一定要结束符了，或者干脆就没有读了，纯用来写。因为读的时候并不知道读到哪里）
    void writeStringWithoutLength(const std::string& value);

    //read
    int8_t readFint8();
    uint8_t readFuint8();
    int16_t readFint16();
    uint16_t readFuint16();
    int32_t readFint32();
    uint32_t readFuint32();
    int64_t readFint64();
    uint64_t readFuint64();

    int32_t readInt32();
    uint32_t readUint32();
    int64_t readInt64();
    uint64_t readUint64();

    float readFloat();
    double readDouble();

    //length:int16, data
    std::string readStringF16();
    //length:int32, data
    std::string readStringF32();
    //length:int64, data
    std::string readStringF64();
    //length:varint, data
    std::string readStringVint();

    //内部操作
    void clear();

    void write(const void* buf, size_t size);
    void read(void* buf, size_t size);
    void read(void* buf, size_t size, size_t position) const;

    //指示位置
    size_t getPosition() const { return m_position; }
    //set 稍微要复杂一些
    void setPosition(size_t v);

    //反正是二进制，顺手提供一些调试的
    bool writeToFile(const std::string& name) const;
    bool readFromFile(const std::string& name);

    bool isLittleEndian() const;
    void setIsLittleEndian(bool val);

    //好看的调试
    std::string toString() const;
    std::string toHexString() const;

    //为了更好的配合 socket 使用。提供的方法。
    //io_vec 刚好也是一个一块一块内存的形式来存储
    //那就提供把 bytearray，转成 io_vec 让socket sendmsg去发
    //以及开辟内存，给socket写入收到的 io_vec 

    //把当前位置所有的位置都塞到  iovec。返回实际长度
    //全部都是只获取，不修改
    uint64_t getReadBuffers(std::vector<iovec>& buffers, uint64_t len = ~0ull) const;
    uint64_t getReadBuffers(std::vector<iovec>& buffers, uint64_t len, uint64_t position) const;

    uint64_t getWriteBuffers(std::vector<iovec>& buffers, uint64_t len);

    size_t getSize() const { return m_size; }
    size_t getBaseSize() const { return m_baseSize; }
    //可以读的部分
    size_t getReadSize() const { return m_size - m_position; }
    
private:
    //做一些内存分配的操作，比如写了一个很大的string。确保能写下来
    void addCapacity(size_t size);
    //剩余的容量
    size_t getCapacity() const { return m_capacity - m_position; }
private:
    size_t m_baseSize; //每一个 Node 有多大。只能读，一旦确定了就不能改
    size_t m_position;
    size_t m_capacity;
    size_t m_size; //当前的真实大小

    //还是考虑字节序，毕竟考虑网络不同设备使用。比如用在协议序列化上的话，A发出去write的是小端，B收的时候读是大端，就错了。
    int8_t m_endian;

    Node* m_root;
    Node* m_cur; //方便写节点，指示当前是在哪儿
};

}

#endif
