/**
 * @ Author: Ran Liu
 * @ Create Time: 2023-11-03 10:16:17
 * @ Modified by: Ran Liu
 * @ Modified time: 2023-11-06 11:09:12
 * @ Description: This file provides a class for serializing HTTP messages.
 */

#ifndef HTTP_BUFFER_H
#define HTTP_BUFFER_H


#include <string>
#include <map>
#include <vector>

const std::vector<std::string> request_method_vec = {"OPTIONS", "HEAD", "GET", "POST", "PUT", "DELETE", "TRACE", "CONNECT"};

// 序列化读写http报文
class HttpBuffer
{
public:
    HttpBuffer();
    HttpBuffer(std::string &&buffer);
    HttpBuffer(HttpBuffer &&http_buffer);
    HttpBuffer& operator=(HttpBuffer &&http_buffer);
    HttpBuffer(const HttpBuffer &http_buffer)=delete;
    HttpBuffer& operator=(const HttpBuffer &http_buffer)=delete;
    
    ~HttpBuffer();

public:
    void ClearHttpBuffer() { m_buffer = m_header = m_body = m_request_method = m_url = m_http_version = m_status_code = "";
        m_head_key_value_map.clear(); }

public:
    //以下为读取http报文的方法
    void ReadBuffer(std::string &&buffer);

    std::string GetHttpHeader() { return m_header; }

    std::string GetHeaderValue(std::string key);

    std::string GetHttpBody() { return m_body; }

    std::string GetHttpBuffer() const {return m_buffer;}

public:
    //以下为写http报文的方法
    void WriteHttpRequest(const std::string &method, const std::string &url, const std::string &version, 
        std::map<std::string, std::string> &&header_key_value, std::string &&body);

    void WriteHttpResponse(const std::string &version, const std::string &status_code, const std::string &tip,
        std::map<std::string, std::string> &&header_key_value, std::string &&body);

private:
    bool ParseHttpBuffer();

    bool IsHttpRequest(std::string buffer);

    std::string BuildHttpBuffer(std::string &&header, std::string &&body);

private:
    std::string m_buffer;
    std::string m_header;
    std::string m_body;

    std::string m_request_method;
    std::string m_url;
    std::string m_http_version;
    std::string m_status_code;

    std::map<std::string, std::string> m_head_key_value_map;
};

#endif