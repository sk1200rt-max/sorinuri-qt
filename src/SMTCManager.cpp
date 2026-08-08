#include "SMTCManager.h"
#include <QDebug>
#include <QBuffer>
#include <QTimer>

#ifdef Q_OS_WIN
// Windows WinRT 헤더 (MSVC 전용)
// ISystemMediaTransportControlsInterop: Win32 데스크탑 앱에서 SMTC 사용
#include <windows.h>
#include <systemmediatransportcontrolsinterop.h>
#include <Windows.Media.h>
#include <wrl/client.h>
#include <wrl/wrappers/corewrappers.h>
#include <wrl/event.h>
#include <robuffer.h>
#include <shcore.h>

using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::Media;
using namespace ABI::Windows::Storage::Streams;

// SMTC 내부 구현체
struct SMTCImpl {
    ComPtr<ISystemMediaTransportControls>              smtc;
    ComPtr<ISystemMediaTransportControlsDisplayUpdater> updater;
    ComPtr<IMusicDisplayProperties>                    musicProps;
    EventRegistrationToken                             btnToken{};
    EventRegistrationToken                             propToken{};
    bool initialized = false;
};
#endif

SMTCManager::SMTCManager(QObject* parent)
    : QObject(parent)
{
    timelineTimer_ = new QTimer(this);
    timelineTimer_->setInterval(5000);  // 5초 주기 타임라인 업데이트
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

    // WinRT 초기화
    HRESULT hr = RoInitialize(RO_INIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        qWarning() << "[SMTC] WinRT 초기화 실패:" << hr;
        return false;
    }

    // ISystemMediaTransportControlsInterop 팩토리 획득
    // Win32 데스크탑 앱에서 HWND 기반으로 SMTC 인스턴스 생성
    ComPtr<ISystemMediaTransportControlsInterop> interop;
    hr = RoGetActivationFactory(
        HStringReference(RuntimeClass_Windows_Media_SystemMediaTransportControls).Get(),
        IID_PPV_ARGS(&interop)
    );
    if (FAILED(hr)) {
        qWarning() << "[SMTC] ISystemMediaTransportControlsInterop 팩토리 획득 실패:" << hr;
        return false;
    }

    // HWND로 SMTC 인스턴스 획득
    auto* impl = new SMTCImpl();
    hr = interop->GetForWindow(
        reinterpret_cast<HWND>(hwnd),
        IID_PPV_ARGS(&impl->smtc)
    );
    if (FAILED(hr)) {
        qWarning() << "[SMTC] GetForWindow 실패:" << hr;
        delete impl;
        return false;
    }

    // 버튼 활성화
    impl->smtc->put_IsPlayEnabled(true);
    impl->smtc->put_IsPauseEnabled(true);
    impl->smtc->put_IsNextEnabled(true);
    impl->smtc->put_IsPreviousEnabled(true);
    impl->smtc->put_IsStopEnabled(true);

    // 버튼 이벤트 핸들러 등록
    auto* self = this;
    hr = impl->smtc->add_ButtonPressed(
        Callback<ITypedEventHandler<SystemMediaTransportControls*,
            SystemMediaTransportControlsButtonPressedEventArgs*>>(
            [self](ISystemMediaTransportControls*,
                   ISystemMediaTransportControlsButtonPressedEventArgs* args) -> HRESULT {
                SystemMediaTransportControlsButton btn;
                args->get_Button(&btn);
                // Qt 스레드로 마샬링
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
            }).Get(),
        &impl->btnToken
    );
    if (FAILED(hr)) {
        qWarning() << "[SMTC] ButtonPressed 핸들러 등록 실패:" << hr;
        delete impl;
        return false;
    }

    // DisplayUpdater 획득
    hr = impl->smtc->get_DisplayUpdater(&impl->updater);
    if (FAILED(hr)) {
        qWarning() << "[SMTC] DisplayUpdater 획득 실패:" << hr;
        delete impl;
        return false;
    }

    // 음악 타입으로 설정
    impl->updater->put_Type(MediaPlaybackType_Music);
    impl->updater->get_MusicProperties(&impl->musicProps);

    smtcPtr_ = impl;
    initialized_ = true;
    enabled_ = true;

    qInfo() << "[SMTC] 초기화 완료 - Windows 잠금 화면 미디어 컨트롤 활성화";
    return true;
#else
    Q_UNUSED(hwnd)
    return false;
#endif
}

void SMTCManager::updateMetadata(const QString& title,
                                  const QString& artist,
                                  const QString& album,
                                  const QPixmap& albumArt) {
#ifdef Q_OS_WIN
    if (!initialized_ || !smtcPtr_) return;
    auto* impl = static_cast<SMTCImpl*>(smtcPtr_);

    // 제목, 아티스트, 앨범 업데이트
    if (impl->musicProps) {
        impl->musicProps->put_Title(HStringReference(title.toStdWString().c_str()).Get());
        impl->musicProps->put_Artist(HStringReference(artist.toStdWString().c_str()).Get());
        impl->musicProps->put_AlbumTitle(HStringReference(album.toStdWString().c_str()).Get());
    }

    // 앨범아트 썸네일 업데이트
    if (!albumArt.isNull() && impl->updater) {
        // QPixmap → PNG 바이트 배열 → IRandomAccessStream
        QByteArray imgData;
        QBuffer buf(&imgData);
        buf.open(QIODevice::WriteOnly);
        albumArt.scaled(300, 300, Qt::KeepAspectRatio, Qt::SmoothTransformation)
                .save(&buf, "PNG");
        buf.close();

        // IBuffer 생성
        ComPtr<IBufferFactory> bufFactory;
        if (SUCCEEDED(RoGetActivationFactory(
                HStringReference(RuntimeClass_Windows_Storage_Streams_Buffer).Get(),
                IID_PPV_ARGS(&bufFactory)))) {
            ComPtr<IBuffer> buffer;
            bufFactory->Create(static_cast<UINT32>(imgData.size()), &buffer);
            buffer->put_Length(static_cast<UINT32>(imgData.size()));

            ComPtr<Windows::Storage::Streams::IBufferByteAccess> byteAccess;
            buffer.As(&byteAccess);
            BYTE* bytes = nullptr;
            byteAccess->Buffer(&bytes);
            if (bytes) memcpy(bytes, imgData.constData(), imgData.size());

            // InMemoryRandomAccessStream 생성
            ComPtr<IInMemoryRandomAccessStreamFactory> streamFactory;
            if (SUCCEEDED(RoGetActivationFactory(
                    HStringReference(RuntimeClass_Windows_Storage_Streams_InMemoryRandomAccessStream).Get(),
                    IID_PPV_ARGS(&streamFactory)))) {
                ComPtr<IRandomAccessStream> stream;
                streamFactory->Create(&stream);
                ComPtr<IOutputStream> outStream;
                stream->GetOutputStreamAt(0, &outStream);
                // 썸네일 설정
                ComPtr<IRandomAccessStreamReference> streamRef;
                ComPtr<IRandomAccessStreamReferenceStatics> refStatics;
                if (SUCCEEDED(RoGetActivationFactory(
                        HStringReference(RuntimeClass_Windows_Storage_Streams_RandomAccessStreamReference).Get(),
                        IID_PPV_ARGS(&refStatics)))) {
                    refStatics->CreateFromStream(stream.Get(), &streamRef);
                    impl->updater->put_Thumbnail(streamRef.Get());
                }
            }
        }
    }

    impl->updater->Update();
    qInfo() << "[SMTC] 메타데이터 업데이트:" << title << "-" << artist;
#else
    Q_UNUSED(title) Q_UNUSED(artist) Q_UNUSED(album) Q_UNUSED(albumArt)
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
#ifdef Q_OS_WIN
    if (!initialized_ || !smtcPtr_) return;
    auto* impl = static_cast<SMTCImpl*>(smtcPtr_);

    ComPtr<ISystemMediaTransportControlsTimelineProperties> timeline;
    HRESULT hr = RoActivateInstance(
        HStringReference(RuntimeClass_Windows_Media_SystemMediaTransportControlsTimelineProperties).Get(),
        &timeline
    );
    if (FAILED(hr)) return;

    // 100ns 단위 (TimeSpan)
    auto toTimeSpan = [](double secs) -> ABI::Windows::Foundation::TimeSpan {
        ABI::Windows::Foundation::TimeSpan ts;
        ts.Duration = static_cast<INT64>(secs * 10000000.0);
        return ts;
    };

    timeline->put_StartTime(toTimeSpan(0));
    timeline->put_EndTime(toTimeSpan(duration));
    timeline->put_Position(toTimeSpan(position));
    timeline->put_MinSeekTime(toTimeSpan(0));
    timeline->put_MaxSeekTime(toTimeSpan(duration));

    impl->smtc->UpdateTimelineProperties(timeline.Get());
#else
    Q_UNUSED(position) Q_UNUSED(duration)
#endif
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
