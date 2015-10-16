/*
	This file is part of cpp-ethereum.

	cpp-ethereum is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	cpp-ethereum is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file WebChannelRpc.h
 * @authors:
 *   Arkadiy Paronyan <arkadiy@ethdev.com>
 * @date 2015
 */

#pragma once

#include <memory>
#include <QObject>
#include <jsonrpccpp/server/abstractserverconnector.h>

class QWebChannel;

namespace dev
{
namespace aleth
{
class WebChannelRpc: public QObject, public jsonrpc::AbstractServerConnector
{
	Q_OBJECT

public:
	WebChannelRpc();
	virtual bool StartListening();
	virtual bool StopListening();
	virtual bool SendResponse(std::string const& _response, void* _addInfo = nullptr);
	QWebChannel* channel() { return m_webChannel; }

	Q_INVOKABLE QString send(QString const& _request);
	Q_INVOKABLE void sendAsync(QString const& _request);

signals:
	void response(QString _response);

private:
	QWebChannel* m_webChannel;
};
}
} // namespace dev
