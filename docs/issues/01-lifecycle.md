# [단계 1] GameObject와 Component의 수명

## 목표와 범위

단계 0의 골격을 이어 Scene·GameObject·Component·Transform을 구현한다. 객체 생성, 활성 상태, 갱신, 삭제 예약과 정리 경계를 다룬다. 에디터·자동 등록·리플렉션·복잡한 검색 프레임워크는 제외한다. 선행: 단계 0.

## 모듈 계약

Scene이 GameObject를, GameObject가 Component를 소유한다. Component의 소유 객체 참조는 소유권을 갖지 않는다. 갱신 중 삭제 요청은 즉시 메모리 해제로 연결하지 않는다.

## 완료 조건

- [x] 생성·활성·갱신·삭제 순서가 문서와 로그에 나타난다.
- [x] 비활성 객체의 동작 Component는 갱신되지 않는다.
- [x] 갱신 중 삭제 요청에도 순회 중인 객체 접근이 깨지지 않는다.
- [x] 반복 생성·정리에서 중복 정리와 잔여 객체가 없다.

## 학생 질문과 과제

1. Scene을 정리하면 Component는 어떤 순서로 정리되는가?
2. 위치는 왜 Transform에 보관하는가?
3. 갱신 중 즉시 삭제하면 어떤 문제가 생길 수 있는가?

변형 과제: 두 번째 갱신에서 한 객체를 비활성화하고 실제 갱신 횟수를 확인한다.

## 상태

- 코드: 검증 완료. 로컬 Debug·Release 빌드·수명 검증 5개와 CI 통과, PR 검토 대기
- 학생 이해: 학생 설명 제출, 담당자 확인 전
- 실제 이슈: https://github.com/jung-seung-hwan/kraken-engine-learning/issues/2; 학생 답변: 질문 3개와 변형 과제 설명, https://github.com/jung-seung-hwan/kraken-engine-learning/pull/10#issuecomment-6593274840
