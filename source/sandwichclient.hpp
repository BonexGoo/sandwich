#pragma once
#include <boss.hpp>

class SandWichClient
{
public:
    void Connect(chars programid, chars host, sint32 port);
    void Login(chars author, chars password);
    void Logout();
    void NewPost(chars text);
    void NewSentence(chars text);
    void NewRipple(chars text);
    void NewUpload(chars lockid, chars route);
    void Select(chars type, sint32 index);
    void PythonExecute(sint32 postindex);

public:
    bool TickOnce();
    bool TryRecvOnce();
    bool TryWorkingOnce();

private:
    void Reconnect();
    void Send(const Context& json);
    String GetCheckSum(chars asset, chars path);

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
    void SendDownloadFile(chars memo, chars path);
    void SendPythonExecute(chars runid, chars path);

private:
    void OnLogined(const Context& json);
    void OnLogouted(const Context& json);
    void OnAssetLocked(const Context& json);
    void OnAssetUpdated(const Context& json);
    void OnAssetChanged(const Context& json);
    void OnRangeUpdated(const Context& json);
    void OnFileDownloading(const Context& json);
    void OnFileDownloaded(const Context& json);
    void OnExecutedPython(const Context& json);
    void OnErrored(const Context& json);

public: // 통신
    String mProgramID;
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
    String mNewPost;
    String mNewSentence;
    String mNewRipple;
    Strings mWorkingLockIDs;
    uint08s mWorkingData;
    Map<uint08s> mDownloadingData;
};
