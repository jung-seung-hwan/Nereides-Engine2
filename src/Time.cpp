#include "Time.h"
#include <algorithm>
#include <stdexcept>

namespace
{
// 0.1을 여러 번 더하면 0.2보다 아주 조금 작아질 수 있다. tick을 셀 때만 이 여유를 둔다.
const double Epsilon = 1e-9;
}

Time::Time(double fixedDeltaTime, int maxTicksPerFrame)
    : m_fixedDeltaTime(fixedDeltaTime), m_maxTicksPerFrame(maxTicksPerFrame)
{
    if (fixedDeltaTime <= 0.0)
        throw std::invalid_argument("Tick length must be positive.");
    if (maxTicksPerFrame < 1)
        throw std::invalid_argument("Max ticks per frame must be at least 1.");
}

void Time::RequestPause(bool paused)
{
    m_requestedPaused = paused;
}

void Time::RequestScale(double scale)
{
    if (scale < 0.0)
        throw std::invalid_argument("Time scale cannot be negative.");
    m_requestedScale = scale;
}

int Time::BeginFrame(double unscaledDeltaTime)
{
    // 호출 약속: 지난 프레임의 tick을 모두 마친 뒤에 다음 프레임을 시작한다.
    if (m_ticksLeft > 0)
        throw std::logic_error("Finish every tick before the next frame.");
    if (unscaledDeltaTime < 0.0)
        throw std::invalid_argument("Real elapsed time cannot be negative.");

    // 적용 경계: 이번 프레임의 tick과 Update는 모두 같은 정지·배속 값으로 실행된다.
    m_paused = m_requestedPaused;
    m_scale = m_requestedScale;

    m_realTime += unscaledDeltaTime;
    if (!m_paused)
        m_accumulated += unscaledDeltaTime * m_scale;

    // Update가 읽을 이번 프레임의 게임 시간. 정지면 0이다.
    // 아주 긴 프레임은 FixedUpdate가 한 프레임에 따라잡는 최대 시간에서 자른다. 연출이 크게 튀지 않게 하기 위해서다.
    if (m_paused)
        m_deltaTime = 0.0;
    else
        m_deltaTime = std::min(unscaledDeltaTime * m_scale, m_maxTicksPerFrame * m_fixedDeltaTime);

    int ticks = 0;
    while (m_accumulated + Epsilon >= m_fixedDeltaTime && ticks < m_maxTicksPerFrame)
    {
        m_accumulated -= m_fixedDeltaTime;
        ++ticks;
    }

    // 따라잡기 상한: 아직 tick 하나 이상이 남았다면 남은 시간을 모두 버린다.
    if (m_accumulated + Epsilon >= m_fixedDeltaTime)
    {
        m_droppedTime += m_accumulated;
        m_accumulated = 0.0;
    }
    if (m_accumulated < 0.0)
        m_accumulated = 0.0; // 여유값 때문에 생긴 아주 작은 음수를 정리한다.

    m_ticksLeft = ticks;
    return ticks;
}

void Time::FinishTick()
{
    if (m_ticksLeft == 0)
        throw std::logic_error("FinishTick was called without a pending tick.");

    --m_ticksLeft;
    m_simulationTime += m_fixedDeltaTime;
}
