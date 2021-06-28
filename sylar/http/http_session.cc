#include "http_session.h"
#include "http_parser.h"

namespace sylar
{
namespace http
{
HttpSession::HttpSession(Socket::ptr sock, bool owner)
    :SocketStream(sock, owner)
{

}

HttpRequest::ptr HttpSession::recvRequest()
{
    HttpRequestParser::ptr parser(new HttpRequestParser);
    //最大的buffer
    uint64_t buff_size = HttpRequestParser::GetHttpRequestBufferSize();
    //智能指针智能看错一个对象，所以要指定数组的析构方法
    std::shared_ptr<char> buffer(new char[buff_size],
        [](char* ptr){
            delete[] ptr;
        });
    //操作裸指针
    char* data = buffer.get();
    //data读出来多少的偏移
    int offset = 0;
    do
    {
        int len = read(data + offset, buff_size - offset);
        if(len <= 0)
        {
            return nullptr;
        }
        len += offset;
        //返回实际解析的长度。这里的execute是简化了的，一直从0开始，否则是可以指定offset的。那样就更复杂了额
        //parser会把已经解析好的给删掉(mov)
        int nparser = parser->execute(data, len);
        if(parser->hasError())
        {
            return nullptr;
        }
        offset = len - nparser;
        if(offset == (int)buff_size)
        {
            //缓冲区满了，还是没有解析完（状态机结束）。也是有问题的
            return nullptr;
        }
        if(parser->isFinished())
        {
            //解析完成
            break;
        }
    } while(true);

    int64_t length = parser->getContentLength();
    if(length > 0)
    {
        std::string body;
        //保留大小
        body.reserve(length);
        
        if(length >= offset)
        {
            //放进去解析剩下的 body
            body.append(data, offset);
        }
        else
        {
            body.append(data, length);
        }

        length -= offset;
        if(length > 0)
        {
            //没读完指定长度，继续读
            if(readFixSize(&body[body.size()], length) <= 0)
            {
                return nullptr;
            }
        }
        //把body放进去
        parser->getData()->setBody(body);
    }

    return parser->getData();
}

int HttpSession::sendResponse(HttpResponse::ptr rsp)
{
    std::stringstream ss;
    ss << *rsp;
    std::string data = ss.str();
    return writeFixSize(data.c_str(), data.size());
}

}
}
