# Windows 터치 로그 부재 재조사 — 진단 보완판

## 승인된 배포 결과 (2026-11-11 한국 시간)

- 기능 커밋 `78467bfb73b2b8d6c6849846e933d8a8da5c5f9c`, dual-monitor commit/push 완료.
- [GitHub Actions 42847760460](https://github.com/Donnowhoo/moonlight-qt/actions/runs/42847760460) 성공. Windows x64만 빌드. MSVC 입력 정책 38개, Python 보호 검사 13개, 독립 진단 파일 테스트(즉시 flush/한글 경로/용량 제한/열기 실패) 모두 통과.
- 릴리스 `dual-monitor-latest` targetCommitish가 위 기능 커밋과 일치하는 것 확인.
- 파일: `C:\Users\Han\OneDrive\Desktop\RemoteControl\듀얼모니터_빌드\Moonlight-Touch-Diagnostic-Windows-x64-78467bfb-20261111-portable.zip`
- 크기 38,209,676 bytes. SHA256 `0552571fbcff51fbb8b9e58d19835e35ba9d0e45eb1dc9c9cfd1192444375b30` (GitHub asset digest 일치).
- ZIP 전체 CRC 성공. Moonlight.exe, SDL2.dll, SDL3.dll, SDL2_ttf.dll, vcruntime140.dll, vcruntime140_1.dll, msvcp140.dll 포함 및 모두 PE x64 확인. 새 진단 문자열 포함 확인. **DLL 로드나 앱 실행 없이 읽기만 했다.**
- CI 증거: 프로젝트 루트 `evidence/touch-build-2026-11-11/windows-78467bfb-tests.txt`.
- 결과 문서만 후속 `[skip ci]` 커밋으로 게시한다. 다운로드 파일 소스는 위 기능 커밋이며, 갤럭시북 문제 해결은 아직 미검증이다.

### David가 할 일

1. 갤럭시북의 새 폴더에 ZIP **전체**를 압축 해제한다. Moonlight.exe만 따로 이동하지 않는다. 함께 있는 SDL DLL/Qt 폴더를 유지한다.
2. 새 폴더의 Moonlight.exe로 접속하고 두/세 손가락 동작을 시도한다.
3. 바에서 **더보기 → 진단 로그**를 누르고 열린 파일을 채팅에 첨부한다. 그 버튼이 없으면 우선 그 사실을 알려준다.
4. 버튼으로 열리지 않으면 `%TEMP%`의 `Moonlight-touch-input-*.log` 중 이번 시각 파일을 첨부한다. 기존 `Moonlight-*.log`와 별개의 진단이다.
5. SDL 오류가 갤럭시북에서도 뜨면 정확한 문구/파일명을 알려준다. 호스트에는 SDL 설치나 Sunshine 재설치가 필요 없다.

## 사용자 확인과 남은 문제

David는 `7de2df76` 새 파일로 실행했음을 확인했다. 두/세 손가락 무반응 지속, 한 손가락 이동은 드래그, 일반 로그에서 `Desktop touch input:`이 보이지 않는다. 다른 실행 파일을 사용했다고 추정하지 않는다.

**다중 터치 무반응의 원인은 아직 확정하지 못했다. 이 변경을 제스처 해결이라고 보고하면 안 된다.** 로그 기록의 누락 경로를 보완하고 입력 경계별 관측을 준비했다. David가 이번 진단 보완판의 dual-monitor commit/push, Windows x64 빌드와 다운로드를 승인했다. 실행과 실기는 David가 직접 한다.

David가 호스트 PC에서 SDL 3이 없다는 취지의 오류 창을 봤다고 추가 확인했다(정확한 문구는 기억하지 못함). 앞선 독립 DLL 로드 검사 실패와 관련됐을 가능성이 있으나 문구/발생 시각 대조는 하지 못했다. 해당 로드 검사는 반복하지 않는다. 이전 배포 ZIP에는 SDL2.dll/SDL3.dll/SDL2_ttf.dll과 VC 런타임이 포함됨을 정적 확인했다. 호스트에 SDL을 설치하지 않는다.

## 코드로 확인한 로그 누락 경로

1. 기존 전용 문구는 `DesktopTouchController` 생성의 마지막에만 출력했다. 기능 옵션이 꺼짐/초기화 실패/원래 입력 경로로 복귀하면 문구가 없었다. 이전 "없으면 새 파일이 아니다" 식 판단은 근거가 부족했다.
2. `main.cpp`는 Windows stderr가 이미 지정되어 있으면 일반 로그 파일을 열지 않는다. 지정되지 않은 경우에도 파일 열기 실패 때 stderr 경로가 유지된다. SDL 로그 우선순위 필터도 거친다.
3. `Path::initialize(true)`는 로그를 현재 작업 디렉터리에 놓고, 일반 모드는 임시 폴더를 사용한다. 따라서 `%TEMP%`의 최신 일반 로그 하나만으로 이번 실행을 확정할 수 없다.
4. 기존 주기 기록은 SDL 손가락 이벤트 또는 제스처 출력으로 dirty가 된 때만 나왔다. SDL에 전달되기 전 차단되는 입력은 도착 유무를 구별할 수 없었다.

위는 실제 소스의 조건이다. **David의 기기에서 어느 조건이 발생했는지는 아직 미확인**이다. 로컬 SDL DLL만 로드해 로거를 확인하려던 독립 검사는 WinError 1114로 실패했으며 로거 동작 증거로 사용하지 않았다. 앱/서비스를 실행하거나 재시작하지 않았다.

## 로컬 수정

- Windows 입력 핸들러의 `setWindow()`에서 기능 분기 전에 별도 파일을 연다: `%TEMP%\Moonlight-touch-input-<프로세스번호>-<시각>.log`.
- SDL/Qt 일반 로그 필터와 stderr를 거치지 않고 파일에 직접 기록·flush한다. 옵션 꺼짐, 초기화 단계 실패, 실행 버전도 남긴다.
- 파일당 최대 256줄, 줄당 2048바이트. 접촉 좌표/장치 ID/입력 글자/접속 정보는 기록하지 않는다.
- 기존 앱 측 접촉 카운터에 앞서 SDL Windows 메시지 관찰 API로 스트림 창의 WM_TOUCH DOWN/MOVE/UP와 동시 접촉 수, 읽기 오류, WM_POINTER 메시지 수를 센다. 관찰 콜백이 얼마나 호출됐는지도 기록한다.
- 이 관찰자는 메시지를 소비하거나 손가락을 합성하지 않으며, 터치 핸들을 닫지 않는다. SDL이 원래 처리를 계속한다. 종료 때 콜백을 해제한다. Windows 설정이나 시스템 전역 후크를 변경하지 않는다.
- 바의 **더보기 → 진단 로그**로 정확히 이 세션의 파일을 연다. 임의의 오래된 로그를 찾을 필요가 없다. 파일 생성 실패 시 버튼에 실패를 표시한다.
- 종료 직전 상태를 reset보다 먼저 기록한다. 기존에는 reset 뒤 기록해 마지막 취소 사유와 상태를 덮어쓸 수 있었다.
- 제스처 임계값이나 터치 입력 전달 경로를 추측으로 교체하지 않았다. 기존 한 손가락 직접 드래그 동작도 유지한다.

## 검증

- 독립 C++ 진단 파일 테스트: 실행 중 flush, 한글 경로, 길이/줄 수 제한, 미오픈/열기 실패 처리 통과. Windows/SDL/Qt 창이나 네트워크 없이 workspace 내부 시험 파일만 사용.
- Python 정적 보호 검사 13개 통과.
- 변경 C++의 g++ `-fsyntax-only` 검사 통과. 후속 GitHub MSVC 앱 빌드/링크도 성공. 실제 Galaxy Book 관측은 미실시.
- GitHub x64 테스트 배치에 새 독립 파일 테스트를 연결했고 위 승인된 CI에서 통과했다.

## 다음 관측의 해석

| 기록 | 다음 조사 지점 |
|---|---|
| controller active=0 | 옵션/초기화 단계, 기존 입력으로 돌아간 이유 |
| native maxContacts>=2, 앱 maxContacts<2 | Windows→SDL 변환 또는 장치 필터 |
| 앱 maxContacts>=2, wheel/drag=0 | ready/blocked/physical/toolbar 취소 이유 및 상태 전환 |
| wheel/drag>0, 호스트 무반응 | 이벤트 전송과 대상 앱/호스트 입력 처리 |
| native=0, pointerMessages>0 | WM_POINTER 경로 확인(메시지 수 자체는 손가락 수가 아님) |
| native=0, observedMessages=0 | 관찰 콜백 경로부터 확인. Windows가 터치를 안 보냈다고 단정 금지 |

승인된 새 빌드가 마련되면 David는 갤럭시북에서 두/세 손가락을 시도하고 **더보기 → 진단 로그**로 열린 파일을 전달한다. 이 실제 입력 증거로 다음 기능 수정을 결정한다. 서비스/설치/디스플레이 변경은 하지 않는다.

근거: [SDL Windows 메시지 관찰 API](https://wiki.libsdl.org/SDL2/SDL_SetWindowsMessageHook), [Microsoft GetTouchInputInfo](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-gettouchinputinfo).
