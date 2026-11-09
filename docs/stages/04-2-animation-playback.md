# 04-2. Animator 재생과 스키닝

목표는 4-1에서 읽은 모델의 클립을 객체마다 따로 재생하고, 뼈 자세로 정점을 움직여(스키닝) 화면에 보이는 것이다. 재생은 tick 안의 애니메이션 단계에서 진행하고, tick마다 "어디서 어디까지 재생했나"를 남겨 단계 5의 판정이 쓰게 한다. 창 모드에서 Space를 누르면 Player가 클립을 한 번 재생하고, Kraken은 계속 반복 재생한다.

이 단계는 [이슈 17](https://github.com/jung-seung-hwan/Nereides-Engine2/issues/17), [PR 20](https://github.com/jung-seung-hwan/Nereides-Engine2/pull/20)이고, 선행은 [단계 4-1](04-1-model-assets.md)이다. 2026-11-09에 SceneManager를 4-2로 끼우려다 학생이 교수 요구 사항(장면 하나를 바탕으로, 입력·재생·타이밍 연결이 먼저)에 맞춰 단계 6 뒤로 미뤘다. 그래서 재생이 다시 4-2다(이슈 19 참고).

코드를 쓰기 전에 검증 세션이 재생 단계 전환 점검을 했다. 결론은 조건부 진행이고 4-1 코드(5b3d3d9)에서 고칠 것은 없었다. 조건은 설계 문서에 세 가지를 정해 적는 것이다: 뼈·클립의 좌표 변환과 그 시험, 뼈 없는 정점의 규칙, 입력 → 재생 → tick → 기록의 계약. 셋 다 아래에 있다.

## 먼저 볼 구조

```mermaid
flowchart LR
    File["arrow.gltf"] --> Loader["LoadGltf<br/>뼈·클립·정점의 뼈 번호까지 읽고<br/>좌표계를 바꾼다"]
    Loader --> Model["ModelData (AssetManager 소유)<br/>mesh, bones, clips"]
    Model -.가리킨다.-> AnimP["Player의 Animator<br/>재생 시각, 자세, 뼈 행렬, tick 기록"]
    Model -.가리킨다.-> AnimK["Kraken의 Animator"]
    PC["Player의 PlayerControl<br/>Space → Play(\"bend\")"] -->|요청| AnimP
    AnimP -->|뼈 행렬| MR["Player의 ModelRenderer"]
    MR --> Frame["RenderFrame<br/>DrawItem.bones"]
    Frame --> Renderer["Renderer<br/>스키닝 셰이더"]
```

| 누가 | 무엇을 | 소유 |
| --- | --- | --- |
| `LoadGltf` (src/GltfLoader.cpp) | 4-1의 메시에 더해 뼈(이름·부모·기본 자세·역바인드 행렬), 클립(채널·키), 정점의 뼈 번호·가중치를 읽는다. 엔진 좌표계로 바꾼다 | 아무것도 소유하지 않는다 |
| `ModelData` (src/GltfLoader.h) | `mesh`, `bones`, `clips`. 4-1의 `boneCount`·`clipNames`는 `bones.size()`·`clips`로 바뀐다 | AssetManager가 프로그램 끝까지 소유 |
| `Animator` (src/Animator.h, 새 컴포넌트) | 객체 하나의 재생 상태: 지금 클립, 재생 시각, 반복 여부, 뼈 자세, 뼈 행렬, 이번 tick의 기록. 모델 원본은 가리키기만 한다 | GameObject가 소유 |
| `PlayerControl` | Space 누름을 읽으면 자기 Animator에 `Play("bend", 반복 없음)`을 요청한다 | Animator를 가리키기만 한다 |
| `ModelRenderer` | 그릴 항목에 자기 Animator의 뼈 행렬을 담는다. Animator가 없으면 비워 둔다 | Animator를 가리키기만 한다 |
| `Renderer` | 정점의 뼈 번호·가중치와 항목의 뼈 행렬로 정점을 옮긴다(스키닝 셰이더) | GPU 자원 |
| `Application` | tick 안의 애니메이션 단계에서 `Scene.AdvanceAnimation()`을 부른다 | Scene, AssetManager |

## 입력·출력·소유자·호출 시점

### 로더 (LoadGltf)
- **입력**: 4-1과 같은 파일. 더 읽는 것은 스킨의 뼈 목록, 역바인드 행렬, 노드의 기본 자세(이동·회전·크기), 애니메이션의 채널(어느 뼈의 이동/회전/크기)과 키(시각, 값), 정점의 `JOINTS_0`·`WEIGHTS_0`.
- **출력**: `ModelData.bones`(부모가 자식보다 앞에 오도록 정렬), `ModelData.clips`, 정점의 `joints[4]`·`weights[4]`.
- **받지 않는 것**(경로와 이유가 적힌 `unsupported data` 오류): 뼈 128개 초과, LINEAR가 아닌 키 보간(STEP·CUBICSPLINE), 모프(weights) 채널, 정점의 뼈 번호가 뼈 개수를 넘는 것. 뼈가 아닌 노드를 움직이는 채널은 무시한다.
- **호출 시점**: 4-1 그대로 AssetManager가 Initialize에서 한 번.

### Animator (컴포넌트)
- **입력**: 생성자에 `ModelData`. `Play(클립 이름, 반복 여부)`는 요청만 기록한다. 없는 이름이면 `invalid_argument`. `Stop()`은 재생을 멈추고 자세를 그대로 둔다.
- **출력**: `SkinMatrices()`(뼈마다 하나, 정점을 옮기는 행렬), `BoneWorldMatrices()`(뼈의 모델 공간 위치, 단계 5·6이 무기를 붙일 때 쓴다), `LastTick()`(아래 기록), `IsPlaying()`, `ClipName()`, `PlaybackTime()`.
- **소유자**: GameObject. 모델 원본을 가리키기만 하고, 원본은 AssetManager가 갖는다.
- **호출 시점**: tick 안, `Scene.FixedUpdate` 뒤의 애니메이션 단계에서 `AdvanceAnimation()`이 한 번 불린다. 이때 ① 기다리던 `Play` 요청을 적용하고(재생 시각 0, 이번 tick의 `started`), ② `GetTime().FixedDeltaTime()`만큼 재생 시각을 옮기고, ③ 반복이면 되감고 아니면 끝에서 멈추고, ④ 키에서 자세를 읽고(이동·크기는 선형, 회전은 사원수 구면 보간), ⑤ 뼈 행렬을 계산하고, ⑥ 기록을 남긴다. Update에서는 아무것도 하지 않는다. Animator는 정지·배속을 모른다(학생 질문 2).
- 처음 만들 때는 기본 자세(바인드 포즈)의 뼈 행렬을 계산해 둔다. 그래서 한 번도 재생하지 않은 객체도 4-1과 같은 모양으로 그려진다.

### 애니메이션 단계가 Animator에 닿는 길
`Component`에 가상 함수 `AdvanceAnimation()`을 더한다. 기본은 비어 있고 Animator만 채운다. `Scene.AdvanceAnimation()`이 모든 켜진 객체의 켜진 컴포넌트를 차례로 돌며 부른다. `AddDrawItems`와 같은 방식이다. 형식 검사도 목록도 없다.

### PlayerControl·ModelRenderer가 Animator에 닿는 길
장면을 만들 때 Animator를 먼저 만들고, 그 주소를 PlayerControl과 ModelRenderer의 생성자에 넘긴다. 카메라 주소를 넘기는 지금 방식과 같다. Animator 없이도 둘 다 만들 수 있다(nullptr). 그러면 PlayerControl은 공격 횟수만 세고, ModelRenderer는 뼈 행렬 없이 그린다.

### 그리기
- `MeshVertex`에 `joints[4]`(뼈 번호)와 `weights[4]`(가중치)가 더해진다. 셜록과 같은 모양이다.
- `DrawItem`에 `bones`(뼈 행렬 목록)가 더해진다. 비어 있으면 Renderer가 단위 행렬을 올린다.
- Renderer는 항목마다 뼈 행렬을 상수 버퍼(`b2`, 최대 128개)에 올린다. 정점 셰이더가 가중치만큼 네 뼈의 행렬을 섞어 위치와 법선을 옮긴다.

## 좌표계 변환 (조건 1)

4-1은 정점의 X를 뒤집었다(거울 S = diag(-1, 1, 1)). 뼈와 클립도 같은 거울을 맞춰야 한다. 로더가 읽을 때 한 번만 바꾼다.

| 값 | 바꾸는 방법 |
| --- | --- |
| 뼈·노드의 이동 (x, y, z) | (-x, y, z) |
| 회전 사원수 (x, y, z, w) | (x, -y, -z, w) |
| 크기 | 그대로 |
| 역바인드 행렬 M | S·M·S. 첫 행과 첫 열에서 대각선이 아닌 원소의 부호를 바꾼다 |
| 클립의 키 | 위 규칙을 키마다 |

검증 세션이 확인한 것: 이 규칙으로 계산한 결과는 "glTF 공간에서 스키닝한 뒤 X를 뒤집은 결과"와 같다. 그리고 **4-1의 시험 모델로는 변환을 빠뜨려도 시험이 통과한다.** 뼈가 모두 Z축 위에 있고 클립 "bend"가 X축 회전뿐이어서, X를 뒤집어도 값이 바뀌지 않기 때문이다. 그래서 시험 모델에 클립 "swing"을 더한다: tip이 Y축으로 돌고 root가 x 방향으로 움직인다. 변환을 빠뜨리면 이 클립에서 정점이 반대쪽으로 간다.

## 뼈 없는 정점 (조건 2)

4-1 로더는 파일의 모든 메시를 하나로 합친다. 시험 모델에서는 뼈를 따르는 화살표(정점 72개)와 뼈가 없는 받침(정점 24개)이 한 메시다. 코드로 만드는 바닥·상자도 같은 정점 형식과 셰이더를 지난다.

규칙: **뼈가 없는 정점은 가중치를 모두 0으로 둔다.** 로더는 스킨이 없는 노드의 정점에, `MakeBox`·`MakeFloor`는 모든 정점에 0을 둔다. 셰이더는 가중치 합이 0인 정점을 뼈 행렬로 옮기지 않고 그대로 둔다. 메시를 나누지 않으므로 4-1의 "하나로 합침"과 ModelRenderer가 그대로다. 대체 뼈(셜록 방식)는 쓰지 않는다. 셜록을 붙일 때 그쪽 규칙으로 바꾼다.

## 입력 → 재생 → tick → 기록 (조건 3)

```mermaid
flowchart LR
    Msg["창 메시지<br/>Space 누름"] --> Collect["Input 수집"]
    Collect --> Consume["tick 시작: ConsumeForTick"]
    Consume --> FU["Scene.FixedUpdate<br/>PlayerControl이 IsPressed(Attack)를 읽고<br/>Animator.Play(\"bend\", false) 요청"]
    FU --> Anim["Scene.AdvanceAnimation<br/>Animator가 요청을 적용하고<br/>FixedDeltaTime만큼 진행, 뼈 행렬 계산, 기록"]
    Anim --> Col["CheckCollision (placeholder)"]
    Col --> End["EndTick, FinishTick"]
```

- **요청이 먹는 때**: 같은 tick의 애니메이션 단계. 누른 tick의 기록은 `start=0`, `end=0.02`(창 모드), `started=true`다. 단계 5의 마커 판정은 이 규칙을 그대로 물려받는다.
- **tick마다 남기는 기록** (`Animator::TickRecord`):

| 값 | 뜻 |
| --- | --- |
| `played` | 이번 tick에 재생이 진행됐나. 클립이 없거나 끝나 멈춘 뒤면 false |
| `clip` | 재생한 클립 번호 |
| `start`, `end` | 이번 tick이 재생한 구간의 시작·끝 시각(클립 안의 초) |
| `started` | 이번 tick에 재생을 시작했나(또는 클립을 바꿨나) |
| `wrapCount` | 반복 클립이 이번 tick에 끝을 지나 처음으로 되감긴 횟수. 클립이 tick보다 짧으면 2 이상일 수 있다 |
| `finished` | 반복하지 않는 클립이 이번 tick에 끝에 닿았나. 한 번만 선다 |

- **구간 끝을 넣는 규칙** (단계 5가 마커 시각 m을 셀 때): 보통 tick은 `start ≤ m < end`. `wrapCount`가 1 이상이면 두 토막 `start ≤ m ≤ 길이`와 `0 ≤ m < end`이고, 그 사이에 클립 전체를 `wrapCount - 1`번 더 지난 것이다. `finished`면 `start ≤ m ≤ 길이`(끝에 놓인 마커를 한 번 세기 위해 닫는다). 이 규칙이면 tick끼리 구간이 겹치지 않고, 시각 0의 마커는 시작 tick과 되감긴 tick마다 한 번씩 센다.
- **끝 판정의 여유**: 0.02를 쉰 번 더하면 1.0보다 아주 조금 작을 수 있다. Animator는 끝에 닿았는지 볼 때 아주 작은 여유(1e-9)를 둬서 되감김이 한 tick 늦어지지 않게 한다. Time이 tick을 셀 때 쓰는 것과 같은 방법이다.
- **뼈 행렬을 담는 곳**: Animator. 스키닝용(`SkinMatrices`)과 뼈의 모델 공간 위치(`BoneWorldMatrices`) 둘 다 tick 안에서 계산해 둔다. 수집 함수와 ModelRenderer는 읽기만 한다.
- **정지·배속·긴 프레임**: Animator 쪽에 따로 할 일은 없다. 시험 7~9가 이것을 확인한다.

## 장면 상태 규칙

재생·판정·전투에서 장면에 속한 상태(재생 시각, 자세, 뼈 행렬, 기록)는 Scene 안의 객체·컴포넌트에 둔다. Application이나 Scene 밖의 어디에도 객체를 가리키는 목록을 두지 않는다. 단계 6에서 재시작을 "장면을 비우고 다시 채우기"로 할 때 이 상태들이 함께 처음으로 돌아가게 하기 위해서다. 클립·뼈 원본은 장면 상태가 아니라 AssetManager 것이다.

## 유니티와 비교

| 유니티 | 여기 | 다른 이유 |
| --- | --- | --- |
| Animator의 기본 Update Mode는 Normal(프레임마다 진행). Animate Physics로 바꾸면 FixedUpdate 간격 | 늘 tick 안에서 진행(Animate Physics와 같다) | 판정과 같은 시계를 쓰기 위해. 그래서 tick이 없는 프레임은 자세가 전 tick과 같다(아래 결정 1) |
| `Animator.Play("clip")`은 바로 바꾸고, CrossFade는 짧게 섞는다 | `Play`는 바로 바꾼다. 섞기는 없다 | 결정 4 |
| `GetComponent<Animator>()`로 같은 객체의 컴포넌트를 찾는다 | 만들 때 주소를 넘긴다 | 결정 3. 템플릿과 형식 검사를 피한다 |
| 뼈 하나하나가 GameObject(Transform)이고 SkinnedMeshRenderer가 읽는다 | 뼈는 Animator 안의 배열이다. 뼈 위치가 필요하면 `BoneWorldMatrices()`를 읽는다 | 객체 수를 늘리지 않는다 |
| 엔진이 Animator를 종류로 알아서 찾아 돌린다 | Component의 가상 함수 `AdvanceAnimation()`을 모든 컴포넌트에 부른다 | 결정 2 |

## 단계 4-2에서 바뀌는 기존 코드

| 파일 | 바뀌는 것 |
| --- | --- |
| `src/GltfLoader.h/.cpp` | `Bone`, `AnimationChannel`, `AnimationClip`. 스킨·클립·정점 뼈 데이터 읽기와 좌표 변환 |
| `src/RenderFrame.h` | `MeshVertex.joints/weights`, `DrawItem.bones`, `MaxBones` |
| `src/Component.h`, `GameObject`, `Scene` | `AdvanceAnimation()` 가상 함수와 그것을 부르는 단계. `Scene::RunObjects`의 bool이 세 단계(FixedUpdate·Update·Animation)를 고르는 enum이 된다 |
| `src/Animator.h/.cpp` | 새 컴포넌트 |
| `src/ModelRenderer`, `src/PlayerControl` | 생성자에 `Animator*`(없어도 됨). PlayerControl은 공격 클립 이름도 받는다 |
| `src/Renderer.cpp`, 셰이더 | 입력 배치 두 칸(BLENDINDICES·BLENDWEIGHT), 뼈 상수 버퍼 `b2`, 스키닝 |
| `src/Application.cpp` | 애니메이션 단계가 `Scene.AdvanceAnimation()`을 부른다. 창 모드 장면에 Animator. 데모의 애니메이션 줄 문구 |
| `src/AssetManager.cpp`, `src/AssetTests.cpp` | `bones.size()`·`clips.size()` 로그, 클립 2개 |
| `tools/make_test_model.py`, `assets/test/arrow.gltf` | 클립 "swing" 추가 |
| `src/AnimationTests.*`, `main.cpp`, `.vcxproj`, `build.yml` | `--animation-tests` |

## 결정 기록

### 학생이 정한 것 (2026-11-09)

| 번호 | 물음 | 결정 | 이유 |
| --- | --- | --- | --- |
| 1 | tick이 없는 프레임의 포즈 | 50Hz 그대로 둔다. 자세는 tick 안에서만 계산하고 그릴 때는 읽기만 한다 | 가장 단순하고 재생·타이밍에 집중한다. 단계 6에서 실제 모델의 끊김이 보이면 "직전 자세와 섞기"를 다시 본다. 몸의 위치는 3-2부터 보간한다 |
| 2 | 애니메이션 단계가 Animator를 찾는 길 | Component 가상 함수 `AdvanceAnimation()` | 그리기(AddDrawItems)와 같은 방식. 형식 검사·목록 관리가 없다 |
| 3 | PlayerControl·ModelRenderer가 Animator에 닿는 길 | 장면을 만들 때 주소를 넘긴다 | 처음 설계의 "명시적인 연결". 카메라와 같은 방식이고 템플릿이 없다 |
| 4 | 클립 바꾸기("기본 전환") | 섞지 않고 넘어간다. 요청은 그 자리에서 저장되고, 자세는 같은 tick의 애니메이션 단계에서 새 클립의 0초로 바뀐다 | 유니티의 `Animator.Play`와 같다. 단계 5의 판정이 클립 하나만 보면 된다. 섞기는 단계 6에서 필요하면 |
| 5 | Space로 재생을 보일지 | 보인다 | 첫 합격 기준의 앞부분 "버튼 하나로 모션 재생"과 교수 요구의 "입력·재생·시간 연결"이 이 단계에서 보인다 |

### 엔진 쪽에서 정한 것

| 항목 | 결정 | 이유 |
| --- | --- | --- |
| 뼈 개수 한도 | 128 | 상수 버퍼 하나(8KB)에 들어간다. 셜록은 127+1이고, 크라켄 모델을 고를 때 이 한도를 본다 |
| 받는 키 보간 | LINEAR만. 회전은 구면 선형 보간(slerp) | glTF 규격의 LINEAR 회전이 slerp다. STEP·CUBICSPLINE은 쓰는 모델이 생기면 |
| 뼈 순서 | 부모가 자식보다 앞에 오게 로더가 정렬하고 정점의 뼈 번호를 고친다 | 뼈 행렬을 앞에서 뒤로 한 번 돌면 끝난다 |
| 관절 사이에 낀 보통 노드, 뼈대 위 노드 | 관절이 아닌 조상 노드의 변환을 자식 관절의 부모 기준 변환에 합친다(root 관절 → 보통 노드 → tip 관절 같은 파일). 스킨이 달린 메시 노드 자체의 변환은 4-1처럼 무시한다 | 리뷰에서 잡힌 빈틈이다. 시험 11이 중간 노드를 끼운 사본으로 확인한다 |
| 가중치 합 | 셰이더가 가중치 합으로 나눈다 | 도구의 반올림으로 합이 1이 아닌 파일에서 정점이 줄거나 늘지 않게 한다 |
| 뼈 상수 버퍼 올리기 | 뼈가 있는 항목마다 8KB를 올리고, 뼈 없는 항목은 단위 행렬 한 벌을 한 번만 올린다 | 항목이 넷뿐이라 지금은 비용이 작다. 항목이 많아지면 재는 지점이다 |
| 끝난 클립의 자세 | 마지막 키에 멈춘다. `IsPlaying()`은 false | 유니티도 반복 없는 클립은 마지막 프레임에 멈춘다 |
| 같은 클립을 다시 `Play` | 처음부터 다시 | 재시작·연타가 단순하다 |
| 데모의 애니메이션 줄 | `4. Animation` 뒤 문구를 "advance each Animator by FixedDeltaTime (none in the demo scene)"으로 바꾼다 | placeholder가 아니게 됐다. 데모 장면에는 Animator가 없다. 데모 SHA가 바뀌므로 새 값을 기록한다 |
| 법선의 스키닝 | 위치와 같은 뼈 행렬로 돌린다 | 뼈에 비균등 크기가 있으면 틀어진다. 알려진 한계 |

## 알려진 한계

- tick이 없는 프레임에서는 자세가 전 tick과 같다(결정 1).
- 뼈에 비균등 크기가 있으면 법선이 틀어진다. 뼈대 위 노드의 변환과 스킨 노드의 변환은 무시한다.
- STEP·CUBICSPLINE 보간, 모프, 뼈 128개 초과, 스킨 둘 이상은 받지 않는다.
- 섞는 전환(크로스페이드)은 없다. 바뀌는 순간 자세가 튄다.
- 재생을 보여 줄 에셋은 시험 화살표뿐이다. 팀의 Player.glb(Blender 내보내기, 1.3MB)는 학습 지도 세션이 4-1 빌드로 읽어 본 결과 뼈도 클립도 없는 정적 메시 둘이다(2026-11-09). 게임에 쓸지는 학생이 아직 정하지 않았다.
- 장면을 바꿀 때 입력이 이어지는 문제는 SceneManager 때 본다(이슈 19).
- 뼈 128개 초과와 길이 0인 클립을 거부·처리하는 시험은 없다. 그런 시험 파일을 만들기 어렵다.
- 뼈에 기울임(shear)이 있는 변환은 이동·회전·크기로 나눌 수 없어 받지 않는다.

## 꼭 필요한 문법만 읽기

- **사원수**: 회전을 숫자 넷 (x, y, z, w)으로 적는 방법이다. 두 회전의 중간을 구하는 `XMQuaternionSlerp`와 행렬로 바꾸는 `XMMatrixRotationQuaternion`만 쓴다. 안의 수학은 몰라도 된다.
- **행렬 곱의 순서**: 행 벡터 규칙이라 `local × parentWorld`처럼 "먼저 적용할 것을 왼쪽에" 쓴다. 4-1의 월드 행렬과 같은 규칙이다.
- **enum으로 단계 고르기**: `Scene::RunObjects(Step step)`의 `switch`. 3-1의 `RunMode`와 같은 모양이다.
- **HLSL의 `mul`과 배열 상수 버퍼**: `bones[input.joints.x]`처럼 번호로 행렬을 고른다.

## 읽기 순서

1. 이 문서의 "먼저 볼 구조"와 "입력 → 재생 → tick → 기록".
2. `src/Animator.h` — 갖는 것, `Play`, `AdvanceAnimation`, `TickRecord`.
3. `src/Application.cpp`의 `BuildWindowScene`(Animator를 만들고 넘기는 자리)과 `UpdateAnimation`.
4. `src/PlayerControl.cpp`의 공격 → `Play`.
5. `src/Animator.cpp`의 `AdvanceAnimation`(요청 적용 → 진행 → 샘플 → 행렬 → 기록).
6. `src/GltfLoader.cpp`의 `ReadSkeleton`·`ReadClips`와 좌표 변환.
7. `src/SceneVertexShader.hlsl`의 스키닝.
8. `src/AnimationTests.cpp`.

## 실행

```powershell
msbuild NereidesEngine2.sln /p:Configuration=Debug /p:Platform=x64
./bin/x64/Debug/NereidesEngine2.exe
./bin/x64/Debug/NereidesEngine2.exe --demo
./bin/x64/Debug/NereidesEngine2.exe --animation-tests
./bin/x64/Debug/NereidesEngine2.exe --asset-tests
```

창 모드(인자 없음): Kraken(빨강)의 화살촉이 계속 꺾였다 펴진다. Space를 누르면 Player(파랑)의 화살촉이 한 번 꺾였다 펴진다. 콘솔에 `Attack #n` 줄.

## 검증 계획

| 명령 | 기대 결과 |
| --- | --- |
| Debug·Release 빌드 | 경고 0 |
| `--animation-tests` | 아래 10개 case 통과 |
| `--asset-tests` | 4개 통과(클립 2개, 정점·인덱스 수 그대로) |
| `--lifecycle-tests`, `--input-time-tests`, `--window-tests`, `--render-tests` | 11, 19, 8, 9개 통과 |
| `--demo` | 104줄. 애니메이션 줄 문구만 바뀐다. 새 SHA-256을 기록한다 |

애니메이션 시험:

1. **읽기**: 뼈 2개(root, tip. tip의 부모가 root), 클립 2개("bend", "swing"), 화살표 정점은 뼈 번호와 가중치 1, 받침 정점은 가중치 0.
2. **기본 자세**: 한 번도 재생하지 않은 Animator의 뼈 행렬이 단위 행렬이다(4-1 화면과 같다).
3. **변환**: "swing" 0.5초(tip이 Y축으로 90도, root가 x로 0.2)에서 정점 (-0.25, 0.05, 1.4)가 (-0.6, 0.05, 0.75)로 간다. "bend" 0.5초에서는 (-0.25, -0.4, 1.05)로 간다. 기대값은 glTF 공간에서 손으로 계산한 뒤 X를 뒤집은 값이다. 변환을 빠뜨리면 "swing"의 x 부호가 반대가 된다.
4. **연결**: Space를 넣은 tick에서 PlayerControl이 `Play`를 요청하고, 같은 tick의 애니메이션 단계에서 기록이 `started=true, start=0, end=0.1`(시험 tick)이다. 다음 tick은 `started=false, start=0.1, end=0.2`.
5. **반복·되감김**: 1초 클립을 0.1초 tick으로 10번 돌리면 10번째 tick에 `wrapCount=1, start=0.9, end=0`. 11번째는 `start=0, end=0.1`(0.1을 열 번 더한 합이 1보다 조금 작아도). 이어서 재생 중 `Play`는 새 클립의 0초부터 시작하고, `Stop` 뒤에는 진행이 없고 자세가 그대로다.
6. **끝**: 반복 없는 클립은 10번째 tick에 `finished=true, end=1.0`, 11번째는 `played=false`, 자세는 마지막 키 그대로, `IsPlaying()` false.
7. **정지**: 정지 요청 뒤의 프레임에서 재생 시각이 그대로다.
8. **배속**: 배속 2에서 같은 실제 시간 동안 두 배 진행한다.
9. **긴 프레임**: 실제 1초 프레임에서 tick 상한(5)만큼만 진행하고 나머지는 Time이 버린다.
10. **독립 재생**: 같은 원본을 쓰는 두 Animator의 재생 시각이 따로 간다.
11. **뼈 순서와 중간 노드**: 관절 순서를 뒤집은 사본에서도 뼈가 부모 먼저이고 정점의 뼈 번호가 고쳐진다. root와 tip 사이에 보통 노드를 끼운 사본에서 tip의 이동이 1 + 0.5가 된다.
12. **오류**: 키 보간을 STEP으로 바꾼 사본은 `unsupported data`. 없는 클립 이름은 `invalid_argument`.

(시험 함수는 12개를 10개 case로 묶는다. 실제 수는 검증 기록에 적는다.)

창 모드는 로컬에서 스크립트로 확인한다.

| 조작 | 기대 결과 |
| --- | --- |
| 인자 없이 실행 | 4-1과 같은 장면. Kraken의 화살촉이 반복해서 꺾인다 |
| Space | Player의 화살촉이 한 번 꺾였다 펴진다. `Attack #1` 로그 |
| W·A·S·D, 크기 변경, 최소화·복원, 닫기 | 4-1과 같다. 최소화 중에는 Kraken도 멈춘다 |

## 학생 질문

1. Space를 누른 tick에서 Player의 재생이 시작되기까지 거치는 함수를 Application → Scene → GameObject → 컴포넌트 순서로 적어라. 그리고 `RunTick`에서 `UpdateAnimation`을 `Scene.FixedUpdate` 앞으로 옮기면, Space를 누른 tick과 그다음 tick의 기록(`started`, `start`, `end`)은 어떻게 바뀌는가?
2. 창을 최소화하면 Kraken의 재생이 멈춘다. Animator 코드에는 정지를 검사하는 줄이 없다. 어디서 멈추는가?
3. 정점 셰이더에서 "가중치가 모두 0인 정점은 그대로 둔다" 처리를 빼면 받침(뼈 없는 정점)은 화면에서 어떻게 될까? 셰이더가 뼈 행렬을 섞는 계산으로 설명하라.

변형 과제: Player의 공격 클립을 "swing"으로 바꾼다. 실행하기 전에 재생이 끝난 뒤 화살표가 어느 자세에 멈출지, 그다음 Space에서 어떻게 시작할지, "swing" 한 번이 창 모드에서 몇 tick인지 예상을 먼저 적는다. 실행해서 확인한 뒤(tick 수는 `TickRecord`로 센다), 그렇게 되는 이유를 코드 줄로 댄다.
정답 변경은 기본 예제에 포함하지 않는다. 학생 이해 상태는 미확인이다.

## 리뷰 기록

코드를 쓴 뒤 GPT(Codex CLI, gpt-6-sol medium, 읽기 전용)와 Gemini(Antigravity CLI, gemini-3.8-flash-high, plan)에게 같은 diff와 이 문서를 주고 독립적으로 리뷰를 받았다. 그 뒤 두 리뷰를 A·B로 익명화해 반영 diff와 함께 다시 주고 교차 확인을 받았다. 모든 항목은 코드와 대조한 뒤에 반영 여부를 정했다. 프롬프트와 결과는 저장소 밖 `.ai-review/`에 있다. 두 리뷰어 모두 "swing"·"bend"의 손 계산 기대값을 직접 계산해 코드의 기대값과 같다고 확인했다.

| 지적 | 누가 | 판정과 반영 |
| --- | --- | --- |
| 관절 사이에 관절이 아닌 노드가 끼면(root → 보통 노드 → tip) 그 노드와 root의 변환이 빠진다 | GPT | 맞음. 관절이 아닌 조상 노드의 변환을 자식 관절의 부모 기준 변환에 합치도록 `ReadSkeleton`을 고침. 중간 노드를 끼운 사본으로 시험 |
| 0.1을 열 번(0.02를 쉰 번) 더하면 1.0보다 조금 작아 되감김이 한 tick 늦어진다. 시험이 0.25로 피해 갔다 | GPT, Gemini | 맞음. 끝 판정에 1e-9 여유를 두고, 되감기를 fmod 대신 빼기로 바꿔 0 근처가 깨끗하게 0이 되게 함. 시험을 0.1×10으로 바꿈 |
| 클립이 tick보다 짧으면 한 tick에 여러 번 되감기는데 bool 하나로는 못 센다 | GPT | 맞음. `wrapped` → `wrapCount`(횟수). 구간 규칙에 "그 사이 클립 전체를 wrapCount−1번" 추가 |
| 키 시각 검사가 같은 시각을 허용한다 | GPT | 맞음. `<=`로 거부 |
| 관절 순서 뒤집기, 중간 부모, 재생 중 Play, Stop, 길이 0 클립, 128개 경계, GPU 스키닝 시험이 없다 | GPT, Gemini(길이 0) | 앞의 넷은 시험 추가(case 10개). 길이 0 클립과 128개 초과는 시험 파일을 만들기 어려워 알려진 한계. GPU 결과는 창 모드 캡처로 확인 |
| 뼈 없는 항목마다 8KB 뼈 상수 버퍼를 올린다 | Gemini, GPT(참고) | 맞음. 단위 행렬 한 벌은 한 번만 올리고, 뼈가 있는 항목만 항목마다 올림. 뼈 없는 항목 뒤에 뼈 있는 항목이 와도 안전하도록 플래그로 기억 |
| 가중치 합이 정확히 1이 아닌 파일에서 정점이 줄거나 는다 | Gemini | 맞음. 셰이더가 가중치 합으로 나눔 |
| "Play는 바로 바꾼다"와 "다음 애니메이션 단계에서 적용"이 섞여 있다 | GPT | 맞음. "요청은 그 자리에서 저장, 자세는 같은 tick의 애니메이션 단계에서"로 통일 |
| 학생 질문 1~3의 답이 문서·주석에 결론으로 적혀 있다 | GPT, Gemini | 일부 반영. 질문 1은 "호출 순서를 추적하라"로 바꾸고, 질문 2의 답이 되는 주석(Animator.h, 시험 주석)을 지움. 질문 3의 규칙(가중치 0)은 검증 조건 2의 설계 규칙이라 문서에 남기고, 질문은 "어디서"를 찾는 것으로 둠 |
| UpdateStep enum이 4-1의 tick 경계·반영 규칙을 유지한다. 정점 배치(12+12+4+16)가 입력 배치와 맞다 | GPT, Gemini | 확인됨. 바꾼 것 없음 |
| (교차 확인) 중간 노드의 변환을 자식 관절의 이동·회전·크기에 합쳐 두면, 그 관절의 채널이 값을 덮을 때 중간 노드 몫이 사라진다 | GPT, Gemini(교차 확인에서 둘 다) | 맞음. 첫 반영의 새 문제였다. `Bone.parentOffset`에 따로 두고 Animator가 자기 행렬 × offset × 부모 행렬로 곱하게 고침. 시험 11이 "bend"를 재생한 뒤에도 tip이 z=1.5인지 확인 |
| (교차 확인) 길이가 끝 여유(1e-9)보다 짧은 클립은 while이 오래 돈다 | GPT, Gemini | 맞음. 그런 클립은 반복하지 않고 바로 끝낸다 |
| (교차 확인) 관절 순서 뒤집기 시험은 정점 번호·역바인드를 함께 바꾸지 않아 정상 파일의 그리기 검증은 아니다 | GPT | 맞음. 재정렬과 번호 고치기만 확인하는 시험이라고 주석에 적음. 정상 역순 파일은 팀 모델을 쓸 때 본다 |
| (교차 확인) 같은 시각의 키 거부 시험이 없는데 주석은 있다고 한다 | GPT | 맞음. 이진 버퍼를 바꿔야 해서 시험하지 않는다고 주석을 고침 |
| (교차 확인) 스키닝 뒤 법선을 정점 셰이더에서 정규화하지 않는다 | Gemini | 반영 안 함. 픽셀 셰이더가 `normalize`하므로 길이는 거기서 맞춰진다. 보간 중 길이 차이는 이 단계의 평면 조명에서 보이지 않는다 |

비용: 리뷰 139KB 프롬프트에 GPT 54,653/3,521 토큰, 교차 확인 41KB에 25,335/4,116 토큰. Gemini는 토큰을 보고하지 않는다.

## 검증 기록

2026-11-09, Windows 11 개발 PC의 VS2022 MSBuild로 Debug·Release x64를 빌드했다. GPT·Gemini 리뷰와 교차 확인을 반영한 뒤(0ef7ef9) 두 구성을 다시 빌드해 아래 명령을 모두 다시 돌렸고, 표는 그 결과다.

| 명령 | 기대 결과 | 실제 결과 |
| --- | --- | --- |
| `msbuild NereidesEngine2.sln /p:Configuration=Debug /p:Platform=x64`, Release도 같게 | 경고 없이 성공 | 두 구성 모두 종료 코드 0, 경고 0 |
| `--animation-tests` | 10개 case 통과 | 두 구성 모두 `Animation tests PASS (10 cases)` |
| `--asset-tests` | 4개 통과(클립 2개) | 두 구성 모두 PASS (4), `Load model: assets/test/arrow.gltf (vertices=96 triangles=48 bones=2 clips=2)` |
| `--lifecycle-tests`, `--input-time-tests`, `--window-tests`, `--render-tests` | 11, 19, 8, 9개 통과 | 두 구성 모두 PASS (11 / 19 / 8 / 9) |
| `--demo` | 104줄, 애니메이션 줄 문구만 바뀜 | 두 구성 모두 종료 코드 0, 104줄, SHA-256 `76F87A743D09C37985FBCEFC8990BFD7AFEF762BD9D9C2B188046809AA69DF6C`(4-1의 `583F23F5…`와 다른 것은 `4. Animation` 줄 7개의 문구뿐) |

창 모드는 Release 실행 파일을 스크립트(`window-check-42.ps1`)로 띄워 창 메시지를 보내고 화면을 캡처해 확인했다.

| 조작 | 기대 결과 | 실제 결과 |
| --- | --- | --- |
| 인자 없이 실행 | 4-1과 같은 장면, Kraken의 화살촉이 반복해서 꺾인다 | 첫 줄 `Stage 4-2: animation playback`, `Load model` 한 줄(bones=2 clips=2). 캡처마다 Kraken의 화살촉 각도가 다르다(반복 재생 중) |
| Space, 0.45초 뒤 캡처, 0.8초 뒤 캡처 | Player의 화살촉이 꺾였다가(0.5초에 90도) 1초에 펴진다. `Attack #1` 로그 | 0.45초 캡처에서 파란 화살촉이 아래로 꺾여 있고, 1.25초 캡처에서 원래 모양이다. `Attack #1` |
| 두 번째 Space | 처음부터 다시 재생. `Attack #2` | 0.45초 캡처에서 다시 꺾여 있다. `Attack #2` |
| D 2초 | 4-1과 같이 이동 | 0.5초마다 x가 0.5씩 는다 |
| 1200×400, 최소화 뒤 복원 | 4-1과 같다 | 비율 유지, 정지·재개 로그, 복원 뒤 그대로 |
| 창 닫기 | Shutdown, 종료 코드 0 | 객체 정리 로그 4줄, 종료 코드 0 |

- 마우스 회전은 3-2·3-3·4-1과 같은 이유로 스크립트로 확인하지 못했다.
- 최소화 중에 Kraken이 멈추는 것은 캡처로 구분할 수 없었다(복원 직후 캡처뿐). 시험 7(정지에서 재생 시각 그대로)이 같은 동작을 CPU에서 확인한다.
- CI는 PR 20의 확인 4개(Debug·Release 빌드와 시험 8종)가 a44e62b에서 모두 성공했다.
