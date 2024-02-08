#pragma once
#include <service/boss_zay.hpp>
#include <service/boss_zaywidget.hpp>
#include "hueboardclient.hpp"

class hueboardData : public ZayObject
{
public:
    hueboardData();
    ~hueboardData();

public:
    void SetCursor(CursorRole role);
    sint32 MoveNcLeft(const rect128& rect, sint32 addx);
    sint32 MoveNcTop(const rect128& rect, sint32 addy);
    sint32 MoveNcRight(const rect128& rect, sint32 addx);
    sint32 MoveNcBottom(const rect128& rect, sint32 addy);
    void RenderWindowSystem(ZayPanel& panel);
    void RenderWindowOutline(ZayPanel& panel);
    bool IsFullScreen();
    void SetFullScreen();
    void SetNormalWindow();
    void InitWidget(ZayWidget& widget, chars name);

public: // 윈도우
    static const sint32 mMinWindowWidth = 400;
    static const sint32 mMinWindowHeight = 400;
    CursorRole mNowCursor {CR_Arrow};
    bool mNcLeftBorder {false};
    bool mNcTopBorder {false};
    bool mNcRightBorder {false};
    bool mNcBottomBorder {false};
    bool mIsFullScreen {false};
    bool mIsWindowMoving {false};
    rect128 mSavedNormalRect {0, 0, 0, 0};

public: // 위젯
    uint64 mUpdateMsec {0};
    uint64 mLastModifyTime {0};
    ZayWidget* mWidgetMain {nullptr};

public: // 통신
    HueBoardClient mClient;
};
