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

#include <multipass/network_access_manager.h>

namespace mp = multipass;

mp::NetworkAccessManager::NetworkAccessManager(QObject* parent) : QNetworkAccessManager(parent)
{
}

QNetworkReply* mp::NetworkAccessManager::createRequest(QNetworkAccessManager::Operation operation,
                                                       const QNetworkRequest& orig_request, QIODevice* device)
{
    auto scheme = orig_request.url().scheme();

    // To support http requests over Unix sockets, the initial URL needs to be in the form of:
    // unix:///path/to/unix_socket@path/in/server (or 'local' instead of 'unix')
    //
    // For example, to get the general LXD configuration when LXD is installed as a snap:
    // unix:////var/snap/lxd/common/lxd/unix.socket@1.0
    if (scheme == "unix" || scheme == "local")
    {
        const auto url_parts = orig_request.url().path().split('@');
        if (url_parts.count() != 2)
        {
            throw std::runtime_error("The local socket scheme is malformed.");
        }

        const auto socket_path = url_parts[0];
        const auto server_path = url_parts[1];
        QNetworkRequest request{orig_request};

        QUrl url(QString("/%1").arg(server_path));
        request.setUrl(url);

        return new LocalSocketReply(socket_path, request, device);
    }
    else
    {
        return QNetworkAccessManager::createRequest(operation, orig_request, device);
    }
}
