#pragma once

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

class OBSBasic;

namespace Iwc {

struct Session {
	QString accessToken;
	QString refreshToken;
	QString userId;
	QString email;
	qint64 signedInAt = 0;

	bool isValid() const { return !accessToken.isEmpty() && !refreshToken.isEmpty(); }
};

struct EventContext {
	QJsonObject bundle;
	QJsonObject event;
	QJsonObject ingest;
	QJsonObject clockPreferences;
	QJsonObject youtube;

	QString id() const { return event.value("id").toString(); }
	QString teamId() const { return event.value("teamId").toString(); }
	bool isGame() const { return event.value("productionType").toString("game") == "game"; }
	bool ingestReady() const { return ingest.value("configured").toBool(false); }
};

class CloudClient final : public QObject {
	Q_OBJECT
public:
	explicit CloudClient(QObject *parent = nullptr);

	bool restoreRememberedSession(QString *error = nullptr);
	bool signIn(const QString &email, const QString &password, const QString &captchaToken,
		    QString *error = nullptr);
	void signOut();
	QJsonObject call(const QJsonObject &body, QString *error = nullptr);
	bool uploadSignedFile(const QString &path, const QString &token, const QString &localFile,
			      const QString &mimeType, QString *error = nullptr);
	const Session &session() const { return currentSession; }

private:
	Session currentSession;
	bool refresh(QString *error = nullptr);
	bool saveSession(QString *error = nullptr) const;
	bool loadSession(QString *error = nullptr);
	QJsonObject request(const QString &url, const QJsonObject &body, const QString &bearer,
			    int *status, QString *error) const;
};

CloudClient *client();
const EventContext &eventContext();

// Runs the blocking native sign-in and event workflow. The application exits
// cleanly if the user closes either required screen.
bool RunStartupWorkflow();

// Applies the operator-safe profile defaults before OBS constructs its output
// handlers. This enables IWC's managed instant replay buffer without exposing
// backend output settings to broadcast staff.
void ConfigureManagedProfile(OBSBasic *mainWindow);

// Installs the native scorebug and comment controls after the broadcast engine
// has finished loading its scene collection and output service.
void InstallStudio(OBSBasic *mainWindow);

// Opens the native Sources -> Add IWC Camera pairing workflow and adds the
// selected phone feed to the active Preview scene as one browser source.
void ShowCameraSourceDialog(OBSBasic *mainWindow);

} // namespace Iwc
