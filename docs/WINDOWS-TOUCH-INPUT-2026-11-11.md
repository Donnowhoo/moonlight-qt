# 갤럭시북 다중 터치 무반응: 입력 경로 수정과 진단

## 상태

David가 Windows 세/네 손가락 시스템 제스처를 끈 뒤 작업 보기는 사라졌지만, Moonlight의 두/세 손가락은 여전히 무반응이라고 확인했다. 한 손가락 클릭과 바는 동작한다. 직접 터치 모드, 갤럭시북 단독 화면이다.

**전체 증상의 원인은 아직 확정되지 않았다.** 아래 확인된 오류 수정과 진단을 준비했고, David가 이번 내용을 dual-monitor에 commit/push하여 Windows x64만 빌드하도록 승인했다. 빌드 결과는 후속 기록으로 갱신한다. Sunshine과 Android 소스 변경 없음. 앱 실행·설치·갤럭시북 실기는 David가 직접 한다.

## 확인하고 수정한 오류

바의 WM_TOUCH 처리 뒤에 오는 마우스 호환 메시지를 걸러내는 조건이 잘못됐다.

- 기존: `(extraInfo & 0xFFFFFF80) == 0xFF515700`
- 터치 입력은 bit 7이 1이므로 이 조건을 통과해 일반 마우스 분기로 들어간다. 그 분기는 `SetCapture`와 `pointer()`를 호출한다. 이미 처리한 접촉을 다시 처리하거나 바가 마우스 캡처를 가져갈 수 있다.
- 수정: 하위 8비트를 제외한 `0xFFFFFF00` 마스크로 터치/펜 호환 메시지를 모두 제외한다. WM_TOUCH에서 실제 접촉을 처리하며 진짜 마우스는 계속 받는다.
- `touchpolicy.h`의 순수 함수로 분리하고 터치/펜 ID 각각 128개 및 일반 마우스 값을 검사한다.
- **이 오류가 갤럭시북 영상 영역의 다중 터치 무반응까지 설명하는지는 실기 로그가 필요하다. 수정 완료와 증상 해결을 혼동하지 않는다.**

근거: [Microsoft 입력 구분 문서](https://learn.microsoft.com/en-us/windows/win32/tablet/system-events-and-mouse-messages).

## 새 진단

기능을 켠 Windows 세션에서만 `Desktop touch input:` 로그가 추가된다.

- 시작: SDL 버전, 바/트랙패드/직접 모드, 스트림 창의 터치 등록 여부.
- 장치 종류 필터 **이전**의 DOWN/MOTION/UP 수, 거부된 장치 수, 펜 우회 및 비활성 경로 수.
- 최대 동시 접촉 수, 휠/드래그/우클릭 출력 수, 취소 수와 마지막 이유.
- 취소 이유: 포커스/초기화, 입력 미활성, 실제 마우스, 바 접촉, 바 위치 변경, 펜, 명시적 바 동작.
- 5초 간격, 변화가 있을 때만 최대 64회 요약 + 종료 시 최종 요약. 좌표, 입력 글자, 장치 ID, 보안 정보는 기록하지 않는다.
- 숫자는 세션 누적값이다. 종료 중 reset으로 마지막 취소 사유가 달라질 수 있으므로 시험 직후의 주기 로그도 함께 본다.

SDL 3.4.18 Windows 소스는 스트림 창을 생성할 때 이미 RegisterTouchWindow를 호출한다. 따라서 근거 없이 중복 등록하거나 별도 Win32 입력 후크로 교체하지 않았다. 참고: https://raw.githubusercontent.com/libsdl-org/SDL/release-3.4.18/src/video/windows/SDL_windowswindow.c

## 검증

- 기존 설치된 g++로 OS/네트워크 호출 없는 순수 C++ 정책 테스트: **38개 통과**, 종료 코드 0.
- Python 정적 보호 검사: **11개 통과**, 종료 코드 0.
- desktoptouch.cpp, input.cpp의 `-fsyntax-only -Wall -Wextra` 검사: 성공. 앱 실행 파일은 만들거나 실행하지 않았다.
- 전체 MSVC 앱 빌드, 링크, 갤럭시북 실기: **아직 미검증**.

수정 파일: app/streaming/input/desktoptouch.cpp/.h, input.cpp, touchpolicy.h, tests/desktop_touch_policy_test.cpp, tests/test_desktop_touch_guards.py.

## David가 새 빌드에서 할 시험

1. 승인 후 제공될 x64 포터블을 갤럭시북에서 실행한다. 기존 파일로는 이번 수정/진단을 시험할 수 없다.
2. 시스템 터치 제스처 OFF 상태로 바를 만지기 전 영상 가운데에서 두 손가락 스크롤, 세 손가락 이동을 각각 한다.
3. 바의 더보기를 터치한 뒤 같은 시험을 반복한다. 손가락을 떼면 바로 멈추는지도 확인한다.
4. 안 되면 5초 기다린 뒤 갤럭시북의 `%TEMP%\Moonlight-*.log` 중 이번 실행 로그에서 `Desktop touch input:` 줄을 전달한다. 전체 로그/개인 정보는 필요 없다. 입력이 전혀 도착하지 않은 경우는 스트림을 종료한 뒤 최종 요약을 확인한다.
5. down=0이면 SDL 도착 전 경로, rejectedDevice 증가면 장치 필터, maxContacts=1이면 동시 접촉 전달, maxContacts>=2인데 출력이 없으면 취소 사유/제스처 상태 순으로 조사한다. 출력이 있는데 호스트가 반응하지 않으면 그때 전송/호스트 처리 경로를 조사한다.
