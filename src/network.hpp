#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace tlp {
struct WinHttpHandle {
    HINTERNET h{nullptr};
    explicit WinHttpHandle(HINTERNET value): h(value) {}
    ~WinHttpHandle(){ if(h) WinHttpCloseHandle(h); }
    WinHttpHandle(const WinHttpHandle&) = delete;
    WinHttpHandle& operator=(const WinHttpHandle&) = delete;
    explicit operator bool() const {return h != nullptr;}
};

inline std::string fetchPvePrices() {
    constexpr wchar_t host[] = L"api.tarkov.dev";
    constexpr wchar_t path[] = L"/graphql";
    // Explicitly request PvE -- regular mode would silently give misleading prices.
    const std::string query = R"({"query":"query { items(gameMode: pve, lang: ru) { id name shortName gridImageLink width height lastLowPrice avg24hPrice sellFor { source priceRUB } } }"})";
    WinHttpHandle session(WinHttpOpen(L"TarkovLootPrices/0.2 (external PvE price overlay)", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session) throw std::runtime_error("WinHttpOpen failed");
    WinHttpSetTimeouts(session.h, 4000, 4000, 8000, 8000);
    WinHttpHandle connection(WinHttpConnect(session.h,host,INTERNET_DEFAULT_HTTPS_PORT,0));
    if (!connection) throw std::runtime_error("WinHttpConnect failed");
    WinHttpHandle request(WinHttpOpenRequest(connection.h,L"POST",path,nullptr,WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE));
    if (!request) throw std::runtime_error("WinHttpOpenRequest failed");
    constexpr wchar_t headers[] = L"Content-Type: application/json\r\nAccept: application/json\r\n";
    if (!WinHttpSendRequest(request.h,headers,static_cast<DWORD>(-1),
                            const_cast<char*>(query.data()),static_cast<DWORD>(query.size()),
                            static_cast<DWORD>(query.size()),0) ||
        !WinHttpReceiveResponse(request.h,nullptr))
        throw std::runtime_error("Price API request failed");
    DWORD status = 0, statusSize = sizeof(status);
    if (!WinHttpQueryHeaders(request.h,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX,&status,&statusSize,WINHTTP_NO_HEADER_INDEX) || status!=200)
        throw std::runtime_error("Price API returned HTTP status " + std::to_string(status));
    std::string response;
    while (true) {
        DWORD available = 0;
        if(!WinHttpQueryDataAvailable(request.h,&available)) throw std::runtime_error("HTTP read failed");
        if(!available) break;
        if(response.size()+available > 32*1024*1024) throw std::runtime_error("Price API response too large");
        const auto old = response.size();
        response.resize(old+available);
        DWORD read = 0;
        if(!WinHttpReadData(request.h,response.data()+old,available,&read))
            throw std::runtime_error("HTTP receive failed");
        response.resize(old+read);
        if(!read) break;
    }
    return response;
}
} // namespace tlp
