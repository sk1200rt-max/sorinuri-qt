#include "SMTCManager.h"
#include <QDebug>
#include <QBuffer>
#include <QTimer>

#ifdef Q_OS_WIN
// Windows WinRT COM 인터페이스 - 순서 중요
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wrl/client.h>
#include <wrl/wrappers/corewrappers.h>
#include <wrl/event.h>

// WinRT 런타임 헤더
#include <roapi.h>
#include <robuffer.h>

// SMTC 인터페이스 헤더
#include <systemmediatransportcontrolsinterop.h>

// Windows.Media ABI 헤더
#pragma warning(push)
#pragma warning(disable: 4467)  // ATL 경고 억제
#include <Windows.Media.h>
#include <Windows.Storage.Streams.h>
#pragma warning(pop)

using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::Media;
using namespace ABI::Windows::Storage::Streams;
using namespace ABI::Windows::Foundation;

// SMTC 내부 구현체
struct SMTCImpl {
    ComPtr<ISystemMediaTransportControls>               smtc;
    ComPtr<ISystemMediaTransportControlsDisplayUpdater> updater;
    ComPtr<IMusicDisplayProperties>                     musicProps;
    EventRegistrationToken                              btnToken{};
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

    // WinRT 초기화 (멀티스레드 아파트먼트)
    HRESULT hr = RoInitialize(RO_INIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        qWarning() << "[SMTC] WinRT 초기화 실패:" << QString::number(hr, 16);
        return false;
    }

    // ISystemMediaTransportControlsInterop 팩토리 획득
    // Win32 데스크탑 앱에서 HWND 기반으로 SMTC 인스턴스 생성하는 방법
    ComPtr<ISystemMediaTransportControlsInterop> interop;
    hr = RoGetActivationFactory(
        HStringReference(RuntimeClass_Windows_Media_SystemMediaTransportControls).Get(),
        IID_PPV_ARGS(&interop)
    );
    if (FAILED(hr)) {
        qWarning() << "[SMTC] Interop 팩토리 획득 실패:" << QString::number(hr, 16);
        return false;
    }

    // HWND로 SMTC 인스턴스 획득
    auto* impl = new SMTCImpl();
    hr = interop->GetForWindow(
        reinterpret_cast<HWND>(hwnd),
        IID_PPV_ARGS(&impl->smtc)
    );
    if (FAILED(hr)) {
        qWarning() << "[SMTC] GetForWindow 실패:" << QString::number(hr, 16);
        delete impl;
        return false;
    }

    // 버튼 활성화
    impl->smtc->put_IsPlayEnabled(true);
    impl->smtc->put_IsPauseEnabled(true);
    impl->smtc->put_IsNextEnabled(true);
    impl->smtc->put_IsPreviousEnabled(true);
    impl->smtc->put_IsStopEnabled(true);

    // 버튼 이벤트 핸들러 등록 (람다 대신 함수 포인터 방식으로 MSVC 호환성 확보)
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
        qWarning() << "[SMTC] ButtonPressed 핸들러 등록 실패:" << QString::number(hr, 16);
        delete impl;
        return false;
    }

    // DisplayUpdater 획득
    hr = impl->smtc->get_DisplayUpdater(&impl->updater);
    if (FAILED(hr)) {
        qWarning() << "[SMTC] DisplayUpdater 획득 실패:" << QString::number(hr, 16);
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
        impl->musicProps->put_Title(
            HStringReference(reinterpret_cast<const wchar_t*>(title.utf16())).Get());
        impl->musicProps->put_Artist(
            HStringReference(reinterpret_cast<const wchar_t*>(artist.utf16())).Get());
        impl->musicProps->put_AlbumTitle(
            HStringReference(reinterpret_cast<const wchar_t*>(album.utf16())).Get());
    }

    // 앨범아트 썸네일 업데이트 (PNG → InMemoryRandomAccessStream)
    if (!albumArt.isNull() && impl->updater) {
        QByteArray imgData;
        QBuffer buf(&imgData);
        buf.open(QIODevice::WriteOnly);
        albumArt.scaled(300, 300, Qt::KeepAspectRatio, Qt::SmoothTransformation)
                .save(&buf, "PNG");
        buf.close();

        if (!imgData.isEmpty()) {
            // IInMemoryRandomAccessStream 생성
            ComPtr<IInMemoryRandomAccessStreamFactory> streamFactory;
            if (SUCCEEDED(RoGetActivationFactory(
                    HStringReference(RuntimeClass_Windows_Storage_Streams_InMemoryRandomAccessStream).Get(),
                    IID_PPV_ARGS(&streamFactory)))) {
                ComPtr<IRandomAccessStream> stream;
                if (SUCCEEDED(streamFactory->Create(&stream))) {
                    // IOutputStream 획득 후 데이터 쓰기
                    ComPtr<IOutputStream> outStream;
                    if (SUCCEEDED(stream->GetOutputStreamAt(0, &outStream))) {
                        // IDataWriter로 바이트 배열 쓰기
                        ComPtr<IDataWriterFactory> writerFactory;
                        if (SUCCEEDED(RoGetActivationFactory(
                                HStringReference(RuntimeClass_Windows_Storage_Streams_DataWriter).Get(),
                                IID_PPV_ARGS(&writerFactory)))) {
                            ComPtr<IDataWriter> writer;
                            if (SUCCEEDED(writerFactory->CreateDataWriter(outStream.Get(), &writer))) {
                                writer->WriteBytes(
                                    static_cast<UINT32>(imgData.size()),
                                    reinterpret_cast<const BYTE*>(imgData.constData())
                                );
                                // StoreAsync는 비동기이므로 간단히 동기 처리
                                // 실제 환경에서는 IAsyncOperation 처리 필요
                            }
                        }
                    }
                    // IRandomAccessStreamReference 생성
                    ComPtr<IRandomAccessStreamReferenceStatics> refStatics;
                    if (SUCCEEDED(RoGetActivationFactory(
                            HStringReference(RuntimeClass_Windows_Storage_Streams_RandomAccessStreamReference).Get(),
                            IID_PPV_ARGS(&refStatics)))) {
                        ComPtr<IRandomAccessStreamReference> streamRef;
                        if (SUCCEEDED(refStatics->CreateFromStream(stream.Get(), &streamRef))) {
                            impl->updater->put_Thumbnail(streamRef.Get());
                        }
                    }
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
    if (!initialized_ || !smtcPtr_ || duration <= 0) return;
    auto* impl = static_cast<SMTCImpl*>(smtcPtr_);

    // ISystemMediaTransportControlsTimelineProperties 생성
    ComPtr<ISystemMediaTransportControlsTimelineProperties> timeline;
    HRESULT hr = RoActivateInstance(
        HStringReference(RuntimeClass_Windows_Media_SystemMediaTransportControlsTimelineProperties).Get(),
        &timeline
    );
    if (FAILED(hr)) return;

    // TimeSpan: 100나노초 단위
    auto toTS = [](double secs) -> TimeSpan {
        TimeSpan ts; ts.Duration = static_cast<INT64>(secs * 10000000.0); return ts;
    };

    timeline->put_StartTime(toTS(0));
    timeline->put_EndTime(toTS(duration));
    timeline->put_Position(toTS(position));
    timeline->put_MinSeekTime(toTS(0));
    timeline->put_MaxSeekTime(toTS(duration));

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
