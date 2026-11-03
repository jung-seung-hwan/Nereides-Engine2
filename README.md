# Nereides Engine2

C++를 6개월 배운 학생이 모듈의 역할과 관계부터 이해하며 만드는 DX11 엔진 학습 프로젝트다.
최종 목표는 캐릭터 한 명과 3D 크라켄 한 마리가 싸우는 작은 프로토타입이다.

## 현재 학습: 단계 2

Input이 키·마우스 사건을 모으고 tick마다 한 번 소비하게 한다.
Time이 실제 경과 시간을 고정 tick 수로 바꾸고, 정지·배속을 프레임 경계에서 적용한다.
PlayerControl이 두 결과를 읽어 Player를 움직인다. 창이 아직 없어 입력 사건과 프레임 길이는 데모 스크립트가 넣는다.
실제 창 메시지·시계·애니메이션·충돌·DX11은 아직 구현하지 않았다.

1. [전체 목표 구조](docs/architecture.md)
2. [이번 단계: 입력 전달과 시간 관리](docs/stages/02-input-time.md)
3. [학습 로드맵](docs/roadmap.md) / [실제 GitHub 이슈 목록](docs/github-issues.md)
4. [에이전트 규칙](AGENTS.md)

이전 단계는 [실행 순서](docs/stages/00-execution.md)와 [객체 수명](docs/stages/01-lifecycle.md) 문서에서 확인한다.
단계별 과거 기록과 현재 구현 범위를 구분한다.

## Visual Studio에서 실행

1. Visual Studio 2022의 C++ 데스크톱 개발 도구(v143)와 Windows SDK를 준비한다.
2. 저장소 루트의 NereidesEngine2.sln을 연다.
3. Debug 또는 Release / x64를 선택한다.
4. NereidesEngine2를 시작 프로젝트로 설정하고 Ctrl+F5를 누른다.

실행 파일은 bin/x64/<구성>/NereidesEngine2.exe다.
소스 파일은 Visual Studio 프로젝트에 추가하고 .vcxproj와 .filters도 함께 커밋한다.
.sln과 .vcxproj를 직접 관리한다. 이전 build 폴더의 생성된 솔루션은 사용하지 않는다.

## 실행 검증

Developer PowerShell for VS 2022:

```powershell
msbuild NereidesEngine2.sln /p:Configuration=Debug /p:Platform=x64
./bin/x64/Debug/NereidesEngine2.exe
./bin/x64/Debug/NereidesEngine2.exe --lifecycle-tests
./bin/x64/Debug/NereidesEngine2.exe --input-time-tests
```

Release도 같은 명령의 Debug를 Release로 바꾸어 검사한다.
GitHub Actions가 두 구성의 빌드, 예제 실행, 수명 검증, 입력·시간 검증을 수행한다.

## 학습 운영

이슈 하나 → 작업 브랜치 하나 → PR 하나로 진행한다.
코드 검증과 학생 이해 확인은 별도 상태다. 학생은 PR에 구조 설명과 작은 변형 과제 결과를 남긴다.
에이전트는 실제로 확인한 결과만 기록하고 학생 답변·교수 승인을 대신 작성하지 않는다.

현재 단계 2는 로컬 Debug·Release 빌드, 입력·시간 검증 16개, 수명 검증 5개를 통과했다. 학생 이해는 미확인이다.
리플렉션, 복잡한 템플릿, 자동 등록, 범용 ECS/RHI, 씬 에디터는 추가하지 않는다.
지침 파일만으로 에이전트가 자동 실행되지는 않는다. 다음 단계는 별도로 할당한다.
