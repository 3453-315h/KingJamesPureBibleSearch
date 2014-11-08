/*  QtSpeech -- a small cross-platform library to use TTS
    Copyright (C) 2010-2011 LynxLine.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 3 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General
    Public License along with this library; if not, write to the
    Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301 USA */

#include <QtCore>
#include <QtSpeech>
#include <QtSpeech_unx.h>
#include <festival.h>

#include <assert.h>

namespace QtSpeech_v1 { // API v1.0

// ============================================================================

#ifdef QT_DEBUG
#define DEBUG_SERVER_IO
#endif
#define SERVER_IO_TIMEOUT 3000				// Timeout in msec for read, write, connect, etc
#define SERVER_IO_BLOCK_MAX 65536l			// Maximum number of read/write bytes per transfer
#define SERVER_AUTO_DISCONNECT_TIME 60000	// TCP Auto Disconnect time from server in msec

// ============================================================================

namespace {
	const QString constr_VoiceId = QString("(voice_%1)");

	const QtSpeech::VoiceName convn_DefaultVoiceName = { "cmu_us_slt_arctic_hts", "English (Male)", "en" };

	const QtSpeech::VoiceName convnarr_commonVoiceNames[] =
	{
		{ "cmu_us_awb_cg", "English (male)", "en" },
		{ "cmu_us_slt_arctic_hts", "English (female)", "en" },
		{ "cmu_us_rms_cg", "English (male)", "en" },
		{ "cmu_us_clb_artic_clunits", "English (female)", "en" },
		{ "ked_diphone", "English (male)", "en" },
		{ "cmu_us_jmk_arctic_clunits", "English (male)", "en" },
		{ "cmu_us_rms_arctic_clunits", "English (male)", "en" },
		{ "en1_mbrola", "English (male)", "en" },
		{ "kal_diphone", "English (male)", "en" },
		{ "don_diphone", "English (male)", "en" },
		{ "rab_diphone", "English (male)", "en" },
		{ "us2_mbrola", "English (male)", "en" },
		{ "us3_mbrola", "English (male)", "en" },
		{ "cmu_us_awb_arctic_clunits", "English (male)", "en" },
		{ "us1_mbrola", "English (male)", "en" },
		{ "cmu_us_bdl_arctic_clunits", "English (male)", "en" },
		{ "el_diphone", "Spanish (male)", "es" },
		{ "", "", "" }
	};
}

// internal data
class QtSpeech::Private
{
public:
	Private() {}

	VoiceName m_vnRequestedVoiceName;
};

// global data
class QtSpeech_GlobalData
{
public:
	QtSpeech_GlobalData()
	{
	}
	~QtSpeech_GlobalData()
	{
#ifdef USE_FESTIVAL_SERVER
		if (!g_pAsyncServerIO.isNull()) delete g_pAsyncServerIO.data();
#endif
	}

	QPointer<QThread> m_pSpeechThread;
	QtSpeech::VoiceName m_vnSelectedVoiceName;
	QtSpeech::VoiceNames m_lstVoiceNames;

#ifdef USE_FESTIVAL_SERVER
	QPointer<QtSpeech_asyncServerIO> g_pAsyncServerIO;
#endif
} g_QtSpeechGlobal;

// ============================================================================

// some defines for throwing exceptions
#define Where QString("%1:%2:").arg(__FILE__).arg(__LINE__)
#define SysCall(x,e) {\
    int ok = x;\
    if (!ok) {\
        QString msg = #e;\
        msg += ":"+QString(__FILE__);\
        msg += ":"+QString::number(__LINE__)+":"+#x;\
        throw e(msg);\
    }\
}

// ============================================================================

// qobject for speech thread
bool QtSpeech_th::init = false;
void QtSpeech_th::doInit()
{
	if (!init) {
		festival_initialize(true, FESTIVAL_HEAP_SIZE);
		init = true;
	}
}

void QtSpeech_th::say(QString strText)
{
    try {
		doInit();
        has_error = false;
		EST_String est_text(strText.toUtf8());
        SysCall(festival_say_text(est_text), QtSpeech::LogicError);
    }
	catch (const QtSpeech::LogicError &e) {
        has_error = true;
        err = e;
    }
    emit finished();
}

void QtSpeech_th::eval(QString strExpr)
{
	try {
		doInit();
		has_error = false;
		EST_String est_voice(strExpr.toUtf8());
		SysCall(festival_eval_command(est_voice), QtSpeech::LogicError);
	}
	catch (const QtSpeech::LogicError &e) {
		has_error = true;
		err = e;
	}
	emit finished();
}

// ============================================================================

// implementation
QtSpeech::QtSpeech(QObject * parent)
    :QObject(parent), d(new Private)
{
	if (convn_DefaultVoiceName.isEmpty())
        throw InitError(Where+"No default voice in system");

	d->m_vnRequestedVoiceName = convn_DefaultVoiceName;
}

QtSpeech::QtSpeech(VoiceName aVoiceName, QObject * parent)
    :QObject(parent), d(new Private)
{
	if (aVoiceName.isEmpty()) {
		aVoiceName = convn_DefaultVoiceName;
    }

	if (aVoiceName.isEmpty())
        throw InitError(Where+"No default voice in system");

	d->m_vnRequestedVoiceName = aVoiceName;
}

QtSpeech::~QtSpeech()
{
	delete d;
}

// ----------------------------------------------------------------------------

const QtSpeech::VoiceName &QtSpeech::name() const
{
	return g_QtSpeechGlobal.m_vnSelectedVoiceName;
}

QtSpeech::VoiceNames QtSpeech::voices()
{
	if (!g_QtSpeechGlobal.m_lstVoiceNames.isEmpty()) return g_QtSpeechGlobal.m_lstVoiceNames;

	// Set default in case we fail to setup client/server or aren't using the server:
	g_QtSpeechGlobal.m_lstVoiceNames.clear();
	for (int ndxCommonVoice = 0; (!convnarr_commonVoiceNames[ndxCommonVoice].isEmpty()); ++ndxCommonVoice) {
		g_QtSpeechGlobal.m_lstVoiceNames << convnarr_commonVoiceNames[ndxCommonVoice];
	}

#ifdef USE_FESTIVAL_SERVER
	if (!g_QtSpeechGlobal.g_pAsyncServerIO.isNull()) {
		QEventLoop el;
		connect(g_QtSpeechGlobal.g_pAsyncServerIO.data(), SIGNAL(operationComplete()), &el, SLOT(quit()), Qt::QueuedConnection);
		g_QtSpeechGlobal.g_pAsyncServerIO->readVoices();
		el.exec(QEventLoop::ExcludeUserInputEvents);
	}
#endif

	return g_QtSpeechGlobal.m_lstVoiceNames;
}

// ----------------------------------------------------------------------------

bool QtSpeech::serverSupported()
{
#ifdef USE_FESTIVAL_SERVER
	return true;
#else
	return false;
#endif
}

bool QtSpeech::serverConnected()
{
#ifdef USE_FESTIVAL_SERVER
	if (!g_QtSpeechGlobal.g_pAsyncServerIO.isNull()) {
		return g_QtSpeechGlobal.g_pAsyncServerIO->isConnected();
	}
#endif

	return false;
}

bool QtSpeech::connectToServer(const QString &strHostname, int nPortNumber)
{
#ifdef USE_FESTIVAL_SERVER
	disconnectFromServer();		// Disconnect from any existing server
	g_QtSpeechGlobal.g_pAsyncServerIO = new QtSpeech_asyncServerIO(strHostname, nPortNumber);
	return serverConnected();
#else
	Q_UNUSED(strHostname);
	Q_UNUSED(nPortNumber);
	return false;
#endif
}

void QtSpeech::disconnectFromServer()
{
#ifdef USE_FESTIVAL_SERVER
	if (!g_QtSpeechGlobal.g_pAsyncServerIO.isNull()) delete g_QtSpeechGlobal.g_pAsyncServerIO.data();
#endif
}

// ----------------------------------------------------------------------------

void QtSpeech::tell(QString text) const
{
	setVoice();

#ifdef USE_FESTIVAL_SERVER
	if (serverConnected()) {
		g_QtSpeechGlobal.g_pAsyncServerIO->say(text);
	}
#else
	if (g_QtSpeechGlobal.m_pSpeechThread.isNull()) {
		g_QtSpeechGlobal.m_pSpeechThread = new QThread;
		g_QtSpeechGlobal.m_pSpeechThread->start();
	}

	QtSpeech_th * th = new QtSpeech_th();
	th->moveToThread(g_QtSpeechGlobal.m_pSpeechThread);
	connect(th, SIGNAL(finished()), this, SIGNAL(finished()), Qt::QueuedConnection);
	connect(th, SIGNAL(finished()), th, SLOT(deleteLater()), Qt::QueuedConnection);
	QMetaObject::invokeMethod(th, "say", Qt::QueuedConnection, Q_ARG(QString,text));
#endif
}

void QtSpeech::say(QString text) const
{
	setVoice();

#ifdef USE_FESTIVAL_SERVER
	if (serverConnected()) {
		QEventLoop el;
		connect(g_QtSpeechGlobal.g_pAsyncServerIO.data(), SIGNAL(operationComplete()), &el, SLOT(quit()), Qt::QueuedConnection);
		g_QtSpeechGlobal.g_pAsyncServerIO->say(text);
		el.exec(QEventLoop::ExcludeUserInputEvents);
	}
#else
	if (g_QtSpeechGlobal.m_pSpeechThread.isNull()) {
		g_QtSpeechGlobal.m_pSpeechThread = new QThread;
		g_QtSpeechGlobal.m_pSpeechThread->start();
	}

	QEventLoop el;
	QtSpeech_th th;
	th.moveToThread(g_QtSpeechGlobal.m_pSpeechThread);
	connect(&th, SIGNAL(finished()), &el, SLOT(quit()), Qt::QueuedConnection);
	QMetaObject::invokeMethod(&th, "say", Qt::QueuedConnection, Q_ARG(QString,text));
	el.exec(QEventLoop::ExcludeUserInputEvents);

	if (th.has_error) qDebug("%s", th.err.msg.toUtf8().data());
#endif
}

void QtSpeech::setVoice(const VoiceName &aVoice) const
{
	VoiceName theVoice = (!aVoice.isEmpty() ? aVoice : d->m_vnRequestedVoiceName);
	d->m_vnRequestedVoiceName.clear();

	if (theVoice.isEmpty()) return;
	if (g_QtSpeechGlobal.m_vnSelectedVoiceName == theVoice) return;

	g_QtSpeechGlobal.m_vnSelectedVoiceName = theVoice;

#ifdef USE_FESTIVAL_SERVER
	if (serverConnected()) {
		QEventLoop el;
		connect(g_QtSpeechGlobal.g_pAsyncServerIO.data(), SIGNAL(operationComplete()), &el, SLOT(quit()), Qt::QueuedConnection);
		g_QtSpeechGlobal.g_pAsyncServerIO->setVoice(theVoice);
		el.exec(QEventLoop::ExcludeUserInputEvents);
	}
#else
	if (g_QtSpeechGlobal.m_pSpeechThread.isNull()) {
		g_QtSpeechGlobal.m_pSpeechThread = new QThread;
		g_QtSpeechGlobal.m_pSpeechThread->start();
	}

	QEventLoop el;
	QtSpeech_th th;
	th.moveToThread(g_QtSpeechGlobal.m_pSpeechThread);
	connect(&th, SIGNAL(finished()), &el, SLOT(quit()), Qt::QueuedConnection);
	QMetaObject::invokeMethod(&th, "eval", Qt::QueuedConnection, Q_ARG(QString, constr_VoiceId.arg(theVoice.id)));
	el.exec(QEventLoop::ExcludeUserInputEvents);

	if (th.has_error) qDebug("%s", th.err.msg.toUtf8().data());
#endif
}

void QtSpeech::timerEvent(QTimerEvent * te)
{
    QObject::timerEvent(te);
}

// ============================================================================

#ifdef USE_FESTIVAL_SERVER

bool QtSpeech_asyncServerIO::isConnected()
{
	return (m_sockFestival.state() == QAbstractSocket::ConnectedState);
}

QtSpeech_asyncServerIO::QtSpeech_asyncServerIO(const QString &strHostname, int nPortNumber, QObject *pParent)
	:	QObject(pParent),
		m_strHostname(strHostname),
		m_nPortNumber(nPortNumber)
{
	connectToServer();
}

QtSpeech_asyncServerIO::~QtSpeech_asyncServerIO()
{
	disconnectFromServer();
}

bool QtSpeech_asyncServerIO::connectToServer()
{
	if (isConnected()) return true;

	m_sockFestival.connectToHost(m_strHostname, m_nPortNumber);

#ifdef DEBUG_SERVER_IO
	qDebug("Connecting to Festival Server...");
#endif

	if (!m_sockFestival.waitForConnected(SERVER_IO_TIMEOUT)) {
#ifdef DEBUG_SERVER_IO
		qDebug("Failed to connect");
#endif
		return false;
	}

#ifdef DEBUG_SERVER_IO
	qDebug("Connected");
#endif

	return true;
}

void QtSpeech_asyncServerIO::disconnectFromServer()
{
#ifdef DEBUG_SERVER_IO
	qDebug("Disconnecting from Festival Server...");
#endif

	m_sockFestival.close();
}

QStringList QtSpeech_asyncServerIO::sendCommand(const QString &strCommand)
{
	if (!isConnected()) return QStringList();

	m_sockFestival.write(QString("%1\n").arg(strCommand).toUtf8());

#ifdef DEBUG_SERVER_IO
	qDebug("Wrote \"%s\" Command", strCommand.toUtf8().data());
#endif

	QStringList lstResultLines;

	if (m_sockFestival.waitForBytesWritten(SERVER_IO_TIMEOUT) && m_sockFestival.waitForReadyRead(SERVER_IO_TIMEOUT)) {

#ifdef DEBUG_SERVER_IO
		qDebug("Read Ready... Data:");
#endif

		QString strResult;
		do {
			strResult += QString::fromUtf8(m_sockFestival.read(SERVER_IO_BLOCK_MAX));
		} while ((!strResult.contains(QLatin1String("ft_StUfF_key"))) && (m_sockFestival.waitForReadyRead(SERVER_IO_TIMEOUT)));

#ifdef DEBUG_SERVER_IO
		qDebug("%s", strResult.toUtf8().data());
#endif

		lstResultLines = strResult.split(QChar('\n'));
	}

	return lstResultLines;
}

// ----------------------------------------------------------------------------

void QtSpeech_asyncServerIO::readVoices()
{
	if (connectToServer()) {
		QStringList lstResultLines = sendCommand("(voice.list)");

		// Example response:
		// LP
		// (cmu_us_awb_cg cmu_us_slt_arctic_hts cmu_us_rms_cg cmu_us_clb_arctic_clunits ked_diphone cmu_us_slt_arctic_clunits cmu_us_jmk_arctic_clunits cmu_us_rms_arctic_clunits en1_mbrola kal_diphone don_diphone rab_diphone us2_mbrola us3_mbrola cmu_us_awb_arctic_clunits us1_mbrola cmu_us_bdl_arctic_clunits el_diphone)
		// ft_StUfF_keyOK

		if ((lstResultLines.count() >= 2) &&
			(lstResultLines.at(0).trimmed().compare("LP") == 0)) {
			QString strVoices = lstResultLines.at(1);
			strVoices.remove(QChar('('));
			strVoices.remove(QChar(')'));
			QStringList lstVoices = strVoices.split(QChar(' '));
			g_QtSpeechGlobal.m_lstVoiceNames.clear();
			for (int ndx = 0; ndx < lstVoices.size(); ++ndx) {
				QtSpeech::VoiceName aVoice = { lstVoices.at(ndx), "Unknown", "" };
				int nFoundNdx = -1;
				for (int ndxCommonVoice = 0; ((nFoundNdx == -1) && (!convnarr_commonVoiceNames[ndxCommonVoice].isEmpty())); ++ndxCommonVoice) {
					if (aVoice.id.compare(convnarr_commonVoiceNames[ndxCommonVoice].id) == 0) nFoundNdx = ndxCommonVoice;
				}
				if (nFoundNdx != -1) {
					g_QtSpeechGlobal.m_lstVoiceNames << convnarr_commonVoiceNames[nFoundNdx];
				} else {
					g_QtSpeechGlobal.m_lstVoiceNames << aVoice;
				}
			}
		}
		emit operationSucceeded();
	} else {
		emit operationFailed();
	}

	emit operationComplete();
}

void QtSpeech_asyncServerIO::say(const QString &strText)
{
	if (connectToServer()) {
		QString strEscText = strText;
		strEscText.remove(QChar('\"'));
		sendCommand(QString("(SayText \"%1\")").arg(strEscText));
		emit operationSucceeded();
	} else {
		emit operationFailed();
	}

	emit operationComplete();
}

void QtSpeech_asyncServerIO::setVoice(const QtSpeech::VoiceName &aVoice)
{
	if (connectToServer()) {
		if (!aVoice.isEmpty()) sendCommand(constr_VoiceId.arg(aVoice.id));
		emit operationSucceeded();
	} else{
		emit operationFailed();
	}

	emit operationComplete();
}

#endif	// USE_FESTIVAL_SERVER

// ============================================================================

} // namespace QtSpeech_v1
