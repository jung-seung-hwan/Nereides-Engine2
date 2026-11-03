#pragma once

// 실제 경과 시간을 받아 이번 프레임에 실행할 고정 tick 수로 바꾼다.
// 시계를 직접 읽지 않는다. 시간을 넣는 쪽은 Application이다.
class Time
{
public:
    Time(double tickSeconds, int maxTicksPerFrame);

    // 요청은 기록만 한다. 실제 적용은 다음 BeginFrame의 시작이다.
    void RequestPause(bool paused);
    void RequestScale(double scale);

    // 프레임 경계. 실제 경과 시간을 받아 이번 프레임의 tick 수를 돌려준다.
    int BeginFrame(double realSeconds);

    double TickSeconds() const { return m_tickSeconds; }
    double RealTime() const { return m_realTime; }
    double SimulationTime() const { return m_simulationTime; }
    double DroppedTime() const { return m_droppedTime; }
    bool IsPaused() const { return m_paused; }
    double Scale() const { return m_scale; }

private:
    double m_tickSeconds;
    int m_maxTicksPerFrame;

    bool m_paused = false;
    double m_scale = 1.0;
    bool m_requestedPaused = false;
    double m_requestedScale = 1.0;

    double m_accumulated = 0.0; // 아직 tick으로 꺼내지 않은 게임 시간
    double m_realTime = 0.0;
    double m_simulationTime = 0.0;
    double m_droppedTime = 0.0; // 따라잡기 상한 때문에 버린 게임 시간
};
