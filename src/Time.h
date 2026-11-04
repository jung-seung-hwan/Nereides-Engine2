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

    // 프레임 경계. 지난 프레임부터 흐른 실제 시간(deltaTime, 초)을 받아 이번 프레임에 실행할 tick 수를 정해 돌려준다.
    // deltaTime은 정지·배속을 적용하기 전 값이다. 게임 코드는 이 값이 아니라 TickSeconds()를 쓴다.
    // 시뮬레이션 시간은 여기서 늘리지 않는다.
    int BeginFrame(double deltaTime);

    // tick 하나를 마쳤음을 알린다. 시뮬레이션 시간이 tick 길이만큼 늘어난다.
    // 그래서 tick 안에서 읽는 SimulationTime은 그 tick이 시작한 시각이다.
    void FinishTick();

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

    int m_ticksLeft = 0;        // BeginFrame이 정했지만 아직 마치지 않은 tick 수
    double m_accumulated = 0.0; // 아직 tick으로 꺼내지 않은 게임 시간
    double m_realTime = 0.0;
    double m_simulationTime = 0.0;
    double m_droppedTime = 0.0; // 따라잡기 상한 때문에 버린 게임 시간
};
