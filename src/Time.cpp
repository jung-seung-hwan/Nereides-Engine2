#include "Time.h"
#include <stdexcept>

namespace
{
// 0.1을 여러 번 더하면 0.2보다 아주 조금 작아질 수 있다. tick을 셀 때만 이 여유를 둔다.
const double Epsilon = 1e-9;
}

Time::Time(double tickSeconds, int maxTicksPerFrame)
    : m_tickSeconds(tickSeconds), m_maxTicksPerFrame(maxTicksPerFrame)
{
    if (tickSeconds <= 0.0)
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

int Time::BeginFrame(double realSeconds)
{
    // 호출 약속: 지난 프레임의 tick을 모두 마친 뒤에 다음 프레임을 시작한다.
    if (m_ticksLeft > 0)
        throw std::logic_error("Finish every tick before the next frame.");
    if (realSeconds < 0.0)
        throw std::invalid_argument("Real elapsed time cannot be negative.");

    // 적용 경계: 이번 프레임의 tick은 모두 같은 정지·배속 값으로 실행된다.
    m_paused = m_requestedPaused;
    m_scale = m_requestedScale;

    m_realTime += realSeconds;
    if (!m_paused)
        m_accumulated += realSeconds * m_scale;

    int ticks = 0;
    while (m_accumulated + Epsilon >= m_tickSeconds && ticks < m_maxTicksPerFrame)
    {
        m_accumulated -= m_tickSeconds;
        ++ticks;
    }

    // 따라잡기 상한: 아직 tick 하나 이상이 남았다면 남은 시간을 모두 버린다.
    if (m_accumulated + Epsilon >= m_tickSeconds)
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
    m_simulationTime += m_tickSeconds;
}
