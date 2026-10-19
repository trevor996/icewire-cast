#include "AutoUpdateThread.hpp"

#include <OBSApp.hpp>
#include <qt-wrappers.hpp>

#include <QCryptographicHash>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStringList>
#include <QSysInfo>
#include <QTemporaryFile>
#include <QUrl>
#include <QUrlQuery>

#include <nlohmann/json.hpp>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#endif

#include "moc_AutoUpdateThread.cpp"

#ifndef IWC_APP_VERSION
#define IWC_APP_VERSION "1.2.0"
#endif

#ifndef IWC_UPDATE_API_URL
#define IWC_UPDATE_API_URL "https://icewire-live-hockey.trevor981385.chatgpt.site/api/iwc/releases/latest"
#endif

using json = nlohmann::json;
using namespace std;

namespace {

struct HttpResult {
	QByteArray body;
	QString error;
	int status = 0;
};

QString ReleasePlatform()
{
#ifdef __APPLE__
	return QSysInfo::buildCpuArchitecture().contains("arm", Qt::CaseInsensitive)
		       ? QStringLiteral("macos-arm64")
		       : QStringLiteral("macos-x64");
#else
	return QStringLiteral("windows-x64");
#endif
}

QNetworkRequest CreateRequest(const QUrl &url)
{
	QNetworkRequest request(url);
	request.setHeader(QNetworkRequest::UserAgentHeader,
			  QString("ICEWIRE-CAST/%1 %2")
				  .arg(QString::fromUtf8(IWC_APP_VERSION), ReleasePlatform()));
	request.setRawHeader("Accept", "application/json");
	request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
	request.setTransferTimeout(30000);
	return request;
}

HttpResult FetchJson(const QUrl &url)
{
	QNetworkAccessManager manager;
	QNetworkReply *reply = manager.get(CreateRequest(url));
	QEventLoop loop;
	QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	loop.exec();

	HttpResult result;
	result.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	result.body = reply->readAll();
	if (reply->error() != QNetworkReply::NoError)
		result.error = reply->errorString();
	reply->deleteLater();
	return result;
}

bool DownloadInstaller(const QUrl &url, const QString &outputPath, QString &error)
{
	QNetworkAccessManager manager;
	QNetworkRequest request = CreateRequest(url);
	request.setRawHeader("Accept", "application/octet-stream");
	QNetworkReply *reply = manager.get(request);
	QSaveFile file(outputPath);
	if (!file.open(QIODevice::WriteOnly)) {
		error = file.errorString();
		reply->abort();
		reply->deleteLater();
		return false;
	}

	bool writeFailed = false;
	QObject::connect(reply, &QIODevice::readyRead, &manager, [&]() {
		const QByteArray chunk = reply->readAll();
		if (!chunk.isEmpty() && file.write(chunk) != chunk.size()) {
			writeFailed = true;
			reply->abort();
		}
	});
	QEventLoop loop;
	QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	loop.exec();
	const QByteArray remaining = reply->readAll();
	if (!remaining.isEmpty() && file.write(remaining) != remaining.size())
		writeFailed = true;

	const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	if (reply->error() != QNetworkReply::NoError || status < 200 || status >= 300 || writeFailed) {
		error = writeFailed ? file.errorString() : reply->errorString();
		file.cancelWriting();
		reply->deleteLater();
		return false;
	}
	reply->deleteLater();
	if (!file.commit()) {
		error = file.errorString();
		return false;
	}
	return true;
}

QString FileSha256(const QString &path)
{
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly))
		return {};
	QCryptographicHash hash(QCryptographicHash::Sha256);
	if (!hash.addData(&file))
		return {};
	return QString::fromLatin1(hash.result().toHex());
}

QString SafeFileName(QString value)
{
	for (qsizetype index = 0; index < value.size(); ++index) {
		const QChar character = value.at(index);
		if (!character.isLetterOrNumber() && character != '.' && character != '-' && character != '_')
			value[index] = '-';
	}
	return value.left(160);
}

QString WritableUpdateDirectory()
{
	QStringList candidates;
	const QString localData = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
	if (!localData.isEmpty())
		candidates.push_back(QDir(localData).filePath("updates"));
	candidates.push_back(QDir(QDir::tempPath()).filePath("ICEWIRE-CAST/updates"));

	for (const QString &candidate : candidates) {
		if (candidate.isEmpty() || !QDir().mkpath(candidate))
			continue;
		QTemporaryFile writeTest(QDir(candidate).filePath(".iwc-update-write-test-XXXXXX"));
		if (writeTest.open())
			return QDir::cleanPath(candidate);
	}
	return {};
}

} // namespace

void AutoUpdateThread::infoMsg(const QString &title, const QString &text)
{
	OBSMessageBox::information(App()->GetMainWindow(), title, text);
}

void AutoUpdateThread::info(const QString &title, const QString &text)
{
	QMetaObject::invokeMethod(this, "infoMsg", Qt::BlockingQueuedConnection, Q_ARG(QString, title),
				  Q_ARG(QString, text));
}

bool AutoUpdateThread::queryUpdateSlot(const QString &version, const QString &notes, bool required)
{
	QString message = QString("ICEWIRE CAST %1 is ready to install.\n\n%2")
				  .arg(version, notes.isEmpty() ? QString("Performance and stability improvements.") : notes);
	if (required)
		message += "\n\nThis release is marked as required by your ICEWIRE system administrator.";
	const auto result = OBSMessageBox::question(App()->GetMainWindow(), "ICEWIRE CAST Update", message,
						       QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
	return result == QMessageBox::Yes;
}

bool AutoUpdateThread::queryUpdate(const QString &version, const QString &notes, bool required)
{
	bool accepted = false;
	QMetaObject::invokeMethod(this, "queryUpdateSlot", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, accepted),
				  Q_ARG(QString, version), Q_ARG(QString, notes), Q_ARG(bool, required));
	return accepted;
}

void AutoUpdateThread::run()
try {
	struct FinishedTrigger {
		~FinishedTrigger() { QMetaObject::invokeMethod(App()->GetMainWindow(), "updateCheckFinished"); }
	} finishedTrigger;

	if (repairMode) {
#ifdef __APPLE__
		info("ICEWIRE CAST", "Repair is handled by downloading the latest ICEWIRE CAST disk image from the Update Center.");
#else
		info("ICEWIRE CAST", "Repair is handled by rerunning the latest ICEWIRE CAST installer from the Update Center.");
#endif
		return;
	}

	QUrl manifestUrl(QString::fromUtf8(IWC_UPDATE_API_URL));
	QUrlQuery query;
	query.addQueryItem("platform", ReleasePlatform());
	query.addQueryItem("channel", "stable");
	query.addQueryItem("current", QString::fromUtf8(IWC_APP_VERSION));
	if (manualUpdate)
		query.addQueryItem("manual", "1");
	manifestUrl.setQuery(query);

	const HttpResult manifestResult = FetchJson(manifestUrl);
	if (!manifestResult.error.isEmpty() || manifestResult.status < 200 || manifestResult.status >= 300)
		throw QString("The ICEWIRE update service could not be reached. %1").arg(manifestResult.error).toStdString();

	json manifest = json::parse(manifestResult.body.constData());
	const bool updateAvailable = manifest.value("updateAvailable", false);
	if (!updateAvailable) {
		config_set_int(App()->GetAppConfig(), "General", "LastUpdateCheck", (long long)time(nullptr));
		config_save_safe(App()->GetAppConfig(), "tmp", nullptr);
		if (manualUpdate)
			info("ICEWIRE CAST Update", QString("ICEWIRE CAST %1 is up to date.").arg(IWC_APP_VERSION));
		return;
	}

	const json &release = manifest.at("release");
	const QString version = QString::fromStdString(release.at("version").get<string>());
	const QString notes = QString::fromStdString(release.value("releaseNotes", string()));
	const bool required = release.value("isRequired", false);
	const QString expectedHash = QString::fromStdString(release.at("sha256").get<string>()).toLower();
	const QString downloadUrl = QString::fromStdString(release.at("downloadUrl").get<string>());
	const QString defaultFileName = ReleasePlatform().startsWith("macos-")
					? QStringLiteral("ICEWIRE-CAST-Update.dmg")
					: QStringLiteral("ICEWIRE-CAST-Update.exe");
	const QString fileName = SafeFileName(
		QString::fromStdString(release.value("fileName", defaultFileName.toStdString())));
	if (version.isEmpty() || expectedHash.size() != 64 || !QUrl(downloadUrl).isValid() || fileName.isEmpty())
		throw string("The ICEWIRE update manifest is incomplete.");

	const char *skipVersion = config_get_string(App()->GetAppConfig(), "General", "SkipIwcUpdateVersion");
	if (!manualUpdate && !required && skipVersion && version == QString::fromUtf8(skipVersion))
		return;
	if (!queryUpdate(version, notes, required)) {
		if (!required)
			config_set_string(App()->GetAppConfig(), "General", "SkipIwcUpdateVersion", version.toUtf8().constData());
		config_set_int(App()->GetAppConfig(), "General", "LastUpdateCheck", (long long)time(nullptr));
		config_save_safe(App()->GetAppConfig(), "tmp", nullptr);
		return;
	}

	const QString updateDirectory = WritableUpdateDirectory();
	if (updateDirectory.isEmpty())
		throw string("The ICEWIRE update folder could not be created.");
	const QString installerPath = QDir(updateDirectory).filePath(fileName);
	QString downloadError;
	if (!DownloadInstaller(QUrl(downloadUrl), installerPath, downloadError))
		throw QString("The installer download failed. %1").arg(downloadError).toStdString();
	if (FileSha256(installerPath) != expectedHash) {
		QFile::remove(installerPath);
		throw string("The installer security checksum did not match. The file was removed.");
	}

#ifdef _WIN32
	SHELLEXECUTEINFOW executeInfo = {};
	executeInfo.cbSize = sizeof(executeInfo);
	executeInfo.lpVerb = L"runas";
	const std::wstring installer = installerPath.toStdWString();
	executeInfo.lpFile = installer.c_str();
	executeInfo.lpParameters = L"/SILENT /CLOSEAPPLICATIONS /RESTARTAPPLICATIONS /IWCUPDATE";
	executeInfo.nShow = SW_SHOWNORMAL;
	if (!ShellExecuteExW(&executeInfo))
		throw QString("Windows could not start the ICEWIRE CAST installer. Error %1.").arg(GetLastError()).toStdString();

	config_set_int(App()->GetAppConfig(), "General", "LastUpdateCheck", 0);
	config_set_string(App()->GetAppConfig(), "General", "SkipIwcUpdateVersion", "");
	config_save_safe(App()->GetAppConfig(), "tmp", nullptr);
	QMetaObject::invokeMethod(App()->GetMainWindow(), "close");
#elif defined(__APPLE__)
	if (!QProcess::startDetached(QStringLiteral("/usr/bin/open"), {installerPath}))
		throw string("macOS could not open the ICEWIRE CAST update disk image.");
	config_set_int(App()->GetAppConfig(), "General", "LastUpdateCheck", 0);
	config_set_string(App()->GetAppConfig(), "General", "SkipIwcUpdateVersion", "");
	config_save_safe(App()->GetAppConfig(), "tmp", nullptr);
	info("ICEWIRE CAST Update",
	     "The verified update disk image is opening. Quit ICEWIRE CAST, then drag ICEWIRE CAST into Applications to finish the update.");
#endif

} catch (const json::exception &error) {
	blog(LOG_WARNING, "ICEWIRE update manifest error: %s", error.what());
	if (manualUpdate)
		info("ICEWIRE CAST Update", "The update service returned an invalid response.");
} catch (const string &error) {
	blog(LOG_WARNING, "ICEWIRE update error: %s", error.c_str());
	if (manualUpdate)
		info("ICEWIRE CAST Update", QString::fromStdString(error));
}
