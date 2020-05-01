/*
 * Copyright (C) 2020 Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "local_socket_reply.h"

namespace mp = multipass;

namespace
{
constexpr int len = 65536;

// The following is mostly "borrowed" from Qt src/network/access/qhttpthreaddelegate.cpp
QNetworkReply::NetworkError statusCodeFromHttp(int httpStatusCode)
{
    QNetworkReply::NetworkError code;

    // Only switch on the HTTP errors LXD will respond with for now
    switch (httpStatusCode)
    {
    case 400: // Bad Request
        code = QNetworkReply::ProtocolInvalidOperationError;
        break;

    case 401: // Authorization required
        code = QNetworkReply::AuthenticationRequiredError;
        break;

    case 403: // Access denied
        code = QNetworkReply::ContentAccessDenied;
        break;

    case 404: // Not Found
        code = QNetworkReply::ContentNotFoundError;
        break;

    case 409: // Resource Conflict
        code = QNetworkReply::ContentConflictError;
        break;

    case 500: // Internal Server Error
        code = QNetworkReply::InternalServerError;
        break;

    default:
        if (httpStatusCode > 500)
        {
            // some kind of server error
            code = QNetworkReply::UnknownServerError;
        }
        else
        {
            // content error we did not handle above
            code = QNetworkReply::UnknownContentError;
        }
    }

    return code;
}
} // namespace

mp::LocalSocketReply::LocalSocketReply(const QString& socket_path, const QNetworkRequest& request,
                                       QIODevice* outgoingData)
    : QNetworkReply(), local_socket{std::make_unique<QLocalSocket>(this)}, reply_data{QByteArray(len, '\0')}
{
    QIODevice::open(QIODevice::ReadOnly);

    local_socket->connectToServer(socket_path);
    local_socket->waitForConnected();

    QObject::connect(local_socket.get(), &QLocalSocket::readyRead, this, &LocalSocketReply::read_reply);

    send_request(request, outgoingData);
}

mp::LocalSocketReply::~LocalSocketReply()
{
    local_socket->disconnectFromServer();
    local_socket->waitForDisconnected();
}

void mp::LocalSocketReply::abort()
{
}

qint64 mp::LocalSocketReply::readData(char* data, qint64 maxSize)
{
    if (offset < content_data.size())
    {
        qint64 number = qMin(maxSize, content_data.size() - offset);
        memcpy(data, content_data.constData() + offset, number);
        offset += number;

        return number;
    }

    return -1;
}

void mp::LocalSocketReply::send_request(const QNetworkRequest& request, QIODevice* outgoingData)
{
    QByteArray http_data;
    http_data.reserve(1024);

    auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toByteArray();

    // Build the HTTP method part
    http_data += op;
    http_data += ' ';
    http_data += request.url().path();
    http_data += " HTTP/1.1\r\n";

    // Build the HTTP Host header
    // Host can be anything, so we'll use 'multipass'
    http_data += "Host: multipass\r\n";

    // Build the HTTP User-Agent header
    // We'll just use what Qt uses
    http_data += "User-Agent: Mozilla/5.0\r\n";

    // Build the HTTP Accept header
    // Default to accept everything
    http_data += "Accept: */*\r\n";

    if (op == "POST")
    {
        http_data += "Content-Type: application/x-www-form-urlencoded\r\n";

        if (outgoingData)
        {
            outgoingData->open(QIODevice::ReadOnly);

            http_data += "Content-Length: ";
            http_data += QByteArray::number(outgoingData->size());
            http_data += "\r\n\r\n";
            http_data += outgoingData->readAll();
        }
    }

    http_data += "\r\n";

    local_socket->write(http_data);
    local_socket->flush();
}

void mp::LocalSocketReply::read_reply()
{
    local_socket->read(reply_data.data(), len);

    parse_reply();

    emit finished();
}

void mp::LocalSocketReply::parse_reply()
{
    auto reply_lines = reply_data.split('\n');
    auto it = reply_lines.constBegin();

    parse_status(*it);

    for (++it; it != reply_lines.constEnd(); ++it)
    {
        if ((*it).contains("Transfer-Encoding") && (*it).contains("chunked"))
            chunked_transfer_encoding = true;

        if ((*it).isEmpty() || (*it).startsWith('\r'))
        {
            // Advance to the body
            // Chunked transfer encoding also includes a line with the amount of
            // bytes (in hex) in the chunk. We just skip it for now.
            it = chunked_transfer_encoding ? it + 2 : it + 1;

            content_data = (*it).trimmed();

            break;
        }
    }
}

// Some of this logic is taken from Qt src/network/access/qhttpnetworkreply.cpp
void mp::LocalSocketReply::parse_status(const QByteArray& status)
{
    const int minLength = 11;
    const int dotPos = 6;
    const int spacePos = 8;
    const char httpMagic[] = "HTTP/";

    if (status.length() < minLength || !status.startsWith(httpMagic) || status.at(dotPos) != '.' ||
        status.at(spacePos) != ' ')
    {
        setError(QNetworkReply::ProtocolFailure, "Malformed HTTP response from server");

        emit error(QNetworkReply::ProtocolFailure);

        return;
    }

    int i = spacePos;
    int j = status.indexOf(' ', i + 1); // j == -1 || at(j) == ' ' so j+1 == 0 && j+1 <= length()
    const QByteArray code = status.mid(i + 1, j - i - 1);

    bool ok;
    auto statusCode = code.toInt(&ok);

    if (statusCode >= 400)
    {
        auto error_code = statusCodeFromHttp(statusCode);

        setError(error_code, QString::fromLatin1(status.constData() + j + 1));

        emit error(error_code);
    }
}
