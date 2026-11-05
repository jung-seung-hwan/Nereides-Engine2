# [단계 3-3] Update와 FixedUpdate 나누기

## 목표와 범위

컴포넌트 갱신을 유니티처럼 두 가지로 나눈다. FixedUpdate는 고정 간격(창 모드 0.02초)마다 0번 이상 돌고, 게임 상태(이동·공격, 이후 애니메이션·판정)를 바꾼다. Update는 프레임마다 한 번 돌고, 배속이 적용된 프레임 시간 `DeltaTime()`을 읽으며 화면 연출에만 쓴다. 시간 함수 이름을 유니티와 맞추고(`FixedDeltaTime()`, `DeltaTime()`, `unscaledDeltaTime`), 실시간 루프의 종료를 while 조건이 아니라 루프 안의 if로 처리한다. 선행: 단계 3-2.

범위 조정: 처음 설계는 컴포넌트 로직을 고정 tick에서만 돌리고, 유니티는 GameObject–Component 관계만 참고했다. 2026-11-05에 학생이 유니티의 Update·FixedUpdate 구분을 따르기로 정했다. 게임 결과에 영향을 주는 일은 계속 FixedUpdate에서 하므로 "입력·애니메이션·판정이 같은 시계"라는 처음 목표는 그대로다.

## 모듈 계약

Application은 프레임마다 창 메시지를 처리하고, 닫기 요청이 있으면 루프를 끝낸다. 이어서 Time.BeginFrame이 정한 횟수만큼 tick을 돌며 Scene.FixedUpdate를 부르고, tick이 끝나면 Scene.Update를 한 번 부른 뒤 렌더 데이터를 모아 그린다. 누름·뗌 같은 한 번짜리 입력과 Transform 변경은 FixedUpdate에서만 한다. Update는 정지 중에도 불리고, 그때 `DeltaTime()`은 0이다.

## 완료 조건

- [x] 컴포넌트가 FixedUpdate와 Update를 따로 재정의할 수 있고, 각각 정해진 시점에 불린다.
- [x] 창 모드의 고정 간격이 0.02초다. `--demo`는 0.1초 그대로라 출력이 바뀌지 않는다.
- [x] Time이 `FixedDeltaTime()`과 `DeltaTime()`(배속 적용, 정지면 0, 따라잡기 상한 적용)을 준다.
- [x] 실시간 루프가 루프 안의 if로 끝난다.
- [x] 바뀐 규칙을 AGENTS.md와 구조 문서에 반영한다.

## 학생 질문과 과제

1. tick이 0번인 프레임과 3번인 프레임에서 FixedUpdate와 Update는 각각 몇 번 불리는가?
2. 공격 입력(누름)을 Update에서 읽으면 어떤 문제가 생길 수 있는가?
3. 정지 중에도 Update가 불리는 이유는 무엇이고, 그때 `DeltaTime()`은 얼마인가?

변형 과제: Update만 재정의한 컴포넌트를 만들어 프레임마다 `DeltaTime()`을 더해 보고, 정지·배속에서 그 합이 어떻게 달라지는지 설명한다.

## 상태

- 코드: 로컬 검증 완료. Debug·Release 빌드, 수명 검증 7개, 입력·시간 검증 19개, 창 메시지 검증 8개, 렌더 데이터 검증 9개, 스크립트 데모 비교, 창 모드 확인 통과. CI 확인 중, PR 검토 대기
- 학생 이해: 미확인
- 실제 이슈: https://github.com/jung-seung-hwan/kraken-engine-learning/issues/15; 작업 브랜치: `codex/issue-15-update-split`; PR·학생 답변: 아직 없음
- 구조 문서: [03-3. Update와 FixedUpdate 나누기](../stages/03-3-update-split.md)
