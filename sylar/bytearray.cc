#include "bytearray.h"
#include <fstream>
#include <sstream>
#include <string.h>
#include <iomanip>
#include "endian.h"

#include "log.h"

namespace sylar
{

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

ByteArray::Node::Node(size_t s)
    :ptr(new char[s])
    ,size(s)
    ,next(nullptr)
{

}

ByteArray::Node::Node()
    :ptr(nullptr)
    ,size(0)
    ,next(nullptr)
{

}

ByteArray::Node::~Node()
{
    if(ptr)
    {
        delete[] ptr;
    }
}

ByteArray::ByteArray(size_t base_size)
    :m_baseSize(base_size)
    ,m_position(0)
    ,m_capacity(base_size)
    ,m_size(0)
    //网络字节序一般是大端
    ,m_endian(SYLAR_BIG_ENDIAN)
    ,m_root(new Node(base_size))
    ,m_cur(m_root)
{

}

ByteArray::~ByteArray()
{
    Node* tmp = m_root;
    while(tmp)
    {
        //把cur利用上
        m_cur = tmp;
        tmp = tmp->next;
        delete m_cur;
    }
}

bool ByteArray::isLittleEndian() const
{
    return m_endian == SYLAR_LITTLE_ENDIAN;
}

void ByteArray::setIsLittleEndian(bool val)
{
    if(val)
    {
        m_endian = SYLAR_LITTLE_ENDIAN;
    }
    else
    {
        m_endian = SYLAR_BIG_ENDIAN;
    }
}

//int 提供 1个字节到8个字节的，固定长度的都一个写法
void ByteArray::writeFint8(const int8_t value)
{
    //一个字节不用考虑字节序
    //利用内部的 write 实现
    write(&value, sizeof(value));
}

void ByteArray::writeFuint8(const uint8_t value)
{
    write(&value, sizeof(value));
}

//所以没有 const...，也没有 &
void ByteArray::writeFint16(int16_t value)
{
    //跟机器一样的话，什么都不用做
    //只有固定长度才有字节序问题
    if(m_endian != SYLAR_BYTE_ORDER)
    {
        //交换一下
        value = byteswap(value);
    }    
    write(&value, sizeof(value));
}

void ByteArray::writeFuint16(uint16_t value)
{
    if(m_endian != SYLAR_BYTE_ORDER)
    {
        //交换一下
        value = byteswap(value);
    }    
    write(&value, sizeof(value));
}

void ByteArray::writeFint32(int32_t value)
{
    if(m_endian != SYLAR_BYTE_ORDER)
    {
        //交换一下
        value = byteswap(value);
    }    
    write(&value, sizeof(value));
}

void ByteArray::writeFuint32(uint32_t value)
{
    if(m_endian != SYLAR_BYTE_ORDER)
    {
        //交换一下
        value = byteswap(value);
    }    
    write(&value, sizeof(value));
}

void ByteArray::writeFint64(int64_t value)
{
    if(m_endian != SYLAR_BYTE_ORDER)
    {
        //交换一下
        value = byteswap(value);
    }    
    write(&value, sizeof(value));
}

void ByteArray::writeFuint64(uint64_t value)
{
    if(m_endian != SYLAR_BYTE_ORDER)
    {
        //交换一下
        value = byteswap(value);
    }    
    write(&value, sizeof(value));
}

//可变长度
//负数无符号化，比如 -1 变成 1。为了区分，正数会直接 * 2。这样 -1 的码就会从 111111....111 变成 0000...0001
//全是 11111....111 就会压成5个字节，反而变大
//所谓的无符号化，就是把最末变成符号位，而不是最高位
static uint32_t EncodeZigzag32(const int32_t v)
{
    if(v < 0)
    {
        return ((uint32_t)(-v)) * 2 - 1;
    }
    else
    {
        return v * 2;
    }
}

static uint64_t EncodeZigzag64(const int64_t& v)
{
    if(v < 0)
    {
        return ((uint64_t)(-v)) * 2 - 1;
    }
    else
    {
        return v * 2;
    }
}

//反解
static int32_t DecodeZigzag32(const uint32_t& v)
{
    return (v >> 1) ^ -(v & 1);
}

static int64_t DecodeZigzag64(const uint64_t& v)
{
    return (v >> 1) ^ -(v & 1);
}

// void writeInt8(const int8_t& value); //一个字节的压缩没意义
// void writeUint8(const uint8_t& value);
// void writeInt16(const int16_t& value);
// void writeUint16(const uint16_t& value);
void ByteArray::writeInt32(int32_t value)
{
    //压缩要区分正负，不然负数可能反而变大
    //所以要有一个技巧，把有符号的无符号化
    writeUint32(EncodeZigzag32(value));
}

//这个pb算法，就是赌大部分claim uint32 的数值，其实不会很大
//可变长字节由于是一个字节一个字节来读的，所以无所谓字节序了
void ByteArray::writeUint32(uint32_t value)
{
    //最极端情况，要5个字节
    uint8_t tmp[5];
    uint8_t i = 0;
    //pb规则，最高位如果是1，说明还有后续数据
    while(value >= 0x80)
    {
        tmp[i++] = (value & 0x7F) | 0x80;
        value >>= 7;
    }
    tmp[i++] = value;
    write(tmp, i);
}

void ByteArray::writeInt64(int64_t value)
{
    writeUint64(EncodeZigzag64(value));
}

void ByteArray::writeUint64(uint64_t value)
{
    //最极端情况，要10个字节
    uint8_t tmp[10];
    uint8_t i = 0;
    //pb规则，最高位如果是1，说明还有后续数据
    while(value >= 0x80)
    {
        tmp[i++] = (value & 0x7F) | 0x80;
        value >>= 7;
    }
    tmp[i++] = value;
    write(tmp, i);
}

void ByteArray::writeFloat(const float value)
{
    //考虑字节序，用uint32的方法来玩
    uint32_t v;
    memcpy(&v, &value, sizeof(value));
    writeFuint32(v);
}

void ByteArray::writeDouble(const double value)
{
    //考虑字节序，用uint64的方法来玩
    uint64_t v;
    memcpy(&v, &value, sizeof(value));
    writeFuint64(v);
}

//length:int16 , data
void ByteArray::writeStringF16(const std::string& value)
{
    writeFuint16(value.size());
    write(value.c_str(), value.size());
}

//length:int32 , data
void ByteArray::writeStringF32(const std::string& value)
{
    writeFuint32(value.size());
    write(value.c_str(), value.size());
}

//length:int64 , data
void ByteArray::writeStringF64(const std::string& value)
{
    writeFuint64(value.size());
    write(value.c_str(), value.size());
}

//压缩可变的长度 //length:varint , data
void ByteArray::writeStringVint(const std::string& value)
{
    //直接给64，反正压缩
    writeUint64(value.size());
    write(value.c_str(), value.size());
}

//没有指定长度，只有 data（就一定要结束符了，或者干脆就没有读了，纯用来写。因为读的时候并不知道读到哪里）
void ByteArray::writeStringWithoutLength(const std::string& value)
{
    //不要长度，只求写入
    write(value.c_str(), value.size());
}

//read
int8_t ByteArray::readFint8()
{
    int8_t v;
    read(&v, sizeof(v));
    return v;
}

uint8_t ByteArray::readFuint8()
{
    uint8_t v;
    read(&v, sizeof(v));
    return v;
}

//类型差不多，直接用宏算了
#define XX(type) \
    type v; \
    read(&v, sizeof(v)); \
    if(m_endian == SYLAR_BYTE_ORDER) \
    { \
        return v; \
    } \
    else \
    { \
        return byteswap(v); \
    }

int16_t ByteArray::readFint16()
{
    XX(int16_t);
}

uint16_t ByteArray::readFuint16()
{
    XX(uint16_t);
}

int32_t ByteArray::readFint32()
{
    XX(int32_t);
}

uint32_t ByteArray::readFuint32()
{
    XX(uint32_t);
}

int64_t ByteArray::readFint64()
{   
    XX(int64_t);
}

uint64_t ByteArray::readFuint64()
{
    XX(uint64_t);
}

#undef XX

int32_t ByteArray::readInt32()
{
    return DecodeZigzag32(readUint32());
}

uint32_t ByteArray::readUint32()
{
    //限定一个长度，不然一直读 1 会有很严重的溢出问题
    uint32_t result = 0;
    for(int i = 0; i < 32; i += 7)
    {
        uint8_t b = readFuint8();
        //小于 80 说明没有数据在后面
        if(b < 0x80)
        {
            result |= ((uint32_t)b) << i;
            break;
        }
        else
        {
            result |= (((uint32_t)(b & 0x7f)) << i);
        }
    }
    return result;
}

int64_t ByteArray::readInt64()
{
    return DecodeZigzag64(readUint64());
}

uint64_t ByteArray::readUint64()
{
    //限定一个长度，不然一直读 1 会有很严重的溢出问题
    uint64_t result = 0;
    for(int i = 0; i < 64; i += 7)
    {
        uint8_t b = readFuint8();
        //小于 80 说明没有数据在后面
        if(b < 0x80)
        {
            result |= ((uint64_t)b) << i;
            break;
        }
        else
        {
            result |= (((uint64_t)(b & 0x7f)) << i);
        }
    }
    return result;
}

float ByteArray::readFloat()
{
    //用四字节放进去的
    uint32_t v = readFuint32();
    float value;

    memcpy(&value, &v, sizeof(v));
    return value;
}

double ByteArray::readDouble()
{
    uint64_t v = readFuint64();
    double value;

    memcpy(&value, &v, sizeof(v));
    return value;
}

//length:int16, data
std::string ByteArray::readStringF16()
{
    uint16_t len = readFuint16();
    std::string buff;
    buff.resize(len);

    read(&buff[0], len);
    return buff;
}

//length:int32, data
std::string ByteArray::readStringF32()
{
    uint32_t len = readFuint32();
    std::string buff;
    buff.resize(len);

    read(&buff[0], len);
    return buff;
}

//length:int64, data
std::string ByteArray::readStringF64()
{
    uint64_t len = readFuint64();
    std::string buff;
    buff.resize(len);

    read(&buff[0], len);
    return buff;
}

//length:varint, data
std::string ByteArray::readStringVint()
{
    uint64_t len = readUint64();
    std::string buff;
    buff.resize(len);

    read(&buff[0], len);
    return buff;
}

//内部操作
void ByteArray::clear()
{
    m_position = m_size = 0;
    m_capacity = m_baseSize; //只留一个节点
    Node* tmp = m_root->next;
    while(tmp)
    {
        m_cur = tmp;
        tmp = tmp->next;
        delete m_cur;
    }
    m_cur = m_root;
    m_root->next = NULL;
}

void ByteArray::write(const void* buf, size_t size)
{
    if(size == 0)
    {
        return;
    }
    //得预先开辟
    addCapacity(size);

    //当前节点里的位置
    size_t npos = m_position % m_baseSize;
    //当前节点的剩余容量
    size_t ncap = m_cur->size - npos;
    //当前buf写入的偏移量
    size_t bpos = 0;

    while(size > 0)
    {
        if(ncap >= size)
        {
            //可以装下
            memcpy(m_cur->ptr + npos, (char*)buf + bpos, size);
            if(m_cur->size == (npos + size))
            {
                //进位
                m_cur = m_cur->next;
            }

            m_position += size;
            bpos += size;
            size = 0;
        }
        else
        {
            memcpy(m_cur->ptr + npos, (char*)buf + bpos, ncap);
            m_position += ncap;
            bpos += ncap;
            size -= ncap;

            m_cur = m_cur->next;
            ncap = m_cur->size;
            npos = 0;
        }
    }

    //调整大小
    if(m_position > m_size)
    {
        m_size = m_position;
    }
}

void ByteArray::read(void* buf, size_t size)
{
    if(size > getReadSize())
    {
        throw std::out_of_range("not enough len");
    }

    size_t npos = m_position % m_baseSize;
    size_t ncap = m_cur->size - npos;
    size_t bpos = 0;
    while(size > 0)
    {
        if(ncap >= size)
        {
            memcpy((char*)buf + bpos, m_cur->ptr + npos, size);
            if(m_cur->size == npos + size)
            {
                //读完，进位
                m_cur = m_cur->next;
            }
            m_position += size;
            bpos += size;
            size = 0;
        }
        else
        {
            memcpy((char*)buf + bpos, m_cur->ptr + npos, ncap);
            m_position += ncap;
            bpos += ncap;
            size -= ncap;
            m_cur = m_cur->next;
            ncap = m_cur->size;
            npos = 0;
        }
    }
}

//没有副作用，不会改变成员变量的read（用在调试）
void ByteArray::read(void* buf, size_t size, size_t position) const
{
    if(size > getReadSize())
    {
        throw std::out_of_range("not enough len");
    }

    size_t npos = position % m_baseSize;
    size_t ncap = m_cur->size - npos;
    size_t bpos = 0;
    Node* cur = m_cur;
    while(size > 0)
    {
        if(ncap >= size)
        {
            memcpy((char*)buf + bpos, cur->ptr + npos, size);
            if(cur->size == npos + size)
            {
                //读完，进位
                cur = cur->next;
            }
            position += size;
            bpos += size;
            size = 0;
        }
        else
        {
            memcpy((char*)buf + bpos, cur->ptr + npos, ncap);
            position += ncap;
            bpos += ncap;
            size -= ncap;
            cur = cur->next;
            ncap = cur->size;
            npos = 0;
        }
    }
}

//set 稍微要复杂一些
void ByteArray::setPosition(size_t v)
{
    if(v > m_size)
    {
        throw std::out_of_range("set_position out of range");
    }

    m_position = v;
    m_cur = m_root;
    while(v > m_cur->size)
    {
        v -= m_cur->size;
        m_cur = m_cur->next;
    }
    if(v == m_cur->size)
    {
        //单独拆过来一个分支，是可能 m_cur 是空，下次 while m_cur->size 就core了
        m_cur = m_cur->next;
    }
}

//反正是二进制，顺手提供一些调试的。把剩余的未读的写入文件
bool ByteArray::writeToFile(const std::string& name) const
{
    std::ofstream ofs;
    ofs.open(name, std::ios::trunc | std::ios::binary);
    if(!ofs)
    {
        SYLAR_LOG_ERROR(g_logger) << "writeToFile name=" << name 
                << " error, errno=" << errno << " errstr=" << strerror(errno);

        return false;
    }

    int64_t read_size = getReadSize();
    int64_t pos = m_position;
    Node* cur = m_cur;

    while(read_size > 0)
    {
        //diff 偏移
        int diff = pos % m_baseSize;
        int64_t len = (read_size > (int64_t)m_baseSize ? m_baseSize : read_size) - diff;

        ofs.write(cur->ptr + diff, len);
        cur = cur->next;
        pos += len;

        read_size -= len;
    }

    return true;
}

bool ByteArray::readFromFile(const std::string& name)
{
    std::ifstream ifs;
    ifs.open(name, std::ios::binary);
    if(!ifs)
    {
        SYLAR_LOG_ERROR(g_logger) << "readFromFile name=" << name
                << " error, errno=" << errno << " errstr=" << strerror(errno);
        return false;
    }

    std::shared_ptr<char> buff(new char[m_baseSize], [](char* ptr){ delete[] ptr; });
    while(!ifs.eof())
    {
        ifs.read(buff.get(), m_baseSize);
        write(buff.get(), ifs.gcount());
    }

    return true;
}

//做一些内存分配的操作，比如写了一个很大的string。确保能写下来
void ByteArray::addCapacity(size_t size)
{
    if(size == 0)
    {
        return;
    }

    size_t old_cap = getCapacity();
    if(old_cap >= size)
    {
        return;
    }

    size -= old_cap;
    //计算出来节点数，多出来的节点，还要计算一下老的节点能否塞得下。塞得下余数就不用加
    size_t count = (size / m_baseSize) + (((size % m_baseSize) > old_cap) ? 1 : 0);

    //找到末尾的节点
    Node* tmp = m_root;
    while(tmp->next)
    {
        tmp = tmp->next;
    }

    Node* first = NULL;
    for(size_t i = 0; i < count; ++i)
    {
        tmp->next = new Node(m_baseSize);
        if(first == NULL)
        {
            first = tmp->next;
        }
        tmp = tmp->next;
        m_capacity += m_baseSize;
    }

    if(old_cap == 0)
    {
        //如果之前刚好用完了，那 m_cur 指的是 NULL（因为最后一个节点的 next 是空的）
        m_cur = first;
    }
}

std::string ByteArray::toString() const
{
    std::string str;
    str.resize(getReadSize());
    if(str.empty())
    {
        return str;
    }

    //不影响 m_position 的读
    read(&str[0], str.size(), m_position);
    return str;
}

std::string ByteArray::toHexString() const
{
    std::string str = toString();
    std::stringstream ss;

    for(size_t i = 0; i < str.size(); ++i)
    {
        if(i > 0 && i % 32 == 0)
        {
            ss << std::endl; //一行显示32个char
        }
        ss << std::setw(2) << std::setfill('0') << std::hex
            << (int)(uint8_t)str[i] << " ";
    }

    return ss.str();
}

uint64_t ByteArray::getReadBuffers(std::vector<iovec>& buffers, uint64_t len) const
{
    len = len > getReadSize() ? getReadSize() : len;
    if(len == 0)
    {
        return 0;
    }

    uint64_t size = len;

    Node* cur = m_cur;
    size_t npos = m_position % m_baseSize;
    size_t ncap = cur->size - npos;
    struct iovec iov;
    

    while(len > 0)
    {
        if(ncap >= len)
        {
            iov.iov_base = cur->ptr + npos;
            iov.iov_len = len;
            len = 0;
        }
        else
        {
            iov.iov_base = cur->ptr + npos;
            iov.iov_len = ncap;
            len -= ncap;
            cur = cur->next;
            ncap = cur->size;
            npos = 0;
        }
        buffers.push_back(iov);
    }
    return size;
}

uint64_t ByteArray::getReadBuffers(std::vector<iovec>& buffers, uint64_t len, uint64_t position) const
{   
    len = len > getReadSize() ? getReadSize() : len;
    if(len == 0)
    {
        return 0;
    }

    uint64_t size = len;

    size_t npos = position % m_baseSize;
    size_t count = position / m_baseSize;

    //找到那个节点
    Node* cur = m_root;
    while(count > 0)
    {
        cur = cur->next;
        --count;
    }

    size_t ncap = cur->size - npos;
    struct iovec iov;
    while(len > 0)
    {
        if(ncap >= len)
        {
            iov.iov_base = cur->ptr + npos;
            iov.iov_len = len;
            len = 0;
        }
        else
        {
            iov.iov_base = cur->ptr + npos;
            iov.iov_len = ncap;
            len -= ncap;
            cur = cur->next;
            ncap = cur->size;
            npos = 0;
        }
        buffers.push_back(iov);
    }
    return size;
}

//一定会预开辟空间，才能存入 iovec。所以不能做成 const。但也不会修改 position
uint64_t ByteArray::getWriteBuffers(std::vector<iovec>& buffers, uint64_t len)
{
    if(len == 0)
    {
        return 0;
    }
    addCapacity(len);
    uint64_t size = len;

    size_t npos = m_position % m_baseSize;
    size_t ncap = m_cur->size - npos;
    struct iovec iov;
    Node* cur = m_cur;
    while(len > 0)
    {
        if(ncap >= len)
        {
            iov.iov_base = cur->ptr + npos;
            iov.iov_len = len;
            len = 0;
        }
        else
        {
            iov.iov_base = cur->ptr + npos;
            iov.iov_len = ncap;

            len -= ncap;
            cur = cur->next;
            ncap = cur->size;
            npos = 0;
        }
        buffers.push_back(iov);
    }

    return size;
}

}
