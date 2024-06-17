#include <boss.hpp>
#include "sandwichclient.hpp"

#include <platform/boss_platform.hpp>
#include <service/boss_zaywidget.hpp>

void SandWichClient::Connect(chars programid, chars host, sint32 port)
{
    mProgramID = programid;
    mHost = host;
    mPort = port;
    Reconnect();
}

void SandWichClient::Login(chars author, chars password)
{
    SendLogin(mProgramID, author, password);
}

void SandWichClient::Logout()
{
    if(mAuthor != "-")
        SendLogout();
}

void SandWichClient::NewPost(chars text)
{
    if(mAuthor != "-")
    {
        mNewPost = text;
        SendLockAsset("NewPost", "post<next>");
    }
}

void SandWichClient::NewSentence(chars text)
{
    if(mAuthor != "-")
    {
        mNewSentence = text;
        const String SelPost = ZayWidgetDOM::GetValue("sandwich.select.post").ToText();
        if(SelPost != "-1")
            SendLockAsset("NewSentence", "post." + SelPost + ".sentence<next>");
    }
}

void SandWichClient::NewRipple(chars text)
{
    if(mAuthor != "-")
    {
        mNewRipple = text;
        const String SelPost = ZayWidgetDOM::GetValue("sandwich.select.post").ToText();
        const String SelSentence = ZayWidgetDOM::GetValue("sandwich.select.sentence").ToText();
        if(SelPost != "-1" && SelSentence != "-1")
            SendLockAsset("NewRipple", "post." + SelPost + ".sentence." + SelSentence + ".ripple<next>");
    }
}

void SandWichClient::NewUpload(chars lockid, chars route)
{
    SendLockAsset(lockid, route);
}

void SandWichClient::Select(chars type, sint32 index)
{
    ZayWidgetDOM::SetValue(String::Format("sandwich.select.%s", type), String::FromInteger(index));
    if(index != -1)
    {
        String Path;
        if(!String::Compare(type, "post"))
            Path = String::Format("post.%d.sentence", index);
        else if(!String::Compare(type, "sentence"))
        {
            const sint32 PostIndex = ZayWidgetDOM::GetValue("sandwich.select.post").ToInteger();
            Path = String::Format("post.%d.sentence.%d.ripple", PostIndex, index);
        }
        else return;

        // 어셋포커싱
        const String DomPath = String::Format("sandwich.range.%s.count", (chars) Path);
        const sint32 SentenceCount = ZayWidgetDOM::GetValue(DomPath).ToInteger();
        for(sint32 i = 0; i < SentenceCount; ++i)
        {
            const String CurRoute = String::Format("%s.%d", (chars) Path, i);
            SendFocusAsset(CurRoute);
        }
    }
}

bool SandWichClient::TickOnce()
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
            SendFastLogin(mProgramID);
        }
        // 지연된 송신처리
        for(sint32 i = 0, iend = mSendTasks.Count(); i < iend; ++i)
            Platform::Socket::Send(mSocket, (bytes)(chars) mSendTasks[i],
                mSendTasks[i].Length() + 1, 3000, true);
        mSendTasks.Clear();
    }
    return NeedUpdate;
}

bool SandWichClient::TryRecvOnce()
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
                    jump(Type == "Logouted") OnLogouted(RecvJson);
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

bool SandWichClient::TryWorkingOnce()
{
    if(const sint32 LockIDCount = mWorkingLockIDs.Count())
    {
        const sint32 LockIDIndex = Platform::Utility::Random() % LockIDCount;
        const String& CurLockID = mWorkingLockIDs[LockIDIndex];
        if(!String::Compare(CurLockID, strpair("NewUpload_")))
        {
            const String Header = "sandwich.work." + CurLockID;
            const sint32 Count = ZayWidgetDOM::GetValue(Header + ".count").ToInteger();
            const sint32 Focus = ZayWidgetDOM::GetValue(Header + ".focus").ToInteger();
            if(Focus < Count)
            {
                const String CurHeader = Header + String::Format(".%d", Focus);
                const String ItemPath = ZayWidgetDOM::GetValue(CurHeader + ".itempath").ToText();
                // 워킹 데이터준비
                if(mWorkingData.Count() == 0)
                {
                    const String Path = ZayWidgetDOM::GetValue(CurHeader + ".localpath").ToText() + "/" + ItemPath;
                    if(auto OneFile = Platform::File::OpenForRead(Path))
                    {
                        const sint32 FileSize = Platform::File::Size(OneFile);
                        ZayWidgetDOM::SetValue(CurHeader + ".total", String::FromInteger(FileSize));
                        ZayWidgetDOM::SetValue(CurHeader + ".offset", "0");
                        if(FileSize == 0)
                        {
                            Platform::File::Close(OneFile);
                            ZayWidgetDOM::SetValue(Header + ".focus", String::FromInteger(Focus + 1));
                            mWorkingData.Clear();
                            return true;
                        }
                        Platform::File::Read(OneFile, mWorkingData.AtDumpingAdded(FileSize), FileSize);
                        Platform::File::Close(OneFile);
                        // 체크섬
                        const String NewMD5 = AddOn::Ssl::ToMD5(&mWorkingData[0], FileSize);
                        ZayWidgetDOM::SetValue(CurHeader + ".checksum",
                            String::Format("'size/%d/md5/%s'", FileSize, (chars) NewMD5));
                    }
                }
                // 분할송신
                if(0 < mWorkingData.Count())
                {
                    const String Path = ZayWidgetDOM::GetValue(CurHeader + ".serverpath").ToText() + "/" + ItemPath;
                    const sint32 Total = ZayWidgetDOM::GetValue(CurHeader + ".total").ToInteger();
                    const sint32 Offset = ZayWidgetDOM::GetValue(CurHeader + ".offset").ToInteger();
                    const sint32 Size = Math::Min(4096, Total - Offset);
                    const String Base64 = AddOn::Ssl::ToBASE64(&mWorkingData[Offset], Size);
                    const bool IsLastData = (Offset + Size == Total);
                    Context Json;
                    Json.At("type").Set((IsLastData)? "FileUploaded" : "FileUploading");
                    Json.At("token").Set(mToken);
                    Json.At("lockid").Set(CurLockID);
                    Json.At("path").Set(Path);
                    Json.At("total").Set(String::FromInteger(Total));
                    Json.At("offset").Set(String::FromInteger(Offset));
                    Json.At("base64").Set(Base64);
                    Send(Json);
                    ZayWidgetDOM::SetValue(CurHeader + ".offset", String::FromInteger(Offset + Size));
                    if(IsLastData)
                    {
                        ZayWidgetDOM::SetValue(Header + ".focus", String::FromInteger(Focus + 1));
                        mWorkingData.Clear();
                    }
                }
            }
            else
            {
                Context Data;
                for(sint32 i = 0; i < Count; ++i)
                {
                    const String CurHeader = Header + String::Format(".%d", i);
                    const String ItemPath = ZayWidgetDOM::GetValue(CurHeader + ".itempath").ToText();
                    const String CheckSum = ZayWidgetDOM::GetValue(CurHeader + ".checksum").ToText();
                    Data.At(ItemPath).Set(CheckSum);
                }
                SendUnlockAsset(CurLockID, Data);
                ZayWidgetDOM::RemoveVariables(Header + ".");
                mWorkingLockIDs.SubtractionSection(LockIDIndex);
                mWorkingData.Clear();
            }
        }
        return true;
    }
    return false;
}

void SandWichClient::Reconnect()
{
    mConnected = false;
    Platform::Socket::Close(mSocket);
    mSocket = Platform::Socket::OpenForWS(false);
    Platform::Socket::ConnectAsync(mSocket, mHost, mPort);
}

void SandWichClient::Send(const Context& json)
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

void SandWichClient::SendFastLogin(chars programid)
{
    Context Json;
    Json.At("type").Set("Login");
    Json.At("programid").Set(programid);
    Json.At("deviceid").Set(Platform::Utility::GetDeviceID());
    Send(Json);
}

void SandWichClient::SendLogin(chars programid, chars author, chars password)
{
    Context Json;
    Json.At("type").Set("LoginUpdate");
    Json.At("programid").Set(programid);
    Json.At("deviceid").Set(Platform::Utility::GetDeviceID());
    Json.At("author").Set(author);
    Json.At("password").Set(password);
    Send(Json);
}

void SandWichClient::SendLogout()
{
    Context Json;
    Json.At("type").Set("Logout");
    Json.At("token").Set(mToken);
    Send(Json);

    mAuthor.Empty();
    mToken.Empty();
    Platform::BroadcastNotify("InitSandWich", sint32o(1));
}

void SandWichClient::SendLockAsset(chars lockid, chars route)
{
    Context Json;
    Json.At("type").Set("LockAsset");
    Json.At("token").Set(mToken);
    Json.At("lockid").Set(lockid);
    Json.At("route").Set(route);
    Send(Json);
}

void SandWichClient::SendUnlockAsset(chars lockid, const Context& data)
{
    Context Json;
    Json.At("type").Set("UnlockAsset");
    Json.At("token").Set(mToken);
    Json.At("lockid").Set(lockid);
    Json.At("data").LoadJson(SO_NeedCopy, data.SaveJson());
    Send(Json);
}

void SandWichClient::SendFocusAsset(chars route)
{
    Context Json;
    Json.At("type").Set("FocusAsset");
    Json.At("token").Set(mToken);
    Json.At("route").Set(route);
    Send(Json);
}

void SandWichClient::SendUnfocusAsset(chars route)
{
    Context Json;
    Json.At("type").Set("UnfocusAsset");
    Json.At("token").Set(mToken);
    Json.At("route").Set(route);
    Send(Json);
}

void SandWichClient::SendFocusRange(chars route)
{
    Context Json;
    Json.At("type").Set("FocusRange");
    Json.At("token").Set(mToken);
    Json.At("route").Set(route);
    Send(Json);
}

void SandWichClient::SendUnfocusRange(chars route)
{
    Context Json;
    Json.At("type").Set("UnfocusRange");
    Json.At("token").Set(mToken);
    Json.At("route").Set(route);
    Send(Json);
}

void SandWichClient::OnLogined(const Context& json)
{
    mAuthor = json("author").GetText();
    mToken = json("token").GetText();
    ZayWidgetDOM::SetValue("sandwich.showlogin", (mAuthor == "-")? "1" : "0");
    ZayWidgetDOM::SetValue("sandwich.author", "'" + mAuthor + "'");
    ZayWidgetDOM::SetValue("sandwich.token", "'" + mToken + "'");
    ZayWidgetDOM::RemoveVariables("sandwich.login.");
    SendFocusRange("post");
}

void SandWichClient::OnLogouted(const Context& json)
{
    if(0 < mProgramID.Length())
        SendFastLogin(mProgramID);
}

void SandWichClient::OnAssetLocked(const Context& json)
{
    const String LockID = json("lockid").GetText();
    if(LockID == "NewPost")
    {
        const String PostType = ZayWidgetDOM::GetValue("sandwich.select.posttype").ToText();
        Context Data;
        Data.At("type").Set(PostType);
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
    else if(!String::Compare(LockID, strpair("NewUpload_")))
    {
        mWorkingLockIDs.AtAdding() = LockID;
    }
}

void SandWichClient::OnAssetUpdated(const Context& json)
{
    const String Author = json("author").GetText();
    const String Path = String(json("path").GetText()).Replace('/', '.');
    const String Status = json("status").GetText();
    const String Version = json("version").GetText();

    const String Header = "sandwich.asset." + Path;
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

void SandWichClient::OnAssetChanged(const Context& json)
{
    const String Path = String(json("path").GetText()).Replace('/', '.');
    const String Status = json("status").GetText();

    const String Header = "sandwich.asset." + Path;
    ZayWidgetDOM::SetValue(Header + ".status", "'" + Status + "'");
}

void SandWichClient::OnRangeUpdated(const Context& json)
{
    const String Path = String(json("path").GetText()).Replace('/', '.');
    const sint32 First = json("first").GetInt();
    const sint32 Last = json("last").GetInt();

    const String Header = "sandwich.range." + Path;
    const sint32 Count = Math::Max(0, Last + 1 - First);
    ZayWidgetDOM::SetValue(Header + ".count", String::FromInteger(Count));

    bool NeedFocusing = false;
    const Strings Pathes = String::Split(Path, '.');
    if(Pathes[-1] == "post") NeedFocusing = true;
    else if(Pathes[-1] == "sentence" && ZayWidgetDOM::GetValue("sandwich.select.post").ToText() != "-1")
        NeedFocusing = true;
    else if(Pathes[-1] == "ripple" && ZayWidgetDOM::GetValue("sandwich.select.sentence").ToText() != "-1")
        NeedFocusing = true;

    // 어셋포커싱
    if(NeedFocusing)
    for(sint32 i = 0; i < Count; ++i)
    {
        const String CurRoute = String::Format("%s.%d", (chars) Path, First + i);
        SendFocusAsset(CurRoute);
    }
}

void SandWichClient::OnErrored(const Context& json)
{
    const String Packet = json("packet").GetText();
    const String Text = json("text").GetText();

    if(Text == "Expired token")
    {
        ZayWidgetDOM::SetValue("sandwich.showlogin", "1");
        ZayWidgetDOM::SetValue("sandwich.login.error", "'" + Text + "'");
    }
    else if(Packet == "LoginUpdate")
        ZayWidgetDOM::SetValue("sandwich.login.error", "'" + Text + "'");
    else BOSS_TRACE("OnErrored[%s] \"%s\"", (chars) Packet, (chars) Text);
}
