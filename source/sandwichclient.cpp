#include <boss.hpp>
#include "sandwichclient.hpp"

#include <sandwichutil.hpp>
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
        {
            Path = String::Format("post.%d.sentence", index);
            const String Type = ZayWidgetDOM::GetValue(String::Format("sandwich.asset.post.%d.data.type", index)).ToText();
            if(Type == "python")
                SendFocusAsset(String::Format("post.%d.python", index));
        }
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

void SandWichClient::PythonExecute(sint32 postindex)
{
    const String Header = String::Format("sandwich.asset.post.%d.python.show.python", postindex);
    const String PythonPath = ZayWidgetDOM::GetValue(Header).ToText();
    SendPythonExecute(String::Format("run_%d", postindex), String::Format("post/%d/python/", postindex) + PythonPath);
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
                    jump(Type == "FileDownloading") OnFileDownloading(RecvJson);
                    jump(Type == "FileDownloaded") OnFileDownloaded(RecvJson);
                    jump(Type == "ExecutedPython") OnExecutedPython(RecvJson);
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
            const String LocalPath = ZayWidgetDOM::GetValue(Header + ".localpath").ToText();
            const String ServerPath = ZayWidgetDOM::GetValue(Header + ".serverpath").ToText();
            if(Focus < Count)
            {
                const String CurHeader = Header + String::Format(".%d", Focus);
                const String ItemPath = ZayWidgetDOM::GetValue(CurHeader + ".itempath").ToText();
                // 워킹 데이터준비
                if(mWorkingData.Count() == 0)
                {
                    if(auto OneFile = Platform::File::OpenForRead(LocalPath + "/" + ItemPath))
                    {
                        bool Success = false;
                        const sint32 FileSize = Platform::File::Size(OneFile);
                        ZayWidgetDOM::SetValue(CurHeader + ".total", String::FromInteger(FileSize));
                        ZayWidgetDOM::SetValue(CurHeader + ".offset", "0");
                        if(0 < FileSize)
                        {
                            Platform::File::Read(OneFile, mWorkingData.AtDumpingAdded(FileSize), FileSize);
                            Platform::File::Close(OneFile);
                            // 체크섬
                            const String NewCRC64 = SandWichUtil::ToCRC64(&mWorkingData[0], FileSize);
                            const String NewCheckSum = String::Format("size/%d/crc64/%s", FileSize, (chars) NewCRC64);
                            const String CurPost = "sandwich.asset.post." + CurLockID.Offset(strsize("NewUpload_"));
                            const String OldCheckSum = ZayWidgetDOM::GetValue(
                                String::Format("%s.python.checksum.%s", (chars) CurPost, (chars) ItemPath)).ToText();
                            ZayWidgetDOM::SetValue(CurHeader + ".checksum", "'" + NewCheckSum + "'");
                            if(NewCheckSum != OldCheckSum)
                                Success = true;
                        }
                        else Platform::File::Close(OneFile);
                        // 파일사이즈가 0이거나 동일한 파일은 스킵
                        if(!Success)
                        {
                            ZayWidgetDOM::SetValue(Header + ".focus", String::FromInteger(Focus + 1));
                            mWorkingData.Clear();
                        }
                    }
                }
                // 분할송신
                if(0 < mWorkingData.Count())
                {
                    const sint32 Total = ZayWidgetDOM::GetValue(CurHeader + ".total").ToInteger();
                    const sint32 Offset = ZayWidgetDOM::GetValue(CurHeader + ".offset").ToInteger();
                    const sint32 Size = Math::Min(4096, Total - Offset);
                    const String& Base64 = SandWichUtil::ToBASE64(&mWorkingData[Offset], Size);
                    const bool IsLastData = (Offset + Size == Total);
                    Context Json;
                    Json.At("type").Set((IsLastData)? "FileUploaded" : "FileUploading");
                    Json.At("token").Set(mToken);
                    Json.At("lockid").Set(CurLockID);
                    Json.At("path").Set(ServerPath + "/" + ItemPath);
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
                Data.At("asset").Set("python_" + CurLockID.Offset(strsize("NewUpload_")));
                const String ShowJson = String::FromFile(LocalPath + "/widget/zayshow.json");
                if(0 < ShowJson.Length())
                    Data.At("show").LoadJson(SO_NeedCopy, ShowJson);
                for(sint32 i = 0; i < Count; ++i)
                {
                    const String CurHeader = Header + String::Format(".%d", i);
                    const String ItemPath = ZayWidgetDOM::GetValue(CurHeader + ".itempath").ToText();
                    const String CheckSum = ZayWidgetDOM::GetValue(CurHeader + ".checksum").ToText();
                    auto& NewData = Data.At("files").AtAdding();
                    NewData.At("path").Set(ItemPath);
                    NewData.At("checksum").Set(CheckSum);
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

String SandWichClient::GetCheckSum(chars asset, chars path)
{
    uint64 DataSize = 0, DataTime = 0;
    const String DataPath = String::Format("%s/data/%s", asset, path);
    if(Asset::Exist(DataPath, nullptr, &DataSize, nullptr, nullptr, &DataTime))
    {
        if(DataSize == 0)
            return "size/0/crc64/0000000000000000";

        uint64 CheckSize = 0, CheckTime = 0;
        const String CheckPath = String::Format("%s/checksum/%s.checksum", asset, path);
        if(Asset::Exist(CheckPath, nullptr, &CheckSize, nullptr, nullptr, &CheckTime))
        {
            if(DataTime < CheckTime)
                return String::FromAsset(CheckPath);
        }

        // CheckSum생성
        if(auto DataAsset = Asset::OpenForRead(DataPath))
        {
            uint08s DataBinary;
            Asset::Read(DataAsset, DataBinary.AtDumpingAdded(DataSize), DataSize);
            Asset::Close(DataAsset);
            const String NewCRC64 = SandWichUtil::ToCRC64(&DataBinary[0], DataSize);
            const String NewCheckSum = String::Format("size/%d/crc64/%s", (sint32) DataSize, (chars) NewCRC64);
            NewCheckSum.ToAsset(CheckPath, true);
            return NewCheckSum;
        }
    }
    return String();
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

void SandWichClient::SendDownloadFile(chars memo, chars path)
{
    Context Json;
    Json.At("type").Set("DownloadFile");
    Json.At("token").Set(mToken);
    Json.At("memo").Set(memo);
    Json.At("path").Set(path);
    Json.At("offset").Set("0");
    Json.At("size").Set("-1");
    Send(Json);
}

void SandWichClient::SendPythonExecute(chars runid, chars path)
{
    Context Json;
    Json.At("type").Set("PythonExecute");
    Json.At("token").Set(mToken);
    Json.At("runid").Set(runid);
    Json.At("path").Set(path);
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
    const String ServerPath = json("path").GetText();
    const String DomPath = String(ServerPath).Replace('/', '.');
    const String Status = json("status").GetText();
    const String Version = json("version").GetText();

    const String Header = "sandwich.asset." + DomPath;
    ZayWidgetDOM::SetValue(Header + ".author", "'" + Author + "'");
    ZayWidgetDOM::SetValue(Header + ".status", "'" + Status + "'");
    ZayWidgetDOM::SetValue(Header + ".version", "'" + Version + "'");

    // 어셋데이터
    const String AssetName = json("data")("asset").GetText();
    if(0 < AssetName.Length())
    {
        ZayWidgetDOM::SetValue(Header + ".asset", "'" + AssetName + "'");
        ZayWidgetDOM::SetJson(json("data")("show"), Header + ".show.");
        sint32 DownloadCount = 0;
        for(sint32 i = 0, iend = json("data")("files").LengthOfIndexable(); i < iend; ++i)
        {
            const String ItemPath = json("data")("files")[i]("path").GetText();
            const String NewCheckSum = json("data")("files")[i]("checksum").GetText();
            const String OldCheckSum = GetCheckSum(AssetName, ItemPath);
            if(NewCheckSum != OldCheckSum)
            {
                SendDownloadFile(AssetName + "/data/" + ItemPath, ServerPath + '/' + ItemPath);
                DownloadCount++;
            }
            ZayWidgetDOM::SetValue(Header + ".checksum." + ItemPath, "'" + NewCheckSum + "'");
        }
        if(0 < DownloadCount)
        {
            ZayWidgetDOM::SetValue(Header + ".download.focus", "0");
            ZayWidgetDOM::SetValue(Header + ".download.count", String::FromInteger(DownloadCount));
        }
    }
    // 일반데이터
    else ZayWidgetDOM::SetJson(json("data"), Header + ".data.");

    const Strings Pathes = String::Split(DomPath, '.');
    if(0 < Pathes.Count())
    {
        const String PathType = Pathes[-2];
        if(PathType == "post")
            SendFocusRange(DomPath + ".sentence");
        else if(PathType == "sentence")
            SendFocusRange(DomPath + ".ripple");
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

void SandWichClient::OnFileDownloading(const Context& json)
{
    const String Memo = json("memo").GetText();
    const sint32 Total = json("total").GetInt();
    const sint32 Offset = json("offset").GetInt();
    const String& Base64 = json("base64").GetText();
    if(buffer NewData = SandWichUtil::FromBASE64(Base64, Base64.Length()))
    {
        const sint32 DataSize = Buffer::CountOf(NewData);
        // 메모리에 쓰기
        if(0 < DataSize)
        {
            auto& CurData = mDownloadingData(Memo);
            Memory::Copy(CurData.AtDumping(Offset, DataSize), NewData, DataSize);
        }
        Buffer::Free(NewData);
    }
}

void SandWichClient::OnFileDownloaded(const Context& json)
{
    OnFileDownloading(json);

    // 파일에 쓰기
    const String Memo = json("memo").GetText();
    auto& CurData = mDownloadingData(Memo);
    if(0 < CurData.Count())
    if(auto NewAsset = Asset::OpenForWrite(Memo, true))
    {
        Asset::Write(NewAsset, &CurData[0], CurData.Count());
        Asset::Close(NewAsset);
        // 프로그레스 업데이트
        if(!String::Compare(Memo, strpair("python_")))
        {
            const String PostInfo = Memo.Offset(strsize("python_"));
            const sint32 SlashPos = PostInfo.Find(0, "/");
            if(SlashPos != -1)
            {
                const sint32 PostIndex = Parser::GetInt(PostInfo.Left(SlashPos));
                const String Header = String::Format("sandwich.asset.post.%d.python.download.focus", PostIndex);
                const sint32 CurFocus = ZayWidgetDOM::GetValue(Header).ToInteger();
                ZayWidgetDOM::SetValue(Header, String::FromInteger(CurFocus + 1));
            }
        }
    }
    mDownloadingData.Remove(Memo);
}

void SandWichClient::OnExecutedPython(const Context& json)
{
    const String RunID = json("runid").GetText();
    const sint32 Port = json("port").GetInt();

    if(!String::Compare(RunID, strpair("run_")))
    {
        const sint32 PostIdx = Parser::GetInt(RunID.Offset(strsize("run_")));
        sint32s Args;
        Args.AtAdding() = PostIdx;
        Args.AtAdding() = Port;
        Platform::BroadcastNotify("ConnectPython", Args, NT_Normal, nullptr, true);
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
