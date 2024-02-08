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

void HueBoardClient::OnLogined(const Context& json)
{
    mAuthor = json("author").GetText();
    mToken = json("token").GetText();
    ZayWidgetDOM::SetValue("hueboard.showlogin", "0");
    ZayWidgetDOM::SetValue("hueboard.author", "'" + mAuthor + "'");
    ZayWidgetDOM::SetValue("hueboard.token", "'" + mToken + "'");
    ZayWidgetDOM::RemoveVariables("hueboard.login.");
}

void HueBoardClient::OnErrored(const Context& json)
{
    const String Packet = json("packet").GetText();
    const String Text = json("text").GetText();

    if(Packet == "Login")
    {
        if(Text == "Unregistered device")
            ZayWidgetDOM::SetValue("hueboard.showlogin", "1");
        else ZayWidgetDOM::SetValue("hueboard.login.error", "'" + Text + "'");
    }
    else if(Packet == "LoginUpdate")
        ZayWidgetDOM::SetValue("hueboard.login.error", "'" + Text + "'");
    else BOSS_TRACE("OnErrored[%s] \"%s\"", (chars) Packet, (chars) Text);
}
