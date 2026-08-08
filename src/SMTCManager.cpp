#include "SMTCManager.h"
#include <QDebug>
#include <QTimer>

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
#pragma warning(pop)

using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::Media;
using namespace ABI::Windows::Foundation;

// SMTC 내부 구현체 (앨범아트 스트림 제외 - 안정성 우선)
struct SMTCImpl {
    ComPtr<ISystemMediaTransportControls>               smtc;
    ComPtr<ISystemMediaTransportControlsDisplayUpdater> updater;
    ComPtr<IMusicDisplayProperties>                     musicProps;
    EventRegistrationToken                              btnToken{};
};
#endif

SMTCManager::SMTCManager(QObject* parent)
    : QObject(parent)
{
    timelineTimer_ = new QTimer(this);
    timelineTimer_->setInterval(5000);
    connect(timelineTimer_, &QTimer::timeout, this, [this]() {
        if (initialized_ && enabled_)
            updateTimeline(currentPos_, currentDur_);
    });
}

SMTCManager::~SMTCManager() {
    cleanup();
}

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

    qInfo() << "[SMTC] 초기화 완료";
    return true;
#else
    Q_UNUSED(hwnd)
    return false;
#endif
}

void SMTCManager::updateMetadata(const QString& title,
                                  const QString& artist,
                                  const QString& album,
                                  const QPixmap& /*albumArt*/) {
    // 앨범아트: WinRT IRandomAccessStream 처리 복잡도로 인해 v6.14.0에서는 생략
    // 제목/아티스트/앨범만 업데이트 (안정성 우선)
#ifdef Q_OS_WIN
    if (!initialized_ || !smtcPtr_) return;
    auto* impl = static_cast<SMTCImpl*>(smtcPtr_);

    if (impl->musicProps) {
        impl->musicProps->put_Title(
            HStringReference(reinterpret_cast<const wchar_t*>(title.utf16())).Get());
        impl->musicProps->put_Artist(
            HStringReference(reinterpret_cast<const wchar_t*>(artist.utf16())).Get());
        impl->musicProps->put_AlbumArtist(
            HStringReference(reinterpret_cast<const wchar_t*>(album.utf16())).Get());
    }

    if (impl->updater) impl->updater->Update();
    qInfo() << "[SMTC] 메타데이터 업데이트:" << title << "-" << artist;
#else
    Q_UNUSED(title) Q_UNUSED(artist) Q_UNUSED(album)
#endif
}

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

void SMTCManager::updateTimeline(double position, double duration) {
    currentPos_ = position;
    currentDur_ = duration;
    // 타임라인 업데이트: ISystemMediaTransportControls2 인터페이스 필요
    // v6.14.0에서는 재생 상태만 업데이트 (안정성 우선)
    // 향후 ISystemMediaTransportControls2::UpdateTimelineProperties 구현 예정
    Q_UNUSED(position) Q_UNUSED(duration)
}

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
