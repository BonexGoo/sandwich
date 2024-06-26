#include <boss.hpp>
#include "sandwich.hpp"

#include <sandwichutil.hpp>
#include <resource.hpp>
#if BOSS_WASM
    #include <boss_assets.hpp>
#endif

#include <service/boss_zaywidget.hpp>
#include <format/boss_bmp.hpp>
#include <format/boss_flv.hpp>

ZAY_DECLARE_VIEW_CLASS("sandwichView", sandwichData)

extern sint32 gProgramWidth;
extern sint32 gProgramHeight;
extern String gBoardName;
extern String gServerHost;
const sint32 gServerPort = 8981;

ZAY_VIEW_API OnCommand(CommandType type, id_share in, id_cloned_share* out)
{
    if(type == CT_Size)
    {
        sint32s WH(in);
        gProgramWidth = WH[0];
        #if BOSS_WINDOWS
            gProgramHeight = WH[1] - 1;
        #else
            gProgramHeight = WH[1];
        #endif
    }
    else if(type == CT_Tick)
    {
        const uint64 NowMsec = Platform::Utility::CurrentTimeMsec();

        if(m->mWidget && m->mWidget->TickOnce())
            m->invalidate();
        if(m->mClient && m->mClient->TickOnce())
            m->invalidate();
        if(m->mClient && m->mClient->TryWorkingOnce())
            m->invalidate();
        for(sint32 i = 0, iend = m->mPythons.Count(); i < iend; ++i)
            if(auto CurPython = m->mPythons.AccessByOrder(i))
            if(CurPython->TickOnce())
                m->invalidate();
        if(NowMsec <= m->mWidgetUpdater)
            m->invalidate(2);
    }
    else if(type == CT_Activate && !boolo(in).ConstValue())
        m->clearCapture();
}

ZAY_VIEW_API OnNotify(NotifyType type, chars topic, id_share in, id_cloned_share* out)
{
    if(type == NT_Normal)
    {
        if(!String::Compare(topic, "ZS_Update"))
        {
            const uint64 NewUpdateMsec = Platform::Utility::CurrentTimeMsec() + sint32o(in).ConstValue();
            if(m->mWidgetUpdater < NewUpdateMsec)
                m->mWidgetUpdater = NewUpdateMsec;
        }
        else if(!String::Compare(topic, "ZS_Log"))
        {
            const String Message(in);
            m->mWidget->SendLog(Message);
        }
        else if(!String::Compare(topic, "ZS_ClearCapture"))
        {
            m->clearCapture();
        }
        else if(!String::Compare(topic, "InitSandWich"))
        {
            const sint32 ShowLogin = sint32o(in).ConstValue();
            m->InitBoard();
            ZayWidgetDOM::SetValue("sandwich.showlogin", String::FromInteger(ShowLogin));
            m->invalidate();
        }
        else if(!String::Compare(topic, "Reconnect"))
        {
            gBoardName = ZayWidgetDOM::GetComment("program.boardname");
            gServerHost = ZayWidgetDOM::GetComment("program.serverhost");
            m->RemoveSet();
            m->CreateSet();
        }
        else if(!String::Compare(topic, "ConnectPython"))
        {
            sint32s Args(in);
            const sint32 PostIdx = Args[0];
            const sint32 Port = Args[1];
            m->mPythons[PostIdx].PythonConnect(gServerHost, Port);
        }
    }
    else if(type == NT_KeyPress)
    {
        const sint32 KeyCode = sint32o(in).ConstValue();
        const sint32 PostIdx = ZayWidgetDOM::GetValue("sandwich.select.post").ToInteger();
        // 파이썬에 키전달
        if(PostIdx != -1)
        if(auto CurPython = m->mPythons.Access(PostIdx))
            CurPython->KeyPress(KeyCode);
    }
    else if(type == NT_KeyRelease)
    {
        const sint32 KeyCode = sint32o(in).ConstValue();
        const sint32 PostIdx = ZayWidgetDOM::GetValue("sandwich.select.post").ToInteger();
        // 파이썬에 키전달
        if(PostIdx != -1)
        if(auto CurPython = m->mPythons.Access(PostIdx))
            CurPython->KeyRelease(KeyCode);
    }
    else if(type == NT_SocketReceive)
    {
        if(!String::Compare(topic, "message"))
        {
            if(m->mClient && m->mClient->TryRecvOnce())
                m->invalidate();
            for(sint32 i = 0, iend = m->mPythons.Count(); i < iend; ++i)
                if(auto CurPython = m->mPythons.AccessByOrder(i))
                    CurPython->TryPythonRecvOnce();
        }
    }
    else if(type == NT_ZayWidget)
    {
        if(!String::Compare(topic, "SetCursor"))
        {
            auto Cursor = CursorRole(sint32o(in).Value());
            m->SetCursor(Cursor);
        }
        else if(!String::Compare(topic, "EnterPressing"))
        {
            const String DomName(in);
            if(!DomName.Compare("editor"))
            {
                const String Text = ZayWidgetDOM::GetComment("editor");
                ZayWidgetDOM::SetComment("editor", "");
                m->setCapture("ui_editor");

                if(m->mClient)
                {
                    // 새 리플추가
                    if(ZayWidgetDOM::GetValue("sandwich.select.sentence").ToInteger() != -1)
                    {
                        if(0 < Text.Length())
                            m->mClient->NewRipple(Text);
                    }
                    // 새 문장추가
                    else if(ZayWidgetDOM::GetValue("sandwich.select.post").ToInteger() != -1)
                    {
                        if(0 < Text.Length())
                            m->mClient->NewSentence(Text);
                    }
                    // 새 포스트추가
                    else if(0 < Text.Length())
                        m->mClient->NewPost(Text);
                }
            }
        }
    }
    else if(type == NT_ZayAtlas)
    {
        if(!String::Compare(topic, "AtlasUpdated"))
        {
            const String AtlasJson = R::PrintUpdatedAtlas();
            if(m->mWidget)
                m->mWidget->UpdateAtlas(AtlasJson);
        }
    }
}

ZAY_VIEW_API OnGesture(GestureType type, sint32 x, sint32 y)
{
    static point64 OldCursorPos;
    static rect128 OldWindowRect;
    static uint64 ReleaseMsec = 0;

    if(type == GT_Moving || type == GT_MovingIdle)
        m->SetCursor(CR_Arrow);
    else if(type == GT_Pressed)
    {
        Platform::Utility::GetCursorPos(OldCursorPos);
        OldWindowRect = Platform::GetWindowRect();
        m->clearCapture();
    }
    else if(type == GT_InDragging || type == GT_OutDragging)
    {
        if(!m->IsFullScreen())
        {
            point64 CurCursorPos;
            Platform::Utility::GetCursorPos(CurCursorPos);
            Platform::SetWindowRect(
                OldWindowRect.l + CurCursorPos.x - OldCursorPos.x,
                OldWindowRect.t + CurCursorPos.y - OldCursorPos.y,
                OldWindowRect.r - OldWindowRect.l, OldWindowRect.b - OldWindowRect.t);
            m->mIsWindowMoving = true;
        }
        ReleaseMsec = 0;
        m->invalidate();
    }
    else if(type == GT_InReleased || type == GT_OutReleased || type == GT_CancelReleased)
    {
        const uint64 CurReleaseMsec = Platform::Utility::CurrentTimeMsec();
        const bool HasDoubleClick = (CurReleaseMsec < ReleaseMsec + 300);
        if(HasDoubleClick)
        {
            if(m->IsFullScreen())
                m->SetNormalWindow();
            else m->SetFullScreen();
        }
        else ReleaseMsec = CurReleaseMsec;
        m->mIsWindowMoving = false;
    }
    else if(type == GT_WheelUp || type == GT_WheelDown)
    {
        if(type == GT_WheelDown)
            m->mZoomPercent = Math::Min(m->mZoomPercent + 5, 300);
        else m->mZoomPercent = Math::Max(30, m->mZoomPercent - 5);
        ZayWidgetDOM::SetValue("sandwich.zoom", String::FromInteger(m->mZoomPercent));
        m->invalidate();
    }
}

ZAY_VIEW_API OnRender(ZayPanel& panel)
{
    ZAY_XYWH(panel, 0, 0, gProgramWidth, gProgramHeight)
    {
        if(m->mWidget)
            m->mWidget->Render(panel);
        else ZAY_RGBA(panel, 255, 0, 0, 64)
            panel.fill();

        // 윈도우시스템
        #if BOSS_WINDOWS
            m->RenderWindowSystem(panel);
        #endif

        // 프레임시간
        const uint64 CurRenderMsec = Platform::Utility::CurrentTimeMsec();
        ZayWidgetDOM::SetValue("program.frame", String::FromFloat(1000.0 / (CurRenderMsec - m->mRenderTester)));
        m->mRenderTester = CurRenderMsec;
    }
}

sandwichData::sandwichData()
{
    #if BOSS_WASM
        for(sint32 i = 0, iend = BOSS_EMBEDDED_ASSET_COUNT; i < iend; ++i)
        {
            const String CurPath = gSortedEmbeddedFiles[i].mPath;
            if(!String::CompareNoCase(CurPath, strpair("font/")))
            {
                if(buffer NewFont = Asset::ToBuffer(CurPath))
                {
                    auto CurFontName = Platform::Utility::CreateSystemFont((bytes) NewFont, Buffer::CountOf(NewFont));
                    Buffer::Free(NewFont);
                    ZayWidgetDOM::SetValue("fonts." + CurPath.Offset(5), "'" + CurFontName + "'");
                }
            }
        }
    #else
        Platform::File::Search(Platform::File::RootForAssets() + "font",
            [](chars itemname, payload data)->void
            {
                if(buffer NewFont = Asset::ToBuffer(String::Format("font/%s", itemname)))
                {
                    auto CurFontName = Platform::Utility::CreateSystemFont((bytes) NewFont, Buffer::CountOf(NewFont));
                    Buffer::Free(NewFont);
                    ZayWidgetDOM::SetValue(String::Format("fonts.%s", itemname), "'" + CurFontName + "'");
                }
            }, nullptr, false);
    #endif

    String DateText = __DATE__;
    String TimeText = __TIME__;
    DateText.Replace("Jan", "01"); DateText.Replace("Feb", "02"); DateText.Replace("Mar", "03");
    DateText.Replace("Apr", "04"); DateText.Replace("May", "05"); DateText.Replace("Jun", "06");
    DateText.Replace("Jul", "07"); DateText.Replace("Aug", "08"); DateText.Replace("Sep", "09");
    DateText.Replace("Oct", "10"); DateText.Replace("Nov", "11"); DateText.Replace("Dec", "12");
    const String Day = String::Format("%02d", Parser::GetInt(DateText.Middle(2, DateText.Length() - 6).Trim()));
    DateText = DateText.Right(4) + "/" + DateText.Left(2) + "/" + Day;
    ZayWidgetDOM::SetValue("program.build", "'" + DateText + "_" + TimeText.Left(2) + "H'");

    mRenderTester = Platform::Utility::CurrentTimeMsec();
    ZayWidgetDOM::SetValue("program.frame", "0");
    ZayWidgetDOM::SetValue("program.debug", "0");

    #if BOSS_WINDOWS
        ZayWidgetDOM::SetValue("program.os", "windows");
    #elif BOSS_WASM
        ZayWidgetDOM::SetValue("program.os", "wasm");
    #else
        ZayWidgetDOM::SetValue("program.os", "unknown");
    #endif
    ZayWidgetDOM::SetComment("program.boardname", gBoardName);
    ZayWidgetDOM::SetComment("program.serverhost", gServerHost);
    CreateSet();
}

sandwichData::~sandwichData()
{
    RemoveSet();
}

void sandwichData::SetCursor(CursorRole role)
{
    if(mNowCursor != role)
    {
        mNowCursor = role;
        Platform::Utility::SetCursor(role);
        if(mNowCursor != CR_SizeVer && mNowCursor != CR_SizeHor && mNowCursor != CR_SizeBDiag && mNowCursor != CR_SizeFDiag && mNowCursor != CR_SizeAll)
        {
            mNcLeftBorder = false;
            mNcTopBorder = false;
            mNcRightBorder = false;
            mNcBottomBorder = false;
        }
    }
}

sint32 sandwichData::MoveNcLeft(const rect128& rect, sint32 addx)
{
    const sint32 NewLeft = rect.l + addx;
    return rect.r - Math::Max(mMinWindowWidth, rect.r - NewLeft);
}

sint32 sandwichData::MoveNcTop(const rect128& rect, sint32 addy)
{
    const sint32 NewTop = rect.t + addy;
    return rect.b - Math::Max(mMinWindowHeight, rect.b - NewTop);
}

sint32 sandwichData::MoveNcRight(const rect128& rect, sint32 addx)
{
    const sint32 NewRight = rect.r + addx;
    return rect.l + Math::Max(mMinWindowWidth, NewRight - rect.l);
}

sint32 sandwichData::MoveNcBottom(const rect128& rect, sint32 addy)
{
    const sint32 NewBottom = rect.b + addy;
    return rect.t + Math::Max(mMinWindowHeight, NewBottom - rect.t);
}

void sandwichData::RenderWindowSystem(ZayPanel& panel)
{
    // 최소화버튼
    ZAY_XYWH_UI(panel, panel.w() - 8 - (24 + 18) * 3, 26, 24, 24, "ui_win_min",
        ZAY_GESTURE_T(t)
        {
            if(t == GT_InReleased)
                Platform::Utility::SetMinimize();
        })
    ZAY_ZOOM(panel, 0.8)
    {
        const bool Hovered = ((panel.state("ui_win_min") & (PS_Focused | PS_Dropping)) == PS_Focused);
        panel.icon(R((Hovered)? "btn_minimimize_h_" : "btn_minimimize_n"), UIA_CenterMiddle);
    }

    // 최대화버튼
    ZAY_XYWH_UI(panel, panel.w() - 8 - (24 + 18) * 2, 26, 24, 24, "ui_win_max",
        ZAY_GESTURE_T(t)
        {
            if(t == GT_InReleased)
            {
                if(m->IsFullScreen())
                    m->SetNormalWindow();
                else m->SetFullScreen();
            }
        })
    ZAY_ZOOM(panel, 0.8)
    {
        const bool Hovered = ((panel.state("ui_win_max") & (PS_Focused | PS_Dropping)) == PS_Focused);
        panel.icon(R((Hovered)? "btn_upsizing_h_p" : "btn_upsizing_n"), UIA_CenterMiddle);
    }

    // 종료버튼
    ZAY_XYWH_UI(panel, panel.w() - 8 - (24 + 18) * 1, 26, 24, 24, "ui_win_close",
        ZAY_GESTURE_T(t)
        {
            if(t == GT_InReleased)
                Platform::Utility::ExitProgram();
        })
    ZAY_ZOOM(panel, 0.8)
    {
        const bool Hovered = ((panel.state("ui_win_close") & (PS_Focused | PS_Dropping)) == PS_Focused);
        panel.icon(R((Hovered)? "btn_close_h_p" : "btn_close_n"), UIA_CenterMiddle);
    }

    // 아웃라인
    if(!IsFullScreen())
        RenderWindowOutline(panel);
}

void sandwichData::RenderWindowOutline(ZayPanel& panel)
{
    ZAY_INNER(panel, 1)
    ZAY_RGBA(panel, 0, 0, 0, 32)
        panel.rect(1);

    // 리사이징바
    ZAY_RGBA(panel, 0, 0, 0, 64 + 128 * Math::Abs(((frame() * 2) % 100) - 50) / 50)
    {
        if(mNcLeftBorder)
        {
            for(sint32 i = 0; i < 5; ++i)
            ZAY_RGBA(panel, 128, 128, 128, 128 - i * 25)
            ZAY_XYWH(panel, i, 0, 1, panel.h())
                panel.fill();
            invalidate(2);
        }
        if(mNcTopBorder)
        {
            for(sint32 i = 0; i < 5; ++i)
            ZAY_RGBA(panel, 128, 128, 128, 128 - i * 25)
            ZAY_XYWH(panel, 0, i, panel.w(), 1)
                panel.fill();
            invalidate(2);
        }
        if(mNcRightBorder)
        {
            for(sint32 i = 0; i < 5; ++i)
            ZAY_RGBA(panel, 128, 128, 128, 128 - i * 25)
            ZAY_XYWH(panel, panel.w() - 1 - i, 0, 1, panel.h())
                panel.fill();
            invalidate(2);
        }
        if(mNcBottomBorder)
        {
            for(sint32 i = 0; i < 5; ++i)
            ZAY_RGBA(panel, 128, 128, 128, 128 - i * 25)
            ZAY_XYWH(panel, 0, panel.h() - 1 - i, panel.w(), 1)
                panel.fill();
            invalidate(2);
        }
    }

    // 윈도우 리사이징 모듈
    #define RESIZING_MODULE(C, L, T, R, B, BORDER) do {\
        static point64 OldMousePos; \
        static rect128 OldWindowRect; \
        if(t == GT_Moving) \
        { \
            SetCursor(C); \
            mNcLeftBorder = false; \
            mNcTopBorder = false; \
            mNcRightBorder = false; \
            mNcBottomBorder = false; \
            BORDER = true; \
        } \
        else if(t == GT_MovingLosed) \
        { \
            SetCursor(CR_Arrow); \
        } \
        else if(t == GT_Pressed) \
        { \
            Platform::Utility::GetCursorPos(OldMousePos); \
            OldWindowRect = Platform::GetWindowRect(); \
        } \
        else if(t == GT_InDragging || t == GT_OutDragging || t == GT_InReleased || t == GT_OutReleased || t == GT_CancelReleased) \
        { \
            point64 NewMousePos; \
            Platform::Utility::GetCursorPos(NewMousePos); \
            const rect128 NewWindowRect = {L, T, R, B}; \
            Platform::SetWindowRect(NewWindowRect.l, NewWindowRect.t, \
                NewWindowRect.r - NewWindowRect.l, NewWindowRect.b - NewWindowRect.t); \
        }} while(false);

    // 윈도우 리사이징 꼭지점
    const sint32 SizeBorder = 10;
    ZAY_LTRB_UI(panel, 0, 0, SizeBorder, SizeBorder, "NcLeftTop",
        ZAY_GESTURE_T(t, this)
        {
            RESIZING_MODULE(CR_SizeFDiag,
                MoveNcLeft(OldWindowRect, NewMousePos.x - OldMousePos.x),
                MoveNcTop(OldWindowRect, NewMousePos.y - OldMousePos.y),
                OldWindowRect.r,
                OldWindowRect.b,
                mNcLeftBorder = mNcTopBorder);
        });
    ZAY_LTRB_UI(panel, panel.w() - SizeBorder, 0, panel.w(), SizeBorder, "NcRightTop",
        ZAY_GESTURE_T(t, this)
        {
            RESIZING_MODULE(CR_SizeBDiag,
                OldWindowRect.l,
                MoveNcTop(OldWindowRect, NewMousePos.y - OldMousePos.y),
                MoveNcRight(OldWindowRect, NewMousePos.x - OldMousePos.x),
                OldWindowRect.b,
                mNcRightBorder = mNcTopBorder);
        });
    ZAY_LTRB_UI(panel, 0, panel.h() - SizeBorder, SizeBorder, panel.h(), "NcLeftBottom",
        ZAY_GESTURE_T(t, this)
        {
            RESIZING_MODULE(CR_SizeBDiag,
                MoveNcLeft(OldWindowRect, NewMousePos.x - OldMousePos.x),
                OldWindowRect.t,
                OldWindowRect.r,
                MoveNcBottom(OldWindowRect, NewMousePos.y - OldMousePos.y),
                mNcLeftBorder = mNcBottomBorder);
        });
    ZAY_LTRB_UI(panel, panel.w() - SizeBorder, panel.h() - SizeBorder, panel.w(), panel.h(), "NcRightBottom",
        ZAY_GESTURE_T(t, this)
        {
            RESIZING_MODULE(CR_SizeFDiag,
                OldWindowRect.l,
                OldWindowRect.t,
                MoveNcRight(OldWindowRect, NewMousePos.x - OldMousePos.x),
                MoveNcBottom(OldWindowRect, NewMousePos.y - OldMousePos.y),
                mNcRightBorder = mNcBottomBorder);
        });

    // 윈도우 리사이징 모서리
    ZAY_LTRB_UI(panel, 0, SizeBorder, SizeBorder, panel.h() - SizeBorder, "NcLeft",
        ZAY_GESTURE_T(t, this)
        {
            RESIZING_MODULE(CR_SizeHor,
                MoveNcLeft(OldWindowRect, NewMousePos.x - OldMousePos.x),
                OldWindowRect.t,
                OldWindowRect.r,
                OldWindowRect.b,
                mNcLeftBorder);
        });
    ZAY_LTRB_UI(panel, SizeBorder, 0, panel.w() - SizeBorder, SizeBorder, "NcTop",
        ZAY_GESTURE_T(t, this)
        {
            RESIZING_MODULE(CR_SizeVer,
                OldWindowRect.l,
                MoveNcTop(OldWindowRect, NewMousePos.y - OldMousePos.y),
                OldWindowRect.r,
                OldWindowRect.b,
                mNcTopBorder);
        });
    ZAY_LTRB_UI(panel, panel.w() - SizeBorder, SizeBorder, panel.w(), panel.h() - SizeBorder, "NcRight",
        ZAY_GESTURE_T(t, this)
        {
            RESIZING_MODULE(CR_SizeHor,
                OldWindowRect.l,
                OldWindowRect.t,
                MoveNcRight(OldWindowRect, NewMousePos.x - OldMousePos.x),
                OldWindowRect.b,
                mNcRightBorder);
        });
    ZAY_LTRB_UI(panel, SizeBorder, panel.h() - SizeBorder, panel.w() - SizeBorder, panel.h(), "NcBottom",
        ZAY_GESTURE_T(t, this)
        {
            RESIZING_MODULE(CR_SizeVer,
                OldWindowRect.l,
                OldWindowRect.t,
                OldWindowRect.r,
                MoveNcBottom(OldWindowRect, NewMousePos.y - OldMousePos.y),
                mNcBottomBorder);
        });
}

bool sandwichData::IsFullScreen()
{
    return mIsFullScreen;
}

void sandwichData::SetFullScreen()
{
    if(!mIsFullScreen)
    {
        mIsFullScreen = true;
        mSavedNormalRect = Platform::GetWindowRect();

        point64 CursorPos;
        Platform::Utility::GetCursorPos(CursorPos);
        sint32 ScreenID = Platform::Utility::GetScreenID(CursorPos);
        rect128 FullScreenRect;
        Platform::Utility::GetScreenRect(FullScreenRect, ScreenID, false);
        Platform::SetWindowRect(FullScreenRect.l, FullScreenRect.t,
            FullScreenRect.r - FullScreenRect.l, FullScreenRect.b - FullScreenRect.t + 1);
    }
}

void sandwichData::SetNormalWindow()
{
    if(mIsFullScreen)
    {
        mIsFullScreen = false;
        Platform::SetWindowRect(mSavedNormalRect.l, mSavedNormalRect.t,
            mSavedNormalRect.r - mSavedNormalRect.l, mSavedNormalRect.b - mSavedNormalRect.t);
    }
}

void sandwichData::CreateSet()
{
    // 파일이 존재하지 않으면 복사
    const String WidgetPath = String::Format("widget/%s.zay", (chars) gBoardName.Lower());
    if(!Asset::Exist(WidgetPath))
        String::FromAsset("widget/sandwich.zay").ToAsset(WidgetPath, true);

    // 위젯구성과 서버연결
    InitBoard();
    mWidget = new ZayWidget();
    InitWidget(*mWidget, "main");
    mWidget->Reload(WidgetPath);
    mClient = new SandWichClient();
    mClient->Connect(gBoardName, gServerHost, gServerPort);

    // 윈도우명
    Platform::SetWindowName("SandWich[" + gBoardName + "]");
}

void sandwichData::RemoveSet()
{
    delete mWidget;
    delete mClient;
    mWidget = nullptr;
    mClient = nullptr;
    for(sint32 i = 0, iend = mPythons.Count(); i < iend; ++i)
        if(auto OldPython = mPythons.AccessByOrder(i))
            OldPython->Destroy();
    mPythons.Reset();
}

void sandwichData::InitBoard()
{
    ZayWidgetDOM::RemoveVariables("sandwich.");
    ZayWidgetDOM::SetValue("sandwich.name", "'" + gBoardName + "'");
    ZayWidgetDOM::SetValue("sandwich.zoom", String::FromInteger(mZoomPercent));
    ZayWidgetDOM::SetValue("sandwich.select.post", "-1");
    ZayWidgetDOM::SetValue("sandwich.select.posttype", "'text'");
    ZayWidgetDOM::SetValue("sandwich.select.sentence", "-1");
}

void sandwichData::CollectFiles(chars path, Strings& collector)
{
    Platform::File::Search(path,
        [](chars itemname, payload data)->void
        {
            Strings& Collector = *((Strings*) data);
            if(Platform::File::ExistForDir(itemname))
                CollectFiles(itemname, Collector);
            else Collector.AtAdding() = itemname;
        }, (payload) &collector, true);
}

void sandwichData::InitWidget(ZayWidget& widget, chars name)
{
    widget.Init(name, nullptr,
        [](chars name)->const Image* {return &((const Image&) R(name));})

        // 특정시간동안 지속적인 화면업데이트
        .AddGlue("update", ZAY_DECLARE_GLUE(params, this)
        {
            if(params.ParamCount() == 1)
            {
                const sint32o Msec(sint32(params.Param(0).ToFloat() * 1000));
                Platform::BroadcastNotify("ZS_Update", Msec, NT_Normal, nullptr, true);
            }
        })

        // 재접속
        .AddGlue("reconnect", ZAY_DECLARE_GLUE(params, this)
        {
            Platform::BroadcastNotify("Reconnect", nullptr);
            clearCapture();
        })

        // 로그인
        .AddGlue("login", ZAY_DECLARE_GLUE(params, this)
        {
            if(params.ParamCount() == 2)
            {
                const String Author = ZayWidgetDOM::GetComment(params.Param(0).ToText());
                const String PasswordSeed = ZayWidgetDOM::GetComment(params.Param(1).ToText()) + "_" + gBoardName + "_" + Author;
                const String PasswordCRC64 = SandWichUtil::ToCRC64((bytes)(chars) PasswordSeed, PasswordSeed.Length());
                if(mClient)
                    mClient->Login(Author, PasswordCRC64);
                clearCapture();
            }
        })

        // 로그아웃
        .AddGlue("logout", ZAY_DECLARE_GLUE(params, this)
        {
            if(mClient)
                mClient->Logout();
            clearCapture();
        })

        // 요소선택(포스트 또는 문장)
        .AddGlue("select", ZAY_DECLARE_GLUE(params, this)
        {
            if(params.ParamCount() == 2)
            {
                const String Type = params.Param(0).ToText();
                const sint32 Index = params.Param(1).ToInteger();
                if(mClient)
                    mClient->Select(Type, Index);
                clearCapture();
            }
        })

        // 포스트형식 전환
        .AddGlue("turn_post_type", ZAY_DECLARE_GLUE(params, this)
        {
            const String OldPostType = ZayWidgetDOM::GetValue("sandwich.select.posttype").ToText();
            if(OldPostType == "text")
                ZayWidgetDOM::SetValue("sandwich.select.posttype", "'python'");
            else ZayWidgetDOM::SetValue("sandwich.select.posttype", "'text'");
        })

        // 문장 위치초기화
        .AddGlue("set_sentence_pos", ZAY_DECLARE_GLUE(params, this)
        {
            if(params.ParamCount() == 3)
            {
                const String UnitID = params.Param(0).ToText();
                const sint32 PosX = params.Param(1).ToInteger();
                const sint32 PosY = params.Param(2).ToInteger();
                ZayWidgetDOM::SetValue(UnitID + ".x", String::FromInteger(PosX));
                ZayWidgetDOM::SetValue(UnitID + ".y", String::FromInteger(PosY));
            }
        })

        // 파일 업로드
        .AddGlue("upload_files", ZAY_DECLARE_GLUE(params, this)
        {
            if(params.ParamCount() == 1)
            {
                const sint32 PostIndex = params.Param(0).ToInteger();
                String LocalPath;
                if(Platform::Popup::FileDialog(DST_Dir, LocalPath, nullptr, "Find Your Sandwich Project Folder"))
                {
                    const String LockID = String::Format("NewUpload_%d", PostIndex);
                    const String Header = "sandwich.work." + LockID;
                    Strings UploadPathes;
                    CollectFiles(LocalPath, UploadPathes);
                    const String ServerPath = String::Format("post/%d/python", PostIndex);
                    ZayWidgetDOM::SetValue(Header + ".localpath", "'" + LocalPath + "'");
                    ZayWidgetDOM::SetValue(Header + ".serverpath", "'" + ServerPath + "'");
                    for(sint32 i = 0, iend = UploadPathes.Count(); i < iend; ++i)
                    {
                        const String ItemPath = UploadPathes[i].Offset(LocalPath.Length() + 1);
                        ZayWidgetDOM::SetValue(Header + String::Format(".%d.itempath", i), "'" + ItemPath + "'");
                    }
                    ZayWidgetDOM::SetValue(Header + ".count", String::FromInteger(UploadPathes.Count()));
                    ZayWidgetDOM::SetValue(Header + ".focus", "0");
                    m->mClient->NewUpload(LockID, String::Format("post.%d.python", PostIndex));
                }
            }
        })

        // 파이썬 실행
        .AddGlue("run_python", ZAY_DECLARE_GLUE(params, this)
        {
            if(params.ParamCount() == 1)
            {
                const sint32 PostIndex = params.Param(0).ToInteger();
                if(mClient)
                    mClient->PythonExecute(PostIndex);
                clearCapture();
            }
        })

        // 디버그형식 전환
        .AddGlue("turn_debug", ZAY_DECLARE_GLUE(params, this)
        {
            const sint32 OldDebug = ZayWidgetDOM::GetValue("program.debug").ToInteger();
            ZayWidgetDOM::SetValue("program.debug", String::FromInteger(1 - OldDebug));
        })

        // user_content
        .AddComponent(ZayExtend::ComponentType::ContentWithParameter, "user_content", ZAY_DECLARE_COMPONENT(panel, params, this)
        {
            if(params.ParamCount() < 1)
                return panel._push_pass();
            const String Type = params.Param(0).ToText();
            bool HasRender = false;

            branch;
            jump(!Type.Compare("editor"))
            {
                HasRender = RenderUC_Editor(panel);
            }
            jump(!Type.Compare("sentence") && params.ParamCount() == 5)
            {
                const String UnitID = params.Param(1).ToText();
                const String Text = params.Param(2).ToText();
                const sint32 LineGap = params.Param(3).ToInteger();
                const sint32 LastGap = params.Param(4).ToInteger();
                HasRender = RenderUC_Sentence(panel, UnitID, Text, LineGap, LastGap);
            }
            jump(!Type.Compare("python") && params.ParamCount() == 2)
            {
                const sint32 PostIndex = params.Param(1).ToInteger();
                HasRender = RenderUC_Python(panel, PostIndex);
            }

            // 그외 처리
            if(!HasRender)
            ZAY_INNER_SCISSOR(panel, 0)
            {
                ZAY_RGBA(panel, 255, 0, 0, 128)
                    panel.fill();
                for(sint32 i = 0; i < 5; ++i)
                {
                    ZAY_INNER(panel, 1 + i)
                    ZAY_RGBA(panel, 255, 0, 0, 128 - 16 * i)
                        panel.rect(1);
                }
                ZAY_FONT(panel, 1.2 * Math::MinF(Math::MinF(panel.w(), panel.h()) / 40, 1))
                ZAY_RGB(panel, 255, 0, 0)
                    panel.text(Type, UIFA_CenterMiddle, UIFE_Right);
            }
            return panel._push_pass();
        });
}

bool sandwichData::RenderUC_Editor(ZayPanel& panel)
{
    if(ZayControl::RenderEditBox(panel, "ui_editor", "editor", 10, true, false, false))
        panel.repaint();
    return true;
}

bool sandwichData::RenderUC_Sentence(ZayPanel& panel, chars unitid, chars text, sint32 linegap, sint32 lastgap)
{
    const String UnitX = String::Format("%s.x", unitid);
    const String UnitY = String::Format("%s.y", unitid);
    sint32 PosX = ZayWidgetDOM::GetValue(UnitX).ToInteger();
    sint32 PosY = ZayWidgetDOM::GetValue(UnitY).ToInteger();
    const sint32 TextHeight = Platform::Graphics::GetStringHeight();
    while(true)
    {
        const sint32 ClippingWidth = Math::Max(0, panel.w() - PosX);
        sint32 TextLength = Platform::Graphics::GetLengthOfString(true, ClippingWidth, text);
        if(TextLength == 0 && PosX == 0) // 들여쓴 첫줄이 아니어야 최소글자수 보장
            TextLength = String::GetLengthOfFirstLetter(text);

        // 텍스트 출력
        panel.text(PosX, PosY, text, TextLength, UIFA_LeftTop);
        chars OldText = text;
        text += TextLength;
        if(*text != '\0')
        {
            PosX = 0;
            PosY += TextHeight + linegap;
        }
        else
        {
            PosX += Platform::Graphics::GetStringWidth(OldText);
            // 문장이 끝난후 확보되어야 하는 공간
            if(PosX + lastgap <= panel.w())
                PosX += lastgap;
            else
            {
                PosX = lastgap;
                PosY += TextHeight + linegap;
            }
            break;
        }
    }
    ZayWidgetDOM::SetValue(UnitX, String::FromInteger(PosX));
    ZayWidgetDOM::SetValue(UnitY, String::FromInteger(PosY));
    return true;
}

bool sandwichData::RenderUC_Python(ZayPanel& panel, sint32 postidx)
{
    if(mPythons[postidx].RenderWidget(panel, postidx))
        mPythons[postidx].RenderLogs(panel);
    else
    {
        ZAY_RGB(panel, 64, 128, 255)
            panel.fill();

        // 앱사이즈
        ZAY_RGB(panel, 255, 255, 0)
        ZAY_FONT(panel, 1.5, "arial")
            panel.text(String::Format("[W%d x H%d x %d%%]",
                (sint32) panel.w(), (sint32) panel.h(), sint32(panel.zoom().scale * 100 + 0.5)),
                UIFA_CenterTop, UIFE_Right);

        // 프로그레스
        const String Header = String::Format("sandwich.asset.post.%d.python.download", postidx);
        const sint32 CurFocus = ZayWidgetDOM::GetValue(Header + ".focus").ToInteger();
        const sint32 CurCount = ZayWidgetDOM::GetValue(Header + ".count").ToInteger();
        if(CurFocus < CurCount)
        ZAY_XYRR(panel, panel.w() / 2, panel.h() / 2, 80, 10)
        {
            ZAY_RGB(panel, 128, 192, 255)
                panel.fill();
            ZAY_RGBA(panel, 0, 0, 0, 128)
                panel.rect(3);
            ZAY_XYWH(panel, 0, 0, panel.w() * CurFocus / CurCount, panel.h())
            ZAY_RGB(panel, 0, 128, 255)
                panel.fill();
        }
    }
    return true;
}
