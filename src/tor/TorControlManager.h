/* Torsion - http://torsionim.org/
 * Copyright (C) 2010, John Brooks <john.brooks@dereferenced.net>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following disclaimer
 *      in the documentation and/or other materials provided with the
 *      distribution.
 *
 *    * Neither the names of the copyright owners nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TORCONTROLMANAGER_H
#define TORCONTROLMANAGER_H

#include <QObject>
#include <QHostAddress>

class QNetworkProxy;

namespace Tor
{

class HiddenService;

class TorControlManager : public QObject
{
    Q_OBJECT
    friend class ProtocolInfoCommand;

public:
    enum Status
    {
        Error = -1,
        NotConnected,
        Connecting,
        Authenticating,
        Connected
    };

    enum TorStatus
    {
        TorUnknown,
        TorOffline,
        TorBootstrapping,
        TorReady,
    };

    explicit TorControlManager(QObject *parent = 0);

    /* Information */
    Status status() const { return pStatus; }
    TorStatus torStatus() const { return (pStatus == Connected) ? pTorStatus : TorOffline; }
    QString torVersion() const { return pTorVersion; }
    QString statusText() const;

    bool isSocksReady() const { return !pSocksAddress.isNull(); }
    QHostAddress socksAddress() const { return pSocksAddress; }
    quint16 socksPort() const { return pSocksPort; }
    QNetworkProxy connectionProxy();

    /* Authentication */
    void setAuthPassword(const QByteArray &password);

    /* Connection */
    bool isConnected() const { return status() == Connected; }
    void connect(const QHostAddress &address, quint16 port);

    /* Hidden Services */
    const QList<HiddenService*> &hiddenServices() const { return pServices; }
    void addHiddenService(HiddenService *service);

signals:
    void statusChanged(int newStatus, int oldStatus);
    void torStatusChanged(int newStatus, int oldStatus);
    void connected();
    void disconnected();
    void socksReady();

public slots:
    /* Instruct Tor to shutdown */
    void shutdown();
    /* Call shutdown(), and wait synchronously for the command to be written */
    void shutdownSync();

    void reconnect();

private slots:
    void socketConnected();
    void socketDisconnected();
    void socketError();

    void commandFinished(class TorControlCommand *command);

    void protocolInfoReply();
    void getTorStatusReply();
    void getSocksInfoReply();

    void setError(const QString &message);

private:
    class TorControlSocket *socket;
    QHostAddress pTorAddress;
    QString pErrorMessage;
    QString pTorVersion;
    QByteArray pAuthPassword;
    QHostAddress pSocksAddress;
    QList<HiddenService*> pServices;
    quint16 pControlPort, pSocksPort;
    Status pStatus;
    TorStatus pTorStatus;

    void setStatus(Status status);
    void setTorStatus(TorStatus status);

    void getTorStatus();
    void getSocksInfo();
    void publishServices();
};

}

extern Tor::TorControlManager *torManager;

#endif // TORCONTROLMANAGER_H
