#include <boss.hpp>
#include "hueboardclient.hpp"

#include <platform/boss_platform.hpp>
#include <service/boss_zaywidget.hpp>

void HueBoardClient::Connect(chars host, sint32 port)
{
    mHost = host;
    mPort = port;
    Reconnect();
}

void HueBoardClient::Login(chars author, chars password)
{
    SendLogin("HueBoard", author, password);
}

void HueBoardClient::Logout()
{
    SendLogout();
}

void HueBoardClient::NewPost(chars text)
{
    mNewPost = text;
    SendLockAsset("NewPost", "post<next>");
}

void HueBoardClient::NewSentence(chars text)
{
    mNewSentence = text;
    const String SelPost = ZayWidgetDOM::GetValue("hueboard.select.post").ToText();
    if(SelPost != "-1")
        SendLockAsset("NewSentence", "post." + SelPost + ".sentence<next>");
}

void HueBoardClient::NewRipple(chars text)
{
    mNewRipple = text;
    const String SelPost = ZayWidgetDOM::GetValue("hueboard.select.post").ToText();
    const String SelSentence = ZayWidgetDOM::GetValue("hueboard.select.sentence").ToText();
    if(SelPost != "-1" && SelSentence != "-1")
        SendLockAsset("NewRipple", "post." + SelPost + ".sentence." + SelSentence + ".ripple<next>");
}

void HueBoardClient::Select(chars type, sint32 index)
{
    ZayWidgetDOM::SetValue(String::Format("hueboard.select.%s", type), String::FromInteger(index));
    if(index != -1)
    {
        String Path;
        if(!String::Compare(type, "post"))
            Path = String::Format("post.%d.sentence", index);
        else if(!String::Compare(type, "sentence"))
        {
            const sint32 PostIndex = ZayWidgetDOM::GetValue("hueboard.select.post").ToInteger();
            Path = String::Format("post.%d.sentence.%d.ripple", PostIndex, index);
        }
        else return;

        // 어셋포커싱
        const String DomPath = String::Format("hueboard.range.%s.count", (chars) Path);
        const sint32 SentenceCount = ZayWidgetDOM::GetValue(DomPath).ToInteger();
        for(sint32 i = 0; i < SentenceCount; ++i)
        {
            const String CurRoute = String::Format("%s.%d", (chars) Path, i);
            SendFocusAsset(CurRoute);
        }
    }
}

bool HueBoardClient::TickOnce()
{
    bool NeedUpdate = false;
    // 통신 연결확인
    if(!mConnected && Platform::Socket::IsConnected(mSocket))
    {
        mConnected = true;
        NeedUpdate = true;
    }
    if(mConnected)
    {
        // 필요시 로그인
        if(mNeedLogin)
        {
            mNeedLogin = false;
            SendFastLogin("HueBoard");
        }
        // 지연된 송신처리
        for(sint32 i = 0, iend = mSendTasks.Count(); i < iend; ++i)
            Platform::Socket::Send(mSocket, (bytes)(chars) mSendTasks[i],
                mSendTasks[i].Length() + 1, 3000, true);
        mSendTasks.Clear();
    }
    return NeedUpdate;
}

bool HueBoardClient::TryRecvOnce()
{
    bool NeedUpdate = false;
    while(0 < Platform::Socket::RecvAvailable(mSocket))
    {
        uint08 RecvTemp[4096];
        sint32 RecvSize = Platform::Socket::Recv(mSocket, RecvTemp, 4096);
        if(0 < RecvSize)
            Memory::Copy(mRecvQueue.AtDumpingAdded(RecvSize), RecvTemp, RecvSize);
        else if(RecvSize < 0)
            break;

        sint32 QueueEndPos = 0;
        for(sint32 iend = mRecvQueue.Count(), i = iend - RecvSize; i < iend; ++i)
        {
            if(mRecvQueue[i] == '\0')
            {
                // 스트링 읽기
                const String Packet((chars) &mRecvQueue[QueueEndPos], i - QueueEndPos);
                QueueEndPos = i + 1;
                if(0 < Packet.Length())
                {
                    const Context RecvJson(ST_Json, SO_OnlyReference, Packet);
                    const String Type = RecvJson("type").GetText();

                    branch;
                    jump(Type == "Logined") OnLogined(RecvJson);
                    jump(Type == "AssetLocked") OnAssetLocked(RecvJson);
                    jump(Type == "AssetUpdated") OnAssetUpdated(RecvJson);
                    jump(Type == "AssetChanged") OnAssetChanged(RecvJson);
                    jump(Type == "RangeUpdated") OnRangeUpdated(RecvJson);
                    jump(Type == "Errored") OnErrored(RecvJson);
                }
                NeedUpdate = true;
            }
        }
        if(0 < QueueEndPos)
            mRecvQueue.SubtractionSection(0, QueueEndPos);
    }
    return NeedUpdate;
}

void HueBoardClient::Reconnect()
{
    mConnected = false;
    Platform::Socket::Close(mSocket);
    mSocket = Platform::Socket::OpenForWS(false);
    Platform::Socket::ConnectAsync(mSocket, mHost, mPort);
}

void HueBoardClient::Send(const Context& json)
{
    const String NewPacket = json.SaveJson();
    if(mConnected)
    {
        const sint32 Result = Platform::Socket::Send(mSocket,
            (bytes)(chars) NewPacket, NewPacket.Length() + 1, 3000, true);
        if(Result < 0)
        {
            mConnected = false;
            Reconnect();
        }
    }
    if(!mConnected)
        mSendTasks.AtAdding() = NewPacket;
}

void HueBoardClient::SendFastLogin(chars programid)
{
    Context Json;
    Json.At("type").Set("Login");
    Json.At("programid").Set(programid);
    Json.At("deviceid").Set(Platform::Utility::GetDeviceID());
    Send(Json);
}

void HueBoardClient::SendLogin(chars programid, chars author, chars password)
{
    Context Json;
    Json.At("type").Set("LoginUpdate");
    Json.At("programid").Set(programid);
    Json.At("deviceid").Set(Platform::Utility::GetDeviceID());
    Json.At("author").Set(author);
    Json.At("password").Set(password);
    Send(Json);
}

void HueBoardClient::SendLogout()
{
    Context Json;
    Json.At("type").Set("Logout");
    Json.At("token").Set(mToken);
    Send(Json);

    mAuthor.Empty();
    mToken.Empty();
    ZayWidgetDOM::RemoveVariables("hueboard.");
    ZayWidgetDOM::SetValue("hueboard.showlogin", "1");
}

void HueBoardClient::SendLockAsset(chars lockid, chars route)
{
    Context Json;
    Json.At("type").Set("LockAsset");
    Json.At("token").Set(mToken);
    Json.At("lockid").Set(lockid);
    Json.At("route").Set(route);
    Send(Json);
}

void HueBoardClient::SendUnlockAsset(chars lockid, const Context& data)
{
    Context Json;
    Json.At("type").Set("UnlockAsset");
    Json.At("token").Set(mToken);
    Json.At("lockid").Set(lockid);
    Json.At("data").LoadJson(SO_NeedCopy, data.SaveJson());
    Send(Json);
}

void HueBoardClient::SendFocusAsset(chars route)
{
    Context Json;
    Json.At("type").Set("FocusAsset");
    Json.At("token").Set(mToken);
    Json.At("route").Set(route);
    Send(Json);
}

void HueBoardClient::SendUnfocusAsset(chars route)
{
    Context Json;
    Json.At("type").Set("UnfocusAsset");
    Json.At("token").Set(mToken);
    Json.At("route").Set(route);
    Send(Json);
}

void HueBoardClient::SendFocusRange(chars route)
{
    Context Json;
    Json.At("type").Set("FocusRange");
    Json.At("token").Set(mToken);
    Json.At("route").Set(route);
    Send(Json);
}

void HueBoardClient::SendUnfocusRange(chars route)
{
    Context Json;
    Json.At("type").Set("UnfocusRange");
    Json.At("token").Set(mToken);
    Json.At("route").Set(route);
    Send(Json);
}

void HueBoardClient::OnLogined(const Context& json)
{
    mAuthor = json("author").GetText();
    mToken = json("token").GetText();
    ZayWidgetDOM::SetValue("hueboard.showlogin", "0");
    ZayWidgetDOM::SetValue("hueboard.author", "'" + mAuthor + "'");
    ZayWidgetDOM::SetValue("hueboard.token", "'" + mToken + "'");
    ZayWidgetDOM::RemoveVariables("hueboard.login.");
    SendFocusRange("post");
}

void HueBoardClient::OnAssetLocked(const Context& json)
{
    const String LockID = json("lockid").GetText();
    if(LockID == "NewPost")
    {
        Context Data;
        Data.At("text").Set(mNewPost);
        SendUnlockAsset(LockID, Data);
    }
    else if(LockID == "NewSentence")
    {
        Context Data;
        Data.At("text").Set(mNewSentence);
        SendUnlockAsset(LockID, Data);
    }
    else if(LockID == "NewRipple")
    {
        Context Data;
        Data.At("text").Set(mNewRipple);
        SendUnlockAsset(LockID, Data);
    }
}

void HueBoardClient::OnAssetUpdated(const Context& json)
{
    const String Author = json("author").GetText();
    const String Path = String(json("path").GetText()).Replace('/', '.');
    const String Status = json("status").GetText();
    const String Version = json("version").GetText();

    const String Header = "hueboard.asset." + Path;
    ZayWidgetDOM::SetValue(Header + ".author", "'" + Author + "'");
    ZayWidgetDOM::SetValue(Header + ".status", "'" + Status + "'");
    ZayWidgetDOM::SetValue(Header + ".version", "'" + Version + "'");
    ZayWidgetDOM::SetJson(json("data"), Header + ".data.");

    const Strings Pathes = String::Split(Path, '.');
    if(0 < Pathes.Count())
    {
        const String PathType = Pathes[-2];
        if(PathType == "post")
            SendFocusRange(Path + ".sentence");
        else if(PathType == "sentence")
            SendFocusRange(Path + ".ripple");
    }
}

void HueBoardClient::OnAssetChanged(const Context& json)
{
    const String Path = String(json("path").GetText()).Replace('/', '.');
    const String Status = json("status").GetText();

    const String Header = "hueboard.asset." + Path;
    ZayWidgetDOM::SetValue(Header + ".status", "'" + Status + "'");
}

void HueBoardClient::OnRangeUpdated(const Context& json)
{
    const String Path = String(json("path").GetText()).Replace('/', '.');
    const sint32 First = json("first").GetInt();
    const sint32 Last = json("last").GetInt();

    const String Header = "hueboard.range." + Path;
    const sint32 Count = Math::Max(0, Last + 1 - First);
    ZayWidgetDOM::SetValue(Header + ".count", String::FromInteger(Count));

    bool NeedFocusing = false;
    const Strings Pathes = String::Split(Path, '.');
    if(Pathes[-1] == "post") NeedFocusing = true;
    else if(Pathes[-1] == "sentence" && ZayWidgetDOM::GetValue("hueboard.select.post").ToText() != "-1")
        NeedFocusing = true;
    else if(Pathes[-1] == "ripple" && ZayWidgetDOM::GetValue("hueboard.select.sentence").ToText() != "-1")
        NeedFocusing = true;

    // 어셋포커싱
    if(NeedFocusing)
    for(sint32 i = 0; i < Count; ++i)
    {
        const String CurRoute = String::Format("%s.%d", (chars) Path, First + i);
        SendFocusAsset(CurRoute);
    }
}

void HueBoardClient::OnErrored(const Context& json)
{
    const String Packet = json("packet").GetText();
    const String Text = json("text").GetText();

    if(Text == "Expired token")
    {
        ZayWidgetDOM::SetValue("hueboard.showlogin", "1");
        ZayWidgetDOM::SetValue("hueboard.login.error", "'" + Text + "'");
    }
    else if(Packet == "Login")
    {
        if(Text == "Unregistered device")
            ZayWidgetDOM::SetValue("hueboard.showlogin", "1");
        else ZayWidgetDOM::SetValue("hueboard.login.error", "'" + Text + "'");
    }
    else if(Packet == "LoginUpdate")
        ZayWidgetDOM::SetValue("hueboard.login.error", "'" + Text + "'");
    else BOSS_TRACE("OnErrored[%s] \"%s\"", (chars) Packet, (chars) Text);
}
