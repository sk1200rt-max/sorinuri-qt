#include "InstanceCoordinator.h"

#include <QDataStream>
#include <QDebug>
#include <QLocalSocket>

namespace {
constexpr quint32 kProtocolMagic = 0x534E4950; // "SNIP" (Sorinuri IPC)
constexpr quint16 kProtocolVersion = 2;
constexpr quint32 kMaximumPayloadBytes = 1024 * 1024;
constexpr char kAck[] = "OK";
}

InstanceCoordinator::InstanceCoordinator(QObject* parent)
    : server_(parent),
      coordinatorLock_(coordinatorLockName()),
      multiSessionMarker_(multiSessionMarkerName()) {
}

InstanceCoordinator::~InstanceCoordinator() {
    if (server_.isListening()) server_.close();
    if (coordinatorLock_.isAttached()) coordinatorLock_.detach();
    if (multiSessionMarker_.isAttached()) multiSessionMarker_.detach();
}

QString InstanceCoordinator::serverName() {
    return QStringLiteral("SorinuriIPC_v2");
}

QString InstanceCoordinator::coordinatorLockName() {
    return QStringLiteral("SorinuriIPC_v2_Coordinator");
}

QString InstanceCoordinator::multiSessionMarkerName() {
    return QStringLiteral("SorinuriIPC_v2_MultiSession");
}

bool InstanceCoordinator::acquireCoordinator() {
    if (coordinator_) return true;

    // 정상 coordinator가 살아 있으면 lock 생성은 실패한다. 이 메서드는 main()에서
    // 기존 server 연결 실패 후에만 호출하므로, stale shared-memory segment는
    // attach/detach로 정리한 뒤 한 번만 생성 재시도한다.
    if (!coordinatorLock_.create(1)) {
        if (coordinatorLock_.attach()) coordinatorLock_.detach();
        if (!coordinatorLock_.create(1)) return false;
    }

    // lock을 확보한 이 프로세스만 stale LocalServer endpoint를 정리한다.
    // 새 창이 정상 coordinator의 server를 무조건 removeServer하던 기존 동작을 금지한다.
    QLocalServer::removeServer(serverName());
    if (!server_.listen(serverName())) {
        qWarning() << "[Instance] coordinator LocalServer 시작 실패:" << server_.errorString();
        coordinatorLock_.detach();
        return false;
    }

    QObject::connect(&server_, &QLocalServer::newConnection, &server_, [this]() {
        while (QLocalSocket* socket = server_.nextPendingConnection()) {
            handleConnection(socket);
        }
    });
    coordinator_ = true;
    qInfo() << "[Instance] coordinator 시작:" << serverName();
    return true;
}

QByteArray InstanceCoordinator::serialize(const Message& message) {
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << kProtocolMagic << kProtocolVersion << static_cast<quint8>(message.type) << message.files;
    return payload;
}

bool InstanceCoordinator::deserialize(const QByteArray& payload, Message& message) {
    QDataStream stream(payload);
    stream.setVersion(QDataStream::Qt_6_0);
    quint32 magic = 0;
    quint16 version = 0;
    quint8 type = 0;
    QStringList files;
    stream >> magic >> version >> type >> files;
    if (stream.status() != QDataStream::Ok || magic != kProtocolMagic || version != kProtocolVersion)
        return false;
    if (type != static_cast<quint8>(MessageType::OpenFiles) &&
        type != static_cast<quint8>(MessageType::StartMultiSession))
        return false;
    message.type = static_cast<MessageType>(type);
    message.files = files;
    return true;
}

bool InstanceCoordinator::sendMessage(const Message& message, int timeoutMs) const {
    QLocalSocket socket;
    socket.connectToServer(serverName());
    if (!socket.waitForConnected(timeoutMs)) return false;

    const QByteArray payload = serialize(message);
    if (payload.size() > static_cast<int>(kMaximumPayloadBytes)) return false;

    QByteArray packet;
    QDataStream packetStream(&packet, QIODevice::WriteOnly);
    packetStream.setVersion(QDataStream::Qt_6_0);
    packetStream << static_cast<quint32>(payload.size());
    packet.append(payload);

    if (socket.write(packet) != packet.size() || !socket.waitForBytesWritten(timeoutMs)) return false;
    if (!socket.waitForReadyRead(timeoutMs)) return false;
    return socket.readAll().contains(kAck);
}

void InstanceCoordinator::setMessageHandler(MessageHandler handler) {
    messageHandler_ = std::move(handler);
}

void InstanceCoordinator::handleConnection(QLocalSocket* socket) {
    auto* buffer = new QByteArray();
    QObject::connect(socket, &QLocalSocket::readyRead, socket, [this, socket, buffer]() {
        buffer->append(socket->readAll());
        if (buffer->size() < static_cast<int>(sizeof(quint32))) return;

        QDataStream stream(*buffer);
        stream.setVersion(QDataStream::Qt_6_0);
        quint32 payloadSize = 0;
        stream >> payloadSize;
        if (payloadSize == 0 || payloadSize > kMaximumPayloadBytes) {
            socket->disconnectFromServer();
            return;
        }
        if (buffer->size() < static_cast<int>(sizeof(quint32) + payloadSize)) return;

        Message message;
        const QByteArray payload = buffer->mid(sizeof(quint32), payloadSize);
        if (deserialize(payload, message) && messageHandler_) {
            messageHandler_(message);
            socket->write(kAck);
            socket->flush();
        }
        socket->disconnectFromServer();
    });
    QObject::connect(socket, &QLocalSocket::disconnected, socket, [socket, buffer]() {
        delete buffer;
        socket->deleteLater();
    });
}

bool InstanceCoordinator::multiSessionActive() {
    QSharedMemory marker(multiSessionMarkerName());
    if (!marker.attach()) return false;
    marker.detach();
    return true;
}

bool InstanceCoordinator::joinMultiSession() {
    if (multiSessionMarker_.isAttached()) return true;
    if (multiSessionMarker_.create(1)) {
        qInfo() << "[Instance] 다중 재생 shared PCM 세션 시작";
        return true;
    }
    if (multiSessionMarker_.attach()) {
        qInfo() << "[Instance] 기존 다중 재생 shared PCM 세션 참가";
        return true;
    }
    qWarning() << "[Instance] 다중 재생 marker 연결 실패:" << multiSessionMarker_.errorString();
    return false;
}

bool InstanceCoordinator::isInMultiSession() const {
    return multiSessionMarker_.isAttached() || multiSessionActive();
}
