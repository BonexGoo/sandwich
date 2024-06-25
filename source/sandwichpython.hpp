#pragma once
#include <service/boss_zay.hpp>
#include <service/boss_zaywidget.hpp>

class SandWichPython
{
public:
    bool TickOnce();
    ZayWidget* ValidWidget(sint32 postidx);
    void PythonConnect(chars host, sint32 port);
    void TryPythonRecvOnce();
    void Destroy();

private:
    void OnPython_log(const Strings& params);
    void OnPython_set(const Strings& params);
    void OnPython_get(const Strings& params);
    void OnPython_call(const Strings& params);
    void PythonSend(const String& comma_params);
    String FindPythonKey(sint32 keycode);

private:
    void Reconnect();
    void UpdateDom(chars key, chars value);
    void PlaySound(chars filename);
    void StopSound();
    void CallGate(chars gatename);

private:
    id_socket mPython {nullptr};
    bool mPythonConnected {false};
    String mPythonHost;
    sint32 mPythonPort {0};
    uint08s mPythonQueue;
    Strings mPythonTasks;
    ZayWidget* mWidget {nullptr};
    bool mWidgetUpdater {false};
    id_sound mSounds[4] {nullptr, nullptr, nullptr, nullptr};
    sint32 mSoundFocus {0};
    Strings mLogs;
};
