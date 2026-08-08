#include "SMTCManager.h"
#include <QDebug>
#include <QTimer>
#include <QFile>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wrl/client.h>
#include <wrl/wrappers/corewrappers.h>
#include <wrl/event.h>
#include <roapi.h>
#include <systemmediatransportcontrolsinterop.h>
#pragma warning(push)
#pragma warning(disable: 4467)
#include <Windows.Media.h>
#include <Windows.Storage.Streams.h>
#pragma warning(pop)

using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::Media;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Storage::Streams;

// ── SMTC 내부 구현체 ──────────────────────────────────────────────────────────
struct SMTCImpl {
    ComPtr<ISystemMediaTransportControls>               smtc;
    ComPtr<ISystemMediaTransportControls2>              smtc2;   // 타임라인용
    ComPtr<ISystemMediaTransportControlsDisplayUpdater> updater;
    ComPtr<IMusicDisplayProperties>                     musicProps;
    EventRegistrationToken                              btnToken{};
};
#endif

// ── 생성자/소멸자 ──────────────────────────────────────────────────────────────
SMTCManager::SMTCManager(QObject* parent)
    : QObject(parent)
{
    timelineTimer_ = new QTimer(this);
    timelineTimer_->setInterval(1000);  // 1초마다 타임라인 업데이트
    connect(timelineTimer_, &QTimer::timeout, this, [this]() {
        if (initialized_ && enabled_)
            updateTimeline(currentPos_, currentDur_);
    });
}

SMTCManager::~SMTCManager() {
    cleanup();
    // 임시 앨범아트 파일 삭제
    if (!albumArtTempPath_.isEmpty() && QFile::exists(albumArtTempPath_)) {
        QFile::remove(albumArtTempPath_);
    }
}

// ── 초기화 ──────────────────────────────────────────────────────────────────
bool SMTCManager::initialize(void* hwnd) {
#ifdef Q_OS_WIN
    if (!hwnd) return false;

    HRESULT hr = RoInitialize(RO_INIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        qWarning() << "[SMTC] WinRT 초기화 실패";
        return false;
    }

    ComPtr<ISystemMediaTransportControlsInterop> interop;
    hr = RoGetActivationFactory(
        HStringReference(RuntimeClass_Windows_Media_SystemMediaTransportControls).Get(),
        IID_PPV_ARGS(&interop)
    );
    if (FAILED(hr)) {
        qWarning() << "[SMTC] Interop 팩토리 획득 실패";
        return false;
    }

    auto* impl = new SMTCImpl();
    hr = interop->GetForWindow(
        reinterpret_cast<HWND>(hwnd),
        IID_PPV_ARGS(&impl->smtc)
    );
    if (FAILED(hr)) {
        qWarning() << "[SMTC] GetForWindow 실패";
        delete impl;
        return false;
    }

    // ISystemMediaTransportControls2 QI (타임라인용)
    hr = impl->smtc.As(&impl->smtc2);
    if (FAILED(hr)) {
        qInfo() << "[SMTC] ISystemMediaTransportControls2 미지원 (타임라인 비활성)";
        impl->smtc2 = nullptr;
    }

    impl->smtc->put_IsPlayEnabled(true);
    impl->smtc->put_IsPauseEnabled(true);
    impl->smtc->put_IsNextEnabled(true);
    impl->smtc->put_IsPreviousEnabled(true);
    impl->smtc->put_IsStopEnabled(true);

    // 버튼 이벤트 핸들러
    SMTCManager* self = this;
    auto handler = Callback<ITypedEventHandler<
        SystemMediaTransportControls*,
        SystemMediaTransportControlsButtonPressedEventArgs*>>(
        [self](ISystemMediaTransportControls*,
               ISystemMediaTransportControlsButtonPressedEventArgs* args) -> HRESULT {
            SystemMediaTransportControlsButton btn;
            args->get_Button(&btn);
            QMetaObject::invokeMethod(self, [self, btn]() {
                switch (btn) {
                case SystemMediaTransportControlsButton_Play:
                    emit self->playRequested(); break;
                case SystemMediaTransportControlsButton_Pause:
                    emit self->pauseRequested(); break;
                case SystemMediaTransportControlsButton_Stop:
                    emit self->stopRequested(); break;
                case SystemMediaTransportControlsButton_Next:
                    emit self->nextRequested(); break;
                case SystemMediaTransportControlsButton_Previous:
                    emit self->previousRequested(); break;
                default: break;
                }
            }, Qt::QueuedConnection);
            return S_OK;
        });

    hr = impl->smtc->add_ButtonPressed(handler.Get(), &impl->btnToken);
    if (FAILED(hr)) {
        qWarning() << "[SMTC] ButtonPressed 핸들러 등록 실패";
        delete impl;
        return false;
    }

    hr = impl->smtc->get_DisplayUpdater(&impl->updater);
    if (FAILED(hr)) {
        qWarning() << "[SMTC] DisplayUpdater 획득 실패";
        delete impl;
        return false;
    }

    impl->updater->put_Type(MediaPlaybackType_Music);
    impl->updater->get_MusicProperties(&impl->musicProps);

    smtcPtr_ = impl;
    initialized_ = true;
    enabled_ = true;
    qInfo() << "[SMTC] 초기화 완료 (타임라인:" << (impl->smtc2 ? "지원" : "미지원") << ")";
    return true;
#else
    Q_UNUSED(hwnd)
    return false;
#endif
}

// ── 메타데이터 업데이트 (앨범아트 임시 파일 경유 방식) ──────────────────────
void SMTCManager::updateMetadata(const QString& title,
                                  const QString& artist,
                                  const QString& album,
                                  const QPixmap& albumArt) {
#ifdef Q_OS_WIN
    if (!initialized_ || !smtcPtr_) return;
    auto* impl = static_cast<SMTCImpl*>(smtcPtr_);

    // 제목/아티스트/앨범 업데이트
    if (impl->musicProps) {
        impl->musicProps->put_Title(
            HStringReference(reinterpret_cast<const wchar_t*>(title.utf16())).Get());
        impl->musicProps->put_Artist(
            HStringReference(reinterpret_cast<const wchar_t*>(artist.utf16())).Get());
        impl->musicProps->put_AlbumArtist(
            HStringReference(reinterpret_cast<const wchar_t*>(album.utf16())).Get());
    }

    // ── 앨범아트 썸네일: 임시 파일 경유 방식 ────────────────────────────────
    // WinRT IRandomAccessStream 직접 생성 대신 임시 PNG 파일 저장 후
    // RandomAccessStreamReference::CreateFromUri() 사용
    if (!albumArt.isNull() && impl->updater) {
        // 이전 임시 파일 삭제
        if (!albumArtTempPath_.isEmpty() && QFile::exists(albumArtTempPath_)) {
            QFile::remove(albumArtTempPath_);
        }

        // 임시 PNG 파일 저장
        QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        albumArtTempPath_ = tempDir + "\\sorinuri_smtc_art.png";
        if (albumArt.save(albumArtTempPath_, "PNG")) {
            // file:// URI 생성 (Windows 경로 → URI 변환)
            QString fileUri = "file:///" + albumArtTempPath_.replace('\\', '/');

            ComPtr<IUriRuntimeClassFactory> uriFactory;
            HRESULT hr = RoGetActivationFactory(
                HStringReference(RuntimeClass_Windows_Foundation_Uri).Get(),
                IID_PPV_ARGS(&uriFactory));

            if (SUCCEEDED(hr)) {
                ComPtr<IUriRuntimeClass> uri;
                hr = uriFactory->CreateUri(
                    HStringReference(reinterpret_cast<const wchar_t*>(fileUri.utf16())).Get(),
                    &uri);

                if (SUCCEEDED(hr)) {
                    ComPtr<IRandomAccessStreamReferenceStatics> streamRefStatics;
                    hr = RoGetActivationFactory(
                        HStringReference(RuntimeClass_Windows_Storage_Streams_RandomAccessStreamReference).Get(),
                        IID_PPV_ARGS(&streamRefStatics));

                    if (SUCCEEDED(hr)) {
                        ComPtr<IRandomAccessStreamReference> streamRef;
                        hr = streamRefStatics->CreateFromUri(uri.Get(), &streamRef);
                        if (SUCCEEDED(hr)) {
                            impl->updater->put_Thumbnail(streamRef.Get());
                            qInfo() << "[SMTC] 앨범아트 썸네일 설정 완료";
                        } else {
                            qWarning() << "[SMTC] CreateFromUri 실패:" << hr;
                        }
                    }
                }
            }
        }
    }

    if (impl->updater) impl->updater->Update();
    qInfo() << "[SMTC] 메타데이터 업데이트:" << title << "-" << artist;
#else
    Q_UNUSED(title) Q_UNUSED(artist) Q_UNUSED(album) Q_UNUSED(albumArt)
#endif
}

// ── 재생 상태 업데이트 ──────────────────────────────────────────────────────
void SMTCManager::setPlaying(bool playing) {
#ifdef Q_OS_WIN
    if (!initialized_ || !smtcPtr_) return;
    auto* impl = static_cast<SMTCImpl*>(smtcPtr_);
    impl->smtc->put_PlaybackStatus(
        playing ? MediaPlaybackStatus_Playing : MediaPlaybackStatus_Paused
    );
    if (playing) timelineTimer_->start();
    else timelineTimer_->stop();
#else
    Q_UNUSED(playing)
#endif
}

void SMTCManager::setStopped() {
#ifdef Q_OS_WIN
    if (!initialized_ || !smtcPtr_) return;
    auto* impl = static_cast<SMTCImpl*>(smtcPtr_);
    impl->smtc->put_PlaybackStatus(MediaPlaybackStatus_Stopped);
    timelineTimer_->stop();
#endif
}

// ── 타임라인 업데이트 (ISystemMediaTransportControls2) ──────────────────────
void SMTCManager::updateTimeline(double position, double duration) {
    currentPos_ = position;
    currentDur_ = duration;

#ifdef Q_OS_WIN
    if (!initialized_ || !smtcPtr_) return;
    auto* impl = static_cast<SMTCImpl*>(smtcPtr_);
    if (!impl->smtc2) return;  // ISystemMediaTransportControls2 미지원

    // SystemMediaTransportControlsTimelineProperties 생성
    // IActivationFactory를 통해 기본 인스턴스 생성
    ComPtr<IInspectable> timelineInspectable;
    HRESULT hr = RoActivateInstance(
        HStringReference(RuntimeClass_Windows_Media_SystemMediaTransportControlsTimelineProperties).Get(),
        &timelineInspectable);
    if (FAILED(hr)) return;

    ComPtr<ISystemMediaTransportControlsTimelineProperties> timelineProps;
    hr = timelineInspectable.As(&timelineProps);
    if (FAILED(hr)) return;

    // 100나노초 단위 (Windows TimeSpan)
    auto secToTimeSpan = [](double sec) -> ABI::Windows::Foundation::TimeSpan {
        ABI::Windows::Foundation::TimeSpan ts;
        ts.Duration = static_cast<INT64>(sec * 10000000.0);
        return ts;
    };

    double safeDur = (duration > 0) ? duration : 0.0;
    double safePos = (position >= 0 && position <= safeDur) ? position : 0.0;

    timelineProps->put_StartTime(secToTimeSpan(0.0));
    timelineProps->put_EndTime(secToTimeSpan(safeDur));
    timelineProps->put_Position(secToTimeSpan(safePos));
    timelineProps->put_MinSeekTime(secToTimeSpan(0.0));
    timelineProps->put_MaxSeekTime(secToTimeSpan(safeDur));

    hr = impl->smtc2->UpdateTimelineProperties(timelineProps.Get());
    if (FAILED(hr)) {
        // 타임라인 미지원 환경에서는 조용히 무시
        impl->smtc2 = nullptr;
    }
#else
    Q_UNUSED(position) Q_UNUSED(duration)
#endif
}

// ── SMTC 활성화/비활성화 ──────────────────────────────────────────────────
void SMTCManager::setEnabled(bool enabled) {
    enabled_ = enabled;
#ifdef Q_OS_WIN
    if (!initialized_ || !smtcPtr_) return;
    auto* impl = static_cast<SMTCImpl*>(smtcPtr_);
    if (!enabled) {
        impl->smtc->put_PlaybackStatus(MediaPlaybackStatus_Closed);
        timelineTimer_->stop();
    }
#endif
}

// ── 정리 ──────────────────────────────────────────────────────────────────
void SMTCManager::cleanup() {
#ifdef Q_OS_WIN
    if (smtcPtr_) {
        auto* impl = static_cast<SMTCImpl*>(smtcPtr_);
        if (impl->smtc && impl->btnToken.value) {
            impl->smtc->remove_ButtonPressed(impl->btnToken);
        }
        delete impl;
        smtcPtr_ = nullptr;
    }
    initialized_ = false;
#endif
}
