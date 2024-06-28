#include <boss.hpp>
#include "sandwichpython.hpp"

#include <resource.hpp>

bool SandWichPython::TickOnce()
{
    // 통신 연결확인
    if(!mPythonConnected && Platform::Socket::IsConnected(mPython))
    {
        if(mPostIndex != -1)
            ZayWidgetDOM::SetValue(String::Format("sandwich.asset.post.%d.python.connected", mPostIndex), "1");
        mPythonConnected = true;
        mWidgetUpdater = true;
        PythonSend(String::Format("init,%d", 0));
    }
    // 지연된 송신처리
    if(mPythonConnected)
    {
        for(sint32 i = 0, iend = mPythonTasks.Count(); i < iend; ++i)
            Platform::Socket::Send(mPython, (bytes)(chars) mPythonTasks[i],
                mPythonTasks[i].Length() + 1, 3000, true);
        mPythonTasks.Clear();
    }

    bool NeedUpdate = mWidgetUpdater;
    NeedUpdate |= (mWidget && mWidget->TickOnce());
    mWidgetUpdater = false;
    return NeedUpdate;
}

void SandWichPython::PythonConnect(chars host, sint32 port, sint32 postidx)
{
    mPythonHost = host;
    mPythonPort = port;
    mPostIndex = postidx;
    Reconnect();
}

void SandWichPython::TryPythonRecvOnce()
{
    while(0 < Platform::Socket::RecvAvailable(mPython))
    {
        uint08 RecvTemp[4096];
        const sint32 RecvSize = Platform::Socket::Recv(mPython, RecvTemp, 4096);
        if(0 < RecvSize)
            Memory::Copy(mPythonQueue.AtDumpingAdded(RecvSize), RecvTemp, RecvSize);
        else if(RecvSize < 0)
            return;

        sint32 QueueEndPos = 0;
        for(sint32 iend = mPythonQueue.Count(), i = iend - RecvSize; i < iend; ++i)
        {
            if(mPythonQueue[i] == '#')
            {
                // 스트링 읽기
                const String Packet((chars) &mPythonQueue[QueueEndPos], i - QueueEndPos);
                QueueEndPos = i + 1;
                if(0 < Packet.Length())
                {
                    const Strings Params = String::Split(Packet);
                    if(0 < Params.Count())
                    {
                        const String Type = Params[0];
                        branch;
                        jump(!Type.Compare("log")) OnPython_log(Params);
                        jump(!Type.Compare("set")) OnPython_set(Params);
                        jump(!Type.Compare("get")) OnPython_get(Params);
                        jump(!Type.Compare("call")) OnPython_call(Params);
                    }
                }
                mWidgetUpdater = true;
            }
        }
        if(0 < QueueEndPos)
            mPythonQueue.SubtractionSection(0, QueueEndPos);
    }
}

void SandWichPython::KeyPress(sint32 keycode)
{
    const String KeyText = FindPythonKey(keycode);
    if(0 < KeyText.Length())
        PythonSend(String::Format("call,KeyPress_%s", (chars) KeyText));
}

void SandWichPython::KeyRelease(sint32 keycode)
{
    const String KeyText = FindPythonKey(keycode);
    if(0 < KeyText.Length())
        PythonSend(String::Format("call,KeyRelease_%s", (chars) KeyText));
}

void SandWichPython::Destroy()
{
    delete mWidget;
    mWidget = nullptr;
}

bool SandWichPython::RenderWidget(ZayPanel& panel, sint32 postidx)
{
    mPostIndex = postidx;
    auto CurWidget = ValidWidget();
    return (CurWidget && CurWidget->Render(panel));
}

void SandWichPython::RenderLogs(ZayPanel& panel)
{
    for(sint32 i = 0, iend = mLogs.Count(); i < iend; ++i)
    {
        const sint32 Index = iend - 1 - i;
        const String& CurText = mLogs[Index];
        const sint32 PosY = 5 + 25 * i;
        if(panel.h() <= PosY)
        {
            mLogs.SubtractionSection(0, Index + 1);
            break;
        }

        const String& UIName = String::Format("ui_sys_log%d", i);
        ZAY_FONT(panel, 1.5, "arial")
        {
            const sint32 TextWidth = Platform::Graphics::GetStringWidth(CurText);
            ZAY_XYWH_UI(panel, 5, PosY, TextWidth + 10, 25, UIName,
                ZAY_GESTURE_T(t, this, i)
                {
                    if(t == GT_Pressed)
                    {
                        const sint32 Index = mLogs.Count() - 1 - i;
                        if(0 <= Index && Index < mLogs.Count())
                        {
                            const String& CurText = mLogs[Index];
                            Platform::Utility::SendToTextClipboard(CurText);
                            mLogs.SubtractionSection(Index);
                        }
                    }
                })
            {
                ZAY_RGBA(panel, 0, 224, 255, 224)
                ZAY_RGBA_IF(panel, 255, 128, 0, 128, panel.state(UIName) & PS_Focused)
                    panel.fill();
                ZAY_RGB(panel, 0, 0, 0)
                    panel.text(5, panel.h() / 2, CurText, UIFA_LeftMiddle);
            }
        }
    }
}

void SandWichPython::OnPython_log(const Strings& params)
{
    if(1 < params.Count())
    {
        const sint32 ColonPos = params[1].Find(0, ":");
        if(ColonPos != -1)
        {
            const String HeadText = params[1].Left(ColonPos + 1);
            for(sint32 i = 0, iend = mLogs.Count(); i < iend; ++i)
                if(!String::Compare(mLogs[i], HeadText, HeadText.Length()))
                {
                    mLogs.At(i) = params[1];
                    mWidgetUpdater = true;
                    return;
                }
        }
        mLogs.AtAdding() = params[1];
        mWidgetUpdater = true;
    }
}

void SandWichPython::OnPython_set(const Strings& params)
{
    if(2 < params.Count())
    {
        UpdateDom(params[1], params[2]);
        mWidgetUpdater = true;
    }
}

void SandWichPython::OnPython_get(const Strings& params)
{
    if(1 < params.Count())
    {
        const chars Key = (chars) params[1];
        if(Key[0] == 'd' && Key[1] == '.')
        {
            const String Value = ZayWidgetDOM::GetValue(&Key[2]).ToText();
            PythonSend(String::Format("put,%s", (chars) Value));
        }
    }
}

void SandWichPython::OnPython_call(const Strings& params)
{
    if(1 < params.Count())
    {
        if(mWidget)
            mWidget->JumpCall(params[1]);
    }
}

void SandWichPython::PythonSend(const String& comma_params)
{
    if(mPythonConnected)
    {
        const sint32 Result = Platform::Socket::Send(mPython, (bytes)(chars) comma_params,
            comma_params.Length(), 3000, true);
        if(Result < 0)
            Reconnect();
    }
    if(!mPythonConnected)
        mPythonTasks.AtAdding() = comma_params;
}

void SandWichPython::Reconnect()
{
    if(mPostIndex != -1)
        ZayWidgetDOM::SetValue(String::Format("sandwich.asset.post.%d.python.connected", mPostIndex), "0");
    mPythonConnected = false;
    Platform::Socket::Close(mPython);
    mPython = Platform::Socket::OpenForWS(false);
    Platform::Socket::ConnectAsync(mPython, mPythonHost, mPythonPort);
}

void SandWichPython::UpdateDom(chars key, chars value)
{
    if(key[0] == 'd' && key[1] == '.')
        ZayWidgetDOM::SetValue(String::Format("dom_%d.%s", mPostIndex, key + 2), value);
}

void SandWichPython::PlaySound(chars filename)
{
    Platform::Sound::Close(mSounds[mSoundFocus]);
    mSounds[mSoundFocus] = Platform::Sound::OpenForFile(Platform::File::RootForAssets() + filename);
    Platform::Sound::SetVolume(mSounds[mSoundFocus], 1);
    Platform::Sound::Play(mSounds[mSoundFocus]);
    mSoundFocus = (mSoundFocus + 1) % 4;
}

void SandWichPython::StopSound()
{
    for(sint32 i = 0; i < 4; ++i)
    {
        Platform::Sound::Close(mSounds[i]);
        mSounds[i] = nullptr;
    }
    mSoundFocus = 0;
}

void SandWichPython::CallGate(chars gatename)
{
    if(mWidget)
        mWidget->JumpCall(gatename);
}

String SandWichPython::FindPythonKey(sint32 keycode)
{
    String KeyText;
    switch(keycode)
    {
    case 8: KeyText = "Backspace"; break;
    case 13: KeyText = "Enter"; break;
    case 16: KeyText = "Shift"; break;
    case 17: KeyText = "Ctrl"; break;
    case 18: KeyText = "Alt"; break;
    case 21: KeyText = "Hangul"; break;
    case 25: KeyText = "Hanja"; break;
    case 32: KeyText = "Space"; break;
    case 37: KeyText = "Left"; break;
    case 38: KeyText = "Up"; break;
    case 39: KeyText = "Right"; break;
    case 40: KeyText = "Down"; break;
    case 48: case 49: case 50: case 51: case 52: case 53: case 54: case 55: case 56: case 57:
        KeyText = '0' + (keycode - 48); break;
    case 65: case 66: case 67: case 68: case 69: case 70: case 71: case 72: case 73: case 74:
    case 75: case 76: case 77: case 78: case 79: case 80: case 81: case 82: case 83: case 84:
    case 85: case 86: case 87: case 88: case 89: case 90:
        KeyText = 'A' + (keycode - 65); break;
    case 112: case 113: case 114: case 115: case 116: case 117:
    case 118: case 119: case 120: case 121: case 122: case 123:
        KeyText = String::Format("F%d", 1 + (keycode - 112)); break;
    }
    return KeyText;
}

ZayWidget* SandWichPython::ValidWidget()
{
    if(mPostIndex == -1 || mWidget)
        return mWidget;
    const String ShowJson = String::FromAsset(String::Format("python_%d/data/widget/zayshow.json", mPostIndex));
    const Context Show(ST_Json, SO_OnlyReference, ShowJson);
    const String Title = Show("title").GetText();
    if(0 < Title.Length())
    {
        mWidget = new ZayWidget();
        mWidget->Init(String::Format("python_%d", mPostIndex), nullptr,
            [](chars name)->const Image* {return &((const Image&) R(name));},
            String::Format("dom_%d", mPostIndex))

            // 특정시간동안 지속적인 화면업데이트
            .AddGlue("update", ZAY_DECLARE_GLUE(params, this)
            {
                if(params.ParamCount() == 1)
                {
                    const sint32o Msec(sint32(params.Param(0).ToFloat() * 1000));
                    Platform::BroadcastNotify("ZS_Update", Msec, NT_Normal, nullptr, true);
                }
            })

            // 로그출력
            .AddGlue("log", ZAY_DECLARE_GLUE(params, this)
            {
                if(params.ParamCount() == 1)
                {
                    const String Message = params.Param(0).ToText();
                    Platform::BroadcastNotify("ZS_Log", Message);
                }
            })

            // 에디터박스 포커싱제거
            .AddGlue("clear_capture", ZAY_DECLARE_GLUE(params, this)
            {
                Platform::BroadcastNotify("ZS_ClearCapture", nullptr);
            })

            // 파이썬실행
            .AddGlue("python", ZAY_DECLARE_GLUE(params, this)
            {
                if(params.ParamCount() == 1)
                {
                    const String Func = params.Param(0).ToText();
                    PythonSend(String::Format("call,%s", (chars) Func));
                }
            })

            // DOM변경
            .AddGlue("update_dom", ZAY_DECLARE_GLUE(params, this)
            {
                if(1 < params.ParamCount())
                {
                    const String Key = params.Param(0).ToText();
                    const String Value = params.Param(1).ToText();
                    UpdateDom(Key, Value);
                    mWidgetUpdater = true;
                }
            })

            // 사운드재생
            .AddGlue("play_sound", ZAY_DECLARE_GLUE(params, this)
            {
                if(0 < params.ParamCount())
                {
                    const String FileName = params.Param(0).ToText();
                    PlaySound(FileName);
                    mWidgetUpdater = true;
                }
            })

            // 사운드중지
            .AddGlue("stop_sound", ZAY_DECLARE_GLUE(params, this)
            {
                StopSound();
                mWidgetUpdater = true;
            })

            // 게이트호출
            .AddGlue("call_gate", ZAY_DECLARE_GLUE(params, this)
            {
                if(params.ParamCount() == 1)
                {
                    const String GateName = params.Param(0).ToText();
                    CallGate(GateName);
                    mWidgetUpdater = true;
                }
            });
        mWidget->Reload(String::Format("python_%d/data/widget/%s.zay", mPostIndex, (chars) Title));
    }
    return mWidget;
}
