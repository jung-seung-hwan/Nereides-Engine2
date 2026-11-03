#pragma once

// 이번 단계에서 다루는 키. 창이 생기면 OS 키 코드를 이 이름으로 바꿔 전달한다.
enum class Key
{
    W,
    A,
    S,
    D,
    Space,
    Count // 키 개수. 실제 키가 아니라 배열 크기로만 쓴다.
};

// 게임 코드가 묻는 행동 이름. 어떤 키와 연결되는지는 Input.cpp의 표에 있다.
enum class Action
{
    MoveForward,
    MoveBack,
    MoveLeft,
    MoveRight,
    Attack
};

const int KeyCount = static_cast<int>(Key::Count);

// tick 하나 동안 고정되는 입력 결과. 수집 중인 사건도 같은 모양으로 보관한다.
struct InputSnapshot
{
    bool pressed[KeyCount] = {};
    bool held[KeyCount] = {};
    bool released[KeyCount] = {};
    int mouseX = 0;
    int mouseY = 0;
};

// 입력을 수집과 소비로 나눈다.
// 수집: On... 함수가 tick 사이에 들어온 사건을 모은다.
// 소비: Application이 tick마다 ConsumeForTick을 한 번 호출한다.
// 행동 Component는 마지막 소비 결과만 읽는다.
class Input
{
public:
    // 수집 쪽. 지금은 데모 스크립트, 단계 3부터는 창 메시지가 호출한다.
    void OnKeyDown(Key key);
    void OnKeyUp(Key key);
    void OnMouseMove(int dx, int dy);
    void OnFocusLost();

    // 소비 경계. 모인 사건을 이번 tick의 결과로 옮기고 일회성 값을 비운다.
    void ConsumeForTick();

    // 읽기 쪽. 마지막 ConsumeForTick의 결과를 돌려준다.
    bool IsPressed(Action action) const;
    bool IsHeld(Action action) const;
    bool IsReleased(Action action) const;
    int MouseDeltaX() const { return m_tick.mouseX; }
    int MouseDeltaY() const { return m_tick.mouseY; }

private:
    InputSnapshot m_collecting; // 지난 소비 이후 모인 사건과 현재 눌린 상태
    InputSnapshot m_tick;       // 이번 tick에 행동 Component가 읽는 결과
};
