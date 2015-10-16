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
/** @file WebChannelRpc.cpp
 * @authors:
 *   Arkadiy Paronyan <arkadiy@ethdev.com>
 * @date 2015
 */

#include "WebChannelRpc.h"
#include <QWebChannel>

using namespace std;
using namespace jsonrpc;
using namespace dev;
using namespace dev::aleth;

WebChannelRpc::WebChannelRpc():
	m_webChannel(new QWebChannel(this))
{
	m_webChannel->registerObject("transport", this);
}

bool WebChannelRpc::StartListening()
{
	return true;
}

bool WebChannelRpc::StopListening()
{
	return true;
}

bool WebChannelRpc::SendResponse(string const& _response, void* _addInfo)
{
	std::string* r = reinterpret_cast<std::string*>(_addInfo);
	if (r)
		*r = _response;
	else
		response(QString::fromStdString(_response));
	return true;
}


QString WebChannelRpc::send(QString const& _request)
{
	std::string response;
	OnRequest(_request.toStdString(), &response);
	return QString::fromStdString(response);
}

void WebChannelRpc::sendAsync(QString const& _request)
{
	OnRequest(_request.toStdString());
}
