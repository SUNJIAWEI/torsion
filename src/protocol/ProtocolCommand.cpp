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

#include "ProtocolCommand.h"
#include <QtEndian>
#include <QtDebug>

ProtocolCommand::ProtocolCommand(QObject *parent)
    : QObject(parent)
{
}

int ProtocolCommand::prepareCommand(quint8 state, unsigned reserveSize)
{
    commandBuffer.reserve(reserveSize + 6);
    commandBuffer.resize(6);

    commandBuffer[2] = command();
    commandBuffer[3] = state;

    return 6;
}

void ProtocolCommand::sendCommand(ProtocolManager *to, bool ordered)
{
        Q_UNUSED(ordered);
    Q_ASSERT(commandBuffer.size() >= 6);
    Q_ASSERT(to);

    if (commandBuffer.size() > maxCommandData)
    {
        Q_ASSERT_X(false, metaObject()->className(), "Command data too large, would be truncated");
        qWarning() << "Truncated command" << metaObject()->className() << " (size " << commandBuffer.size()
                << ")";
        commandBuffer.resize(maxCommandData);
    }

    ProtocolSocket *socket = to->primary();
    Q_ASSERT(socket);

    /* [2*length][1*command][1*state][2*identifier] */

    /* length is 1 more than the size of data non-inclusive of the header */
    qToBigEndian(quint16(commandBuffer.size() - 6 + 1), (uchar*)commandBuffer.data());

    pIdentifier = socket->getIdentifier();
    if (!pIdentifier)
        qFatal("Unable to acquire an identifier for command; report this");

    qToBigEndian(pIdentifier, (uchar*)commandBuffer.data() + 4);

    socket->sendCommand(this);
}

bool ProtocolCommand::beginUnbufferedReply(quint8 state)
{
    Q_UNUSED(state);
    qWarning() << "Received an invalid unbuffered reply for command 0x" << hex << command()
            << " (no unbuffered reply expected)";
    return false;
}

int ProtocolCommand::processUnbufferedReply(const uchar *data, unsigned dataSize)
{
    Q_UNUSED(data);
    Q_UNUSED(dataSize);

    /* This is never called unless startUnbufferedReply is implemented, so any path reaching here is
     * an implementation error. */
    Q_ASSERT_X(false, metaObject()->className(), "Command does not implement processUnbufferedReply");
    return -1;
}
