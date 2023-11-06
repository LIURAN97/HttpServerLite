#include "HttpBuffer.h"
#include <algorithm>

HttpBuffer::HttpBuffer()
    : m_buffer(""), m_header(""), m_body(""), m_request_method(""), m_url(""), m_http_version(""), m_status_code("")
{

}

HttpBuffer::HttpBuffer(std::string &&buffer)
{
    m_buffer = buffer;
    ParseHttpBuffer();
}

HttpBuffer::HttpBuffer(HttpBuffer &&http_buffer)
{
    if (this != &http_buffer)
    {
        m_buffer = http_buffer.m_buffer;
        m_header = http_buffer.m_header;
        m_body = http_buffer.m_body;
        m_request_method = http_buffer.m_request_method;
        m_url = http_buffer.m_url;
        m_http_version = http_buffer.m_http_version;
        m_status_code = http_buffer.m_status_code;
        m_head_key_value_map = http_buffer.m_head_key_value_map;
    }
}

HttpBuffer &HttpBuffer::operator=(HttpBuffer &&http_buffer)
{
    if (this != &http_buffer)
    {
        m_buffer = http_buffer.m_buffer;
        m_header = http_buffer.m_header;
        m_body = http_buffer.m_body;
        m_request_method = http_buffer.m_request_method;
        m_url = http_buffer.m_url;
        m_http_version = http_buffer.m_http_version;
        m_status_code = http_buffer.m_status_code;
        m_head_key_value_map = http_buffer.m_head_key_value_map;
    }

    return *this;
}

HttpBuffer::~HttpBuffer()
{
}

void HttpBuffer::ReadBuffer(std::string &&buffer)
{
    m_buffer = buffer;
    ParseHttpBuffer();
}

std::string HttpBuffer::GetHeaderValue(std::string key)
{
    std::string value;
    if (auto iter = m_head_key_value_map.find(key);
    iter != m_head_key_value_map.end())
    {
        value = iter->second;
    }
    return value;
}

void HttpBuffer::WriteHttpRequest(const std::string &method, const std::string &url, const std::string &version, \
    std::map<std::string, std::string> &&header_key_value, std::string &&body)
{
    std::string header = method + ' ' + url + ' ' + version + "\r\n";
    m_buffer = BuildHttpBuffer(std::move(header), std::forward<std::string &&>(body));
}

void HttpBuffer::WriteHttpResponse(const std::string &version, const std::string &status_code, const std::string &tip, \
    std::map<std::string, std::string> &&header_key_value, std::string &&body)
{
    std::string header = version + ' ' + status_code + ' ' + tip + "\r\n";
    m_buffer = BuildHttpBuffer(std::move(header), std::forward<std::string &&>(body));
}

bool HttpBuffer::ParseHttpBuffer()
{
    std::size_t header_pos = m_buffer.find("\r\n\r\n");
    if (header_pos == std::string::npos)
    {
        printf("ParseHttpBuffer cant find \\r\\n\\r\\n !\n");
        return false;
    }
    
    m_header = m_buffer.substr(0, header_pos + 2);
    m_body = m_buffer.substr(header_pos + 4, m_buffer.length() - 1);

    std::string tmp_header = m_header;
    std::size_t crlf_pos = tmp_header.find("\r\n");
    if (crlf_pos == std::string::npos)
    {
        crlf_pos = tmp_header.length() - 1;
    }

    //用空格分割字符串，专门用来分割header中第一行字符串
    auto split_str = [](std::string &str) -> std::string {
        std::string tmp_str = str;
        std::size_t tmp_pos = str.find(' ');
        if (tmp_pos == std::string::npos)
        {
            str = "";
            return tmp_str;
        }

        tmp_str = str.substr(0, tmp_pos);
        str = str.substr(tmp_pos + 1, str.length() - 1);
        return tmp_str;
    };

    auto parse_header_first_line = [split_str](std::string &&str, std::string &attr1, std::string &attr2, std::string &attr3){
        attr1 = split_str(str);
        if (!str.empty())
        {
            attr2 = split_str(str);
            if (!str.empty())
            {
                attr3 = split_str(str);
            }
        }
    };

    std::string tmp_first_line = tmp_header.substr(0, crlf_pos);
    if (IsHttpRequest(tmp_first_line))
    {
        parse_header_first_line(std::move(tmp_first_line), m_request_method, m_url, m_http_version);
    }
    else
    {
        std::string tmp_str;
        parse_header_first_line(std::move(tmp_first_line), m_http_version, m_status_code, tmp_str);
    }
    
    //解析header中的字段
    while ((crlf_pos = tmp_header.find("\r\n")) != std::string::npos)
    {
        std::string key_value = tmp_header.substr(0, crlf_pos);
        if (auto key_pos = key_value.find(":");
        key_pos != std::string::npos)
        {
            std::string key = key_value.substr(0, key_pos);
            std::string value = key_value.substr(key_pos + 2, key_value.length() - 1);
            m_head_key_value_map.emplace(key, value);
        }

        tmp_header = tmp_header.substr(crlf_pos + 2, tmp_header.length() - 1);
    }

    return true;
}

bool HttpBuffer::IsHttpRequest(std::string buffer)
{
    for (std::string method : request_method_vec)
    {
        if (buffer.find(method) != std::string::npos)
        {
            return true;
        }
    }

    return false;
}

std::string HttpBuffer::BuildHttpBuffer(std::string &&header, std::string &&body)
{
    std::for_each(m_head_key_value_map.begin(), m_head_key_value_map.end(), [&header](std::pair<std::string, std::string> key_value){
        header += key_value.first + ":" + ' ' + key_value.second + "\r\n";
    });

    return header + "\r\n" + body;
}
