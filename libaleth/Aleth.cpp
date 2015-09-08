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
/** @file Aleth.h
 * @author Gav Wood <i@gavwood.com>
 * @date 2015
 */

#include "Aleth.h"
#include <QTimer>
#include <QSettings>
#include <QInputDialog>
#include <QMessageBox>
#include <libdevcore/Log.h>
#include <libethcore/ICAP.h>
#include <libethereum/Client.h>
#include "ui_GetPassword.h"
using namespace std;
using namespace dev;
using namespace eth;
using namespace aleth;

Aleth::Aleth(QObject* _parent):
	AlethFace(_parent)
{
}

Aleth::~Aleth()
{
	close();
	m_destructing = true;
}

void Aleth::openKeyManager()
{
	if (!getPassword("Enter your MASTER account password.", "Master password", "", [&](string const& s){ return !!keyManager().load(s); }, DefaultPasswordFlags, "The password you entered is incorrect. If you have forgotten your password, and you wish to start afresh, manually remove the file: " + getDataDir("ethereum") + "/keys.info").second)
		exit(-1);
}

void Aleth::createKeyManager()
{
	auto p = getPassword("Enter a MASTER password for your key store. Make it strong. You probably want to write it down somewhere and keep it safe and secure; your identity will rely on this - you never want to lose it.", "Master Password", string(), DoNotVerify, NeedConfirm, "You must have a master password to use this program. Exiting.");
	if (!p.second)
		exit(-1);

	keyManager().create(p.first);
	keyManager().import(ICAP::createDirect(), "Default identity");
}

void Aleth::init()
{
	// Get options
	std::string m_dbPath = getDataDir();
	for (int i = 1; i < qApp->arguments().size(); ++i)
	{
		QString arg = qApp->arguments()[i];
		if (arg == "--frontier")
			resetNetwork(eth::Network::Frontier);
		else if (arg == "--olympic")
			resetNetwork(eth::Network::Olympic);
		else if (arg == "--genesis-json" && i + 1 < qApp->arguments().size())
			CanonBlockChain<Ethash>::setGenesis(contentsString(qApp->arguments()[++i].toStdString()));
		else if ((arg == "--db-path" || arg == "-d") && i + 1 < qApp->arguments().size())
			m_dbPath = qApp->arguments()[++i].toStdString();
	}
	if (!dev::contents(m_dbPath + "/genesis.json").empty())
		CanonBlockChain<Ethash>::setGenesis(contentsString(m_dbPath + "/genesis.json"));

	// Open Key Store
	if (keyManager().exists())
		openKeyManager();
	else
		createKeyManager();

	{
		QTimer* t = new QTimer(this);
		connect(t, SIGNAL(timeout()), SLOT(checkHandlers()));
		t->start(200);
	}

	open();
}

void Aleth::open()
{
	if (!m_webThree)
	{
		QSettings s("ethereum", "alethzero");
		auto configBytes = s.value("peers").toByteArray();
		bytesConstRef network((byte*)configBytes.data(), configBytes.size());

		m_webThree.reset(new WebThreeDirect(string("AlethZero/v") + dev::Version + "/" DEV_QUOTED(ETH_BUILD_TYPE) "/" DEV_QUOTED(ETH_BUILD_PLATFORM), m_dbPath, WithExisting::Trust, {"eth"/*, "shh"*/}, p2p::NetworkPreferences(), network));
		setBeneficiary(keyManager().accounts().front());
		ethereum()->setDefault(LatestBlock);
	}
}

void Aleth::close()
{
	QSettings s("ethereum", "alethzero");
	bytes d = web3()->saveNetwork();
	s.setValue("peers", QByteArray((char*)d.data(), (int)d.size()));

	m_webThree.reset(nullptr);
}

unsigned Aleth::installWatch(LogFilter const& _tf, WatchHandler const& _f)
{
	auto ret = ethereum()->installWatch(_tf, Reaping::Manual);
	m_handlers[ret] = _f;
	_f(LocalisedLogEntries());
	return ret;
}

unsigned Aleth::installWatch(h256 const& _tf, WatchHandler const& _f)
{
	auto ret = ethereum()->installWatch(_tf, Reaping::Manual);
	m_handlers[ret] = _f;
	_f(LocalisedLogEntries());
	return ret;
}

void Aleth::uninstallWatch(unsigned _w)
{
	cdebug << "!!! Main: uninstalling watch" << _w;
	ethereum()->uninstallWatch(_w);
	m_handlers.erase(_w);
}

void Aleth::checkHandlers()
{
	for (auto const& i: m_handlers)
	{
		auto ls = ethereum()->checkWatchSafe(i.first);
		if (ls.size())
		{
//				cnote << "FIRING WATCH" << i.first << ls.size();
			i.second(ls);
		}
	}
}

void Aleth::install(AccountNamer* _adopt)
{
	m_namers.insert(_adopt);
	_adopt->m_aleth = this;
	emit knownAddressesChanged();
}

void Aleth::uninstall(AccountNamer* _kill)
{
	m_namers.erase(_kill);
	if (!m_destructing)
		emit knownAddressesChanged();
}

void Aleth::noteKnownAddressesChanged(AccountNamer*)
{
	emit knownAddressesChanged();
}

void Aleth::noteAddressNamesChanged(AccountNamer*)
{
	emit addressNamesChanged();
}

Address Aleth::toAddress(string const& _n) const
{
	for (AccountNamer* n: m_namers)
		if (n->toAddress(_n))
			return n->toAddress(_n);
	return Address();
}

string Aleth::toName(Address const& _a) const
{
	for (AccountNamer* n: m_namers)
		if (!n->toName(_a).empty())
			return n->toName(_a);
	return string();
}

Addresses Aleth::allKnownAddresses() const
{
	Addresses ret;
	for (AccountNamer* i: m_namers)
		ret += i->knownAddresses();
	sort(ret.begin(), ret.end());
	return ret;
}

Secret Aleth::retrieveSecret(Address const& _address) const
{
	bool first = true;
	while (true)
	{
		bool ok = false;
		Secret s = m_keyManager.secret(_address, [&](){
			string r;
			tie(r, ok) = getPassword("Enter the password for the account " + m_keyManager.accountName(_address) + " (" + _address.abridged() + "):", "Unlock Account", m_keyManager.passwordHint(_address), DoNotVerify, first ? DefaultPasswordFlags : IsRetry);
			return r;
		});
		if (s)
			return s;
		if (!ok)
			return Secret();
		first = false;
	}
}

pair<string, bool> Aleth::getPassword(string const& _prompt, string const& _title, string const& _hint, function<bool(string const&)> const& _verify, int _flags, string const& _failMessage) const
{
	QDialog d;
	Ui_GetPassword gp;
	gp.setupUi(&d);
	d.setWindowTitle(QString::fromStdString(_title));
	gp.label->setText(QString::fromStdString(_prompt));
	if (!_hint.empty())
		gp.entry->setPlaceholderText("Hint: " + QString::fromStdString(_hint));
	gp.confirm->setVisible(!!(_flags & NeedConfirm));

	if (_flags & NeedConfirm)
	{
		connect(gp.entry, &QLineEdit::textChanged, [&](){ gp.ok->setEnabled(gp.confirm->text() == gp.entry->text()); });
		connect(gp.confirm, &QLineEdit::textChanged, [&](){ auto m = gp.confirm->text() == gp.entry->text(); gp.ok->setEnabled(m); gp.error->setText(m ? "Passphrases must match." : ""); });
	}

	while (true)
	{
		if (_flags & IsRetry)
			gp.error->setText("The password you gave is incorrect.");
		if (d.exec() == QDialog::Accepted)
		{
			std::string ret = gp.entry->text().toStdString();
			if (_verify(ret))
				return make_pair(ret, true);
			else
				_flags |= IsRetry;
		}
		else
		{
			if (!_failMessage.empty())
				QMessageBox::information(nullptr, "Failed Password Entry", QString::fromStdString(_failMessage));
			return make_pair(string(), false);
		}
	}
}
