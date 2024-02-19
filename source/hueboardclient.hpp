#pragma once
#include <boss.hpp>

class HueBoardClient
{
public:
    void Connect(chars host, sint32 port);
    void Login(chars author, chars password);
    void Logout();
    void NewPost();
    void Select(chars path);
    void NewSentence();
    void NewRipple();

public:
    bool TickOnce();
    bool TryRecvOnce();

private:
    void Reconnect();
    void Send(const Context& json);

private:
    void SendFastLogin(chars programid);
    void SendLogin(chars programid, chars author, chars password);
    void SendLogout();
    void SendLockAsset(chars lockid, chars route);
    void SendUnlockAsset(chars lockid, const Context& data);
    void SendFocusAsset(chars route);
    void SendUnfocusAsset(chars route);
    void SendFocusRange(chars route);
    void SendUnfocusRange(chars route);

private:
    void OnLogined(const Context& json);
    void OnAssetLocked(const Context& json);
    void OnAssetUpdated(const Context& json);
    void OnAssetChanged(const Context& json);
    void OnRangeUpdated(const Context& json);
    void OnErrored(const Context& json);

public: // 통신
    String mHost;
    sint32 mPort {0};
    id_socket mSocket {nullptr};
    bool mConnected {false};
    bool mNeedLogin {true};
    Strings mSendTasks;
    uint08s mRecvQueue;

public: // 계정
    String mAuthor;
    String mToken;
};
