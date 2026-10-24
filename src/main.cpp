#include <windows.h>
#include <shlobj.h>
#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <cwctype>
#include "catalog.hpp"
#include "network.hpp"
#include "icon_scan.hpp"

namespace {
constexpr wchar_t kClass[] = L"TLPOverlay";
constexpr wchar_t kFinderClass[] = L"TLPFinder";
constexpr COLORREF kKey = RGB(1,2,3);
constexpr UINT kDataReady = WM_APP+1;
constexpr UINT kScanReady = WM_APP+2;
constexpr UINT kHotkeyToggle = 1, kHotkeyQuit = 2, kHotkeyFind = 3;
constexpr UINT kHotkeyRefresh = 4, kHotkeyScreenshot=5;
constexpr int kPitch=63;
struct Screen { int width{}, height{}; std::vector<std::uint8_t> pixels; };
struct Region { POINT a{}, b{}; bool valid() const {return b.x>a.x+kPitch && b.y>a.y+kPitch;} };
struct Label { int x{},y{}; std::wstring text; COLORREF color{}; };
HWND g_overlay{}, g_finder{}, g_edit{}, g_list{}, g_info{};
RECT g_virtual{};
std::mutex g_lock;
std::atomic<bool> g_running{true}, g_refresh{false}, g_visible{true};
std::atomic<std::uint64_t> g_catalogVersion{0};
std::atomic<int> g_priceMode{0}; // 0=flea, 1=roubles per slot, 2=best selling price
bool g_demo=false;
std::vector<tlp::Pattern> g_patterns;
std::vector<tlp::Item> g_catalog;
std::unordered_map<std::string,tlp::Item> g_byId;
std::vector<Label> g_labels;
std::vector<std::size_t> g_listMapping;
Region g_regions[2];
std::wstring g_error=L"Loading PvE prices...";
std::wstring g_selected;
std::jthread g_apiThread, g_scanThread;
std::filesystem::path g_storage, g_program;

std::wstring widen(const std::string& src){
    if(src.empty())return {};
    int len=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,src.c_str(),static_cast<int>(src.size()),nullptr,0);
    if(len<=0)return {};
    std::wstring dst(static_cast<std::size_t>(len),L'\0');
    MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,src.c_str(),static_cast<int>(src.size()),dst.data(),len);
    return dst;
}
std::wstring money(std::int64_t x) {
    if(x>=1000000)return std::to_wstring(x/1000000)+L"."+
      std::to_wstring((x%1000000)/100000)+L"M";
    if(x>=10000)return std::to_wstring(x/1000)+L"k";
    return std::to_wstring(x);
}
std::filesystem::path userPath() {
    wchar_t path[MAX_PATH]{};
    if(SUCCEEDED(SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,path)))
        return std::filesystem::path(path)/L"TarkovLootPrices";
    return g_program;
}
void readSettings(){
    const auto cfg=(g_storage/L"settings.ini").wstring();
    for(int n=0;n<2;++n) {
        auto section=n==0?L"inventory":L"container";
        auto& r=g_regions[n];
        r.a.x=GetPrivateProfileIntW(section,L"x1",0,cfg.c_str());
        r.a.y=GetPrivateProfileIntW(section,L"y1",0,cfg.c_str());
        r.b.x=GetPrivateProfileIntW(section,L"x2",0,cfg.c_str());
        r.b.y=GetPrivateProfileIntW(section,L"y2",0,cfg.c_str());
    }
}
void saveSettings(){
    const auto cfg=(g_storage/L"settings.ini").wstring();
    for(int n=0;n<2;++n) {
        const auto section=n==0?L"inventory":L"container";
        const auto& r=g_regions[n];
        const std::pair<const wchar_t*,int> fields[]={{L"x1",r.a.x},{L"y1",r.a.y},{L"x2",r.b.x},{L"y2",r.b.y}};
        for(auto [key,value]:fields) {
            auto s=std::to_wstring(value);
            WritePrivateProfileStringW(section,key,s.c_str(),cfg.c_str());
        }
    }
}
bool tarkovForeground(){
    HWND h=GetForegroundWindow();
    wchar_t title[256]{};
    if(!h || GetWindowTextW(h,title,256)==0)return false;
    std::wstring value(title);
    std::transform(value.begin(),value.end(),value.begin(),[](wchar_t c){return static_cast<wchar_t>(std::towlower(c));});
    return value.find(L"tarkov")!=std::wstring::npos;
}
Screen capture(const RECT& rect){
    Screen result;
    result.width=rect.right-rect.left; result.height=rect.bottom-rect.top;
    if(result.width<1||result.height<1||result.width>8192||result.height>8192)return {};
    HDC screen=GetDC(nullptr);
    if(!screen)return {};
    HDC memory=CreateCompatibleDC(screen);
    HBITMAP bitmap=CreateCompatibleBitmap(screen,result.width,result.height);
    if(!memory||!bitmap){if(bitmap)DeleteObject(bitmap);if(memory)DeleteDC(memory);ReleaseDC(nullptr,screen);return {};}
    HGDIOBJ old=SelectObject(memory,bitmap);
    const BOOL captured=BitBlt(memory,0,0,result.width,result.height,screen,rect.left,rect.top,SRCCOPY|CAPTUREBLT);
    BITMAPINFO info{};
    info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=result.width;
    info.bmiHeader.biHeight=-result.height;
    info.bmiHeader.biPlanes=1;
    info.bmiHeader.biBitCount=32;
    info.bmiHeader.biCompression=BI_RGB;
    result.pixels.resize(static_cast<std::size_t>(result.width)*result.height*4);
    if(!captured || !GetDIBits(memory,bitmap,0,static_cast<UINT>(result.height),result.pixels.data(),&info,DIB_RGB_COLORS))
        result={};
    SelectObject(memory,old);
    DeleteObject(bitmap); DeleteDC(memory); ReleaseDC(nullptr,screen);
    return result;
}
void saveScreenshot(){
    const RECT full{g_virtual.left,g_virtual.top,g_virtual.right,g_virtual.bottom};
    const auto screen=capture(full);
    if(screen.pixels.empty())return;
    BITMAPFILEHEADER fh{};
    BITMAPINFOHEADER ih{};
    ih.biSize=sizeof(ih); ih.biWidth=screen.width; ih.biHeight=-screen.height;
    ih.biPlanes=1; ih.biBitCount=32; ih.biCompression=BI_RGB;
    fh.bfType=0x4D42; fh.bfOffBits=sizeof(fh)+sizeof(ih);
    fh.bfSize=fh.bfOffBits+static_cast<DWORD>(screen.pixels.size());
    std::ofstream file(g_storage/"screen_capture.bmp",std::ios::binary|std::ios::trunc);
    if(!file)return;
    file.write(reinterpret_cast<const char*>(&fh),sizeof(fh));
    file.write(reinterpret_cast<const char*>(&ih),sizeof(ih));
    file.write(reinterpret_cast<const char*>(screen.pixels.data()),static_cast<std::streamsize>(screen.pixels.size()));
}
std::string readFile(const std::filesystem::path& path){
    std::ifstream file(path,std::ios::binary);
    if(!file)return {};
    return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
}
void writeFile(const std::filesystem::path& path,const std::string& content){
    try{
        auto tmp=path;tmp+=L".tmp";
        {std::ofstream file(tmp,std::ios::binary|std::ios::trunc);
         if(!file)return;file.write(content.data(),static_cast<std::streamsize>(content.size()));}
        std::filesystem::rename(tmp,path);
    }catch(...){}
}
void replacePrices(const std::string& raw, const std::string& locale, bool fresh){
    const auto parsed=tlp::parseCatalog(raw,locale);
    std::lock_guard guard(g_lock);
    g_catalog=parsed;
    g_byId.clear();
    for(const auto& item:g_catalog)g_byId.emplace(item.id,item);
    g_error=fresh?L"PvE prices updated":L"Using cached PvE prices";
    ++g_catalogVersion;
}
void apiLoop(std::stop_token token){
    const auto cache=g_storage/L"pve_prices.json";
    const auto translationCache=g_storage/L"pve_items_ru.json";
    std::string locale=readFile(translationCache);
    if(locale.empty()){
        try{
            locale=tlp::fetchPveRussian();
            writeFile(translationCache,locale);
        }catch(const std::exception&){}
    }
    try{
        const auto disk=readFile(cache);
        if(!disk.empty()){replacePrices(disk,locale,false);PostMessageW(g_overlay,kDataReady,0,0);}
    }catch(...){}
    int seconds=300;
    while(!token.stop_requested()&&g_running){
        if(seconds>=300||g_refresh.exchange(false)){
            try{
                if(locale.empty()){
                    try{locale=tlp::fetchPveRussian();writeFile(translationCache,locale);}
                    catch(const std::exception&){}
                }
                const auto response=tlp::fetchPvePrices();
                replacePrices(response,locale,true);
                writeFile(cache,response);
            }catch(const std::exception&){
                std::lock_guard guard(g_lock);
                if(g_catalog.empty())g_error=L"PvE JSON API unavailable; Ctrl+Shift+R retries";
            }
            PostMessageW(g_overlay,kDataReady,0,0);
            seconds=0;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
        ++seconds;
    }
}
void scanLoop(std::stop_token token){
    std::array<std::uint64_t,2> lastHashes{};
    std::array<std::vector<Label>,2> cachedLabels;
    while(!token.stop_requested()&&g_running){
        if(!g_visible||!tarkovForeground()||g_patterns.empty()){
            {std::lock_guard guard(g_lock);
                if(!g_labels.empty()){g_labels.clear();PostMessageW(g_overlay,kScanReady,0,0);}
            }
            lastHashes={};
            cachedLabels={};
            std::this_thread::sleep_for(std::chrono::milliseconds(550));
            continue;
        }
        Region areas[2];
        std::unordered_map<std::string,tlp::Item> prices;
        {std::lock_guard guard(g_lock);
            areas[0]=g_regions[0]; areas[1]=g_regions[1];prices=g_byId;
        }
        std::vector<Label> labels;
        bool changed=false;
        for(int n=0;n<2;++n){
            const auto& area=areas[n];
            if(!area.valid()){
                if(!cachedLabels[n].empty())changed=true;
                cachedLabels[n].clear();lastHashes[n]=0;
                continue;
            }
            RECT rect{area.a.x,area.a.y,area.b.x,area.b.y};
            auto frame=capture(rect);
            if(frame.pixels.empty())continue;
            std::uint64_t hash=14695981039346656037ull ^ g_catalogVersion.load() ^ (static_cast<std::uint64_t>(g_priceMode.load())<<56);
            for(int y=0;y<frame.height;y+=17)for(int x=0;x<frame.width;x+=17){
                const auto i=(static_cast<std::size_t>(y)*frame.width+x)*4;
                for(int k=0;k<3;++k){hash^=frame.pixels[i+k];hash*=1099511628211ull;}
            }
            hash^=static_cast<std::uint64_t>(static_cast<std::uint32_t>(area.a.x));hash*=1099511628211ull;
            hash^=static_cast<std::uint64_t>(static_cast<std::uint32_t>(area.a.y));hash*=1099511628211ull;
            if(lastHashes[n]==hash){
                labels.insert(labels.end(),cachedLabels[n].begin(),cachedLabels[n].end());
                continue;
            }
            changed=true;
            lastHashes[n]=hash;
            cachedLabels[n].clear();
            const tlp::ImageView view{frame.pixels.data(),frame.width,frame.height,frame.width*4};
            const auto matches=tlp::scanGrid(view,g_patterns,kPitch,19.0);
            for(const auto& found:matches){
                auto it=prices.find(found.id);
                if(it==prices.end())continue;
                const auto& item=it->second;
                const int mode=g_priceMode.load();
                const auto raw=item.fleaOrAverage()>0?item.fleaOrAverage():item.trader;
                const auto value=mode==2?item.bestOffer():mode==1?item.perSlot():raw;
                if(value<=0)continue;
                const wchar_t source=mode==1?L'S':mode==2?L'B':item.fleaOrAverage()>0?L'F':L'T';
                cachedLabels[n].push_back({area.a.x+found.x*kPitch+3,area.a.y+found.y*kPitch+3,
                    money(value)+L" "+source, value>=100000?RGB(110,235,165):RGB(245,214,128)});
            }
            labels.insert(labels.end(),cachedLabels[n].begin(),cachedLabels[n].end());
        }
        if(changed){
            {std::lock_guard guard(g_lock);g_labels=std::move(labels);}
            PostMessageW(g_overlay,kScanReady,0,0);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    }
}
void drawText(HDC dc,int x,int y,const std::wstring& content,COLORREF color,int px,int weight=FW_BOLD){
    HFONT font=CreateFontW(px,0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
       OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_SWISS,L"Segoe UI");
    if(!font)return;
    const HGDIOBJ original=SelectObject(dc,font);
    SetBkMode(dc,TRANSPARENT);SetTextColor(dc,color);
    TextOutW(dc,x,y,content.c_str(),static_cast<int>(content.size()));
    SelectObject(dc,original);DeleteObject(font);
}
void render(HWND h){
    PAINTSTRUCT ps{};HDC dc=BeginPaint(h,&ps);
    RECT rc{};GetClientRect(h,&rc);
    HBRUSH key=CreateSolidBrush(kKey);
    FillRect(dc,&rc,key); DeleteObject(key);
    std::vector<Label> labels;
    std::wstring status;
    std::size_t count=0;
    {std::lock_guard guard(g_lock);labels=g_labels;status=g_error;count=g_catalog.size();}
    if(g_visible)for(const auto& label:labels){
        const int x=label.x-g_virtual.left,y=label.y-g_virtual.top;
        RECT rectangle{x-2,y-1,x+110,y+22};
        HBRUSH bg=CreateSolidBrush(RGB(18,23,19));
        FillRect(dc,&rectangle,bg);DeleteObject(bg);
        drawText(dc,x+1,y,label.text,label.color,17);
    }
    if(g_demo||!g_selected.empty()){
        const int x=std::max(10,static_cast<int>(rc.right)-390),y=100;
        RECT panel{x,y,x+370,y+124};
        HBRUSH bg=CreateSolidBrush(RGB(22,27,26));FillRect(dc,&panel,bg);DeleteObject(bg);
        drawText(dc,x+12,y+8,L"TARKOV LOOT PRICES | PvE",RGB(222,215,180),20);
        if(g_demo){
            drawText(dc,x+12,y+44,L"ICON SCANNER / PRICE CATALOG",RGB(132,215,168),16);
            drawText(dc,x+12,y+69,status,RGB(235,220,166),15);
            drawText(dc,x+12,y+93,std::to_wstring(count)+L" PvE items / "+std::to_wstring(g_patterns.size())+L" icons",RGB(205,210,210),15);
        }else{
            drawText(dc,x+12,y+51,g_selected,RGB(132,215,168),18);
            drawText(dc,x+12,y+86,L"F = flea    T = trader (fallback)",RGB(205,210,210),14);
        }
    }
    EndPaint(h,&ps);
}
void updateFinder(){
    if(!g_finder||!IsWindowVisible(g_finder))return;
    wchar_t query[256]{};
    GetWindowTextW(g_edit,query,256);
    std::wstring term(query);
    CharLowerBuffW(term.data(),static_cast<DWORD>(term.size()));
    SendMessageW(g_list,LB_RESETCONTENT,0,0);
    g_listMapping.clear();
    std::vector<tlp::Item> snapshot;
    std::wstring status;
    {std::lock_guard guard(g_lock);snapshot=g_catalog;status=g_error;}
    for(std::size_t i=0;i<snapshot.size()&&g_listMapping.size()<200;++i){
        auto name=widen(snapshot[i].name);
        std::wstring lc=name; CharLowerBuffW(lc.data(),static_cast<DWORD>(lc.size()));
        if(!term.empty()&&lc.find(term)==std::wstring::npos)continue;
        auto line=name+L"     "+money(snapshot[i].fleaOrAverage())+L" F / "+money(snapshot[i].trader)+L" T";
        SendMessageW(g_list,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(line.c_str()));
        g_listMapping.push_back(i);
    }
    SetWindowTextW(g_info,(L"PvE items: "+std::to_wstring(snapshot.size())+L" | "+status).c_str());
}
LRESULT CALLBACK finderProc(HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam){
    switch(message){
    case WM_CREATE:{
        g_edit=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,
                              12,14,540,28,hwnd,reinterpret_cast<HMENU>(101),nullptr,nullptr);
        g_list=CreateWindowExW(WS_EX_CLIENTEDGE,L"LISTBOX",L"",WS_CHILD|WS_VISIBLE|LBS_NOTIFY|WS_VSCROLL,
                              12,50,540,335,hwnd,reinterpret_cast<HMENU>(102),nullptr,nullptr);
        g_info=CreateWindowExW(0,L"STATIC",L"Loading...",WS_CHILD|WS_VISIBLE,
                              12,397,540,25,hwnd,nullptr,nullptr,nullptr);
        return 0;
    }
    case WM_COMMAND:
        if(LOWORD(wParam)==101&&HIWORD(wParam)==EN_CHANGE){updateFinder();return 0;}
        if(LOWORD(wParam)==102&&(HIWORD(wParam)==LBN_SELCHANGE||HIWORD(wParam)==LBN_DBLCLK)){
            const auto row=SendMessageW(g_list,LB_GETCURSEL,0,0);
            if(row!=LB_ERR&&static_cast<std::size_t>(row)<g_listMapping.size()){
                std::lock_guard guard(g_lock);
                if(g_listMapping[row]<g_catalog.size()){
                    const auto& i=g_catalog[g_listMapping[row]];
                    g_selected=widen(i.name)+L"  "+money(i.fleaOrAverage())+L" F | "+money(i.trader)+L" T";
                }
            }
            InvalidateRect(g_overlay,nullptr,FALSE);
            return 0;
        }
        break;
    case WM_CLOSE:ShowWindow(hwnd,SW_HIDE);g_selected.clear();InvalidateRect(g_overlay,nullptr,FALSE);return 0;
    }
    return DefWindowProcW(hwnd,message,wParam,lParam);
}
void hotkey(HWND hwnd,WPARAM id){
    if(id==kHotkeyQuit){DestroyWindow(hwnd);return;}
    if(id==kHotkeyToggle){g_visible=!g_visible;InvalidateRect(hwnd,nullptr,FALSE);return;}
    if(id==kHotkeyFind){
        if(IsWindowVisible(g_finder)){ShowWindow(g_finder,SW_HIDE);g_selected.clear();}
        else {ShowWindow(g_finder,SW_SHOW);SetForegroundWindow(g_finder);SetFocus(g_edit);updateFinder();}
        InvalidateRect(hwnd,nullptr,FALSE);
        return;
    }
    if(id==kHotkeyRefresh){g_refresh=true;return;}
    if(id==6){g_priceMode=(g_priceMode.load()+1)%3; ++g_catalogVersion;return;}
    if(id==kHotkeyScreenshot){
        ShowWindow(g_overlay,SW_HIDE);
        Sleep(80);saveScreenshot();
        ShowWindow(g_overlay,SW_SHOWNOACTIVATE);
        return;
    }
    if(id>=10&&id<=13){
        POINT p{};GetCursorPos(&p);
        const int n=(static_cast<int>(id)-10)/2;
        {
            std::lock_guard guard(g_lock);
            if((id-10)%2==0)g_regions[n].a=p;
            else g_regions[n].b=p;
            saveSettings();
        }
        return;
    }
}
LRESULT CALLBACK overlayProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam){
    switch(msg){
    case WM_CREATE:
        RegisterHotKey(hwnd,kHotkeyToggle,MOD_CONTROL|MOD_SHIFT|MOD_NOREPEAT,'P');
        RegisterHotKey(hwnd,kHotkeyQuit,MOD_CONTROL|MOD_SHIFT|MOD_NOREPEAT,'Q');
        RegisterHotKey(hwnd,kHotkeyFind,MOD_CONTROL|MOD_SHIFT|MOD_NOREPEAT,'F');
        RegisterHotKey(hwnd,kHotkeyRefresh,MOD_CONTROL|MOD_SHIFT|MOD_NOREPEAT,'R');
        RegisterHotKey(hwnd,kHotkeyScreenshot,MOD_CONTROL|MOD_SHIFT|MOD_NOREPEAT,'S');
        RegisterHotKey(hwnd,6,MOD_CONTROL|MOD_SHIFT|MOD_NOREPEAT,'V');
        for(int i=0;i<4;++i)RegisterHotKey(hwnd,10+i,MOD_CONTROL|MOD_ALT|MOD_NOREPEAT,'1'+i);
        return 0;
    case WM_HOTKEY:hotkey(hwnd,wParam);return 0;
    case kDataReady:updateFinder();InvalidateRect(hwnd,nullptr,FALSE);return 0;
    case kScanReady:InvalidateRect(hwnd,nullptr,FALSE);return 0;
    case WM_NCHITTEST:return HTTRANSPARENT;
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:render(hwnd);return 0;
    case WM_DESTROY:
        g_running=false;
        for(int i=1;i<=6;++i)UnregisterHotKey(hwnd,i);
        for(int i=10;i<=13;++i)UnregisterHotKey(hwnd,i);
        PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(hwnd,msg,wParam,lParam);
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR args,int){
    g_demo=args&&wcsstr(args,L"--demo");
    SetProcessDPIAware();
    wchar_t exe[MAX_PATH]{};
    GetModuleFileNameW(nullptr,exe,MAX_PATH);
    g_program=std::filesystem::path(exe).parent_path();
    g_storage=userPath();
    try{std::filesystem::create_directories(g_storage);}catch(...){}
    readSettings();
    try{g_patterns=tlp::parsePatterns(readFile(g_program/L"data"/L"icons.json"));}catch(...){}
    g_virtual={GetSystemMetrics(SM_XVIRTUALSCREEN),GetSystemMetrics(SM_YVIRTUALSCREEN),
       GetSystemMetrics(SM_XVIRTUALSCREEN)+GetSystemMetrics(SM_CXVIRTUALSCREEN),
       GetSystemMetrics(SM_YVIRTUALSCREEN)+GetSystemMetrics(SM_CYVIRTUALSCREEN)};
    WNDCLASSEXW cls{};cls.cbSize=sizeof(cls);cls.lpfnWndProc=overlayProc;
    cls.hInstance=instance;cls.lpszClassName=kClass;
    if(!RegisterClassExW(&cls))return 1;
    WNDCLASSEXW f{};f.cbSize=sizeof(f);f.lpfnWndProc=finderProc;f.hInstance=instance;
    f.lpszClassName=kFinderClass;f.hCursor=LoadCursor(nullptr,IDC_ARROW);
    if(!RegisterClassExW(&f))return 2;
    g_overlay=CreateWindowExW(WS_EX_TOPMOST|WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,
       kClass,L"Tarkov Loot Prices Overlay",WS_POPUP,g_virtual.left,g_virtual.top,
       g_virtual.right-g_virtual.left,g_virtual.bottom-g_virtual.top,nullptr,nullptr,instance,nullptr);
    if(!g_overlay)return 3;
    SetLayeredWindowAttributes(g_overlay,kKey,0,LWA_COLORKEY);
    SetWindowDisplayAffinity(g_overlay,0x00000011); // WDA_EXCLUDEFROMCAPTURE where supported.
    g_finder=CreateWindowExW(WS_EX_TOOLWINDOW,kFinderClass,L"Tarkov Loot Prices - PvE Search",
       WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,
       130,130,580,470,nullptr,nullptr,instance,nullptr);
    ShowWindow(g_overlay,SW_SHOWNOACTIVATE);
    g_apiThread=std::jthread(apiLoop);
    g_scanThread=std::jthread(scanLoop);
    MSG message{};
    while(GetMessageW(&message,nullptr,0,0)>0){TranslateMessage(&message);DispatchMessageW(&message);}
    g_running=false;
    g_apiThread.request_stop();
    g_scanThread.request_stop();
    if(g_apiThread.joinable())g_apiThread.join();
    if(g_scanThread.joinable())g_scanThread.join();
    if(g_finder)DestroyWindow(g_finder);
    return static_cast<int>(message.wParam);
}
