#pragma once

#include <QByteArray>
#include <QLocalServer>
#include <QSharedMemory>
#include <QStringList>
#include <functional>

class QLocalSocket;

/**
 * InstanceCoordinator
 *
 * 소리누리의 기본 단일 인스턴스 파일 전달과 명시적 다중 재생 세션을 분리한다.
 * 기본 실행은 coordinator에 파일을 전달하고 종료하며, --new-window 요청만
 * 모든 참여 인스턴스를 shared PCM 세션으로 전환한 뒤 별도 창을 허용한다.
 *
 * 중요한 원칙:
 * - 정상 coordinator의 LocalServer를 새 창이 삭제하지 않는다.
 * - multi-session marker는 참여한 모든 프로세스가 attach하여 primary 비정상 종료 뒤에도 유지한다.
 * - marker가 살아 있는 한 새 프로세스가 exclusive/bitstream으로 자동 복귀하지 않는다.
 */
class InstanceCoordinator {
public:
    enum class MessageType : quint8 {
        OpenFiles = 1,
        StartMultiSession = 2,
    };

    struct Message {
        MessageType type = MessageType::OpenFiles;
        QStringList files;
    };

    using MessageHandler = std::function<void(const Message&)>;

    explicit InstanceCoordinator(QObject* parent = nullptr);
    ~InstanceCoordinator();

    // 이 프로세스가 기본 coordinator가 되었으면 true를 반환한다.
    // QSharedMemory lock을 획득한 후에만 stale LocalServer endpoint를 정리한다.
    bool acquireCoordinator();
    bool isCoordinator() const { return coordinator_; }

    // 기존 coordinator로 메시지를 전달하고 처리 ACK를 기다린다.
    // false는 기존 coordinator가 없거나 정상 통신에 실패했음을 뜻한다.
    bool sendMessage(const Message& message, int timeoutMs = 1800) const;

    void setMessageHandler(MessageHandler handler);

    // 다중 재생 세션 marker 관리. 모든 다중 참여 프로세스가 marker에 attach한다.
    static bool multiSessionActive();
    bool joinMultiSession();
    bool isInMultiSession() const;

private:
    static QString serverName();
    static QString coordinatorLockName();
    static QString multiSessionMarkerName();
    static QByteArray serialize(const Message& message);
    static bool deserialize(const QByteArray& payload, Message& message);
    void handleConnection(QLocalSocket* socket);

    QLocalServer server_;
    QSharedMemory coordinatorLock_;
    QSharedMemory multiSessionMarker_;
    MessageHandler messageHandler_;
    bool coordinator_ = false;
};
