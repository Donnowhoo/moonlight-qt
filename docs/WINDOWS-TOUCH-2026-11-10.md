# Windows 터치 조작 — 로컬 구현 결과 (2026-11-10)

## 승인 후 Windows x64 CI 결과

기능 커밋 `18ec62fd24589aca7c026d801a5a675745e351d6`를 `dual-monitor`에 게시했다.
[GitHub Actions 42835774392](https://github.com/Donnowhoo/moonlight-qt/actions/runs/42835774392)는 성공했다.
MSVC 순수 정책 37개 시나리오, 정적 보호 검사 10개, Windows x64 앱 빌드·ZIP 게시를 통과했다.
ARM64/macOS/Linux/Steam Link는 빌드하지 않았다.
릴리스 파일: `Moonlight-DualMonitor-Windows-x64-portable.zip`.
GitHub SHA256: `9ace72bddfe300c3f2ec3a9b1fbc8ef9cf7fae09d015eef3e5f252e85aadc09f`.
Qt/ENet 및 기존 FFmpeg 경로의 비차단 경고가 남아 있다. 실물 터치·OSK 포커스·한글 입력은 미검증이다.
앱 실행·설치는 David가 직접 한다. 아래 로컬 검토 시점의 ‘전체 빌드 미실시’는 과거 기록이며, 이번 CI 성공과 구분한다.

대상은 GalaxyBook5 Pro 360 같은 Windows 터치 PC의 Moonlight입니다. 이 문서는 **최종 사용자 교정: 손가락을 떼면 즉시 끌기 종료**를 반영합니다. 이전의 손 떼고 재배치, 드래그 잠금, 5초 유지 설계는 폐기했습니다. Ctrl/Shift/Alt의 선택적 고정과 마우스 끌기 잠금은 다른 기능입니다.

## 범위와 상태

- `moonlight-qt`의 `dual-monitor` 브랜치 로컬 변경만 있습니다. Sunshine/Android 수정, 커밋, push, CI 실행은 하지 않았습니다.
- Moonlight 전체 빌드, 앱/OSK 실행, 설치, 서비스·디스플레이·레지스트리 설정 변경은 하지 않았습니다.
- 의존성이 없는 정책 시험만 작은 시험 실행 파일로 컴파일·실행했습니다. 순수 정책 37개 시나리오와 정적 보호 시험 10개 통과. 실제 Windows 창·터치 장치·원격 전송은 이 시험에 포함되지 않습니다.
- 새 Windows 제어 파일 및 입력 연결 파일은 기존 설치 헤더를 사용해 출력 파일을 만들지 않는 문법 검사 대상으로 삼았습니다. 이것은 정식 MSVC/Qt 릴리스 빌드 성공을 뜻하지 않습니다.

## 터치만으로 사용하기

1. Moonlight 설정에서 **Windows 터치 단축키 바**를 켭니다. 처음에는 꺼져 있습니다. 새 연결부터 적용됩니다.
2. 직접 터치는 화면의 대상을 누릅니다. 가만히 대고 있는 동안에는 왼쪽 버튼을 누르지 않습니다. 손을 떼면 클릭하고, 8 DIP 이상 움직이면 처음 댄 위치에서 끌기를 시작해 현재 위치까지 움직입니다. 손을 떼면 즉시 버튼을 놓습니다.
3. 기본 바는 이동 / 더보기 / 키보드 / 복사 / 붙여넣기 / 실행취소입니다. **더보기**에서 모든 버튼을 찾을 수 있습니다.
4. 펼친 바의 **직접 터치→트랙패드** 또는 **트랙패드→직접 터치**로 연결 중 방식을 전환합니다. 이 선택은 현재 연결에만 적용됩니다. 전환 전에 진행 중 입력을 정리합니다.
5. 트랙패드를 기본으로 쓰려면 기존 **Use touchscreen as a virtual trackpad**와 새 **Windows 터치 트랙패드 끌기 개선**을 켭니다. 둘 다 새 연결부터 적용됩니다. 단축키 바에서 트랙패드로 전환한 경우에는 현재 연결에서 개선 경로를 사용합니다.
6. 키보드 버튼은 **클라이언트 Windows의 OSK**를 엽니다. 원격 PC에 키보드 열기 단축키를 보내지 않습니다. OSK를 먼저 연 뒤 필요한 Ctrl/Shift/Alt를 선택하세요. 키보드 열 때 기존 가상 수정키는 안전하게 해제합니다.
7. 이동 손잡이로 바를 옮깁니다. 위치는 창 대비 비율, 접힘 상태와 함께 Moonlight의 기존 설정 저장소에 저장하는 코드입니다. 앱을 실행하지 않았으므로 이번 작업에서 설정을 저장한 것은 아닙니다.

지원 버튼: 키보드, 복사(Ctrl+C), 붙여넣기(Ctrl+V), 잘라내기(Ctrl+X), 실행취소(Ctrl+Z), 다시실행(Ctrl+Y), 저장(Ctrl+S), 전체선택(Ctrl+A), Esc, Tab, Enter, Delete, F2, 네 방향키, 다음 터치 우클릭, Ctrl/Shift/Alt, 터치 방식 전환. **전체 해제**는 펼친 메뉴에 있는 보조 복구 기능이며 정상적인 끌기 종료에 필요하지 않습니다.

Ctrl/Shift/Alt 버튼은 **끔→한 번→고정→끔** 순환이며 상태를 표시합니다. 한 번은 키 또는 클릭/끌기의 완료 때 해제합니다. 고정은 해당 버튼으로 끄거나 연결 창을 벗어나면 해제합니다. 물리 키보드와 바의 키 소유를 분리하여 바가 물리 Ctrl을 임의로 놓지 않도록 했습니다. 방향키는 탭마다 한 번이며 길게 눌러 자동 반복은 없습니다.

## 제스처와 판정 기준

새 직접 터치/개선 트랙패드 경로에서만 적용합니다. 새 옵션을 모두 끄면 기존 코드 경로를 사용합니다.

| 동작 | 결과 |
|---|---|
| 트랙패드 한 번 탭 | 첫 탭 후 180ms 동안 다음 탭을 기다린 뒤 한 번 클릭 |
| 트랙패드 두 번 탭하고 두 번째 손가락 접촉을 움직임 | 왼쪽 버튼을 누른 채 끌기, 손을 떼면 즉시 종료 |
| 트랙패드 두 번 탭하고 움직이지 않음 | 두 번 클릭. 각 짧은 탭은 250ms 미만 |
| 트랙패드 650ms 정지 후 이동 | 길게 눌러 끌기, 손을 떼면 즉시 종료 |
| 두 손가락이 같은 세로 방향으로 각각 8 DIP 이상 이동 | 부드러운 고해상도 세로 스크롤 |
| 한 손가락은 8 DIP 이내에 고정, 다른 손가락은 12 DIP 이상 이동 | 왼쪽 버튼을 누른 채 끌기. 고정 후보를 40ms 더 확인해 SDL의 개별 손가락 이벤트를 스크롤로 오인하지 않도록 함 |
| 고정 손가락을 유지하고 다른 손가락으로 근처를 두 번 짧게 탭 | 정확히 한 번 우클릭. 각 탭 220ms 이하, 첫 탭 종료부터 두 번째 탭 종료까지 300ms 이하, 두 탭 위치 24 DIP 이내. 고정 손가락 8 DIP 초과 이동 시 취소 |
| 세 손가락 접촉 상태에서 중심점이 12 DIP 이상 이동 | 가운데 버튼을 누른 채 끌기. 최초 판정 프레임에는 이동을 누적 전송하지 않음 |
| 세 손가락 정지 탭 | 아무 동작 없음. 키보드는 바의 키보드 버튼으로 열기 |

두/세 손가락 끌기는 **어느 손가락이든 처음 떼는 순간 종료**합니다. 나머지 접촉은 모두 뗄 때까지 소비하여 추가 클릭을 만들지 않습니다. 세 번째 손가락이 들어오면 진행 중인 두 손가락 버튼을 먼저 놓고 세 손가락 판정을 시작합니다. 네 손가락 이상은 취소합니다. 판정된 스크롤/끌기는 우클릭으로 변하지 않습니다. 우클릭 완료 후에도 남은 접촉을 소비합니다.

직접 모드의 고정+끌기 시작 대상은 **움직이는 손가락을 처음 댄 위치**입니다. 고정 손가락은 제어용입니다. 시작 대상에서 버튼을 누른 후 후속 상대 이동만 보내므로 제스처 판정 때 누적 거리를 한 번에 끌지 않습니다. 직접 모드의 우클릭 대상은 두 번째 탭 위치, 가운데 끌기 대상은 판정 시 세 손가락 중심입니다. 트랙패드 모드에서는 현재 원격 커서 위치가 대상입니다.

스크롤은 자연스러운 방향으로 100 DIP 이동당 휠 120단위를 보내며 소수 잔여량을 보존합니다. 첫 손가락 이동만으로 즉시 끌기를 선택하지 않고 40ms 동안 다른 손가락 이동을 기다립니다. 기기의 매우 느린/불규칙한 접촉 전달은 실기 확인이 필요합니다. 직접 터치의 정지 손가락에는 시간만으로 클릭을 만들지 않으며 고정 120ms 대기도 없습니다.

## 구현 구조와 안전 처리

- 앱은 `QGuiApplication`이며 스트리밍 때 Qt 처리를 멈추고 SDL 이벤트 루프를 사용합니다. 따라서 QWidget/Qt 이벤트 루프에 의존하지 않는 **SDL 창 소유의 비활성 Win32 도구 창**을 만들었습니다. 스트리밍과 같은 스레드에서 생성·입력·해제·파괴합니다.
- 바는 터치 메시지 전체를 소비하고 터치가 합성한 마우스 이벤트를 중복 처리하지 않습니다. 클릭 동작은 눌렀다 같은 버튼에서 뗄 때 확정됩니다. 버튼 밖으로 움직이면 취소합니다.
- 직접 손가락은 `LiSendMousePositionEvent`로 보내 Sunshine의 기존 듀얼 캔버스 마우스 역매핑을 사용합니다. `LiSendTouchEvent`로 듀얼 정확도를 주장하지 않습니다. 펜 이름을 확인한 경우 기존 네이티브 경로로 넘깁니다. 기존 펜의 듀얼 매핑 한계까지 해결한 것은 아닙니다.
- 트랙패드는 절대 마우스 모드(듀얼 포함)에서 `LiSendMouseMoveAsMousePositionEvent`, 나머지는 기존 상대 이동을 사용합니다.
- 직접 터치의 두/세 손가락 끌기는 Down 위치를 저장하고 후속 이동량을 클라이언트 좌표에 누적한 뒤 절대 위치로 전송합니다. 호스트 마우스 가속에 의존하는 상대 이동을 보내지 않습니다.
- 포커스 상실, 입력 캡처 해제, 연결 종료, 모드 전환, 창/바 배치 변경에서 새 기능이 보유한 입력과 예약을 정리합니다. 새 기능의 지연은 SDL 루프에서 처리하며 별도 네트워크 입력 타이머 스레드를 만들지 않았습니다.
- 도구 창을 표시하려고 물리 모니터 해상도나 저장된 창 모드를 바꾸지 않습니다. 옵션을 켠 연결의 전체화면만 테두리 없는 전체화면으로 선택합니다. 옵션을 끈 기존 PC 경로는 변경하지 않습니다.
- 한국어 도구막대는 C++ 유니코드 이스케이프로 저장하여 MSVC 소스 인코딩에 따른 `??` 손상을 피했습니다.

## 제한과 남은 실기 검증

전체 빌드·연결 실기는 아직입니다. GalaxyBook의 DPI/확장 화면 조합, OSK와 창 포커스, 버튼 아래 클릭 유출 여부, Excel/PDF/ERP의 실제 버튼 해제와 글자 입력을 검증해야 합니다. 좌표 전송 경로를 수정한 것이지 실물 듀얼 터치 정확도를 확인한 것은 아닙니다.

OSK는 Windows 기본 화면 키보드를 명시적으로 여는 기능이며 TabTip/삼성 키보드나 자체 IME 구현이 아닙니다. 실행 실패는 키보드 버튼 오류 표시와 로그로 알립니다. OSK가 시스템 환경에 따라 포커스를 가져가면 안전상 수정키를 해제하고 원격 입력을 멈춥니다. 스트림으로 포커스를 돌린 다음 수정키를 다시 선택해야 합니다. **OSK와 고정 Ctrl의 동시 사용, 한글 조합/ERP 입력은 미검증**입니다. 임의로 포커스를 빼앗거나 로컬 키 입력을 주입하는 우회는 하지 않습니다.

복사/붙여넣기는 호스트 앱의 단축키입니다. 클라이언트 클립보드 동기화, 자체 한글 IME, 포인터 속도 설정, 가장자리 자동 이동은 포함하지 않습니다. 바를 아주 작은 창에 펼칠 경우 모든 버튼 표시를 보장하지 않으므로 일반 창 크기/전체화면에서 사용합니다.

## 검증 재현과 다음 CI

실행은 별도 승인을 받은 환경에서 합니다. 정책 시험은 네트워크·키 주입·창 생성 없이 순수 상태 전이만 확인합니다.

```powershell
python tests/test_desktop_touch_guards.py
# MSVC가 설치된 Windows, 저장소 루트에서:
cmd /c scripts\test-desktop-touch.bat
```

`scripts/test-desktop-touch.bat`는 순수 C++ 시험을 `/UNDEBUG /W4 /WX`로 컴파일하고 실행한 뒤 Python 정적 보호 시험을 실행합니다. 모든 오류는 실패 종료 코드로 반환합니다. Windows 워크플로의 앱 빌드 전에 연결했으므로 다음 승인된 CI에서 실패하면 x64 빌드·업로드·상위 릴리스 게시까지 진행되지 않습니다. 이번 작업에서 CI를 실행하거나 push한 것은 아닙니다.

주요 시험: 클릭 대기/더블탭/650ms 끌기, 중복 Down, 손 떼자마자 Up, 반복 취소, 시간 정수 오버플로, 물리/가상 수정키 취득·해제 순서, 양쪽 손가락의 고정 역할, 지연된 SDL 손가락 전달과 스크롤, 고정+두 번 탭의 거리·시간·이동 취소, 세 번째 손가락 전환과 가운데 버튼 해제, 기본 OFF·펜·절대 좌표 분기, 한국어 문자열, 배열 경계, CI 차단 연결입니다.

참고 API: [SDL 터치 장치 이름](https://wiki.libsdl.org/SDL2/SDL_GetTouchName), [Windows 터치 창 등록](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registertouchwindow).

## 실제 로컬 검증 명령과 증거

아래는 이미 완료한 실행 기록입니다. 이 절을 작성하면서 추가 컴파일·시험·앱 실행을 하지 않았습니다. 모든 명령의 작업 폴더는 `C:\Workspace\dual-monitor\moonlight-qt`입니다.

### 1. 순수 C++ 정책 시험 — 앱 빌드와 구분

```powershell
New-Item -ItemType Directory -Force -Path build/touch-policy-tests | Out-Null
& C:/msys64/ucrt64/bin/g++.exe -std=c++17 -Wall -Wextra -Werror -static -o build/touch-policy-tests/policy.exe tests/desktop_touch_policy_test.cpp
if ($LASTEXITCODE -eq 0) { & ./build/touch-policy-tests/policy.exe }
```

- 최종 정책 변경 후 이 컴파일·시험 블록이 포함된 도구 호출 종료 코드: **0**.
- 컴파일러 진단 출력 없음. `$LASTEXITCODE -eq 0`인 경우에만 시험을 실행했고 아래 출력이 있으므로 컴파일 성공 코드 **0**은 확인됩니다.
- 시험 출력: `37 desktop touch policy scenarios passed`.
- 시험은 `main()`의 마지막 출력에 도달했습니다. 별도로 시험 직후 `$LASTEXITCODE`를 출력한 로그는 없으므로, 최종 호출 코드와 개별 프로세스 종료 코드를 혼동하지 않습니다. 앞선 독립 컴파일·시험 호출의 도구 종료 코드도 **0**이었습니다.
- 생성한 실행 파일: `C:\Workspace\dual-monitor\moonlight-qt\build\touch-policy-tests\policy.exe`.
- 빌드 입력은 `tests/desktop_touch_policy_test.cpp`와 순수 정책 헤더입니다. Qt·SDL·Win32 창·Sunshine·네트워크 전송을 연결하지 않으며 `Moonlight.exe`를 생성하거나 실행하지 않습니다.
- 이 단계는 **소규모 시험 프로그램의 로컬 컴파일·실행**입니다. 따라서 “로컬 컴파일을 전혀 하지 않았다”는 표현은 사용하지 않습니다. 금지된 Moonlight 전체 앱 빌드는 하지 않았습니다.

### 2. Python 정적 보호 시험

```powershell
python tests/test_desktop_touch_guards.py
```

최종 출력의 끝부분:

```text
Ran 10 tests in 0.004s

OK
```

10개 항목 모두 `... ok`로 출력되었습니다. 해당 명령이 포함된 최종 검증 도구 호출의 종료 코드는 **0**입니다. Python 명령 직후 개별 `$LASTEXITCODE`를 별도 출력하지 않았지만 `unittest`의 `OK` 완료 결과를 확인했습니다. 정적 소스 검사이며 실행 파일을 생성하지 않습니다.

### 3. C++ 문법 검사 — 링크·앱 생성·실행 없음

입력 연결 파일 4개의 완료된 검사 명령:

```powershell
$touchArgs = @('-std=c++17','-fsyntax-only','-Wall','-Wextra','-DQT_NO_DEBUG','-Iapp','-IC:/Qt/6.10.3/mingw_64/include','-IC:/Qt/6.10.3/mingw_64/include/QtCore','-IC:/Qt/6.10.3/mingw_64/include/QtGui','-IC:/Qt/6.10.3/mingw_64/include/QtQml','-IC:/Qt/6.10.3/mingw_64/include/QtNetwork','-IC:/Qt/6.10.3/mingw_64/include/QtQuick','-IC:/Qt/6.10.3/mingw_64/include/QtSvg','-Ilibs/windows/include','-Ilibs/windows/include/x64','-Ilibs/windows/include/x64/SDL2','-Imoonlight-common-c/moonlight-common-c/src','-Iqmdnsengine/qmdnsengine/src/include','-Iqmdnsengine')
& C:/msys64/ucrt64/bin/g++.exe @touchArgs app/streaming/input/desktoptouch.cpp app/streaming/input/input.cpp app/streaming/input/keyboard.cpp app/streaming/input/mouse.cpp
```

- 이 호출의 완료 기록: **exit_code 0, 출력 없음**.
- 이후 직접 다중 터치 좌표 누적 변경 뒤 `desktoptouch.cpp`만 다시 검사했습니다. 명령은 위 배열에서 `QtQuick`, `QtSvg` 경로를 뺀 나머지 동일 인자와 아래 파일을 사용했습니다.

```powershell
& C:/msys64/ucrt64/bin/g++.exe @touchArgs app/streaming/input/desktoptouch.cpp
```

- 최종 단일 파일 검사에는 진단 출력이 없었습니다. 이 검사 및 Python 시험·`git diff --check`가 포함된 도구 호출 종료 코드는 **0**입니다. 단일 파일 검사 직후 개별 `$LASTEXITCODE`를 별도로 출력하지 않은 점을 구분합니다.
- `-fsyntax-only`이므로 `.obj`, 라이브러리, 앱 실행 파일을 생성하지 않았습니다. 아티팩트 경로: **없음**.
- 초기 시도에는 누락된 include 경로 때문에 `qmdnsengine_export.h`, `opus_multistream.h`를 못 찾는 **종료 코드 1**이 있었습니다. 기존 폴더의 include 경로를 추가해 위 검사에서 해결했습니다. 초기 함수 포인터 변환 경고도 코드 수정 후 최종 검사에는 나오지 않았습니다.
- 이 검사는 MinGW GCC와 설치된 Qt 6.10.3 헤더 기준입니다. 정식 MSVC/Qt CI 환경, `session.cpp`, QML 렌더링, 링크·배포·실제 장치 작동을 검증한 결과가 아닙니다.

### 4. 변경 파일 형식 검사와 미실행 범위

```powershell
git diff --check
```

최종 검증 호출 종료 코드 **0**. 공백 오류는 없었으며 Git의 기존 줄바꿈 정책 안내가 출력되었습니다:

```text
warning: in the working copy of 'app/gui/SettingsView.qml', LF will be replaced by CRLF the next time Git touches it
```

`scripts/test-desktop-touch.bat`와 GitHub Actions 연결은 **파일만 작성**했으며 로컬 MSVC 배치나 CI를 실행하지 않았습니다. Moonlight 전체 빌드·링크·패키징, Moonlight/OSK 실행, 설치, 서비스·디스플레이·레지스트리 설정 변경, 커밋·push는 모두 미실행입니다. 기존 작업 범위를 지켜 이 증거도 Moonlight 저장소 내부 문서에만 기록했습니다.
