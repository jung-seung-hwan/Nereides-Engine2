#pragma once

// 실제 경과 시간을 받아 이번 프레임에 실행할 고정 tick 수로 바꾼다.
// 시계를 직접 읽지 않는다. 시간을 넣는 쪽은 Application이다.
// 시간 이름은 유니티의 Time과 같은 뜻이다: FixedDeltaTime, DeltaTime, unscaledDeltaTime.
class Time
{
public:
    // fixedDeltaTime: tick 하나의 길이(초). FixedUpdate가 도는 고정 간격이다.
    Time(double fixedDeltaTime, int maxTicksPerFrame);

    // 요청은 기록만 한다. 실제 적용은 다음 BeginFrame의 시작이다.
    void RequestPause(bool paused);
    void RequestScale(double scale);

    // 프레임 경계. 지난 프레임부터 흐른 실제 시간(unscaledDeltaTime, 초)을 받아 이번 프레임에 실행할 tick 수를 정해 돌려준다.
    // unscaledDeltaTime은 정지·배속을 적용하기 전 값이다. 게임 코드는 이 값 대신 FixedDeltaTime()이나 DeltaTime()을 쓴다.
    // 이번 프레임의 DeltaTime()도 여기서 정한다. 시뮬레이션 시간은 여기서 늘리지 않는다.
    int BeginFrame(double unscaledDeltaTime);

    // tick 하나를 마쳤음을 알린다. 시뮬레이션 시간이 tick 길이만큼 늘어난다.
    // 그래서 tick 안에서 읽는 SimulationTime은 그 tick이 시작한 시각이다.
    void FinishTick();

    // tick 하나의 길이. 늘 같은 값이다. FixedUpdate에서 읽는다.
    double FixedDeltaTime() const { return m_fixedDeltaTime; }
    // 이번 프레임의 게임 시간. 배속을 적용한 값이고, 정지면 0이고, maxTicksPerFrame × FixedDeltaTime()을 넘지 않는다.
    // 프레임마다 한 번 도는 Update에서 읽는다.
    double DeltaTime() const { return m_deltaTime; }
    double RealTime() const { return m_realTime; }
    double SimulationTime() const { return m_simulationTime; }
    double DroppedTime() const { return m_droppedTime; }
    bool IsPaused() const { return m_paused; }
    double Scale() const { return m_scale; }

    // 보간 계수: 아직 tick으로 쓰지 않은 남은 시간 ÷ tick 길이. 0 이상 1 미만이다.
    // 모든 tick이 끝난 뒤 그릴 때 읽는다. 정지 중에는 바뀌지 않고, 따라잡기 상한에 걸린 프레임은 0이다.
    double InterpolationAlpha() const { return m_accumulated / m_fixedDeltaTime; }

private:
    double m_fixedDeltaTime;
    int m_maxTicksPerFrame;

    bool m_paused = false;
    double m_scale = 1.0;
    bool m_requestedPaused = false;
    double m_requestedScale = 1.0;

    int m_ticksLeft = 0;        // BeginFrame이 정했지만 아직 마치지 않은 tick 수
    double m_accumulated = 0.0; // 아직 tick으로 꺼내지 않은 게임 시간
    double m_deltaTime = 0.0;   // 이번 프레임의 게임 시간. BeginFrame이 정한다
    double m_realTime = 0.0;
    double m_simulationTime = 0.0;
    double m_droppedTime = 0.0; // 따라잡기 상한 때문에 버린 게임 시간
};
