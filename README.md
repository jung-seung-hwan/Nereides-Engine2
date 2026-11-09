# Nereides Engine2

C++를 6개월 배운 학생이 모듈의 역할과 관계부터 이해하며 만드는 DX11 엔진 학습 프로젝트다.
최종 목표는 캐릭터 한 명과 3D 크라켄 한 마리가 싸우는 작은 프로토타입이다.

## 현재 학습: 단계 4-1

모델 파일(glTF 2.0)을 읽어 엔진의 메시 원본으로 바꾸고, 여러 객체가 그 원본을 함께 쓴다. 원본의 주인은 새로 만든 AssetManager다. 파일 형식을 아는 곳은 `LoadGltf` 하나이고, 외부 로더 cgltf(헤더 한 파일, MIT)를 `third_party/`에 둔다.
glTF는 오른손 좌표계·반시계 앞면이라 로더가 X를 뒤집고 삼각형 순서를 바꿔 엔진의 왼손 좌표계·시계 앞면으로 만든다.
생성·삭제 반영을 Scene.FixedUpdate 끝에서 tick 끝(`Scene.EndTick`)으로 옮겨, 4-3의 애니메이션 단계가 객체를 도는 동안 목록이 바뀌지 않게 했다.
저장소에는 작은 시험 모델 `assets/test/arrow.gltf`만 둔다. 창 모드에서 Player와 Kraken이 같은 화살표 원본을 쓴다. 재생·스키닝은 단계 4-3이다(2026-11-09 학생 결정으로 4-2는 SceneManager가 되었다).
C++ 표준은 C++20으로 올렸다. 팀원의 셜록 엔진과 같다.

1. [전체 목표 구조](docs/architecture.md)
2. [이번 단계: 모델 원본 읽기와 공유](docs/stages/04-1-model-assets.md)
3. [학습 로드맵](docs/roadmap.md) / [실제 GitHub 이슈 목록](docs/github-issues.md)
4. [에이전트 규칙](AGENTS.md)

이전 단계는 [실행 순서](docs/stages/00-execution.md), [객체 수명](docs/stages/01-lifecycle.md), [입력과 시간](docs/stages/02-input-time.md), [창과 실시간 루프](docs/stages/03-1-window-loop.md), [카메라·도형·렌더 데이터](docs/stages/03-2-scene-rendering.md), [Update와 FixedUpdate](docs/stages/03-3-update-split.md) 문서에서 확인한다.
단계별 과거 기록과 현재 구현 범위를 구분한다.

## Visual Studio에서 실행

1. Visual Studio 2022의 C++ 데스크톱 개발 도구(v143)와 Windows SDK를 준비한다.
2. 저장소 루트의 NereidesEngine2.sln을 연다.
3. Debug 또는 Release / x64를 선택한다.
4. NereidesEngine2를 시작 프로젝트로 설정하고 Ctrl+F5를 누른다. 인자 없이 실행하면 창이 열린다.

실행 파일은 bin/x64/<구성>/NereidesEngine2.exe다.
소스 파일은 Visual Studio 프로젝트에 추가하고 .vcxproj와 .filters도 함께 커밋한다.
.sln과 .vcxproj를 직접 관리한다. 이전 build 폴더의 생성된 솔루션은 사용하지 않는다.

## 실행 검증

Developer PowerShell for VS 2022:

```powershell
msbuild NereidesEngine2.sln /p:Configuration=Debug /p:Platform=x64
./bin/x64/Debug/NereidesEngine2.exe --demo
./bin/x64/Debug/NereidesEngine2.exe --lifecycle-tests
./bin/x64/Debug/NereidesEngine2.exe --input-time-tests
./bin/x64/Debug/NereidesEngine2.exe --window-tests
./bin/x64/Debug/NereidesEngine2.exe --render-tests
./bin/x64/Debug/NereidesEngine2.exe --asset-tests
```

Release도 같은 명령의 Debug를 Release로 바꾸어 검사한다.
GitHub Actions가 두 구성의 빌드, 스크립트 데모, 수명 검증, 입력·시간 검증, 창 메시지 검증, 렌더 데이터 검증, 모델 읽기 검증을 수행한다.
모델 읽기 검증은 저장소 루트에서 실행해야 `assets/test/arrow.gltf`를 찾는다(실행 파일 위치에서 위로 올라가 찾기도 한다).
창 모드는 CI에서 돌리지 않는다. 인자 없이 실행해 창을 열고, 단계 문서의 절차로 확인한다.

## 학습 운영

이슈 하나 → 작업 브랜치 하나 → PR 하나로 진행한다.
코드 검증과 학생 이해 확인은 별도 상태다. 학생은 PR에 구조 설명과 작은 변형 과제 결과를 남긴다.
에이전트는 실제로 확인한 결과만 기록하고 학생 답변·교수 승인을 대신 작성하지 않는다.

단계 3-1은 로컬 Debug·Release 빌드, 수명 검증 5개, 입력·시간 검증 17개, 창 메시지 검증 8개, 창 모드 확인과 CI를 통과했다. 학생 이해는 학생 설명 제출, 담당자 확인 전이다.
단계 3-2는 로컬 Debug·Release 빌드, 수명 검증 5개, 입력·시간 검증 17개, 창 메시지 검증 8개, 렌더 데이터 검증 9개, 창 모드 확인을 통과했다. CI는 확인 중이다. 학생 이해는 미확인이다.
단계 3-3은 로컬 Debug·Release 빌드, 수명 검증 8개, 입력·시간 검증 19개, 창 메시지 검증 8개, 렌더 데이터 검증 9개, 스크립트 데모 비교, 창 모드 확인을 통과했다. CI는 확인 중이다. 학생 이해는 미확인이다.
단계 4-1은 로컬 Debug·Release 빌드, 수명 검증 11개, 입력·시간 검증 19개, 창 메시지 검증 8개, 렌더 데이터 검증 9개, 모델 읽기 검증 4개, 스크립트 데모 비교, 창 모드 확인을 통과했다. CI는 확인 중이다. 학생 이해는 미확인이다.
단계 4부터 브랜치 이름은 `stage/<단계>-<주제>`(구현)와 `student/<단계>-<주제>`(학생 변형 과제)다.
리플렉션, 복잡한 템플릿, 자동 등록, 범용 ECS/RHI, 씬 에디터는 추가하지 않는다.
지침 파일만으로 에이전트가 자동 실행되지는 않는다. 다음 단계는 별도로 할당한다.
