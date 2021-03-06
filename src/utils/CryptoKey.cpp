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

#include "CryptoKey.h"
#include "SecureRNG.h"
#include <QtDebug>
#include <QFile>
#include <openssl/bio.h>
#include <openssl/pem.h>

void base32_encode(char *dest, unsigned destlen, const char *src, unsigned srclen);
bool base32_decode(char *dest, unsigned destlen, const char *src, unsigned srclen);

CryptoKey::CryptoKey()
{
}

CryptoKey::~CryptoKey()
{
    clear();
}

CryptoKey::Data::~Data()
{
    if (key)
    {
        RSA_free(key);
        key = 0;
    }
}

void CryptoKey::clear()
{
    d = 0;
}

bool CryptoKey::loadFromData(const QByteArray &data, bool privateKey)
{
    BIO *b = BIO_new_mem_buf((void*)data.constData(), -1);

    RSA *key;
    if (privateKey)
        key = PEM_read_bio_RSAPrivateKey(b, NULL, NULL, NULL);
    else
        key = PEM_read_bio_RSAPublicKey(b, NULL, NULL, NULL);

    BIO_free(b);

    if (!key)
    {
        qWarning() << "Failed to parse" << (privateKey ? "private" : "public") << "key from data";
        return false;
    }

    d = new Data(key);
    return true;
}

bool CryptoKey::loadFromFile(const QString &path, bool privateKey)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning() << "Failed to open private key from" << path << "-" << file.errorString();
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    return loadFromData(data, privateKey);
}

bool CryptoKey::isPrivate() const
{
    return isLoaded() && d->key->p != 0;
}

QByteArray CryptoKey::publicKeyDigest() const
{
    if (!isLoaded())
        return QByteArray();

    int len = i2d_RSAPublicKey(d->key, NULL);
    if (len < 0)
        return QByteArray();

    QByteArray buf;
    buf.resize(len);
    unsigned char *bufp = reinterpret_cast<unsigned char*>(buf.data());

    len = i2d_RSAPublicKey(d->key, &bufp);
    if (len < 0)
    {
        qWarning() << "Failed to encode public key for digest";
        return QByteArray();
    }

    QByteArray re;
    re.resize(20);

    bool ok = SHA1(reinterpret_cast<const unsigned char*>(buf.constData()), buf.size(),
         reinterpret_cast<unsigned char*>(re.data())) != NULL;

    if (!ok)
    {
        qWarning() << "Failed to hash public key data for digest";
        return QByteArray();
    }

    return re;
}

QByteArray CryptoKey::encodedPublicKey() const
{
    if (!isLoaded())
        return QByteArray();

    BIO *b = BIO_new(BIO_s_mem());

    if (!PEM_write_bio_RSAPublicKey(b, d->key))
    {
        qWarning() << "Failed to encode public key";
        BIO_free(b);
        return QByteArray();
    }

    BUF_MEM *buf;
    BIO_get_mem_ptr(b, &buf);

    /* Close BIO, but don't free buf. */
    (void)BIO_set_close(b, BIO_NOCLOSE);
    BIO_free(b);

    QByteArray re((const char *)buf->data, (int)buf->length);
    BUF_MEM_free(buf);

    return re;
}

QString CryptoKey::torServiceID() const
{
    if (!isLoaded())
        return QString();

    QByteArray digest = publicKeyDigest();
    if (digest.isNull())
        return QString();

    QByteArray re;
    re.resize(17);

    base32_encode(re.data(), 17, digest.constData(), 10);

    return QString::fromLatin1(re);
}

QByteArray CryptoKey::signData(const QByteArray &data) const
{
    if (!isPrivate())
        return QByteArray();

    QByteArray re;
    re.resize(RSA_size(d->key));

    int r = RSA_private_encrypt(data.size(), reinterpret_cast<const unsigned char*>(data.constData()),
                                reinterpret_cast<unsigned char*>(re.data()), d->key, RSA_PKCS1_PADDING);

    if (r < 0)
    {
        qWarning() << "RSA encryption failed when generating signature";
        return QByteArray();
    }

    re.resize(r);
    return re;
}

bool CryptoKey::verifySignature(const QByteArray &data, const QByteArray &signature) const
{
    if (!isLoaded())
        return false;

    QByteArray re;
    re.resize(RSA_size(d->key) - 11);

    int r = RSA_public_decrypt(signature.size(), reinterpret_cast<const unsigned char*>(signature.constData()),
                               reinterpret_cast<unsigned char*>(re.data()), d->key, RSA_PKCS1_PADDING);

    if (r < 0)
    {
        qWarning() << "RSA decryption failed when verifying signature";
        return false;
    }

    re.resize(r);

    return (re == data);
}

void CryptoKey::test(const QString &file)
{
    CryptoKey key;

    bool ok = key.loadFromFile(file, true);
    Q_ASSERT(ok);
    Q_ASSERT(key.isLoaded());

    QByteArray pubkey = key.encodedPublicKey();
    Q_ASSERT(!pubkey.isEmpty());

    qDebug() << "(crypto test) Encoded public key:" << pubkey;

    QByteArray pubdigest = key.publicKeyDigest();
    Q_ASSERT(!pubdigest.isEmpty());

    qDebug() << "(crypto test) Public key digest:" << pubdigest.toHex();

    QString serviceid = key.torServiceID();
    Q_ASSERT(!serviceid.isEmpty());

    qDebug() << "(crypto test) Tor service ID:" << serviceid;

    QByteArray data = SecureRNG::random(16);
    Q_ASSERT(!data.isNull());
    qDebug() << "(crypto test) Random data:" << data.toHex();

    QByteArray signature = key.signData(data);
    Q_ASSERT(!signature.isNull());
    qDebug() << "(crypto test) Signature:" << signature.toHex();

    ok = key.verifySignature(data, signature);
    qDebug() << "(crypto test) Verified signature:" << ok;
}

/* Cryptographic hash of a password as expected by Tor's HashedControlPassword */
QByteArray torControlHashedPassword(const QByteArray &password)
{
    QByteArray salt = SecureRNG::random(8);
    if (salt.isNull())
        return QByteArray();

    int count = ((quint32)16 + (96 & 15)) << ((96 >> 4) + 6);

    SHA_CTX hash;
    SHA1_Init(&hash);

    QByteArray tmp = salt + password;
    while (count)
    {
        int c = qMin(count, tmp.size());
        SHA1_Update(&hash, reinterpret_cast<const void*>(tmp.constData()), c);
        count -= c;
    }

    unsigned char md[20];
    SHA1_Final(md, &hash);

    /* 60 is the hex-encoded value of 96, which is a constant used by Tor's algorithm. */
    return QByteArray("16:") + salt.toHex().toUpper() + QByteArray("60") +
           QByteArray::fromRawData(reinterpret_cast<const char*>(md), 20).toHex().toUpper();
}

/* Copyright (c) 2001-2004, Roger Dingledine
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson
 * Copyright (c) 2007-2010, The Tor Project, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 *   Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following disclaimer
 *   in the documentation and/or other materials provided with the
 *   distribution.
 *
 *   Neither the names of the copyright owners nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
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

#define BASE32_CHARS "abcdefghijklmnopqrstuvwxyz234567"

/* Implements base32 encoding as in rfc3548. Requires that srclen*8 is a multiple of 5. */
void base32_encode(char *dest, unsigned destlen, const char *src, unsigned srclen)
{
    unsigned i, bit, v, u;
    unsigned nbits = srclen * 8;

    Q_ASSERT((nbits%5) == 0); /* We need an even multiple of 5 bits. */
    Q_ASSERT((nbits/5)+1 <= destlen); /* We need enough space. */

    for (i = 0, bit = 0; bit < nbits; ++i, bit += 5)
    {
        /* set v to the 16-bit value starting at src[bits/8], 0-padded. */
        v = ((quint8) src[bit / 8]) << 8;
        if (bit + 5 < nbits)
            v += (quint8) src[(bit/8)+1];

        /* set u to the 5-bit value at the bit'th bit of src. */
        u = (v >> (11 - (bit % 8))) & 0x1F;
        dest[i] = BASE32_CHARS[u];
    }

    dest[i] = '\0';
}

/* Implements base32 decoding as in rfc3548. Requires that srclen*5 is a multiple of 8. */
bool base32_decode(char *dest, unsigned destlen, const char *src, unsigned srclen)
{
    unsigned int i, j, bit;
    unsigned nbits = srclen * 5;

    Q_ASSERT((nbits%8) == 0); /* We need an even multiple of 8 bits. */
    Q_ASSERT((nbits/8) <= destlen); /* We need enough space. */

    char *tmp = new char[srclen];

    /* Convert base32 encoded chars to the 5-bit values that they represent. */
    for (j = 0; j < srclen; ++j)
    {
        if (src[j] > 0x60 && src[j] < 0x7B)
            tmp[j] = src[j] - 0x61;
        else if (src[j] > 0x31 && src[j] < 0x38)
            tmp[j] = src[j] - 0x18;
        else if (src[j] > 0x40 && src[j] < 0x5B)
            tmp[j] = src[j] - 0x41;
        else
        {
            delete[] tmp;
            return false;
        }
    }

    /* Assemble result byte-wise by applying five possible cases. */
    for (i = 0, bit = 0; bit < nbits; ++i, bit += 8)
    {
        switch (bit % 40)
        {
        case 0:
            dest[i] = (((quint8)tmp[(bit/5)]) << 3) + (((quint8)tmp[(bit/5)+1]) >> 2);
            break;
        case 8:
            dest[i] = (((quint8)tmp[(bit/5)]) << 6) + (((quint8)tmp[(bit/5)+1]) << 1)
                      + (((quint8)tmp[(bit/5)+2]) >> 4);
            break;
        case 16:
            dest[i] = (((quint8)tmp[(bit/5)]) << 4) + (((quint8)tmp[(bit/5)+1]) >> 1);
            break;
        case 24:
            dest[i] = (((quint8)tmp[(bit/5)]) << 7) + (((quint8)tmp[(bit/5)+1]) << 2)
                      + (((quint8)tmp[(bit/5)+2]) >> 3);
            break;
        case 32:
            dest[i] = (((quint8)tmp[(bit/5)]) << 5) + ((quint8)tmp[(bit/5)+1]);
            break;
        }
    }

    delete[] tmp;
    return true;
}
