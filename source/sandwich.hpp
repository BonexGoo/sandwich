#pragma once
#include <service/boss_zay.hpp>
#include <service/boss_zaywidget.hpp>
#include "sandwichclient.hpp"

class sandwichData : public ZayObject
{
public:
    sandwichData();
    ~sandwichData();

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
    void CreateSet();
    void RemoveSet();
    void InitBoard();
    static void CollectFiles(chars path, Strings& collector);
    void InitWidget(ZayWidget& widget, chars name);

public:
    bool RenderUC_Editor(ZayPanel& panel);
    bool RenderUC_Sentence(ZayPanel& panel, chars unitid, chars text, sint32 linegap, sint32 lastgap);
    bool RenderUC_Python(ZayPanel& panel, sint32 postidx);

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
    sint32 mZoomPercent {100};

public: // 위젯과 통신
    uint64 mUpdateMsec {0};
    uint64 mRenderMsec {0};
    ZayWidget* mWidget {nullptr};
    SandWichClient* mClient {nullptr};
};
