#include <boss.hpp>
#include "hueboard.hpp"

#include <resource.hpp>
#include <service/boss_zaywidget.hpp>
#include <format/boss_bmp.hpp>
#include <format/boss_flv.hpp>

ZAY_DECLARE_VIEW_CLASS("hueboardView", hueboardData)

extern sint32 gProgramWidth;
extern sint32 gProgramHeight;

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
        const uint64 CurMsec = Platform::Utility::CurrentTimeMsec();

        // Update처리
        if(m->mUpdateMsec != 0)
        {
            if(m->mUpdateMsec < CurMsec)
                m->mUpdateMsec = 0;
            m->invalidate(2);
        }

        // 위젯 틱실행
        if(m->mWidgetMain && m->mWidgetMain->TickOnce())
            m->invalidate();

        // 통신 틱실행
        if(m->mClient.TickOnce())
            m->invalidate();
    }
}

ZAY_VIEW_API OnNotify(NotifyType type, chars topic, id_share in, id_cloned_share* out)
{
    if(type == NT_Normal)
    {
        if(!String::Compare(topic, "Update"))
        {
            const uint64 Msec = (in)? Platform::Utility::CurrentTimeMsec() + sint32o(in).ConstValue() : 1;
            if(m->mUpdateMsec < Msec)
                m->mUpdateMsec = Msec;
        }
    }
    else if(type == NT_SocketReceive)
    {
        if(m->mClient.TryRecvOnce())
            m->invalidate();
    }
    else if(type == NT_ZayWidget)
    {
        if(!String::Compare(topic, "SetCursor"))
        {
            auto Cursor = CursorRole(sint32o(in).Value());
            m->SetCursor(Cursor);
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
}

ZAY_VIEW_API OnRender(ZayPanel& panel)
{
    ZAY_XYWH(panel, 0, 0, gProgramWidth, gProgramHeight)
    {
        if(m->mWidgetMain)
            m->mWidgetMain->Render(panel);
        else ZAY_RGBA(panel, 255, 0, 0, 64)
            panel.fill();

        // 윈도우시스템
        #if !BOSS_ANDROID
            m->RenderWindowSystem(panel);
        #endif
    }
}

hueboardData::hueboardData()
{
    mWidgetMain = new ZayWidget();
    InitWidget(*mWidgetMain, "login");
    mWidgetMain->Reload("widget/login.json");
}

hueboardData::~hueboardData()
{
}

void hueboardData::SetCursor(CursorRole role)
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

sint32 hueboardData::MoveNcLeft(const rect128& rect, sint32 addx)
{
    const sint32 NewLeft = rect.l + addx;
    return rect.r - Math::Max(mMinWindowWidth, rect.r - NewLeft);
}

sint32 hueboardData::MoveNcTop(const rect128& rect, sint32 addy)
{
    const sint32 NewTop = rect.t + addy;
    return rect.b - Math::Max(mMinWindowHeight, rect.b - NewTop);
}

sint32 hueboardData::MoveNcRight(const rect128& rect, sint32 addx)
{
    const sint32 NewRight = rect.r + addx;
    return rect.l + Math::Max(mMinWindowWidth, NewRight - rect.l);
}

sint32 hueboardData::MoveNcBottom(const rect128& rect, sint32 addy)
{
    const sint32 NewBottom = rect.b + addy;
    return rect.t + Math::Max(mMinWindowHeight, NewBottom - rect.t);
}

void hueboardData::RenderWindowSystem(ZayPanel& panel)
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
        panel.icon(R((Hovered)? "btn_minimize_h" : "btn_minimize_n"), UIA_CenterMiddle);
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
        panel.icon(R((Hovered)? "btn_downsizing_h" : "btn_downsizing_n"), UIA_CenterMiddle);
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
        panel.icon(R((Hovered)? "btn_close_h" : "btn_close_n"), UIA_CenterMiddle);
    }

    // 아웃라인
    if(!IsFullScreen())
        RenderWindowOutline(panel);
}

void hueboardData::RenderWindowOutline(ZayPanel& panel)
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

bool hueboardData::IsFullScreen()
{
    return mIsFullScreen;
}

void hueboardData::SetFullScreen()
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

void hueboardData::SetNormalWindow()
{
    if(mIsFullScreen)
    {
        mIsFullScreen = false;
        Platform::SetWindowRect(mSavedNormalRect.l, mSavedNormalRect.t,
            mSavedNormalRect.r - mSavedNormalRect.l, mSavedNormalRect.b - mSavedNormalRect.t);
    }
}

void hueboardData::InitWidget(ZayWidget& widget, chars name)
{
    widget.Init(name, nullptr,
        [](chars name)->const Image* {return &((const Image&) R(name));})
        // 특정시간동안 지속적인 화면업데이트(60fps)
        .AddGlue("update", ZAY_DECLARE_GLUE(params, this)
        {
            if(params.ParamCount() == 1)
            {
                const uint64 UpdateMsec = Platform::Utility::CurrentTimeMsec() + params.Param(0).ToFloat() * 1000;
                if(mUpdateMsec < UpdateMsec)
                    mUpdateMsec = UpdateMsec;
            }
        })
        // 통신연결
        .AddGlue("connect", ZAY_DECLARE_GLUE(params, this)
        {
            if(params.ParamCount() == 2)
            {
                const String Host = params.Param(0).ToText();
                const sint32 Port = params.Param(1).ToInteger();
                mClient.Connect(Host, Port);
            }
        })
        // 로그인
        .AddGlue("login", ZAY_DECLARE_GLUE(params, this)
        {
            if(params.ParamCount() == 2)
            {
                const String Author = ZayWidgetDOM::GetComment(params.Param(0).ToText());
                const String PasswordSeed = ZayWidgetDOM::GetComment(params.Param(1).ToText()) + "_HueBoard_" + Author;
                const String PasswordMD5 = AddOn::Ssl::ToMD5((bytes)(chars) PasswordSeed, PasswordSeed.Length());
                mClient.Login(Author, PasswordMD5);
                clearCapture();
            }
        })
        // 로그아웃
        .AddGlue("logout", ZAY_DECLARE_GLUE(params, this)
        {
            mClient.Logout();
            clearCapture();
        })
        // 새 포스트추가
        .AddGlue("newpost", ZAY_DECLARE_GLUE(params, this)
        {
            mClient.NewPost();
            clearCapture();
        })
        // 포스트선택
        .AddGlue("selpost", ZAY_DECLARE_GLUE(params, this)
        {
            if(params.ParamCount() == 1)
            {
                const String Post = params.Param(0).ToText();
                mClient.SelectPost(Post);
                clearCapture();
            }
        })
        // 새 문장추가
        .AddGlue("newsentence", ZAY_DECLARE_GLUE(params, this)
        {
            mClient.NewSentence();
            clearCapture();
        })
        // user_content
        .AddComponent(ZayExtend::ComponentType::ContentWithParameter, "user_content", ZAY_DECLARE_COMPONENT(panel, params, this)
        {
            if(params.ParamCount() < 1)
                return panel._push_pass();
            const String Type = params.Param(0).ToText();
            bool HasRender = false;

            branch;
            jump(!Type.Compare("test") && params.ParamCount() == 3)
            {
                ////////////////////
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
