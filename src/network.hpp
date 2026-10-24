#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>
#include <stdexcept>
#include <string>

namespace tlp {
struct WinHttpHandle {
    HINTERNET h{nullptr};
    explicit WinHttpHandle(HINTERNET value):h(value){}
    ~WinHttpHandle(){if(h)WinHttpCloseHandle(h);}
    WinHttpHandle(const WinHttpHandle&)=delete;
    WinHttpHandle& operator=(const WinHttpHandle&)=delete;
    explicit operator bool()const{return h!=nullptr;}
};

// Current maintained API: GET https://json.tarkov.dev/pve/items
// Do not fall back to "regular": a PvP value is incorrect for the user's official PvE mode.
inline std::string fetchJson(const wchar_t* path) {
    constexpr wchar_t host[]=L"json.tarkov.dev";
    WinHttpHandle session(WinHttpOpen(L"TarkovLootPrices/0.3",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
    if(!session)throw std::runtime_error("WinHttpOpen failed");
    WinHttpSetTimeouts(session.h,5000,5000,15000,15000);
    WinHttpHandle connection(WinHttpConnect(session.h,host,INTERNET_DEFAULT_HTTPS_PORT,0));
    if(!connection)throw std::runtime_error("WinHttpConnect failed");
    WinHttpHandle request(WinHttpOpenRequest(connection.h,L"GET",path,nullptr,WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE));
    if(!request)throw std::runtime_error("WinHttpOpenRequest failed");
    constexpr wchar_t headers[]=L"Accept: application/json\r\n";
    if(!WinHttpSendRequest(request.h,headers,static_cast<DWORD>(-1),
                           WINHTTP_NO_REQUEST_DATA,0,0,0) ||
       !WinHttpReceiveResponse(request.h,nullptr))
        throw std::runtime_error("Price API request failed");
    DWORD status=0,size=sizeof(status);
    if(!WinHttpQueryHeaders(request.h,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX)||status!=200)
        throw std::runtime_error("JSON API returned HTTP "+std::to_string(status));
    std::string result;
    while(true){
        DWORD available=0;
        if(!WinHttpQueryDataAvailable(request.h,&available))throw std::runtime_error("HTTP data availability failed");
        if(!available)break;
        if(result.size()+available > 40*1024*1024)throw std::runtime_error("JSON API response too large");
        const auto pos=result.size();
        result.resize(pos+available);
        DWORD read=0;
        if(!WinHttpReadData(request.h,result.data()+pos,available,&read))
            throw std::runtime_error("JSON API response read failed");
        result.resize(pos+read);
        if(!read)break;
    }
    return result;
}
inline std::string fetchPvePrices(){return fetchJson(L"/pve/items");}
inline std::string fetchPveRussian(){return fetchJson(L"/pve/items_ru");}
} // namespace tlp
