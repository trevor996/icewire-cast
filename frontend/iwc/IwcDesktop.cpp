#include "IwcDesktop.hpp"

#include "OBSApp.hpp"
#include "utility/platform.hpp"
#include "widgets/OBSBasic.hpp"

#include <obs-frontend-api.h>
#include <obs.hpp>

#ifdef BROWSER_AVAILABLE
#include <browser-panel.hpp>
extern QCef *cef;
#endif

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QColor>
#include <QColorDialog>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDateTimeEdit>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMimeDatabase>
#include <QMenu>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QPointer>
#include <QPainter>
#include <QPixmap>
#include <QScreen>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTableWidget>
#include <QTabWidget>
#include <QTextEdit>
#include <QTimer>
#include <QTreeWidget>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>
#include <qrcodegen.hpp>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dpapi.h>
#endif

#ifdef __APPLE__
#include <Security/Security.h>
#endif

#include <algorithm>
#include <cstring>
#include <cmath>
#include <functional>

namespace {

#ifndef IWC_APP_VERSION
#define IWC_APP_VERSION "1.2.0"
#endif

constexpr auto kSupabaseUrl = "https://yhlrldmkotzsvurdeprz.supabase.co";
constexpr auto kPublishableKey = "sb_publishable_C6gymNRsjAUvdlIgzGGfGg_W7EmWkZt";
constexpr qint64 kRememberForMs = 30LL * 24LL * 60LL * 60LL * 1000LL;

#ifdef __APPLE__
constexpr auto kMacKeychainService = "com.turnkeydsn.icewirecast.session";
constexpr auto kMacKeychainAccount = "ICEWIRE CAST remembered session";
constexpr auto kMacKeychainMarker = "iwc-macos-keychain-v1";

bool saveMacKeychainSession(const QByteArray &plain)
{
	CFStringRef service = CFStringCreateWithCString(nullptr, kMacKeychainService, kCFStringEncodingUTF8);
	CFStringRef account = CFStringCreateWithCString(nullptr, kMacKeychainAccount, kCFStringEncodingUTF8);
	CFDataRef value = CFDataCreate(nullptr, reinterpret_cast<const UInt8 *>(plain.constData()),
				       static_cast<CFIndex>(plain.size()));
	if (!service || !account || !value) {
		if (service)
			CFRelease(service);
		if (account)
			CFRelease(account);
		if (value)
			CFRelease(value);
		return false;
	}

	const void *queryKeys[] = {kSecClass, kSecAttrService, kSecAttrAccount};
	const void *queryValues[] = {kSecClassGenericPassword, service, account};
	CFDictionaryRef query = CFDictionaryCreate(nullptr, queryKeys, queryValues, 3,
					    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	const void *updateKeys[] = {kSecValueData};
	const void *updateValues[] = {value};
	CFDictionaryRef update = CFDictionaryCreate(nullptr, updateKeys, updateValues, 1,
					     &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	OSStatus status = SecItemUpdate(query, update);
	if (status == errSecItemNotFound) {
		const void *addKeys[] = {kSecClass, kSecAttrService, kSecAttrAccount, kSecValueData};
		const void *addValues[] = {kSecClassGenericPassword, service, account, value};
		CFDictionaryRef add = CFDictionaryCreate(nullptr, addKeys, addValues, 4,
						   &kCFTypeDictionaryKeyCallBacks,
						   &kCFTypeDictionaryValueCallBacks);
		status = SecItemAdd(add, nullptr);
		CFRelease(add);
	}

	CFRelease(update);
	CFRelease(query);
	CFRelease(value);
	CFRelease(account);
	CFRelease(service);
	return status == errSecSuccess;
}

QByteArray loadMacKeychainSession()
{
	CFStringRef service = CFStringCreateWithCString(nullptr, kMacKeychainService, kCFStringEncodingUTF8);
	CFStringRef account = CFStringCreateWithCString(nullptr, kMacKeychainAccount, kCFStringEncodingUTF8);
	if (!service || !account) {
		if (service)
			CFRelease(service);
		if (account)
			CFRelease(account);
		return {};
	}

	const void *keys[] = {kSecClass, kSecAttrService, kSecAttrAccount, kSecReturnData, kSecMatchLimit};
	const void *values[] = {kSecClassGenericPassword, service, account, kCFBooleanTrue, kSecMatchLimitOne};
	CFDictionaryRef query = CFDictionaryCreate(nullptr, keys, values, 5,
					    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFTypeRef result = nullptr;
	const OSStatus status = SecItemCopyMatching(query, &result);
	QByteArray plain;
	if (status == errSecSuccess && result && CFGetTypeID(result) == CFDataGetTypeID()) {
		CFDataRef data = static_cast<CFDataRef>(result);
		plain = QByteArray(reinterpret_cast<const char *>(CFDataGetBytePtr(data)),
				   static_cast<qsizetype>(CFDataGetLength(data)));
	}
	if (result)
		CFRelease(result);
	CFRelease(query);
	CFRelease(account);
	CFRelease(service);
	return plain;
}
#endif

const char *kAppStyle = R"(
QDialog,QDockWidget,QWidget#iwcRoot{background:#091015;color:#f4f7f9;font-family:"Segoe UI";font-size:13px}
QLabel#eyebrow{color:#e6515b;font-weight:800;letter-spacing:2px}
QLabel#title{font-size:28px;font-weight:900;color:#ffffff}
QLabel#muted{color:#82909a}
QLineEdit,QComboBox,QDateTimeEdit,QSpinBox,QTextEdit{min-height:34px;border:1px solid #3a4852;background:#060b0f;color:#fff;padding:4px 8px}
QPushButton{min-height:34px;border:1px solid #41515d;background:#13202a;color:#d9e4eb;padding:4px 12px;font-weight:700}
QPushButton:hover{border-color:#6994b6;background:#193044}
QPushButton#primary{border-color:#e30613;background:#a9000a;color:#fff}
QPushButton#danger{border-color:#7b343a;background:#351015;color:#f4a0a6}
QListWidget,QTreeWidget,QTableWidget{border:1px solid #34414a;background:#080e12;color:#d7e0e6;alternate-background-color:#0e171d}
QHeaderView::section{background:#121d24;color:#7f909b;border:0;border-bottom:1px solid #3a4852;padding:8px;font-weight:700}
QGroupBox{margin-top:14px;border:1px solid #34414a;padding:14px 10px 10px;font-weight:800}
QGroupBox::title{subcontrol-origin:margin;left:10px;padding:0 5px;color:#78a2c2}
QCheckBox{spacing:8px} QCheckBox::indicator{width:17px;height:17px}
QSlider::groove:horizontal{height:5px;background:#24323c} QSlider::handle:horizontal{width:16px;margin:-6px 0;background:#e30613}
)";

QJsonObject errorObject(const QByteArray &body)
{
	const auto document = QJsonDocument::fromJson(body);
	return document.isObject() ? document.object() : QJsonObject{};
}

QString readableError(const QByteArray &body, const QString &fallback)
{
	const auto object = errorObject(body);
	for (const char *key : {"error_description", "message", "msg", "error"}) {
		const QString value = object.value(key).toString();
		if (!value.isEmpty())
			return value;
	}
	return fallback;
}

QByteArray protectBytes(const QByteArray &plain)
{
#ifdef _WIN32
	DATA_BLOB input{DWORD(plain.size()), reinterpret_cast<BYTE *>(const_cast<char *>(plain.data()))};
	DATA_BLOB output{};
	if (!CryptProtectData(&input, L"ICEWIRE CAST session", nullptr, nullptr, nullptr,
			      CRYPTPROTECT_UI_FORBIDDEN, &output))
		return {};
	QByteArray encrypted(reinterpret_cast<const char *>(output.pbData), int(output.cbData));
	LocalFree(output.pbData);
	return encrypted;
#elif defined(__APPLE__)
	return saveMacKeychainSession(plain) ? QByteArray(kMacKeychainMarker) : QByteArray{};
#else
	return plain;
#endif
}

QByteArray unprotectBytes(const QByteArray &encrypted)
{
#ifdef _WIN32
	DATA_BLOB input{DWORD(encrypted.size()),
			reinterpret_cast<BYTE *>(const_cast<char *>(encrypted.data()))};
	DATA_BLOB output{};
	if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr,
				CRYPTPROTECT_UI_FORBIDDEN, &output))
		return {};
	QByteArray plain(reinterpret_cast<const char *>(output.pbData), int(output.cbData));
	LocalFree(output.pbData);
	return plain;
#elif defined(__APPLE__)
	// Builds created before native Keychain support wrote the session JSON
	// directly. Accept it once so the next successful refresh can migrate it.
	return encrypted == QByteArray(kMacKeychainMarker) ? loadMacKeychainSession() : encrypted;
#else
	return encrypted;
#endif
}

QString sessionFile()
{
	const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
	QDir().mkpath(directory);
	return directory + "/iwc-session.bin";
}

QString installationId()
{
	QSettings settings(QSettings::UserScope, "ICEWIRE", "ICEWIRE CAST");
	QString value = settings.value("cloud/installationId").toString().trimmed().toLower();
	const QUuid parsed(value);
	if (parsed.isNull()) {
		value = QUuid::createUuid().toString(QUuid::WithoutBraces).toLower();
		settings.setValue("cloud/installationId", value);
		settings.sync();
	}
	return value;
}

#ifdef BROWSER_AVAILABLE
class CaptchaDialog final : public QDialog {
public:
	explicit CaptchaDialog(QWidget *parent = nullptr) : QDialog(parent)
	{
		setWindowTitle("ICEWIRE CAST · Security check");
		setModal(true);
		setMinimumSize(430, 330);
		resize(470, 360);
		setStyleSheet(kAppStyle);
		setObjectName("iwcRoot");

		auto *layout = new QVBoxLayout(this);
		layout->setContentsMargins(22, 20, 22, 20);
		layout->setSpacing(10);
		auto *eyebrow = new QLabel("IWC  •  ACCOUNT SECURITY");
		eyebrow->setObjectName("eyebrow");
		layout->addWidget(eyebrow);
		auto *heading = new QLabel("QUICK SECURITY CHECK");
		heading->setObjectName("title");
		layout->addWidget(heading);
		auto *copy = new QLabel("Complete the check below. ICEWIRE Cast will continue signing you in automatically.");
		copy->setWordWrap(true);
		copy->setObjectName("muted");
		layout->addWidget(copy);
		status = new QLabel("Loading secure check…");
		status->setWordWrap(true);
		status->setStyleSheet("color:#8296a5;padding:3px 0");

		OBSBasic::InitBrowserPanelSafeBlock();
		if (!cef) {
			status->setText("ICEWIRE Cast could not start its secure browser. Install the latest update and try again.");
			layout->addWidget(status);
			return;
		}
		browser = cef->create_widget(this, "https://icewirelive.com/iwc/captcha");
		if (!browser) {
			status->setText("ICEWIRE Cast could not open the security check. Install the latest update and try again.");
			layout->addWidget(status);
			return;
		}
		browser->setFixedHeight(118);
		layout->addWidget(browser);
		connect(browser, &QCefWidget::titleChanged, this, [this](const QString &title) {
			constexpr auto prefix = "IWC_CAPTCHA_COMPLETE:";
			if (title.startsWith(prefix)) {
				captchaToken = title.mid(QString::fromLatin1(prefix).size()).trimmed();
				if (!captchaToken.isEmpty())
					accept();
				return;
			}
			if (title == "IWC_CAPTCHA_READY") {
				status->setText("Security check ready.");
				status->setStyleSheet("color:#7fc49a;padding:3px 0");
			} else if (title == "IWC_CAPTCHA_ERROR") {
				status->setText("The security check could not be completed. Try again.");
				status->setStyleSheet("color:#ef858c;padding:3px 0");
			} else if (title == "IWC_CAPTCHA_EXPIRED") {
				status->setText("The security check expired. Complete the new check to continue.");
				status->setStyleSheet("color:#ef858c;padding:3px 0");
			}
		});

		layout->addWidget(status);
		auto *cancel = new QPushButton("CANCEL");
		connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
		layout->addWidget(cancel);
	}

	~CaptchaDialog() override { closeBrowser(); }
	const QString &token() const { return captchaToken; }

	void accept() override
	{
		closeBrowser();
		QDialog::accept();
	}

	void reject() override
	{
		closeBrowser();
		QDialog::reject();
	}

private:
	void closeBrowser()
	{
		if (browser && !browserClosed) {
			browser->closeBrowser();
			browserClosed = true;
		}
	}

	QCefWidget *browser = nullptr;
	QLabel *status = nullptr;
	QString captchaToken;
	bool browserClosed = false;
};
#endif

class SignInDialog final : public QDialog {
public:
	explicit SignInDialog(Iwc::CloudClient *cloud)
	{
		setWindowTitle("ICEWIRE CAST · Sign in");
		setModal(true);
		setMinimumSize(530, 520);
		setStyleSheet(kAppStyle);
		setObjectName("iwcRoot");

		auto *layout = new QVBoxLayout(this);
		layout->setContentsMargins(42, 38, 42, 38);
		layout->setSpacing(13);
		auto *brand = new QLabel("IWC  •  ICEWIRE CAST");
		brand->setObjectName("eyebrow");
		layout->addWidget(brand);
		auto *title = new QLabel("SIGN IN TO BROADCAST");
		title->setObjectName("title");
		layout->addWidget(title);
		auto *copy = new QLabel("Use the same ICEWIRE account you use on the website. Access is limited to your assigned league and teams.");
		copy->setWordWrap(true);
		copy->setObjectName("muted");
		layout->addWidget(copy);
		layout->addSpacing(14);

		email = new QLineEdit;
		email->setPlaceholderText("Email address");
		email->setInputMethodHints(Qt::ImhEmailCharactersOnly);
		password = new QLineEdit;
		password->setPlaceholderText("Password");
		password->setEchoMode(QLineEdit::Password);
		layout->addWidget(new QLabel("EMAIL"));
		layout->addWidget(email);
		layout->addWidget(new QLabel("PASSWORD"));
		layout->addWidget(password);

		feedback = new QLabel;
		feedback->setWordWrap(true);
		feedback->setStyleSheet("color:#ef858c;padding:5px 0");
		layout->addWidget(feedback);
		layout->addStretch();

		auto *signIn = new QPushButton("SIGN IN");
		signIn->setObjectName("primary");
		layout->addWidget(signIn);
		auto *forgot = new QPushButton("Forgot your password?");
		forgot->setFlat(true);
		layout->addWidget(forgot);

		connect(signIn, &QPushButton::clicked, this, [this, cloud, signIn] {
			feedback->clear();
			signIn->setEnabled(false);
			signIn->setText("VERIFYING…");
			QApplication::processEvents();
			QString captchaToken;
#ifdef BROWSER_AVAILABLE
			CaptchaDialog security(this);
			if (security.exec() != QDialog::Accepted || security.token().isEmpty()) {
				feedback->setText("Complete the security check to sign in.");
				signIn->setEnabled(true);
				signIn->setText("SIGN IN");
				return;
			}
			captchaToken = security.token();
#else
			feedback->setText("This ICEWIRE Cast build cannot open the required security check. Install the latest update.");
			signIn->setEnabled(true);
			signIn->setText("SIGN IN");
			return;
#endif
			signIn->setText("SIGNING IN…");
			QApplication::processEvents();
			QString error;
			if (cloud->signIn(email->text().trimmed(), password->text(), captchaToken, &error)) {
				accept();
				return;
			}
			feedback->setText(error.contains("captcha", Qt::CaseInsensitive)
					      ? "The security check expired. Press Sign In to complete a new check."
					      : error.isEmpty() ? "Sign-in failed. Check your credentials or ask an administrator for access." : error);
			signIn->setEnabled(true);
			signIn->setText("SIGN IN");
		});
		connect(password, &QLineEdit::returnPressed, signIn, &QPushButton::click);
		connect(forgot, &QPushButton::clicked, this, [] {
			QDesktopServices::openUrl(QUrl("https://icewirelive.com/admin/forgot-password"));
		});
	}

private:
	QLineEdit *email = nullptr;
	QLineEdit *password = nullptr;
	QLabel *feedback = nullptr;
};

class NewEventDialog final : public QDialog {
public:
	explicit NewEventDialog(const QJsonArray &teams, QWidget *parent = nullptr) : QDialog(parent)
	{
		setWindowTitle("Schedule a new IWC event");
		setMinimumWidth(550);
		setStyleSheet(kAppStyle);
		auto *outer = new QVBoxLayout(this);
		auto *eyebrow = new QLabel("IWC EVENT BUILDER");
		eyebrow->setObjectName("eyebrow");
		outer->addWidget(eyebrow);
		auto *heading = new QLabel("NEW BROADCAST EVENT");
		heading->setObjectName("title");
		outer->addWidget(heading);
		auto *form = new QFormLayout;
		team = new QComboBox;
		for (const auto entry : teams) {
			const auto object = entry.toObject();
			team->addItem(object.value("name").toString(), object.value("id").toString());
		}
		auto *teamLabel = new QLabel("BROADCASTING TEAM");
		type = new QComboBox;
		type->addItem("Live game", "game");
		type->addItem("Coaches' show", "coaches_show");
		opponentName = new QLineEdit;
		opponentName->setPlaceholderText("Opponent name");
		opponentAbbreviation = new QLineEdit;
		opponentAbbreviation->setPlaceholderText("OPP");
		opponentAbbreviation->setMaxLength(8);
		showTitle = new QLineEdit;
		showTitle->setPlaceholderText("Coaches' Show");
		startsAt = new QDateTimeEdit(QDateTime::currentDateTime().addSecs(3600));
		startsAt->setCalendarPopup(true);
		startsAt->setDisplayFormat("MMM d, yyyy  h:mm AP");
		venue = new QLineEdit;
		venue->setPlaceholderText("Arena or studio");
		auto *opponentLabel = new QLabel("OPPONENT");
		auto *opponentAbbreviationLabel = new QLabel("OPPONENT ABBREVIATION");
		auto *showTitleLabel = new QLabel("SHOW TITLE");
		form->addRow(teamLabel, team);
		form->addRow("EVENT TYPE", type);
		form->addRow(opponentLabel, opponentName);
		form->addRow(opponentAbbreviationLabel, opponentAbbreviation);
		form->addRow(showTitleLabel, showTitle);
		form->addRow("START", startsAt);
		form->addRow("VENUE", venue);
		if (team->count() == 1) {
			teamLabel->hide();
			team->hide();
		}
		showTitleLabel->hide();
		showTitle->hide();
		outer->addLayout(form);
		feedback = new QLabel;
		feedback->setStyleSheet("color:#ef858c");
		outer->addWidget(feedback);
		auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Save);
		buttons->button(QDialogButtonBox::Save)->setText("SCHEDULE EVENT");
		buttons->button(QDialogButtonBox::Save)->setObjectName("primary");
		outer->addWidget(buttons);
		connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
		connect(buttons, &QDialogButtonBox::accepted, this, [this] {
			const bool show = type->currentData().toString() == "coaches_show";
			if (show && showTitle->text().trimmed().isEmpty()) {
				feedback->setText("Enter a show title.");
				return;
			}
			if (!show && opponentName->text().trimmed().isEmpty()) {
				feedback->setText("Enter an opponent.");
				return;
			}
			if (!show && opponentAbbreviation->text().trimmed().size() < 2) {
				feedback->setText("Enter an opponent abbreviation.");
				return;
			}
			accept();
		});
		connect(type, &QComboBox::currentIndexChanged, this,
			[this, teamLabel, opponentLabel, opponentAbbreviationLabel, showTitleLabel] {
			const bool show = type->currentData().toString() == "coaches_show";
			// A coaches' show has no opponent selection. Keep its internal team
			// ownership automatic so the operator only configures the show itself.
			teamLabel->setVisible(!show && team->count() > 1);
			team->setVisible(!show && team->count() > 1);
			opponentLabel->setVisible(!show);
			opponentName->setVisible(!show);
			opponentAbbreviationLabel->setVisible(!show);
			opponentAbbreviation->setVisible(!show);
			showTitleLabel->setVisible(show);
			showTitle->setVisible(show);
		});
	}

	QJsonObject payload() const
	{
		const bool show = type->currentData().toString() == "coaches_show";
		return {{"action", "create_event"},
			{"teamId", team->currentData().toString()},
			{"productionType", type->currentData().toString()},
			{"title", show ? showTitle->text().trimmed() : QString()},
			{"opponentName", show ? QString() : opponentName->text().trimmed()},
			{"opponentAbbreviation", show ? "SHOW" : opponentAbbreviation->text().trimmed().toUpper()},
			{"startsAt", startsAt->dateTime().toUTC().toString(Qt::ISODate)},
			{"venue", venue->text().trimmed()}};
	}

private:
	QComboBox *team = nullptr;
	QComboBox *type = nullptr;
	QLineEdit *opponentName = nullptr;
	QLineEdit *opponentAbbreviation = nullptr;
	QLineEdit *showTitle = nullptr;
	QDateTimeEdit *startsAt = nullptr;
	QLineEdit *venue = nullptr;
	QLabel *feedback = nullptr;
};

class YoutubeDialog final : public QDialog {
public:
	explicit YoutubeDialog(QWidget *parent = nullptr) : QDialog(parent)
	{
		setWindowTitle("Connect this team's YouTube");
		setMinimumWidth(500);
		setStyleSheet(kAppStyle);
		auto *layout = new QVBoxLayout(this);
		auto *eyebrow = new QLabel("OPTIONAL SIMULCAST");
		eyebrow->setObjectName("eyebrow");
		layout->addWidget(eyebrow);
		auto *title = new QLabel("CONNECT YOUTUBE ONCE");
		title->setObjectName("title");
		layout->addWidget(title);
		auto *copy = new QLabel("Paste this team's YouTube Live stream key. IWC stores it securely for this team and will ask whether to use it for future events. YouTube is never required for ICEWIRE.");
		copy->setWordWrap(true);
		copy->setObjectName("muted");
		layout->addWidget(copy);
		label = new QLineEdit("Team YouTube Live");
		key = new QLineEdit;
		key->setEchoMode(QLineEdit::Password);
		key->setPlaceholderText("YouTube stream key");
		layout->addWidget(new QLabel("DESTINATION LABEL"));
		layout->addWidget(label);
		layout->addWidget(new QLabel("YOUTUBE STREAM KEY"));
		layout->addWidget(key);
		auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Save);
		buttons->button(QDialogButtonBox::Save)->setText("SAVE CONNECTION");
		buttons->button(QDialogButtonBox::Save)->setObjectName("primary");
		layout->addWidget(buttons);
		connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
		connect(buttons, &QDialogButtonBox::accepted, this, [this] {
			if (key->text().trimmed().size() >= 6)
				accept();
		});
	}
	QString destinationLabel() const { return label->text().trimmed(); }
	QString streamKey() const { return key->text().trimmed(); }

private:
	QLineEdit *label = nullptr;
	QLineEdit *key = nullptr;
};

class MediaUploadDialog final : public QDialog {
public:
	explicit MediaUploadDialog(const QString &filePath, QWidget *parent = nullptr) : QDialog(parent)
	{
		setWindowTitle("Add production media to IWC Cloud");
		setMinimumWidth(520);
		setStyleSheet(kAppStyle);
		auto *layout = new QVBoxLayout(this);
		auto *eyebrow = new QLabel("IWC MEDIA & TEMPLATES");
		eyebrow->setObjectName("eyebrow");
		layout->addWidget(eyebrow);
		auto *title = new QLabel("ADD PRODUCTION ASSET");
		title->setObjectName("title");
		layout->addWidget(title);
		auto *file = new QLabel(QString("%1  ·  %2 MB").arg(QFileInfo(filePath).fileName()).arg(double(QFileInfo(filePath).size()) / 1024.0 / 1024.0, 0, 'f', 1));
		file->setWordWrap(true);
		file->setObjectName("muted");
		layout->addWidget(file);
		auto *form = new QFormLayout;
		name = new QLineEdit(QFileInfo(filePath).completeBaseName());
		name->setMaxLength(120);
		category = new QComboBox;
		category->addItem("Commercial", "commercial");
		category->addItem("Sponsor file", "sponsor");
		category->addItem("In-game graphic", "graphic");
		category->addItem("Intro / bumper", "intro");
		category->addItem("Audio / music", "music");
		category->addItem("Other production media", "other");
		productionUse = new QComboBox;
		productionUse->addItem("Games + Coaches' Shows", "both");
		productionUse->addItem("Games only", "game");
		productionUse->addItem("Coaches' Shows only", "coaches_show");
		form->addRow("ASSET LABEL", name);
		form->addRow("CATEGORY", category);
		form->addRow("AVAILABLE IN", productionUse);
		layout->addLayout(form);
		auto *copy = new QLabel("The category controls where this file appears in the IWC source queue on every signed-in computer for this team.");
		copy->setWordWrap(true);
		copy->setObjectName("muted");
		layout->addWidget(copy);
		auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Save);
		buttons->button(QDialogButtonBox::Save)->setText("UPLOAD TO IWC CLOUD");
		buttons->button(QDialogButtonBox::Save)->setObjectName("primary");
		layout->addWidget(buttons);
		connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
		connect(buttons, &QDialogButtonBox::accepted, this, [this] {
			if (!name->text().trimmed().isEmpty())
				accept();
		});
	}

	QString assetName() const { return name->text().trimmed(); }
	QString assetCategory() const { return category->currentData().toString(); }
	QString assetUse() const { return productionUse->currentData().toString(); }

private:
	QLineEdit *name = nullptr;
	QComboBox *category = nullptr;
	QComboBox *productionUse = nullptr;
};

class EventsDialog final : public QDialog {
public:
	explicit EventsDialog(Iwc::CloudClient *cloud, Iwc::EventContext *selection)
		: cloud(cloud), selection(selection)
	{
		setWindowTitle("ICEWIRE CAST · Events");
		setModal(true);
		resize(1050, 700);
		setMinimumSize(820, 580);
		setStyleSheet(kAppStyle);
		setObjectName("iwcRoot");
		auto *outer = new QVBoxLayout(this);
		outer->setContentsMargins(26, 24, 26, 24);
		auto *top = new QHBoxLayout;
		auto *titles = new QVBoxLayout;
		auto *brand = new QLabel("IWC  •  ICEWIRE CAST");
		brand->setObjectName("eyebrow");
		titles->addWidget(brand);
		auto *heading = new QLabel("EVENTS");
		heading->setObjectName("title");
		titles->addWidget(heading);
		auto *copy = new QLabel("Choose a scheduled event or create a game or coaches' show. Your studio layout changes to match the event type.");
		copy->setObjectName("muted");
		copy->setWordWrap(true);
		titles->addWidget(copy);
		top->addLayout(titles, 1);
		auto *newEvent = new QPushButton("+ SCHEDULE NEW EVENT");
		newEvent->setObjectName("primary");
		top->addWidget(newEvent, 0, Qt::AlignTop);
		outer->addLayout(top);

		eventTabs = new QTabWidget;
		gamesTable = createEventTable();
		showsTable = createEventTable();
		eventTabs->addTab(gamesTable, "LIVE GAMES");
		eventTabs->addTab(showsTable, "COACHES' SHOWS");
		outer->addWidget(eventTabs, 1);
		feedback = new QLabel;
		feedback->setStyleSheet("color:#ef858c");
		outer->addWidget(feedback);
		auto *bottom = new QHBoxLayout;
		auto *account = new QLabel(QString("SIGNED IN · %1 · session remembered for 30 days").arg(cloud->session().email));
		account->setObjectName("muted");
		bottom->addWidget(account, 1);
		auto *signOut = new QPushButton("SIGN OUT");
		auto *start = new QPushButton("OPEN IWC STUDIO");
		start->setObjectName("primary");
		bottom->addWidget(signOut);
		bottom->addWidget(start);
		outer->addLayout(bottom);

		connect(newEvent, &QPushButton::clicked, this, &EventsDialog::createEvent);
		connect(start, &QPushButton::clicked, this, &EventsDialog::openSelected);
		connect(gamesTable, &QTableWidget::cellDoubleClicked, this, [this] { openSelected(); });
		connect(showsTable, &QTableWidget::cellDoubleClicked, this, [this] { openSelected(); });
		connect(signOut, &QPushButton::clicked, this, [this] {
			this->cloud->signOut();
			done(2);
		});
		loadWorkspace();
	}

private:
	QTableWidget *createEventTable()
	{
		auto *eventTable = new QTableWidget;
		eventTable->setColumnCount(5);
		eventTable->setHorizontalHeaderLabels({"EVENT", "TEAM", "START", "STATUS", "DELIVERY"});
		eventTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
		eventTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
		eventTable->setSelectionBehavior(QAbstractItemView::SelectRows);
		eventTable->setSelectionMode(QAbstractItemView::SingleSelection);
		eventTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
		eventTable->setAlternatingRowColors(true);
		return eventTable;
	}

	void loadWorkspace()
	{
		QString error;
		workspace = cloud->call({{"action", "workspace"}}, &error);
		if (!error.isEmpty()) {
			feedback->setText(error);
			return;
		}
		const auto events = workspace.value("events").toArray();
		gamesTable->setRowCount(0);
		showsTable->setRowCount(0);
		for (const auto entry : events) {
			const auto event = entry.toObject();
			const bool show = event.value("productionType").toString() == "coaches_show";
			auto *eventTable = show ? showsTable : gamesTable;
			const int row = eventTable->rowCount();
			eventTable->insertRow(row);
			const QString eventName = show ? event.value("opponentName").toString()
						       : QString("%1 vs. %2").arg(event.value("teamAbbreviation").toString(), event.value("opponentName").toString());
			const QStringList values = {eventName, event.value("teamName").toString(),
						    QDateTime::fromString(event.value("startsAt").toString(), Qt::ISODate).toLocalTime().toString("MMM d · h:mm AP"),
						    event.value("broadcastStatus").toString("offline").toUpper(),
						    event.value("provisioned").toBool() ? "READY" : "AUTOMATIC"};
			for (int column = 0; column < values.size(); ++column) {
				auto *item = new QTableWidgetItem(values.at(column));
				item->setData(Qt::UserRole, event.value("id").toString());
				eventTable->setItem(row, column, item);
			}
		}
		eventTabs->setTabText(0, QString("LIVE GAMES  ·  %1").arg(gamesTable->rowCount()));
		eventTabs->setTabText(1, QString("COACHES' SHOWS  ·  %1").arg(showsTable->rowCount()));
		if (gamesTable->rowCount()) gamesTable->selectRow(0);
		if (showsTable->rowCount()) showsTable->selectRow(0);
	}

	void createEvent()
	{
		NewEventDialog dialog(workspace.value("teams").toArray(), this);
		if (dialog.exec() != QDialog::Accepted)
			return;
		const QJsonObject payload = dialog.payload();
		QString error;
		cloud->call(payload, &error);
		if (!error.isEmpty()) {
			feedback->setText(error);
			return;
		}
		feedback->setStyleSheet("color:#7fc49a");
		feedback->setText("Event scheduled. Choose it from the list when you are ready to open IWC Studio.");
		loadWorkspace();
		eventTabs->setCurrentIndex(payload.value("productionType").toString() == "coaches_show" ? 1 : 0);
	}

	void openSelected()
	{
		auto *eventTable = eventTabs->currentIndex() == 1 ? showsTable : gamesTable;
		const int row = eventTable->currentRow();
		if (row < 0 || !eventTable->item(row, 0)) {
			feedback->setText("Choose an event first.");
			return;
		}
		QString error;
		const auto bundle = cloud->call({{"action", "event"}, {"eventId", eventTable->item(row, 0)->data(Qt::UserRole).toString()}}, &error);
		if (!error.isEmpty()) {
			feedback->setText(error);
			return;
		}
		prepareAndAccept(bundle);
	}

	void prepareAndAccept(QJsonObject bundle)
	{
		const QJsonObject event = bundle.value("event").toObject();
		QJsonObject youtube = bundle.value("youtube").toObject();
		bool useYoutube = false;
		if (youtube.value("connected").toBool()) {
			const QString label = youtube.value("label").toString("YouTube Live");
			useYoutube = QMessageBox::question(this, "Optional YouTube simulcast",
							 QString("Also send this event to the team's saved %1 destination?\n\nICEWIRE will stream whether you choose Yes or No.").arg(label),
							 QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes;
		} else if (QMessageBox::question(this, "Optional YouTube simulcast",
						   "Would you like to connect this team's YouTube channel? This is optional and does not affect ICEWIRE streaming.",
						   QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes) {
			YoutubeDialog youtubeDialog(this);
			if (youtubeDialog.exec() == QDialog::Accepted) {
				QString saveError;
				const auto result = cloud->call({{"action", "save_youtube"}, {"eventId", event.value("id")},
								 {"label", youtubeDialog.destinationLabel()}, {"streamKey", youtubeDialog.streamKey()}}, &saveError);
				if (!saveError.isEmpty()) {
					QMessageBox::warning(this, "YouTube connection", saveError);
				} else {
					youtube = result.value("youtube").toObject();
					useYoutube = true;
				}
			}
		}

		QString provisionError;
		QJsonObject provisioned = cloud->call({{"action", "provision"}, {"eventId", event.value("id")}, {"youtubeEnabled", useYoutube}}, &provisionError);
		if (!provisionError.isEmpty()) {
			QMessageBox::warning(this, "ICEWIRE delivery", provisionError);
			provisioned = bundle;
		}
		selection->bundle = provisioned;
		selection->event = provisioned.value("event").toObject();
		selection->ingest = provisioned.value("ingest").toObject();
		selection->clockPreferences = provisioned.value("clockPreferences").toObject();
		selection->youtube = provisioned.value("youtube").toObject();
		if (!selection->ingestReady()) {
			const QString message = selection->ingest.value("error").toString("The ICEWIRE streaming provider connection is pending. You can build the production now, but Go Live will stay disabled until the system connection is complete.");
			QMessageBox::information(this, "IWC studio is ready to build", message);
		}
		accept();
	}

	Iwc::CloudClient *cloud = nullptr;
	Iwc::EventContext *selection = nullptr;
	QTabWidget *eventTabs = nullptr;
	QTableWidget *gamesTable = nullptr;
	QTableWidget *showsTable = nullptr;
	QLabel *feedback = nullptr;
	QJsonObject workspace;
};

struct Runtime {
	std::unique_ptr<Iwc::CloudClient> cloud;
	Iwc::EventContext event;
	QPointer<QDockWidget> scoreDock;
	QPointer<QDockWidget> commentsDock;
	QPointer<QDockWidget> mediaDock;
	QPointer<QDockWidget> replayDock;
	QPointer<QDockWidget> leagueBoardDock;
	QPointer<QTimer> reconnectTimer;
};

Runtime &runtime()
{
	static Runtime value;
	return value;
}

QPixmap cameraPairingQr(const QString &value)
{
	QPixmap image(260, 260);
	image.fill(Qt::white);
	QPainter painter(&image);
	const QByteArray encoded = value.toUtf8();
	const qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(encoded.constData(), qrcodegen::QrCode::Ecc::MEDIUM);
	const int modules = std::max(1, qr.getSize());
	const double scale = double(image.width()) / double(modules + 4);
	painter.setPen(Qt::NoPen);
	painter.setBrush(QColor("#071018"));
	for (int y = 0; y < modules; ++y) {
		for (int x = 0; x < modules; ++x) {
			if (qr.getModule(x, y))
				painter.drawRect(QRectF((x + 2) * scale, (y + 2) * scale, scale + 0.25, scale + 0.25));
		}
	}
	return image;
}

bool addCameraSourceToPreview(OBSBasic *mainWindow, const QJsonObject &room, const QJsonObject &feed,
			      QString *error)
{
	if (!mainWindow) {
		if (error) *error = "The IWC studio window is not available.";
		return false;
	}
	OBSScene scene = mainWindow->GetCurrentScene();
	if (!scene) {
		if (error) *error = "Create or select a Preview scene before adding the phone camera.";
		return false;
	}
	const QString cameraId = feed.value("cameraId").toString();
	const QString cameraName = feed.value("cameraName").toString("Phone Camera").simplified().left(40);
	const QString signalToken = room.value("signalToken").toString();
	if (cameraId.isEmpty() || signalToken.isEmpty()) {
		if (error) *error = "The selected phone has not finished pairing yet.";
		return false;
	}
	const QString sourceName = QString("IWC Camera · %1 · %2").arg(cameraName, cameraId.left(6)).left(120);
	const QString url = QString("https://icewire-live-hockey.trevor981385.chatgpt.site/iwc/camera-feed?token=%1&camera=%2")
				    .arg(QString::fromLatin1(QUrl::toPercentEncoding(signalToken)),
					 QString::fromLatin1(QUrl::toPercentEncoding(cameraId)));
	OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_string(settings, "url", url.toUtf8().constData());
	obs_data_set_int(settings, "width", 1920);
	obs_data_set_int(settings, "height", 1080);
	obs_data_set_int(settings, "fps", 30);
	obs_data_set_bool(settings, "shutdown", false);
	obs_data_set_bool(settings, "restart_when_active", true);
	obs_data_set_bool(settings, "reroute_audio", feed.value("hasAudio").toBool(false));

	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName.toUtf8().constData());
	if (source) {
		obs_source_update(source, settings);
	} else {
		const char *sourceType = obs_get_latest_input_type_id("browser_source");
		if (!sourceType)
			sourceType = "browser_source";
		source = obs_source_create(sourceType, sourceName.toUtf8().constData(), settings, nullptr);
	}
	if (!source) {
		if (error) *error = "ICEWIRE Cast could not create the phone camera source. The Browser Source component may be missing.";
		return false;
	}

	obs_sceneitem_t *item = obs_scene_find_source(scene, sourceName.toUtf8().constData());
	if (!item)
		item = obs_scene_add(scene, source);
	if (!item) {
		if (error) *error = "The phone camera could not be added to the selected Preview scene.";
		return false;
	}
	struct obs_video_info video{};
	obs_get_video_info(&video);
	vec2 bounds{float(video.base_width), float(video.base_height)};
	vec2 position{0.0f, 0.0f};
	obs_sceneitem_set_bounds_type(item, OBS_BOUNDS_SCALE_INNER);
	obs_sceneitem_set_bounds_alignment(item, OBS_ALIGN_CENTER);
	obs_sceneitem_set_bounds(item, &bounds);
	obs_sceneitem_set_pos(item, &position);
	obs_sceneitem_set_visible(item, true);
	obs_sceneitem_set_locked(item, false);
	obs_sceneitem_set_order(item, OBS_ORDER_MOVE_TOP);
	obs_sceneitem_select(item, true);
	return true;
}

class CameraSourceDialog final : public QDialog {
public:
	explicit CameraSourceDialog(OBSBasic *mainWindow) : QDialog(mainWindow), main(mainWindow)
	{
		setWindowTitle("ICEWIRE CAST · Add IWC Camera");
		setModal(true);
		setMinimumSize(780, 660);
		resize(860, 700);
		setStyleSheet(kAppStyle);
		setObjectName("iwcRoot");

		auto *layout = new QVBoxLayout(this);
		layout->setContentsMargins(28, 24, 28, 24);
		layout->setSpacing(12);
		auto *eyebrow = new QLabel("IWC WIRELESS CAMERA NETWORK");
		eyebrow->setObjectName("eyebrow");
		layout->addWidget(eyebrow);
		auto *title = new QLabel("ADD IWC CAMERA");
		title->setObjectName("title");
		layout->addWidget(title);
		auto *description = new QLabel("Scan the code with the ICEWIRE Camera phone app. The named phone will appear below, ready to add directly to this Preview scene.");
		description->setObjectName("muted");
		description->setWordWrap(true);
		layout->addWidget(description);

		auto *pairing = new QHBoxLayout;
		pairing->setSpacing(22);
		qr = new QLabel;
		qr->setFixedSize(270, 270);
		qr->setAlignment(Qt::AlignCenter);
		qr->setStyleSheet("background:#ffffff;border:5px solid #ffffff");
		pairing->addWidget(qr, 0, Qt::AlignTop);
		auto *instructions = new QVBoxLayout;
		instructions->setSpacing(10);
		auto *codeCaption = new QLabel("EVENT CAMERA CODE");
		codeCaption->setObjectName("muted");
		instructions->addWidget(codeCaption);
		code = new QLabel("CREATING…");
		code->setStyleSheet("font-size:32px;font-weight:900;letter-spacing:5px;color:#ffffff");
		instructions->addWidget(code);
		expires = new QLabel;
		expires->setObjectName("muted");
		instructions->addWidget(expires);
		auto *steps = new QLabel("1. Scan this QR code with the crew phone.\n2. Name the camera and press Pair & Start Camera.\n3. Confirm the live view on the phone.\n4. Press Add to Preview below.");
		steps->setWordWrap(true);
		steps->setStyleSheet("font-size:14px;color:#dce7ee");
		instructions->addWidget(steps);
		auto *pairButtons = new QHBoxLayout;
		auto *copy = new QPushButton("COPY CODE");
		auto *open = new QPushButton("OPEN PHONE APP");
		pairButtons->addWidget(copy);
		pairButtons->addWidget(open);
		instructions->addLayout(pairButtons);
		newCode = new QPushButton("CREATE A NEW PAIRING CODE");
		instructions->addWidget(newCode);
		instructions->addStretch();
		pairing->addLayout(instructions, 1);
		layout->addLayout(pairing);

		auto *cameraTitle = new QLabel("CONNECTED CAMERAS");
		cameraTitle->setObjectName("eyebrow");
		layout->addWidget(cameraTitle);
		cameras = new QTreeWidget;
		cameras->setColumnCount(3);
		cameras->setHeaderLabels({"CAMERA", "SIGNAL", ""});
		cameras->setRootIsDecorated(false);
		cameras->setAlternatingRowColors(true);
		cameras->header()->setSectionResizeMode(0, QHeaderView::Stretch);
		cameras->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
		cameras->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
		cameras->setMinimumHeight(145);
		layout->addWidget(cameras, 1);
		feedback = new QLabel("Creating the secure event camera room…");
		feedback->setObjectName("muted");
		feedback->setWordWrap(true);
		layout->addWidget(feedback);

		auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
		layout->addWidget(buttons);
		connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
		connect(copy, &QPushButton::clicked, this, [this] {
			if (!room.value("pairCode").toString().isEmpty())
				QGuiApplication::clipboard()->setText(room.value("pairCode").toString());
		});
		connect(open, &QPushButton::clicked, this, [this] {
			const QUrl url(room.value("pairUrl").toString());
			if (url.isValid()) QDesktopServices::openUrl(url);
		});
		connect(newCode, &QPushButton::clicked, this, [this] { refresh(true); });
		poll.setInterval(2000);
		connect(&poll, &QTimer::timeout, this, [this] { refresh(false); });
		QTimer::singleShot(0, this, [this] { refresh(false); poll.start(); });
	}

private:
	void refresh(bool reset)
	{
		if (loading || !runtime().cloud || runtime().event.id().isEmpty())
			return;
		loading = true;
		newCode->setEnabled(false);
		QString error;
		const QJsonObject result = runtime().cloud->call({{"action", "camera_room"}, {"eventId", runtime().event.id()}, {"reset", reset}}, &error);
		loading = false;
		newCode->setEnabled(true);
		if (!error.isEmpty()) {
			feedback->setText(error);
			return;
		}
		const QJsonObject nextRoom = result.value("room").toObject();
		if (nextRoom.isEmpty()) {
			feedback->setText("ICEWIRE did not return a camera room for this event.");
			return;
		}
		const bool changed = room.value("signalToken") != nextRoom.value("signalToken");
		room = nextRoom;
		const QString pairCode = room.value("pairCode").toString();
		code->setText(pairCode.size() == 8 ? pairCode.left(4) + "  " + pairCode.mid(4) : pairCode);
		expires->setText(QString("PAIRING EXPIRES %1").arg(QDateTime::fromString(room.value("expiresAt").toString(), Qt::ISODate).toLocalTime().toString("h:mm AP")));
		if (changed || qr->pixmap().isNull())
			qr->setPixmap(cameraPairingQr(room.value("pairUrl").toString()));
		showFeeds(room.value("feeds").toArray());
	}

	void showFeeds(const QJsonArray &feeds)
	{
		cameras->clear();
		if (feeds.isEmpty()) {
			auto *item = new QTreeWidgetItem(cameras, {"Waiting for a phone to pair…", "WAITING", ""});
			item->setFlags(Qt::NoItemFlags);
			item->setForeground(0, QColor("#82909a"));
			feedback->setText("Keep this window open. A camera appears here automatically after the phone starts its live preview.");
			return;
		}
		for (const QJsonValue value : feeds) {
			const QJsonObject feed = value.toObject();
			const QString status = feed.value("status").toString("pairing").toUpper();
			auto *item = new QTreeWidgetItem(cameras, {feed.value("cameraName").toString("Phone Camera"), status, ""});
			auto *add = new QPushButton("ADD TO PREVIEW");
			add->setObjectName("primary");
			add->setEnabled(status == "ONLINE");
			cameras->setItemWidget(item, 2, add);
			connect(add, &QPushButton::clicked, this, [this, feed, add] {
				QString error;
				if (!addCameraSourceToPreview(main, room, feed, &error)) {
					feedback->setText(error);
					return;
				}
				add->setText("ADDED");
				add->setEnabled(false);
				feedback->setText(QString("%1 is in Preview. Use Cut or Fade when it is ready for Program.").arg(feed.value("cameraName").toString("Phone Camera")));
			});
		}
	}

	QPointer<OBSBasic> main;
	QJsonObject room;
	QPointer<QLabel> qr;
	QPointer<QLabel> code;
	QPointer<QLabel> expires;
	QPointer<QTreeWidget> cameras;
	QPointer<QLabel> feedback;
	QPointer<QPushButton> newCode;
	QTimer poll;
	bool loading = false;
};

bool installAutomaticService(OBSBasic *mainWindow, const QJsonObject &ingest)
{
	if (!mainWindow || !ingest.value("configured").toBool(false))
		return false;

	const QString server = ingest.value("rtmpsUrl").toString();
	const QString streamKey = ingest.value("streamKey").toString();
	if (server.isEmpty() || streamKey.isEmpty())
		return false;

	OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_string(settings, "server", server.toUtf8().constData());
	obs_data_set_string(settings, "key", streamKey.toUtf8().constData());
	obs_data_set_bool(settings, "use_auth", false);
	OBSServiceAutoRelease service = obs_service_create("rtmp_custom", "iwc_automatic_service", settings, nullptr);
	if (!service)
		return false;

	mainWindow->SetService(service);
	mainWindow->SaveService();
	if (auto *button = mainWindow->findChild<QPushButton *>("streamButton")) {
		button->setEnabled(true);
		button->setText("Start Broadcast");
		button->setToolTip("Connected to ICEWIRE. The private broadcast destination was loaded automatically.");
	}
	return true;
}

QString formatClock(int seconds)
{
	seconds = std::max(0, seconds);
	return QString("%1:%2").arg(seconds / 60, 2, 10, QLatin1Char('0')).arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

uint32_t colorValue(const QString &hex, uint32_t fallback)
{
	QColor color(hex);
	if (!color.isValid())
		return fallback;
	return uint32_t(color.red()) | (uint32_t(color.green()) << 8) | (uint32_t(color.blue()) << 16) | 0xFF000000u;
}

QString normalizedHexColor(const QString &value, const QString &fallback)
{
	const QColor color(value);
	return color.isValid() ? color.name(QColor::HexRgb).toUpper() : fallback;
}

uint32_t contrastingTextColor(const QString &background)
{
	const QColor color(background);
	if (!color.isValid())
		return colorValue("#FFFFFF", 0xFFFFFFFFu);
	const double luminance = (color.red() * 299.0 + color.green() * 587.0 + color.blue() * 114.0) / 1000.0;
	return luminance > 150.0 ? colorValue("#071019", 0xFF191007u) : colorValue("#FFFFFF", 0xFFFFFFFFu);
}

void styleColorButton(QPushButton *button, const QString &value)
{
	const QString background = normalizedHexColor(value, "#111B23");
	const QColor color(background);
	const QString foreground = (color.red() * 299 + color.green() * 587 + color.blue() * 114) / 1000 > 150 ? "#071019" : "#FFFFFF";
	button->setText(background);
	button->setStyleSheet(QString("background:%1;color:%2;border:1px solid #5c6a74;font-weight:800").arg(background, foreground));
}

OBSDataAutoRelease textSettings(const QString &text, int size, uint32_t color = 0xFFFFFFFFu)
{
	OBSDataAutoRelease settings = obs_data_create();
	OBSDataAutoRelease font = obs_data_create();
	obs_data_set_string(font, "face", "Bahnschrift SemiBold");
	obs_data_set_int(font, "flags", 1);
	obs_data_set_int(font, "size", size);
	obs_data_set_obj(settings, "font", font);
	obs_data_set_string(settings, "text", text.toUtf8().constData());
	obs_data_set_int(settings, "color", color);
	obs_data_set_bool(settings, "outline", false);
	return settings;
}

OBSSourceAutoRelease createOrUpdateText(const char *name, const QString &text, int size,
					uint32_t color = 0xFFFFFFFFu)
{
	OBSSourceAutoRelease source = obs_get_source_by_name(name);
	OBSDataAutoRelease settings = textSettings(text, size, color);
	if (source) {
		obs_source_update(source, settings);
		return source;
	}
	return OBSSourceAutoRelease(obs_source_create("text_gdiplus", name, settings, nullptr));
}

OBSSourceAutoRelease createOrUpdateColor(const char *name, uint32_t color, int width, int height)
{
	OBSSourceAutoRelease source = obs_get_source_by_name(name);
	OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_int(settings, "width", width);
	obs_data_set_int(settings, "height", height);
	obs_data_set_int(settings, "color", color);
	if (source) {
		obs_source_update(source, settings);
		return source;
	}
	return OBSSourceAutoRelease(obs_source_create("color_source", name, settings, nullptr));
}

QString cachedRemoteImage(const QString &url, const QString &label)
{
	if (url.trimmed().isEmpty())
		return {};
	const QString cacheRoot = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/scorebug";
	QDir().mkpath(cacheRoot);
	const QString fingerprint = QString::fromLatin1(QCryptographicHash::hash(url.toUtf8(), QCryptographicHash::Sha256).toHex());
	QString extension = QFileInfo(QUrl(url).path()).suffix().toLower();
	if (!QStringList{"png", "jpg", "jpeg", "webp", "gif", "bmp"}.contains(extension))
		extension = "png";
	const QString path = QString("%1/%2-%3.%4").arg(cacheRoot, label, fingerprint.left(16), extension);
	if (QFileInfo(path).exists() && QFileInfo(path).size() > 0)
		return path;

	QNetworkAccessManager manager;
	QNetworkRequest request{QUrl(url)};
	request.setRawHeader("User-Agent", "ICEWIRE-CAST/1.1");
	QNetworkReply *reply = manager.get(request);
	QEventLoop loop;
	QTimer timeout;
	timeout.setSingleShot(true);
	QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
	QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	timeout.start(5000);
	loop.exec();
	if (!reply->isFinished())
		reply->abort();
	const QByteArray bytes = reply->error() == QNetworkReply::NoError ? reply->readAll() : QByteArray{};
	reply->deleteLater();
	if (bytes.isEmpty())
		return {};
	QFile file(path);
	if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size())
		return {};
	file.close();
	return path;
}

OBSSourceAutoRelease createOrUpdateImage(const char *name, const QString &path)
{
	if (path.isEmpty())
		return {};
	OBSSourceAutoRelease source = obs_get_source_by_name(name);
	OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_string(settings, "file", path.toUtf8().constData());
	obs_data_set_bool(settings, "unload", false);
	if (source) {
		obs_source_update(source, settings);
		return source;
	}
	return OBSSourceAutoRelease(obs_source_create("image_source", name, settings, nullptr));
}

struct SceneSourcePlacement {
	const char *name;
	float x;
	float y;
	float scale;
	float width = 0;
	float height = 0;
	bool visible = true;
};

struct SceneApplyData {
	QVector<SceneSourcePlacement> placements;
	float originX = 0;
	float originY = 0;
	bool visible = true;
};

struct ScoreBugApplyData {
	obs_source_t *scoreBugSource = nullptr;
	QVector<const char *> legacyNames;
	float x = 0;
	float y = 0;
	float scale = 1;
	bool visible = true;
};

bool applySourcesToScene(void *parameter, obs_source_t *sceneSource)
{
	auto *data = static_cast<SceneApplyData *>(parameter);
	obs_scene_t *scene = obs_scene_from_source(sceneSource);
	if (!scene)
		return true;
	for (const auto &placement : data->placements) {
		obs_sceneitem_t *item = obs_scene_find_source(scene, placement.name);
		if (!item) {
			OBSSourceAutoRelease source = obs_get_source_by_name(placement.name);
			if (source)
				item = obs_scene_add(scene, source);
		}
		if (!item)
			continue;
		// Placements are authored in scorebug pixels. Scale their coordinates
		// with the source so the segmented panels never overlap at sizes other
		// than 100%.
		vec2 position{data->originX + placement.x * placement.scale,
			      data->originY + placement.y * placement.scale};
		obs_sceneitem_set_pos(item, &position);
		if (placement.width > 0 && placement.height > 0) {
			vec2 sourceScale{1.0f, 1.0f};
			vec2 bounds{placement.width * placement.scale, placement.height * placement.scale};
			obs_sceneitem_set_scale(item, &sourceScale);
			obs_sceneitem_set_bounds_type(item, OBS_BOUNDS_SCALE_INNER);
			obs_sceneitem_set_bounds_alignment(item, OBS_ALIGN_CENTER);
			obs_sceneitem_set_bounds(item, &bounds);
		} else {
			vec2 sourceScale{placement.scale, placement.scale};
			obs_sceneitem_set_bounds_type(item, OBS_BOUNDS_NONE);
			obs_sceneitem_set_scale(item, &sourceScale);
		}
		obs_sceneitem_set_visible(item, data->visible && placement.visible);
		obs_sceneitem_set_locked(item, true);
		// Reassert the authored layer stack for both new and upgraded scene
		// collections. Later placements are foreground elements (especially
		// score/clock text) and must sit above their color panels.
		obs_sceneitem_set_order(item, OBS_ORDER_MOVE_TOP);
	}
	return true;
}

bool applyScoreBugToScene(void *parameter, obs_source_t *sceneSource)
{
	auto *data = static_cast<ScoreBugApplyData *>(parameter);
	obs_scene_t *scene = obs_scene_from_source(sceneSource);
	if (!scene || !data->scoreBugSource)
		return true;
	for (const char *legacyName : data->legacyNames) {
		if (obs_sceneitem_t *legacy = obs_scene_find_source(scene, legacyName))
			obs_sceneitem_remove(legacy);
	}
	obs_sceneitem_t *item = obs_scene_find_source(scene, "Score Bug");
	if (!item)
		item = obs_scene_add(scene, data->scoreBugSource);
	if (!item)
		return true;
	vec2 position{data->x, data->y};
	vec2 scale{data->scale, data->scale};
	obs_sceneitem_set_pos(item, &position);
	obs_sceneitem_set_bounds_type(item, OBS_BOUNDS_NONE);
	obs_sceneitem_set_scale(item, &scale);
	obs_sceneitem_set_visible(item, data->visible);
	obs_sceneitem_set_locked(item, true);
	obs_sceneitem_set_order(item, OBS_ORDER_MOVE_TOP);
	return true;
}

class ScorebugController final : public QObject {
public:
	ScorebugController(OBSBasic *main, const Iwc::EventContext &context, QObject *parent = nullptr)
		: QObject(parent), main(main), context(context)
	{
		localSeconds = context.event.value("clockSeconds").toInt(1200);
		clockRunning = context.event.value("clockRunning").toBool(false);
		powerPlaySeconds = context.event.value("powerPlaySeconds").toInt(0);
		powerPlayRunning = context.event.value("powerPlayRunning").toBool(false);
		backgroundColor = normalizedHexColor(context.event.value("scoreBugBackgroundColor").toString(), "#071019");
		panelColor = normalizedHexColor(context.event.value("scoreBugPanelColor").toString(), "#111B23");
		textColor = normalizedHexColor(context.event.value("scoreBugTextColor").toString(), "#FFFFFF");
		accentColor = normalizedHexColor(context.event.value("scoreBugAccentColor").toString(), "#E30613");
		sponsors = context.bundle.value("sponsors").toArray();
		homeLogoPath = cachedRemoteImage(context.event.value("teamLogoUrl").toString(), "home");
		awayLogoPath = cachedRemoteImage(context.event.value("opponentLogoUrl").toString(), "away");
		std::string logoPath;
		if (GetDataFilePath("images/icewire-live-logo.png", logoPath))
			icewireLogoPath = QString::fromStdString(logoPath);
		scoreBugScene = OBSSceneAutoRelease(obs_scene_create_private("Score Bug"));
		ticker.setInterval(1000);
		connect(&ticker, &QTimer::timeout, this, [this] {
			if (clockRunning && localSeconds > 0) {
				--localSeconds;
				if (clockEdit)
					clockEdit->setText(formatClock(localSeconds));
				updateRenderer();
			}
			if (powerPlayRunning && powerPlaySeconds > 0) {
				--powerPlaySeconds;
				if (powerPlayClockEdit)
					powerPlayClockEdit->setText(formatClock(powerPlaySeconds));
				if (powerPlaySeconds == 0) {
					powerPlayRunning = false;
					pushPowerPlay();
				}
				updateRenderer();
			}
		});
		ticker.start();
	}

	QDockWidget *buildDock()
	{
		auto *dock = new QDockWidget("IWC · SCOREBUG", main);
		dock->setObjectName("iwcScorebugDock");
		dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
		dock->setMinimumWidth(250);
		dock->setMaximumWidth(310);
		auto *root = new QWidget;
		root->setObjectName("iwcRoot");
		root->setStyleSheet(QString::fromUtf8(kAppStyle) + R"(
QWidget#iwcRoot{font-size:12px}
QWidget#iwcRoot QLineEdit,QWidget#iwcRoot QComboBox,QWidget#iwcRoot QSpinBox{min-height:26px;max-height:28px;padding:1px 5px}
QWidget#iwcRoot QPushButton{min-height:26px;max-height:28px;padding:1px 6px}
QWidget#iwcRoot QGroupBox{margin-top:10px;padding:8px 6px 6px}
QWidget#iwcRoot QGroupBox::title{color:#8fb4d0;font-size:10px;letter-spacing:1px;font-weight:800}
QWidget#iwcRoot QCheckBox::indicator{width:14px;height:14px}
QWidget#iwcRoot QSpinBox{font-size:16px;font-weight:900;text-align:center;background:#05090c;border-color:#536471}
QWidget#iwcRoot QSpinBox::up-button,QWidget#iwcRoot QSpinBox::down-button{width:0;border:0}
QWidget#iwcRoot QPushButton#scorePlus{background:#a9000a;border-color:#e30613;color:#fff;font-size:16px}
QWidget#iwcRoot QPushButton#scorePlus:hover{background:#cf0010}
QWidget#iwcRoot QPushButton#scoreMinus{background:#101a21;border-color:#40515e;color:#b7c5cf;font-size:16px}
QWidget#iwcRoot QLabel#scoreTeam{font-size:11px;font-weight:800;letter-spacing:1px;color:#dce5eb}
QWidget#iwcRoot QLabel#scorebugStatus{font-size:9px;font-weight:800;letter-spacing:1px;color:#e6515b}
QWidget#iwcRoot QTabWidget::pane{border:1px solid #2f3b45;background:#080d11}
QWidget#iwcRoot QTabBar::tab{min-width:105px;padding:7px 9px;background:#10171d;color:#82929f;font-size:10px;font-weight:800;letter-spacing:1px}
QWidget#iwcRoot QTabBar::tab:selected{background:#18242d;color:#fff;border-bottom:2px solid #e30613}
)");
		auto *layout = new QVBoxLayout(root);
		layout->setContentsMargins(8, 8, 8, 8);
		layout->setSpacing(5);
		auto *eventLabel = new QLabel(QString("%1  vs  %2").arg(context.event.value("teamAbbreviation").toString(), context.event.value("opponentAbbreviation").toString()));
		eventLabel->setObjectName("title");
		eventLabel->setStyleSheet("font-size:15px;letter-spacing:1px");
		layout->addWidget(eventLabel);
		auto *statusLabel = new QLabel("●  PROGRAM GRAPHICS");
		statusLabel->setObjectName("scorebugStatus");
		layout->addWidget(statusLabel);

		auto *tabs = new QTabWidget;
		auto *gamePage = new QWidget;
		auto *gameLayout = new QVBoxLayout(gamePage);
		gameLayout->setContentsMargins(6, 6, 6, 6);
		gameLayout->setSpacing(5);
		auto *scoreBox = new QGroupBox("SCORE & SHOTS ON GOAL");
		auto *scores = new QGridLayout(scoreBox);
		scores->setContentsMargins(5, 5, 5, 5);
		scores->setHorizontalSpacing(6);
		scores->setVerticalSpacing(3);
		homeScore = new QSpinBox;
		homeScore->setRange(0, 999);
		homeScore->setValue(context.event.value("homeScore").toInt());
		awayScore = new QSpinBox;
		awayScore->setRange(0, 999);
		awayScore->setValue(context.event.value("awayScore").toInt());
		homeShots = new QSpinBox;
		homeShots->setRange(0, 999);
		homeShots->setValue(context.event.value("homeShots").toInt());
		awayShots = new QSpinBox;
		awayShots->setRange(0, 999);
		awayShots->setValue(context.event.value("awayShots").toInt());
		auto scoreControl = [](QSpinBox *value, const std::function<void(int)> &changed, bool goal) {
			auto *widget = new QWidget;
			auto *row = new QHBoxLayout(widget);
			row->setContentsMargins(0, 0, 0, 0);
			row->setSpacing(3);
			auto *minus = new QPushButton("−");
			auto *plus = new QPushButton("+");
			minus->setObjectName("scoreMinus");
			plus->setObjectName(goal ? "scorePlus" : "scoreMinus");
			minus->setFixedSize(24, 24);
			plus->setFixedSize(24, 24);
			value->setFixedWidth(44);
			row->addWidget(minus);
			row->addWidget(value);
			row->addWidget(plus);
			QObject::connect(minus, &QPushButton::clicked, widget, [value, changed] { value->setValue(value->value() - 1); changed(value->value()); });
			QObject::connect(plus, &QPushButton::clicked, widget, [value, changed] { value->setValue(value->value() + 1); changed(value->value()); });
			return widget;
		};
		auto *homeLabel = new QLabel(context.event.value("teamAbbreviation").toString());
		auto *awayLabel = new QLabel(context.event.value("opponentAbbreviation").toString());
		homeLabel->setObjectName("scoreTeam");
		awayLabel->setObjectName("scoreTeam");
		homeLabel->setAlignment(Qt::AlignCenter);
		awayLabel->setAlignment(Qt::AlignCenter);
		scores->addWidget(new QLabel(""), 0, 0);
		scores->addWidget(homeLabel, 0, 1);
		scores->addWidget(awayLabel, 0, 2);
		scores->addWidget(new QLabel("SCORE"), 1, 0);
		scores->addWidget(scoreControl(homeScore, [this](int) { pushScore(); }, true), 1, 1);
		scores->addWidget(scoreControl(awayScore, [this](int) { pushScore(); }, true), 1, 2);
		scores->addWidget(new QLabel("SOG"), 2, 0);
		scores->addWidget(scoreControl(homeShots, [this](int) { pushShots(); }, false), 2, 1);
		scores->addWidget(scoreControl(awayShots, [this](int) { pushShots(); }, false), 2, 2);
		gameLayout->addWidget(scoreBox);
		connect(homeScore, &QSpinBox::editingFinished, this, &ScorebugController::pushScore);
		connect(awayScore, &QSpinBox::editingFinished, this, &ScorebugController::pushScore);
		connect(homeShots, &QSpinBox::editingFinished, this, &ScorebugController::pushShots);
		connect(awayShots, &QSpinBox::editingFinished, this, &ScorebugController::pushShots);

		auto *clockBox = new QGroupBox("PERIOD & CLOCK");
		// Keep the five clock-control rows from collapsing into each other when
		// the Score Bug shares the right rail with Live Comments.
		clockBox->setMinimumHeight(190);
		clockBox->setMaximumHeight(190);
		auto *clockLayout = new QGridLayout(clockBox);
		clockLayout->setContentsMargins(5, 5, 5, 5);
		clockLayout->setHorizontalSpacing(4);
		clockLayout->setVerticalSpacing(4);
		for (int row = 0; row < 5; ++row)
			clockLayout->setRowMinimumHeight(row, 28);
		period = new QComboBox;
		period->addItems({"Pregame", "1st", "1st Intermission", "2nd", "2nd Intermission", "3rd", "Overtime", "Shootout", "Final"});
		period->setCurrentText(context.event.value("period").toString("Pregame"));
		clockEdit = new QLineEdit(formatClock(localSeconds));
		clockEdit->setPlaceholderText("MM:SS");
		auto *setClock = new QPushButton("SET");
		auto *startClock = new QPushButton("START");
		auto *pauseClock = new QPushButton("PAUSE");
		for (auto *control : {static_cast<QWidget *>(period), static_cast<QWidget *>(clockEdit),
				      static_cast<QWidget *>(setClock), static_cast<QWidget *>(startClock),
				      static_cast<QWidget *>(pauseClock)})
			control->setFixedHeight(28);
		clockLayout->addWidget(period, 0, 0, 1, 4);
		clockLayout->addWidget(clockEdit, 1, 0, 1, 3);
		clockLayout->addWidget(setClock, 1, 3);
		clockLayout->addWidget(startClock, 2, 0, 1, 2);
		clockLayout->addWidget(pauseClock, 2, 2, 1, 2);
		gameLayout->addWidget(clockBox);
		connect(period, &QComboBox::currentTextChanged, this, [this](const QString &value) {
			updateRenderer();
			call({{"action", "period"}, {"period", value}});
		});
		connect(setClock, &QPushButton::clicked, this, &ScorebugController::setClockFromEditor);
		connect(startClock, &QPushButton::clicked, this, [this] { clockRunning = true; call({{"action", "clock"}, {"operation", "start"}}); });
		connect(pauseClock, &QPushButton::clicked, this, [this] { clockRunning = false; call({{"action", "clock"}, {"operation", "stop"}}); });

		const QVector<QPair<QString, QString>> presetData = {{"PERIOD", "periodSeconds"}, {"INTERM.", "intermissionSeconds"}, {"TIMEOUT", "timeoutSeconds"}, {"MEDIA", "mediaTimeoutSeconds"}};
		for (int i = 0; i < presetData.size(); ++i) {
			auto *button = new QPushButton(presetData[i].first);
			button->setFixedHeight(28);
			button->setToolTip(i == 1 ? "Set the configured intermission time" : i == 3 ? "Set the configured media timeout" : QString());
			button->setStyleSheet("font-size:10px;padding:1px 3px");
			clockLayout->addWidget(button, 3 + i / 2, (i % 2) * 2, 1, 2);
			connect(button, &QPushButton::clicked, this, [this, key = presetData[i].second] {
				localSeconds = context.clockPreferences.value(key).toInt(key == "periodSeconds" ? 1200 : 60);
				clockRunning = false;
				clockEdit->setText(formatClock(localSeconds));
				updateRenderer();
				call({{"action", "clock"}, {"operation", "set"}, {"seconds", localSeconds}});
			});
		}

		auto *powerBox = new QGroupBox("POWER PLAY");
		auto *power = new QGridLayout(powerBox);
		powerPlayTeam = new QComboBox;
		powerPlayTeam->addItem("NONE", "");
		powerPlayTeam->addItem(context.event.value("teamAbbreviation").toString() + " · HOME", "home");
		powerPlayTeam->addItem(context.event.value("opponentAbbreviation").toString() + " · AWAY", "away");
		powerPlayTeam->setCurrentIndex(std::max(0, powerPlayTeam->findData(context.event.value("powerPlayTeam").toString())));
		powerPlayClockEdit = new QLineEdit(formatClock(powerPlaySeconds));
		powerPlayClockEdit->setPlaceholderText("MM:SS");
		auto *powerStart = new QPushButton("START");
		auto *powerPause = new QPushButton("PAUSE");
		auto *powerClear = new QPushButton("CLEAR");
		power->addWidget(powerPlayTeam, 0, 0, 1, 2);
		power->addWidget(powerPlayClockEdit, 0, 2);
		power->addWidget(powerStart, 1, 0);
		power->addWidget(powerPause, 1, 1);
		power->addWidget(powerClear, 1, 2);
		connect(powerStart, &QPushButton::clicked, this, [this] { setPowerPlayFromEditor(true); });
		connect(powerPause, &QPushButton::clicked, this, [this] { setPowerPlayFromEditor(false); });
		connect(powerClear, &QPushButton::clicked, this, [this] { powerPlaySeconds = 0; powerPlayRunning = false; powerPlayTeam->setCurrentIndex(0); powerPlayClockEdit->setText("00:00"); pushPowerPlay(); });
		gameLayout->addWidget(powerBox);
		gameLayout->addStretch();
		tabs->addTab(gamePage, "GAME");

		auto *lookPage = new QWidget;
		auto *lookLayout = new QVBoxLayout(lookPage);
		lookLayout->setContentsMargins(6, 6, 6, 6);
		lookLayout->setSpacing(5);
		auto *styleBox = new QGroupBox("SCOREBUG STYLE");
		auto *styleLayout = new QFormLayout(styleBox);
		templateBox = new QComboBox;
		templateBox->addItem("BOLD · STACKED", "bold");
		templateBox->addItem("CHAMPION · HORIZONTAL", "champion");
		templateBox->addItem("STANDARD · CLEAN", "standard");
		templateBox->setCurrentIndex(std::max(0, templateBox->findData(context.event.value("scoreBugTemplate").toString("bold"))));
		styleLayout->addRow("TEMPLATE", templateBox);
		lookLayout->addWidget(styleBox);

		auto *colorsBox = new QGroupBox("FULL PANEL COLORS");
		auto *colorsLayout = new QFormLayout(colorsBox);
		auto addColorControl = [this, colorsLayout](const QString &label, QString *value, QPushButton **target) {
			auto *button = new QPushButton;
			styleColorButton(button, *value);
			connect(button, &QPushButton::clicked, this, [this, button, value, label] {
				const QColor selected = QColorDialog::getColor(QColor(*value), main, QString("Choose %1 color").arg(label));
				if (!selected.isValid())
					return;
				*value = selected.name(QColor::HexRgb).toUpper();
				styleColorButton(button, *value);
				pushDisplay();
			});
			colorsLayout->addRow(label, button);
			*target = button;
		};
		addColorControl("BACKGROUND", &backgroundColor, &backgroundColorButton);
		addColorControl("INFO PANEL", &panelColor, &panelColorButton);
		addColorControl("TEXT", &textColor, &textColorButton);
		addColorControl("ACCENT", &accentColor, &accentColorButton);
		auto *resetColors = new QPushButton("RESET TO ICEWIRE");
		connect(resetColors, &QPushButton::clicked, this, [this] {
			backgroundColor = "#071019";
			panelColor = "#111B23";
			textColor = "#FFFFFF";
			accentColor = "#E30613";
			styleColorButton(backgroundColorButton, backgroundColor);
			styleColorButton(panelColorButton, panelColor);
			styleColorButton(textColorButton, textColor);
			styleColorButton(accentColorButton, accentColor);
			pushDisplay();
		});
		colorsLayout->addRow(resetColors);
		lookLayout->addWidget(colorsBox);

		auto *sponsorBox = new QGroupBox("SPONSOR PANEL");
		auto *sponsorLayout = new QVBoxLayout(sponsorBox);
		sponsorVisible = new QCheckBox("Show sponsor beside scorebug");
		sponsorVisible->setChecked(context.event.value("scoreBugSponsorVisible").toBool(false));
		sponsor = new QComboBox;
		sponsor->addItem("NO SPONSOR", "");
		for (const auto value : sponsors) {
			const QJsonObject asset = value.toObject();
			sponsor->addItem(asset.value("name").toString(), asset.value("id").toString());
		}
		sponsor->setCurrentIndex(std::max(0, sponsor->findData(context.event.value("scoreBugSponsorAssetId").toString())));
		sponsorLayout->addWidget(sponsorVisible);
		sponsorLayout->addWidget(sponsor);
		lookLayout->addWidget(sponsorBox);

		auto *displayBox = new QGroupBox("PLACEMENT");
		auto *display = new QGridLayout(displayBox);
		display->setContentsMargins(5, 5, 5, 5);
		display->setHorizontalSpacing(6);
		display->setVerticalSpacing(3);
		enabled = new QCheckBox("Show scorebug on program");
		enabled->setChecked(context.event.value("scoreBugEnabled").toBool(true));
		position = new QComboBox;
		position->addItems({"top-left", "top-center", "top-right", "bottom-left", "bottom-center", "bottom-right"});
		position->setCurrentText(context.event.value("scoreBugPosition").toString("top-center"));
		scale = new QSlider(Qt::Horizontal);
		scale->setRange(60, 140);
		scale->setValue(qRound(context.event.value("scoreBugScale").toDouble(1.0) * 100.0));
		display->addWidget(enabled, 0, 0, 1, 2);
		display->addWidget(new QLabel("POSITION"), 1, 0);
		display->addWidget(position, 1, 1);
		display->addWidget(new QLabel("SIZE"), 2, 0);
		display->addWidget(scale, 2, 1);
		lookLayout->addWidget(displayBox);
		lookLayout->addStretch();
		tabs->addTab(lookPage, "LOOK");
		layout->addWidget(tabs);
		for (QObject *control : {static_cast<QObject *>(enabled), static_cast<QObject *>(position), static_cast<QObject *>(scale), static_cast<QObject *>(templateBox), static_cast<QObject *>(sponsorVisible), static_cast<QObject *>(sponsor)}) {
			if (auto *check = qobject_cast<QCheckBox *>(control)) connect(check, &QCheckBox::toggled, this, &ScorebugController::pushDisplay);
			if (auto *combo = qobject_cast<QComboBox *>(control)) connect(combo, &QComboBox::currentTextChanged, this, &ScorebugController::pushDisplay);
			if (auto *slider = qobject_cast<QSlider *>(control)) connect(slider, &QSlider::sliderReleased, this, &ScorebugController::pushDisplay);
		}
		dock->setWidget(root);
		updateRenderer();
		return dock;
	}

	void updateRenderer()
	{
		if (!context.isGame())
			return;
		const QString homePrimaryHex = normalizedHexColor(context.event.value("teamPrimaryColor").toString(), "#2E5D8A");
		const QString homeSecondaryHex = normalizedHexColor(context.event.value("teamSecondaryColor").toString(), "#79B9FF");
		const QString awayPrimaryHex = normalizedHexColor(context.event.value("opponentPrimaryColor").toString(), "#6B6B6B");
		const QString awaySecondaryHex = normalizedHexColor(context.event.value("opponentSecondaryColor").toString(), "#E30613");
		const uint32_t homeColor = colorValue(homePrimaryHex, 0xFF8A5D2Eu);
		const uint32_t homeSecondary = colorValue(homeSecondaryHex, 0xFFFFB979u);
		const uint32_t awayColor = colorValue(awayPrimaryHex, 0xFF6B6B6Bu);
		const uint32_t awaySecondary = colorValue(awaySecondaryHex, 0xFF1306E3u);
		const uint32_t customBackground = colorValue(backgroundColor, 0xFF191007u);
		const uint32_t customPanel = colorValue(panelColor, 0xFF231B11u);
		const uint32_t customText = colorValue(textColor, 0xFFFFFFFFu);
		const uint32_t customAccent = colorValue(accentColor, 0xFF1306E3u);
		const QString selectedTemplate = templateBox ? templateBox->currentData().toString() : context.event.value("scoreBugTemplate").toString("bold");
		const bool hasSponsor = sponsorVisible && sponsorVisible->isChecked() && sponsor && !sponsor->currentData().toString().isEmpty();
		const bool powerActive = powerPlayTeam && !powerPlayTeam->currentData().toString().isEmpty() && powerPlaySeconds > 0;
		const QString sponsorImagePath = hasSponsor ? cachedRemoteImage(selectedSponsorUrl(), "sponsor") : QString{};
		const QString sponsorName = hasSponsor ? sponsor->currentText().toUpper() : QString{};
		int bugWidth = selectedTemplate == "champion" ? (hasSponsor ? 920 : 770) : selectedTemplate == "standard" ? (hasSponsor ? 850 : 700) : (hasSponsor ? 750 : 560);
		int bugHeight = selectedTemplate == "bold" ? 166 : selectedTemplate == "champion" ? 112 : 122;
		auto shadow = createOrUpdateColor("IWC SCOREBUG • SHADOW", colorValue("#020406", 0xFF060402u), bugWidth, bugHeight);
		auto background = createOrUpdateColor("IWC SCOREBUG • BACKGROUND", customBackground, bugWidth, selectedTemplate == "bold" ? 112 : 82);
		auto brandPanel = createOrUpdateColor("IWC SCOREBUG • BRAND PANEL", customBackground, selectedTemplate == "bold" ? 464 : selectedTemplate == "champion" ? 118 : 100, selectedTemplate == "bold" ? 24 : selectedTemplate == "champion" ? 82 : 25);
		auto homePanel = createOrUpdateColor("IWC SCOREBUG • HOME PANEL", homeColor, selectedTemplate == "bold" ? 360 : selectedTemplate == "champion" ? 220 : 250, selectedTemplate == "bold" ? 56 : 70);
		auto homeScorePanel = createOrUpdateColor("IWC SCOREBUG • HOME SCORE PANEL", homeSecondary, selectedTemplate == "bold" ? 58 : 60, selectedTemplate == "bold" ? 56 : 70);
		auto awayScorePanel = createOrUpdateColor("IWC SCOREBUG • AWAY SCORE PANEL", awaySecondary, selectedTemplate == "bold" ? 58 : 60, selectedTemplate == "bold" ? 56 : 70);
		auto awayPanel = createOrUpdateColor("IWC SCOREBUG • AWAY PANEL", awayColor, selectedTemplate == "bold" ? 360 : selectedTemplate == "champion" ? 220 : 250, selectedTemplate == "bold" ? 56 : 70);
		auto infoPanel = createOrUpdateColor("IWC SCOREBUG • INFO PANEL", customPanel, selectedTemplate == "bold" ? 96 : selectedTemplate == "champion" ? 92 : 80, selectedTemplate == "bold" ? 112 : 70);
		auto sponsorPanel = createOrUpdateColor("IWC SCOREBUG • SPONSOR PANEL", customPanel, selectedTemplate == "bold" ? 190 : 150, selectedTemplate == "bold" ? 112 : 70);
		auto powerPanel = createOrUpdateColor("IWC SCOREBUG • POWER PANEL", powerPlayTeam && powerPlayTeam->currentData().toString() == "away" ? awayColor : homeColor, selectedTemplate == "bold" ? 464 : selectedTemplate == "champion" ? 652 : 700, 30);
		auto legacyLiveBar = createOrUpdateColor("IWC SCOREBUG • LIVE BAR", customAccent, 1, 1);
		auto homeAccent = createOrUpdateColor("IWC SCOREBUG • HOME ACCENT", homeSecondary, selectedTemplate == "bold" ? 5 : selectedTemplate == "champion" ? 220 : 250, 5);
		auto awayAccent = createOrUpdateColor("IWC SCOREBUG • AWAY ACCENT", awaySecondary, selectedTemplate == "bold" ? 5 : selectedTemplate == "champion" ? 220 : 250, 5);
		auto homeTeam = createOrUpdateText("IWC SCOREBUG • HOME TEAM", context.event.value("teamAbbreviation").toString().toUpper(), 28, contrastingTextColor(homePrimaryHex));
		auto homeScoreSource = createOrUpdateText("IWC SCOREBUG • HOME SCORE", homeScore ? QString::number(homeScore->value()) : QString::number(context.event.value("homeScore").toInt()), 40, contrastingTextColor(homeSecondaryHex));
		auto awayTeam = createOrUpdateText("IWC SCOREBUG • AWAY TEAM", context.event.value("opponentAbbreviation").toString().toUpper(), 28, contrastingTextColor(awayPrimaryHex));
		auto awayScoreSource = createOrUpdateText("IWC SCOREBUG • AWAY SCORE", awayScore ? QString::number(awayScore->value()) : QString::number(context.event.value("awayScore").toInt()), 40, contrastingTextColor(awaySecondaryHex));
		auto homeSogSource = createOrUpdateText("IWC SCOREBUG • HOME SOG", QString("SOG  %1").arg(homeShots ? homeShots->value() : context.event.value("homeShots").toInt()), 14, contrastingTextColor(homePrimaryHex));
		auto awaySogSource = createOrUpdateText("IWC SCOREBUG • AWAY SOG", QString("SOG  %1").arg(awayShots ? awayShots->value() : context.event.value("awayShots").toInt()), 14, contrastingTextColor(awayPrimaryHex));
		auto periodSource = createOrUpdateText("IWC SCOREBUG • PERIOD", period ? period->currentText().toUpper() : context.event.value("period").toString().toUpper(), 15, customText);
		auto clockSource = createOrUpdateText("IWC SCOREBUG • CLOCK", formatClock(localSeconds), 28, customText);
		auto brandSource = createOrUpdateText("IWC SCOREBUG • BRAND", selectedTemplate == "bold" ? "ICEWIRE HOCKEY" : "IW", selectedTemplate == "bold" ? 16 : 24);
		auto liveSource = createOrUpdateText("IWC SCOREBUG • LIVE", "LIVE", 14, colorValue("#ff3440", 0xFF4034FFu));
		auto sponsorText = createOrUpdateText("IWC SCOREBUG • SPONSOR TEXT", sponsorName, 16, customText);
		const uint32_t powerTextColor = powerPlayTeam && powerPlayTeam->currentData().toString() == "away" ? contrastingTextColor(awayPrimaryHex) : contrastingTextColor(homePrimaryHex);
		auto powerTeamText = createOrUpdateText("IWC SCOREBUG • POWER TEAM", powerPlayTeam ? powerPlayTeam->currentText().section(" ·", 0, 0) : QString{}, 15, powerTextColor);
		auto powerLabelText = createOrUpdateText("IWC SCOREBUG • POWER LABEL", "POWER PLAY", 14, powerTextColor);
		auto powerClockText = createOrUpdateText("IWC SCOREBUG • POWER CLOCK", formatClock(powerPlaySeconds), 21, powerTextColor);
		auto homeLogo = createOrUpdateImage("IWC SCOREBUG • HOME LOGO", homeLogoPath);
		auto awayLogo = createOrUpdateImage("IWC SCOREBUG • AWAY LOGO", awayLogoPath);
		auto sponsorLogo = createOrUpdateImage("IWC SCOREBUG • SPONSOR LOGO", sponsorImagePath);
		auto icewireLogo = createOrUpdateImage("IWC SCOREBUG • ICEWIRE LIVE", icewireLogoPath);

		struct obs_video_info video{};
		obs_get_video_info(&video);
		const float bugScale = scale ? float(scale->value()) / 100.0f : float(context.event.value("scoreBugScale").toDouble(1.0));
		const QString where = position ? position->currentText() : context.event.value("scoreBugPosition").toString("top-center");
		const float width = float(bugWidth) * bugScale;
		const float height = float(bugHeight) * bugScale;
		float x = 34.0f, y = 34.0f;
		if (where.contains("center")) x = (float(video.base_width) - width) / 2.0f;
		if (where.endsWith("right")) x = float(video.base_width) - width - 34.0f;
		if (where.startsWith("bottom")) y = float(video.base_height) - height - 34.0f;
		SceneApplyData data;
		data.originX = x; data.originY = y; data.visible = enabled ? enabled->isChecked() : context.event.value("scoreBugEnabled").toBool(true);
		if (selectedTemplate == "bold") {
			data.placements = {{"IWC SCOREBUG • SHADOW", 5, 7, bugScale}, {"IWC SCOREBUG • BACKGROUND", 0, 24, bugScale},
				{"IWC SCOREBUG • BRAND PANEL", 96, 0, bugScale}, {"IWC SCOREBUG • INFO PANEL", 0, 24, bugScale},
				{"IWC SCOREBUG • HOME PANEL", 96, 24, bugScale}, {"IWC SCOREBUG • HOME SCORE PANEL", 402, 24, bugScale},
				{"IWC SCOREBUG • AWAY PANEL", 96, 80, bugScale}, {"IWC SCOREBUG • AWAY SCORE PANEL", 402, 80, bugScale},
				{"IWC SCOREBUG • HOME ACCENT", 96, 24, bugScale}, {"IWC SCOREBUG • AWAY ACCENT", 96, 80, bugScale},
				{"IWC SCOREBUG • SPONSOR PANEL", 560, 24, bugScale, 0, 0, hasSponsor}, {"IWC SCOREBUG • POWER PANEL", 96, 136, bugScale, 0, 0, powerActive},
				{"IWC SCOREBUG • BRAND", 0, 0, bugScale, 0, 0, false}, {"IWC SCOREBUG • LIVE", 0, 0, bugScale, 0, 0, false}, {"IWC SCOREBUG • ICEWIRE LIVE", 106, 3, bugScale, 148, 18, !icewireLogoPath.isEmpty()},
				{"IWC SCOREBUG • CLOCK", 12, 48, bugScale}, {"IWC SCOREBUG • PERIOD", 24, 91, bugScale},
				{"IWC SCOREBUG • HOME LOGO", 104, 30, bugScale, 44, 44, !homeLogoPath.isEmpty()}, {"IWC SCOREBUG • AWAY LOGO", 104, 86, bugScale, 44, 44, !awayLogoPath.isEmpty()},
				{"IWC SCOREBUG • HOME TEAM", 158, 38, bugScale}, {"IWC SCOREBUG • AWAY TEAM", 158, 94, bugScale},
				{"IWC SCOREBUG • HOME SCORE", 416, 31, bugScale}, {"IWC SCOREBUG • AWAY SCORE", 416, 87, bugScale},
				{"IWC SCOREBUG • HOME SOG", 472, 43, bugScale}, {"IWC SCOREBUG • AWAY SOG", 472, 99, bugScale},
				{"IWC SCOREBUG • SPONSOR LOGO", 578, 43, bugScale, 154, 66, hasSponsor && !sponsorImagePath.isEmpty()}, {"IWC SCOREBUG • SPONSOR TEXT", 578, 68, bugScale, 0, 0, hasSponsor && sponsorImagePath.isEmpty()},
				{"IWC SCOREBUG • POWER TEAM", 109, 141, bugScale, 0, 0, powerActive}, {"IWC SCOREBUG • POWER LABEL", 202, 143, bugScale, 0, 0, powerActive}, {"IWC SCOREBUG • POWER CLOCK", 486, 139, bugScale, 0, 0, powerActive}};
		} else if (selectedTemplate == "champion") {
			data.placements = {{"IWC SCOREBUG • SHADOW", 5, 7, bugScale}, {"IWC SCOREBUG • BACKGROUND", 0, 0, bugScale},
				{"IWC SCOREBUG • BRAND PANEL", 0, 0, bugScale}, {"IWC SCOREBUG • HOME PANEL", 118, 0, bugScale}, {"IWC SCOREBUG • HOME SCORE PANEL", 338, 0, bugScale},
				{"IWC SCOREBUG • INFO PANEL", 398, 0, bugScale}, {"IWC SCOREBUG • AWAY SCORE PANEL", 490, 0, bugScale}, {"IWC SCOREBUG • AWAY PANEL", 550, 0, bugScale},
				{"IWC SCOREBUG • HOME ACCENT", 118, 0, bugScale}, {"IWC SCOREBUG • AWAY ACCENT", 550, 0, bugScale}, {"IWC SCOREBUG • SPONSOR PANEL", 770, 0, bugScale, 0, 0, hasSponsor},
				{"IWC SCOREBUG • POWER PANEL", 118, 82, bugScale, 0, 0, powerActive}, {"IWC SCOREBUG • BRAND", 0, 0, bugScale, 0, 0, false}, {"IWC SCOREBUG • LIVE", 0, 0, bugScale, 0, 0, false}, {"IWC SCOREBUG • ICEWIRE LIVE", 8, 23, bugScale, 102, 34, !icewireLogoPath.isEmpty()},
				{"IWC SCOREBUG • HOME LOGO", 128, 13, bugScale, 48, 48, !homeLogoPath.isEmpty()}, {"IWC SCOREBUG • HOME TEAM", 186, 15, bugScale}, {"IWC SCOREBUG • HOME SOG", 186, 48, bugScale}, {"IWC SCOREBUG • HOME SCORE", 355, 12, bugScale},
				{"IWC SCOREBUG • CLOCK", 410, 13, bugScale}, {"IWC SCOREBUG • PERIOD", 425, 48, bugScale}, {"IWC SCOREBUG • AWAY SCORE", 507, 12, bugScale}, {"IWC SCOREBUG • AWAY TEAM", 578, 15, bugScale}, {"IWC SCOREBUG • AWAY SOG", 578, 48, bugScale}, {"IWC SCOREBUG • AWAY LOGO", 710, 13, bugScale, 48, 48, !awayLogoPath.isEmpty()},
				{"IWC SCOREBUG • SPONSOR LOGO", 780, 8, bugScale, 130, 54, hasSponsor && !sponsorImagePath.isEmpty()}, {"IWC SCOREBUG • SPONSOR TEXT", 784, 26, bugScale, 0, 0, hasSponsor && sponsorImagePath.isEmpty()},
				{"IWC SCOREBUG • POWER TEAM", 132, 87, bugScale, 0, 0, powerActive}, {"IWC SCOREBUG • POWER LABEL", 238, 89, bugScale, 0, 0, powerActive}, {"IWC SCOREBUG • POWER CLOCK", 720, 85, bugScale, 0, 0, powerActive}};
		} else {
			data.placements = {{"IWC SCOREBUG • SHADOW", 5, 7, bugScale}, {"IWC SCOREBUG • BACKGROUND", 0, 0, bugScale}, {"IWC SCOREBUG • BRAND PANEL", 0, 0, bugScale, 0, 0, false},
				{"IWC SCOREBUG • HOME PANEL", 0, 0, bugScale}, {"IWC SCOREBUG • HOME SCORE PANEL", 250, 0, bugScale}, {"IWC SCOREBUG • INFO PANEL", 310, 0, bugScale}, {"IWC SCOREBUG • AWAY SCORE PANEL", 390, 0, bugScale}, {"IWC SCOREBUG • AWAY PANEL", 450, 0, bugScale},
				{"IWC SCOREBUG • HOME ACCENT", 0, 65, bugScale}, {"IWC SCOREBUG • AWAY ACCENT", 450, 65, bugScale}, {"IWC SCOREBUG • SPONSOR PANEL", 700, 0, bugScale, 0, 0, hasSponsor}, {"IWC SCOREBUG • POWER PANEL", 0, 92, bugScale, 0, 0, powerActive},
				{"IWC SCOREBUG • BRAND", 0, 0, bugScale, 0, 0, false}, {"IWC SCOREBUG • LIVE", 0, 0, bugScale, 0, 0, false}, {"IWC SCOREBUG • ICEWIRE LIVE", 303, 70, bugScale, 94, 20, !icewireLogoPath.isEmpty()}, {"IWC SCOREBUG • HOME LOGO", 12, 11, bugScale, 46, 46, !homeLogoPath.isEmpty()}, {"IWC SCOREBUG • HOME TEAM", 70, 19, bugScale}, {"IWC SCOREBUG • HOME SCORE", 268, 11, bugScale},
				{"IWC SCOREBUG • CLOCK", 322, 11, bugScale}, {"IWC SCOREBUG • PERIOD", 329, 46, bugScale}, {"IWC SCOREBUG • AWAY SCORE", 408, 11, bugScale}, {"IWC SCOREBUG • AWAY TEAM", 478, 19, bugScale}, {"IWC SCOREBUG • AWAY LOGO", 640, 11, bugScale, 46, 46, !awayLogoPath.isEmpty()},
				{"IWC SCOREBUG • HOME SOG", 80, 72, bugScale}, {"IWC SCOREBUG • AWAY SOG", 548, 72, bugScale}, {"IWC SCOREBUG • SPONSOR LOGO", 710, 8, bugScale, 130, 54, hasSponsor && !sponsorImagePath.isEmpty()}, {"IWC SCOREBUG • SPONSOR TEXT", 714, 26, bugScale, 0, 0, hasSponsor && sponsorImagePath.isEmpty()},
				{"IWC SCOREBUG • POWER TEAM", 14, 97, bugScale, 0, 0, powerActive}, {"IWC SCOREBUG • POWER LABEL", 120, 99, bugScale, 0, 0, powerActive}, {"IWC SCOREBUG • POWER CLOCK", 638, 95, bugScale, 0, 0, powerActive}};
		}
		data.placements.push_back({"IWC SCOREBUG • LIVE BAR", 0, 0, bugScale, 0, 0, false});
		if (!scoreBugScene)
			return;
		SceneApplyData componentData = data;
		componentData.originX = 0;
		componentData.originY = 0;
		componentData.visible = true;
		for (auto &placement : componentData.placements)
			placement.scale = 1.0f;
		obs_source_t *scoreBugSource = obs_scene_get_source(scoreBugScene);
		applySourcesToScene(&componentData, scoreBugSource);
		ScoreBugApplyData outputData;
		outputData.scoreBugSource = scoreBugSource;
		outputData.x = x;
		outputData.y = y;
		outputData.scale = bugScale;
		outputData.visible = data.visible;
		for (const auto &placement : data.placements)
			outputData.legacyNames.push_back(placement.name);
		obs_enum_scenes(applyScoreBugToScene, &outputData);
	}

private:
	void call(QJsonObject payload)
	{
		payload.insert("eventId", context.id());
		QString error;
		runtime().cloud->call(payload, &error);
		if (!error.isEmpty())
			QMessageBox::warning(main, "IWC control", error);
	}
	void pushScore()
	{
		updateRenderer();
		call({{"action", "score"}, {"homeScore", homeScore->value()}, {"awayScore", awayScore->value()}});
	}
	void pushShots()
	{
		updateRenderer();
		call({{"action", "shots"}, {"homeShots", homeShots->value()}, {"awayShots", awayShots->value()}});
	}
	void setClockFromEditor()
	{
		const QStringList parts = clockEdit->text().trimmed().split(':');
		bool minutesOk = false, secondsOk = false;
		int minutes = parts.value(0).toInt(&minutesOk);
		int seconds = parts.size() > 1 ? parts.value(1).toInt(&secondsOk) : 0;
		if (!minutesOk || (parts.size() > 1 && !secondsOk) || seconds > 59) {
			QMessageBox::warning(main, "Clock", "Enter the time as MM:SS.");
			return;
		}
		localSeconds = std::clamp(minutes * 60 + seconds, 0, 5999);
		clockRunning = false;
		clockEdit->setText(formatClock(localSeconds));
		updateRenderer();
		call({{"action", "clock"}, {"operation", "set"}, {"seconds", localSeconds}});
	}
	void setPowerPlayFromEditor(bool running)
	{
		int parsed = 0;
		if (!parseTime(powerPlayClockEdit->text(), &parsed)) {
			QMessageBox::warning(main, "Power play", "Enter the power-play time as MM:SS.");
			return;
		}
		if (running && (powerPlayTeam->currentData().toString().isEmpty() || parsed <= 0)) {
			QMessageBox::warning(main, "Power play", "Choose the team on the power play and enter a time.");
			return;
		}
		powerPlaySeconds = parsed;
		powerPlayRunning = running && parsed > 0;
		pushPowerPlay();
	}
	void pushPowerPlay()
	{
		updateRenderer();
		call({{"action", "power_play"}, {"team", powerPlayTeam ? powerPlayTeam->currentData().toString() : QString{}}, {"seconds", powerPlaySeconds}, {"running", powerPlayRunning}});
	}
	bool parseTime(const QString &value, int *result) const
	{
		const QStringList parts = value.trimmed().split(':');
		bool minutesOk = false, secondsOk = parts.size() == 1;
		const int minutes = parts.value(0).toInt(&minutesOk);
		const int seconds = parts.size() > 1 ? parts.value(1).toInt(&secondsOk) : 0;
		if (!minutesOk || !secondsOk || seconds < 0 || seconds > 59)
			return false;
		*result = std::clamp(minutes * 60 + seconds, 0, 5999);
		return true;
	}
	QString selectedSponsorUrl() const
	{
		if (!sponsor)
			return {};
		const QString id = sponsor->currentData().toString();
		for (const auto value : sponsors) {
			const QJsonObject asset = value.toObject();
			if (asset.value("id").toString() == id)
				return asset.value("url").toString();
		}
		return {};
	}
	void pushDisplay()
	{
		updateRenderer();
		call({{"action", "scorebug"}, {"enabled", enabled->isChecked()}, {"position", position->currentText()}, {"scale", double(scale->value()) / 100.0}, {"template", templateBox->currentData().toString()}, {"sponsorVisible", sponsorVisible->isChecked()}, {"sponsorAssetId", sponsor->currentData().toString()}, {"backgroundColor", backgroundColor}, {"panelColor", panelColor}, {"textColor", textColor}, {"accentColor", accentColor}});
	}

	OBSBasic *main = nullptr;
	Iwc::EventContext context;
	QSpinBox *homeScore = nullptr;
	QSpinBox *awayScore = nullptr;
	QSpinBox *homeShots = nullptr;
	QSpinBox *awayShots = nullptr;
	QComboBox *period = nullptr;
	QLineEdit *clockEdit = nullptr;
	QCheckBox *enabled = nullptr;
	QComboBox *position = nullptr;
	QSlider *scale = nullptr;
	QComboBox *templateBox = nullptr;
	QPushButton *backgroundColorButton = nullptr;
	QPushButton *panelColorButton = nullptr;
	QPushButton *textColorButton = nullptr;
	QPushButton *accentColorButton = nullptr;
	QComboBox *sponsor = nullptr;
	QCheckBox *sponsorVisible = nullptr;
	QComboBox *powerPlayTeam = nullptr;
	QLineEdit *powerPlayClockEdit = nullptr;
	QJsonArray sponsors;
	QString homeLogoPath;
	QString awayLogoPath;
	QString icewireLogoPath;
	QString backgroundColor;
	QString panelColor;
	QString textColor;
	QString accentColor;
	OBSSceneAutoRelease scoreBugScene;
	int localSeconds = 0;
	int powerPlaySeconds = 0;
	bool clockRunning = false;
	bool powerPlayRunning = false;
	QTimer ticker;
};

class MediaLibraryController final : public QObject {
public:
	MediaLibraryController(OBSBasic *main, const Iwc::EventContext &context, QObject *parent = nullptr)
		: QObject(parent), main(main), context(context)
	{
	}

	QDockWidget *buildDock()
	{
		auto *dock = new QDockWidget("IWC · MEDIA & TEMPLATES", main);
		dock->setObjectName("iwcMediaDock");
		dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);
		dock->setMinimumWidth(280);
		dock->setMaximumWidth(340);
		auto *root = new QWidget;
		root->setObjectName("iwcRoot");
		root->setStyleSheet(QString::fromUtf8(kAppStyle) + R"(
QWidget#iwcRoot{font-size:12px}
QWidget#iwcRoot QTreeWidget{font-size:12px;border:1px solid #34414a;background:#060b0f;outline:0}
QWidget#iwcRoot QTreeWidget::item{min-height:30px;border-bottom:1px solid #1d2a33;padding:3px}
QWidget#iwcRoot QTreeWidget::item:selected{background:#173047;border-left:3px solid #e30613}
QWidget#iwcRoot QPushButton{min-height:27px;max-height:30px;padding:2px 8px;font-size:10px}
QWidget#iwcRoot QComboBox{min-height:28px;max-height:30px}
)");
		auto *layout = new QVBoxLayout(root);
		layout->setContentsMargins(8, 8, 8, 8);
		layout->setSpacing(6);
		auto *heading = new QHBoxLayout;
		auto *titles = new QVBoxLayout;
		auto *eyebrow = new QLabel("TEAM CLOUD PRODUCTION LIBRARY");
		eyebrow->setObjectName("eyebrow");
		titles->addWidget(eyebrow);
		auto *copy = new QLabel("Commercials · sponsors · graphics · intros · audio");
		copy->setObjectName("muted");
		titles->addWidget(copy);
		heading->addLayout(titles, 1);
		categoryFilter = new QComboBox;
		categoryFilter->addItem("ALL MEDIA", "all");
		categoryFilter->addItem("COMMERCIALS", "commercial");
		categoryFilter->addItem("SPONSORS", "sponsor");
		categoryFilter->addItem("IN-GAME GRAPHICS", "graphic");
		categoryFilter->addItem("INTROS", "intro");
		categoryFilter->addItem("AUDIO", "music");
		categoryFilter->addItem("OTHER", "other");
		heading->addWidget(categoryFilter);
		layout->addLayout(heading);

		tree = new QTreeWidget;
		tree->setColumnCount(4);
		tree->setHeaderLabels({"ASSET", "CATEGORY", "USE", "SIZE"});
		tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
		tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
		tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
		tree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
		tree->setSelectionMode(QAbstractItemView::SingleSelection);
		tree->setRootIsDecorated(false);
		tree->setMinimumHeight(120);
		layout->addWidget(tree, 1);

		auto *actions = new QHBoxLayout;
		actions->setSpacing(4);
		auto *upload = new QPushButton("+ ADD FILE TO CLOUD");
		upload->setObjectName("primary");
		auto *add = new QPushButton("ADD TO PREVIEW");
		auto *remove = new QPushButton("DELETE");
		remove->setObjectName("danger");
		auto *refreshButton = new QPushButton("REFRESH");
		actions->addWidget(upload);
		actions->addWidget(add);
		actions->addWidget(remove);
		actions->addStretch();
		actions->addWidget(refreshButton);
		layout->addLayout(actions);
		feedback = new QLabel;
		feedback->setObjectName("muted");
		feedback->setWordWrap(true);
		layout->addWidget(feedback);
		dock->setWidget(root);

		connect(categoryFilter, &QComboBox::currentIndexChanged, this, &MediaLibraryController::populate);
		connect(refreshButton, &QPushButton::clicked, this, &MediaLibraryController::refresh);
		connect(upload, &QPushButton::clicked, this, &MediaLibraryController::upload);
		connect(remove, &QPushButton::clicked, this, &MediaLibraryController::remove);
		connect(add, &QPushButton::clicked, this, &MediaLibraryController::addToPreview);
		connect(tree, &QTreeWidget::itemDoubleClicked, this, [this] { addToPreview(); });
		refresh();
		return dock;
	}

private:
	QJsonObject selectedAsset() const
	{
		if (!tree || !tree->currentItem())
			return {};
		const QString id = tree->currentItem()->data(0, Qt::UserRole).toString();
		for (const auto entry : assets) {
			const auto asset = entry.toObject();
			if (asset.value("id").toString() == id)
				return asset;
		}
		return {};
	}

	void setBusy(bool value, const QString &message = {})
	{
		busy = value;
		if (tree)
			tree->setEnabled(!value);
		if (feedback && !message.isEmpty())
			feedback->setText(message);
		QApplication::processEvents();
	}

	void refresh()
	{
		if (busy)
			return;
		setBusy(true, "Loading the team's IWC cloud library…");
		QString error;
		const QJsonObject result = runtime().cloud->call({{"action", "media_library"}, {"eventId", context.id()}}, &error);
		if (!error.isEmpty()) {
			setBusy(false, error);
			return;
		}
		assets = result.value("assets").toArray();
		populate();
		setBusy(false, QString("%1 reusable production assets available on this computer.").arg(assets.size()));
	}

	void populate()
	{
		if (!tree)
			return;
		const QString previous = tree->currentItem() ? tree->currentItem()->data(0, Qt::UserRole).toString() : QString();
		tree->clear();
		const QString filter = categoryFilter ? categoryFilter->currentData().toString() : "all";
		QTreeWidgetItem *reselect = nullptr;
		for (const auto entry : assets) {
			const QJsonObject asset = entry.toObject();
			if (filter != "all" && asset.value("category").toString() != filter)
				continue;
			const QString use = asset.value("productionUse").toString() == "both" ? "BOTH" : asset.value("productionUse").toString() == "game" ? "GAMES" : "SHOWS";
			auto *item = new QTreeWidgetItem(tree, {asset.value("name").toString(), asset.value("category").toString().toUpper(), use,
								 QString("%1 MB").arg(asset.value("sizeBytes").toDouble() / 1024.0 / 1024.0, 0, 'f', 1)});
			item->setData(0, Qt::UserRole, asset.value("id").toString());
			item->setToolTip(0, "Double-click to add this cloud asset to the current Preview scene.");
			if (asset.value("id").toString() == previous)
				reselect = item;
		}
		if (reselect)
			tree->setCurrentItem(reselect);
		else if (tree->topLevelItemCount())
			tree->setCurrentItem(tree->topLevelItem(0));
	}

	void upload()
	{
		if (busy)
			return;
		const QString filePath = QFileDialog::getOpenFileName(main, "Add media to the IWC cloud library", {},
			"Production media (*.png *.jpg *.jpeg *.webp *.gif *.mp4 *.webm *.mp3 *.wav *.ogg)");
		if (filePath.isEmpty())
			return;
		const QFileInfo info(filePath);
		if (!info.exists() || info.size() < 1 || info.size() > 52'428'800) {
			QMessageBox::warning(main, "IWC media", "Choose a supported file up to 50 MB.");
			return;
		}
		const QString mime = QMimeDatabase().mimeTypeForFile(filePath, QMimeDatabase::MatchContent).name();
		const QStringList supported = {"image/png", "image/jpeg", "image/webp", "image/gif", "video/mp4", "video/webm", "audio/mpeg", "audio/wav", "audio/ogg"};
		if (!supported.contains(mime)) {
			QMessageBox::warning(main, "IWC media", "That file type is not supported. Use PNG, JPG, WebP, GIF, MP4, WebM, MP3, WAV, or OGG.");
			return;
		}
		MediaUploadDialog dialog(filePath, main);
		if (dialog.exec() != QDialog::Accepted)
			return;
		setBusy(true, QString("Preparing %1 for secure upload…").arg(info.fileName()));
		QString error;
		const QJsonObject metadata{{"name", dialog.assetName()}, {"category", dialog.assetCategory()}, {"productionUse", dialog.assetUse()},
					   {"fileName", info.fileName()}, {"mimeType", mime}, {"sizeBytes", double(info.size())}};
		QJsonObject begin = metadata;
		begin.insert("action", "media_create_upload");
		begin.insert("eventId", context.id());
		const QJsonObject started = runtime().cloud->call(begin, &error);
		if (error.isEmpty()) {
			setBusy(true, QString("Uploading %1 to IWC Cloud…").arg(info.fileName()));
			runtime().cloud->uploadSignedFile(started.value("path").toString(), started.value("token").toString(), filePath, mime, &error);
		}
		if (error.isEmpty()) {
			QJsonObject registration = metadata;
			registration.insert("action", "media_register");
			registration.insert("eventId", context.id());
			registration.insert("assetId", started.value("assetId"));
			registration.insert("path", started.value("path"));
			runtime().cloud->call(registration, &error);
		}
		setBusy(false, error.isEmpty() ? QString("%1 is now available in the team's IWC cloud library.").arg(dialog.assetName()) : error);
		if (error.isEmpty())
			refresh();
	}

	void remove()
	{
		if (busy)
			return;
		const QJsonObject asset = selectedAsset();
		if (asset.isEmpty()) {
			feedback->setText("Choose a media asset first.");
			return;
		}
		if (QMessageBox::question(main, "Delete cloud media", QString("Remove “%1” from this team's IWC library?").arg(asset.value("name").toString()), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
			return;
		setBusy(true, "Removing the selected media…");
		QString error;
		runtime().cloud->call({{"action", "media_delete"}, {"eventId", context.id()}, {"assetId", asset.value("id")}}, &error);
		setBusy(false, error.isEmpty() ? "Media removed from the team cloud library." : error);
		if (error.isEmpty())
			refresh();
	}

	void addToPreview()
	{
		if (busy)
			return;
		const QJsonObject asset = selectedAsset();
		if (asset.isEmpty()) {
			feedback->setText("Choose a media asset first.");
			return;
		}
		const QString url = asset.value("url").toString();
		const QString mime = asset.value("mimeType").toString();
		if (url.isEmpty()) {
			feedback->setText("This asset does not have a usable cloud playback link. Press Refresh and try again.");
			return;
		}
		OBSDataAutoRelease settings = obs_data_create();
		QString sourceType;
		if (mime.startsWith("image/")) {
			setBusy(true, "Downloading the selected graphic to this IWC workstation…");
			const QString path = cachedRemoteImage(url, QString("media-%1").arg(asset.value("id").toString().left(8)));
			if (path.isEmpty()) {
				setBusy(false, "The graphic could not be downloaded. Check the connection and try again.");
				return;
			}
			obs_data_set_string(settings, "file", path.toUtf8().constData());
			obs_data_set_bool(settings, "unload", false);
			sourceType = "image_source";
		} else {
			obs_data_set_bool(settings, "is_local_file", false);
			obs_data_set_string(settings, "input", url.toUtf8().constData());
			obs_data_set_bool(settings, "restart_on_activate", true);
			obs_data_set_bool(settings, "close_when_inactive", true);
			obs_data_set_bool(settings, "clear_on_media_end", true);
			sourceType = "ffmpeg_source";
		}
		const QByteArray sourceTypeUtf8 = sourceType.toUtf8();
		const char *latestType = obs_get_latest_input_type_id(sourceTypeUtf8.constData());
		if (!latestType)
			latestType = sourceTypeUtf8.constData();
		QString sourceName = QString("IWC %1 · %2").arg(asset.value("category").toString().toUpper(), asset.value("name").toString());
		const QString baseName = sourceName;
		for (int suffix = 2;; ++suffix) {
			OBSSourceAutoRelease existing = obs_get_source_by_name(sourceName.toUtf8().constData());
			if (!existing)
				break;
			sourceName = QString("%1 (%2)").arg(baseName).arg(suffix);
		}
		OBSSourceAutoRelease source = obs_source_create(latestType, sourceName.toUtf8().constData(), settings, nullptr);
		OBSScene scene = main ? main->GetCurrentScene() : nullptr;
		obs_sceneitem_t *item = source && scene ? obs_scene_add(scene, source) : nullptr;
		if (!item) {
			setBusy(false, "IWC could not add the selected asset to Preview.");
			return;
		}
		if (!mime.startsWith("audio/")) {
			struct obs_video_info video{};
			obs_get_video_info(&video);
			vec2 bounds{float(video.base_width), float(video.base_height)};
			obs_sceneitem_set_bounds_type(item, OBS_BOUNDS_SCALE_INNER);
			obs_sceneitem_set_bounds_alignment(item, OBS_ALIGN_CENTER);
			obs_sceneitem_set_bounds(item, &bounds);
		}
		obs_sceneitem_select(item, true);
		obs_sceneitem_set_order(item, OBS_ORDER_MOVE_TOP);
		setBusy(false, QString("%1 was added to Preview. Press Cut or Fade when it is ready for Program.").arg(asset.value("name").toString()));
	}

	QPointer<OBSBasic> main;
	Iwc::EventContext context;
	QPointer<QTreeWidget> tree;
	QPointer<QComboBox> categoryFilter;
	QPointer<QLabel> feedback;
	QJsonArray assets;
	bool busy = false;
};

class InstantReplayController final : public QObject {
public:
	explicit InstantReplayController(OBSBasic *main, QObject *parent = nullptr) : QObject(parent), main(main)
	{
		connect(main, &OBSBasic::ReplayBufStarted, this, [this] {
			if (status) status->setText("BUFFER READY · LAST 15 SECONDS");
			if (captureRequested) {
				captureRequested = false;
				QTimer::singleShot(250, this, [this] { capture(); });
			}
		});
		connect(main, &OBSBasic::ReplayBufStopped, this, [this] {
			if (status) status->setText("BUFFER STARTS AUTOMATICALLY WITH THE BROADCAST");
		});
	}

	QDockWidget *buildDock()
	{
		auto *dock = new QDockWidget("IWC · INSTANT REPLAY", main);
		dock->setObjectName("iwcInstantReplayDock");
		dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);
		dock->setMinimumWidth(250);
		auto *root = new QWidget;
		root->setObjectName("iwcRoot");
		root->setStyleSheet(QString::fromUtf8(kAppStyle) + R"(
QWidget#iwcRoot{font-size:12px}
QWidget#iwcRoot QPushButton{min-height:30px;max-height:34px;font-size:10px;letter-spacing:1px}
)");
		auto *layout = new QVBoxLayout(root);
		layout->setContentsMargins(8, 8, 8, 8);
		layout->setSpacing(6);
		status = new QLabel(main->ReplayBufferActive() ? "BUFFER READY · LAST 15 SECONDS" : "BUFFER STARTS AUTOMATICALLY WITH THE BROADCAST");
		status->setObjectName("muted");
		status->setWordWrap(true);
		layout->addWidget(status);
		captureButton = new QPushButton("CAPTURE INSTANT REPLAY");
		captureButton->setObjectName("primary");
		captureButton->setToolTip("Save the last 15 seconds and place the clip in Preview");
		layout->addWidget(captureButton);
		auto *copy = new QLabel("The saved replay opens in Preview. Press Cut or Fade when it is ready for Program.");
		copy->setObjectName("muted");
		copy->setWordWrap(true);
		layout->addWidget(copy);
		connect(captureButton, &QPushButton::clicked, this, &InstantReplayController::capture);
		dock->setWidget(root);
		return dock;
	}

private:
	QString lastReplayPath() const
	{
		char *path = obs_frontend_get_last_replay();
		const QString result = path ? QString::fromUtf8(path) : QString();
		bfree(path);
		return result;
	}

	void capture()
	{
		if (!main)
			return;
		if (!main->ReplayBufferActive()) {
			captureRequested = true;
			status->setText("STARTING THE 15-SECOND REPLAY BUFFER…");
			main->StartReplayBuffer();
			return;
		}
		previousReplay = lastReplayPath();
		pollAttempts = 0;
		captureButton->setEnabled(false);
		captureButton->setText("CAPTURING…");
		status->setText("SAVING THE LAST 15 SECONDS…");
		main->ReplayBufferSave();
		QTimer::singleShot(250, this, &InstantReplayController::pollForReplay);
	}

	void pollForReplay()
	{
		const QString path = lastReplayPath();
		if (!path.isEmpty() && path != previousReplay && QFileInfo::exists(path)) {
			addToPreview(path);
			return;
		}
		if (++pollAttempts < 32) {
			QTimer::singleShot(250, this, &InstantReplayController::pollForReplay);
			return;
		}
		captureButton->setEnabled(true);
		captureButton->setText("CAPTURE INSTANT REPLAY");
		status->setText("THE REPLAY TOOK TOO LONG TO SAVE · TRY AGAIN");
	}

	void addToPreview(const QString &path)
	{
		constexpr auto sourceName = "IWC Instant Replay";
		OBSDataAutoRelease settings = obs_data_create();
		obs_data_set_bool(settings, "is_local_file", true);
		obs_data_set_string(settings, "local_file", path.toUtf8().constData());
		obs_data_set_string(settings, "input", path.toUtf8().constData());
		obs_data_set_bool(settings, "restart_on_activate", true);
		obs_data_set_bool(settings, "close_when_inactive", true);
		obs_data_set_bool(settings, "clear_on_media_end", true);
		OBSSourceAutoRelease source = obs_get_source_by_name(sourceName);
		if (source) {
			obs_source_update(source, settings);
		} else {
			const char *sourceType = obs_get_latest_input_type_id("ffmpeg_source");
			if (!sourceType) sourceType = "ffmpeg_source";
			source = obs_source_create(sourceType, sourceName, settings, nullptr);
		}
		OBSScene scene = main->GetCurrentScene();
		obs_sceneitem_t *item = scene ? obs_scene_find_source(scene, sourceName) : nullptr;
		if (!item && source && scene) item = obs_scene_add(scene, source);
		if (!item) {
			captureButton->setEnabled(true);
			captureButton->setText("CAPTURE INSTANT REPLAY");
			status->setText("IWC COULD NOT PLACE THE REPLAY IN PREVIEW");
			return;
		}
		struct obs_video_info video{};
		obs_get_video_info(&video);
		vec2 bounds{float(video.base_width), float(video.base_height)};
		obs_sceneitem_set_bounds_type(item, OBS_BOUNDS_SCALE_INNER);
		obs_sceneitem_set_bounds_alignment(item, OBS_ALIGN_CENTER);
		obs_sceneitem_set_bounds(item, &bounds);
		obs_sceneitem_set_order(item, OBS_ORDER_MOVE_TOP);
		obs_sceneitem_set_visible(item, true);
		obs_source_media_restart(source);
		captureButton->setEnabled(true);
		captureButton->setText("CAPTURE INSTANT REPLAY");
		status->setText("REPLAY READY IN PREVIEW · PRESS CUT OR FADE");
	}

	QPointer<OBSBasic> main;
	QPointer<QLabel> status;
	QPointer<QPushButton> captureButton;
	QString previousReplay;
	int pollAttempts = 0;
	bool captureRequested = false;
};

class LeagueBoardController final : public QObject {
public:
	LeagueBoardController(OBSBasic *main, const Iwc::EventContext &context, QObject *parent = nullptr)
		: QObject(parent), main(main), context(context)
	{
	}

	QDockWidget *buildDock()
	{
		auto *dock = new QDockWidget("IWC · LEAGUE BOARD", main);
		dock->setObjectName("iwcLeagueBoardDock");
		dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);
		dock->setMinimumWidth(250);
		auto *root = new QWidget;
		root->setObjectName("iwcRoot");
		root->setStyleSheet(QString::fromUtf8(kAppStyle) + R"(
QWidget#iwcRoot{font-size:12px}
QWidget#iwcRoot QPushButton{min-height:30px;max-height:34px;font-size:10px;letter-spacing:1px}
)");
		auto *layout = new QVBoxLayout(root);
		layout->setContentsMargins(8, 8, 8, 8);
		layout->setSpacing(6);
		status = new QLabel("OFFICIAL STANDINGS · LIVE LEAGUE SCORES");
		status->setObjectName("muted");
		layout->addWidget(status);
		auto *buttons = new QHBoxLayout;
		showButton = new QPushButton("SHOW IN PREVIEW");
		showButton->setObjectName("primary");
		hideButton = new QPushButton("HIDE");
		buttons->addWidget(showButton, 1);
		buttons->addWidget(hideButton);
		layout->addLayout(buttons);
		auto *copy = new QLabel("Uses the league's official standings page and current ICEWIRE scores. Press Cut or Fade to take the board to Program.");
		copy->setObjectName("muted");
		copy->setWordWrap(true);
		layout->addWidget(copy);
		connect(showButton, &QPushButton::clicked, this, [this] { setVisible(true); });
		connect(hideButton, &QPushButton::clicked, this, [this] { setVisible(false); });
		dock->setWidget(root);
		return dock;
	}

private:
	obs_sceneitem_t *ensureSource()
	{
		constexpr auto sourceName = "IWC League Board";
		OBSScene scene = main ? main->GetCurrentScene() : nullptr;
		if (!scene)
			return nullptr;
		obs_sceneitem_t *item = obs_scene_find_source(scene, sourceName);
		if (item)
			return item;
		const QString url = QString("https://icewire-live-hockey.trevor981385.chatgpt.site/iwc/league-board/%1?display=iwc-v31").arg(context.id());
		OBSDataAutoRelease settings = obs_data_create();
		obs_data_set_string(settings, "url", url.toUtf8().constData());
		obs_data_set_int(settings, "width", 1200);
		obs_data_set_int(settings, "height", 720);
		obs_data_set_bool(settings, "shutdown", false);
		obs_data_set_bool(settings, "restart_when_active", true);
		obs_data_set_bool(settings, "reroute_audio", false);
		const char *sourceType = obs_get_latest_input_type_id("browser_source");
		if (!sourceType)
			sourceType = "browser_source";
		OBSSourceAutoRelease source = obs_source_create(sourceType, sourceName, settings, nullptr);
		item = source ? obs_scene_add(scene, source) : nullptr;
		if (!item)
			return nullptr;
		struct obs_video_info video{};
		obs_get_video_info(&video);
		vec2 bounds{float(video.base_width), float(video.base_height)};
		vec2 position{0.0f, 0.0f};
		obs_sceneitem_set_bounds_type(item, OBS_BOUNDS_SCALE_INNER);
		obs_sceneitem_set_bounds_alignment(item, OBS_ALIGN_LEFT | OBS_ALIGN_TOP);
		obs_sceneitem_set_bounds(item, &bounds);
		obs_sceneitem_set_pos(item, &position);
		obs_sceneitem_set_order(item, OBS_ORDER_MOVE_TOP);
		obs_sceneitem_set_locked(item, true);
		return item;
	}

	void setVisible(bool visible)
	{
		obs_sceneitem_t *item = visible ? ensureSource() : (main && main->GetCurrentScene() ? obs_scene_find_source(main->GetCurrentScene(), "IWC League Board") : nullptr);
		if (!item) {
			status->setText(visible ? "LEAGUE BOARD COULD NOT BE ADDED TO PREVIEW" : "LEAGUE BOARD IS NOT IN THIS SCENE");
			return;
		}
		obs_sceneitem_set_visible(item, visible);
		if (visible) {
			obs_sceneitem_set_order(item, OBS_ORDER_MOVE_TOP);
			obs_sceneitem_select(item, true);
			status->setText("READY IN PREVIEW · PRESS CUT OR FADE");
		} else {
			status->setText("HIDDEN IN PREVIEW");
		}
	}

	QPointer<OBSBasic> main;
	Iwc::EventContext context;
	QPointer<QLabel> status;
	QPointer<QPushButton> showButton;
	QPointer<QPushButton> hideButton;
};

class CommentsController final : public QObject {
public:
	CommentsController(OBSBasic *main, const Iwc::EventContext &context, QObject *parent = nullptr)
		: QObject(parent), main(main), context(context)
	{
		poller.setInterval(3000);
		connect(&poller, &QTimer::timeout, this, &CommentsController::refresh);
	}

	QDockWidget *buildDock()
	{
		auto *dock = new QDockWidget("IWC · LIVE COMMENTS", main);
		dock->setObjectName("iwcCommentsDock");
		dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);
		dock->setMinimumWidth(270);
		dock->setMaximumWidth(320);
		auto *root = new QWidget;
		root->setObjectName("iwcRoot");
		root->setStyleSheet(QString::fromUtf8(kAppStyle) + R"(
QWidget#iwcRoot{font-size:12px}
QWidget#iwcRoot QTreeWidget{font-size:13px;border:1px solid #34414a;background:#060b0f;outline:0}
QWidget#iwcRoot QTreeWidget::item{border-bottom:1px solid #1d2a33;padding:2px}
QWidget#iwcRoot QTreeWidget::item:selected{background:#173047;border-left:3px solid #e30613}
QWidget#iwcRoot QWidget#commentCard{background:transparent}
QWidget#iwcRoot QLabel#commentAuthor{font-size:13px;font-weight:800;color:#f4f7f9}
QWidget#iwcRoot QLabel#commentTime{font-size:10px;color:#82909a}
QWidget#iwcRoot QLabel#commentMessage{font-size:13px;color:#d7e0e6}
QWidget#iwcRoot QLabel#commentBadge{font-size:9px;font-weight:800;letter-spacing:1px;color:#8fb4d0}
QWidget#iwcRoot QPushButton{min-height:22px;max-height:24px;padding:1px 5px;font-size:9px}
)");
		auto *layout = new QVBoxLayout(root);
		layout->setContentsMargins(8, 8, 8, 8);
		layout->setSpacing(6);
		auto *heading = new QHBoxLayout;
		commentsEnabled = new QCheckBox("Allow fan comments");
		commentsEnabled->setChecked(context.event.value("commentsEnabled").toBool(false));
		connection = new QLabel("SYNCING…");
		connection->setObjectName("muted");
		heading->addWidget(commentsEnabled);
		heading->addStretch();
		heading->addWidget(connection);
		layout->addLayout(heading);
		tree = new QTreeWidget;
		tree->setColumnCount(1);
		tree->setHeaderHidden(true);
		tree->setRootIsDecorated(true);
		tree->setIndentation(14);
		tree->setWordWrap(true);
		tree->setUniformRowHeights(false);
		tree->setTextElideMode(Qt::ElideNone);
		tree->setMinimumHeight(140);
		layout->addWidget(tree, 1);
		auto *actions = new QHBoxLayout;
		actions->setContentsMargins(0, 0, 0, 0);
		actions->setSpacing(4);
		auto *reply = new QPushButton("REPLY");
		reply->setObjectName("primary");
		reply->setToolTip("Reply as the team");
		auto *hide = new QPushButton("HIDE");
		hide->setToolTip("Hide or restore the selected comment");
		auto *pin = new QPushButton("PIN");
		pin->setToolTip("Pin or unpin the selected comment");
		auto *remove = new QPushButton("DELETE");
		remove->setObjectName("danger");
		remove->setToolTip("Permanently delete the selected comment");
		for (auto *button : {reply, hide, pin, remove}) {
			button->setMinimumWidth(50);
			button->setMaximumWidth(62);
			actions->addWidget(button);
		}
		actions->addStretch();
		layout->addLayout(actions);
		dock->setWidget(root);

		connect(commentsEnabled, &QCheckBox::toggled, this, [this](bool enabled) {
			if (loading)
				return;
			call({{"action", "comments_toggle"}, {"enabled", enabled}});
		});
		connect(reply, &QPushButton::clicked, this, &CommentsController::reply);
		connect(hide, &QPushButton::clicked, this, &CommentsController::toggleHidden);
		connect(pin, &QPushButton::clicked, this, &CommentsController::togglePinned);
		connect(remove, &QPushButton::clicked, this, [this] {
			if (selectedId().isEmpty()) return;
			if (QMessageBox::question(main, "Delete comment", "Delete this comment from the conversation?", QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
				call({{"action", "comment_moderate"}, {"commentId", selectedId()}, {"status", "deleted"}});
		});
		refresh();
		poller.start();
		return dock;
	}

private:
	QString selectedId() const
	{
		return tree && tree->currentItem() ? tree->currentItem()->data(0, Qt::UserRole).toString() : QString();
	}
	void refresh()
	{
		// The cloud request runs a nested event loop. Keep shutdown safe if the
		// studio is closed while a comment refresh is in flight.
		QPointer<CommentsController> guard(this);
		QString error;
		const auto result = runtime().cloud->call({{"action", "comments"}, {"eventId", context.id()}}, &error);
		if (!guard || !tree || !commentsEnabled || !connection)
			return;
		if (!error.isEmpty()) {
			connection->setText("OFFLINE");
			connection->setToolTip(error);
			return;
		}
		loading = true;
		commentsEnabled->setChecked(result.value("enabled").toBool(false));
		loading = false;
		connection->setText("LIVE");
		const QString previousSelection = selectedId();
		tree->clear();
		QHash<QString, QTreeWidgetItem *> parents;
		QHash<QString, QTreeWidgetItem *> items;
		for (const auto entry : result.value("comments").toArray()) {
			const auto comment = entry.toObject();
			const QString id = comment.value("id").toString();
			const QString parentId = comment.value("parentId").toString();
			const bool team = comment.value("authorType").toString() == "team";
			const QString author = (team ? "★ TEAM · " : "") + comment.value("displayName").toString();
			const QString message = comment.value("message").toString();
			const QString status = comment.value("status").toString().toUpper();
			const bool pinned = comment.value("pinned").toBool();
			const QString time = QDateTime::fromString(comment.value("createdAt").toString(), Qt::ISODate).toLocalTime().toString("h:mm AP");
			auto *item = parentId.isEmpty() || !parents.contains(parentId)
					     ? new QTreeWidgetItem(tree)
					     : new QTreeWidgetItem(parents.value(parentId));
			item->setData(0, Qt::UserRole, id);
			item->setData(0, Qt::UserRole + 1, comment.value("status").toString());
			item->setData(0, Qt::UserRole + 2, pinned);

			auto *card = new QWidget;
			card->setObjectName("commentCard");
			auto *cardLayout = new QVBoxLayout(card);
			cardLayout->setContentsMargins(8, 6, 8, 6);
			cardLayout->setSpacing(3);
			auto *meta = new QHBoxLayout;
			meta->setContentsMargins(0, 0, 0, 0);
			auto *authorLabel = new QLabel(author);
			authorLabel->setObjectName("commentAuthor");
			if (team)
				authorLabel->setStyleSheet("color:#75add7");
			auto *timeLabel = new QLabel(time);
			timeLabel->setObjectName("commentTime");
			meta->addWidget(authorLabel, 1);
			meta->addWidget(timeLabel);
			cardLayout->addLayout(meta);
			auto *messageLabel = new QLabel(message);
			messageLabel->setObjectName("commentMessage");
			messageLabel->setWordWrap(true);
			cardLayout->addWidget(messageLabel);
			auto *badge = new QLabel(status + (pinned ? "  •  PINNED" : ""));
			badge->setObjectName("commentBadge");
			cardLayout->addWidget(badge);
			const int approximateLines = std::max(1, int((message.size() + 37) / 38));
			item->setSizeHint(0, QSize(0, 48 + approximateLines * 18));
			tree->setItemWidget(item, 0, card);
			parents.insert(id, item);
			items.insert(id, item);
		}
		if (items.isEmpty()) {
			auto *empty = new QTreeWidgetItem(tree, {"No fan comments yet. New comments will appear here automatically."});
			empty->setFlags(Qt::NoItemFlags);
			empty->setForeground(0, QColor("#82909a"));
			empty->setSizeHint(0, QSize(0, 54));
		} else if (items.contains(previousSelection)) {
			tree->setCurrentItem(items.value(previousSelection));
		}
		tree->expandAll();
	}
	void call(QJsonObject payload)
	{
		payload.insert("eventId", context.id());
		QPointer<CommentsController> guard(this);
		QString error;
		runtime().cloud->call(payload, &error);
		if (!guard)
			return;
		if (!error.isEmpty() && main)
			QMessageBox::warning(main, "IWC comments", error);
		refresh();
	}
	void reply()
	{
		if (selectedId().isEmpty())
			return;
		bool ok = false;
		const QString message = QInputDialog::getMultiLineText(main, "Reply as team", "Official team reply:", {}, &ok).trimmed();
		if (ok && !message.isEmpty())
			call({{"action", "comment_reply"}, {"commentId", selectedId()}, {"message", message.left(500)}});
	}
	void toggleHidden()
	{
		if (!tree->currentItem()) return;
		const QString next = tree->currentItem()->data(0, Qt::UserRole + 1).toString() == "hidden" ? "visible" : "hidden";
		call({{"action", "comment_moderate"}, {"commentId", selectedId()}, {"status", next}});
	}
	void togglePinned()
	{
		if (!tree->currentItem()) return;
		call({{"action", "comment_moderate"}, {"commentId", selectedId()}, {"pinned", !tree->currentItem()->data(0, Qt::UserRole + 2).toBool()}});
	}

	QPointer<OBSBasic> main;
	Iwc::EventContext context;
	QPointer<QTreeWidget> tree;
	QPointer<QCheckBox> commentsEnabled;
	QPointer<QLabel> connection;
	QTimer poller;
	bool loading = false;
};

void applyManagedIwcPolicy(OBSBasic *mainWindow)
{
	if (!mainWindow)
		return;
	// ICEWIRE manages delivery, encoding defaults, application appearance,
	// profiles, and cloud scene persistence. Operators keep production-facing
	// source/device properties without access to backend configuration.
	const QStringList hiddenActions = {
		"action_Settings", "actionShowSettingsFolder", "actionShowProfileFolder", "autoConfigure",
		"actionNewProfile", "actionDupProfile", "actionRenameProfile", "actionRemoveProfile",
		"actionImportProfile", "actionExportProfile", "actionNewSceneCollection",
		"actionDupSceneCollection", "actionRenameSceneCollection", "actionRemoveSceneCollection",
		"actionImportSceneCollection", "actionExportSceneCollection", "actionRemigrateSceneCollection",
		"resetUI", "toggleListboxToolbars", "toggleContextBar", "toggleSourceIcons", "toggleStatusBar"
	};
	for (const QString &name : hiddenActions) {
		if (auto *action = mainWindow->findChild<QAction *>(name)) {
			action->setEnabled(false);
			action->setVisible(false);
		}
	}
	for (const QString &name : {"profileMenu", "sceneCollectionMenu", "menuTools", "menuDocks"}) {
		if (auto *menu = mainWindow->findChild<QMenu *>(name)) {
			menu->setEnabled(false);
			menu->menuAction()->setVisible(false);
		}
	}
	if (auto *settingsButton = mainWindow->findChild<QPushButton *>("settingsButton"))
		settingsButton->hide();
	for (auto *dock : mainWindow->findChildren<QDockWidget *>())
		dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
}

} // namespace

namespace Iwc {

CloudClient::CloudClient(QObject *parent) : QObject(parent) {}

QJsonObject CloudClient::request(const QString &url, const QJsonObject &body, const QString &bearer,
				 int *status, QString *error) const
{
	QNetworkAccessManager manager;
	QNetworkRequest networkRequest{QUrl(url)};
	networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	networkRequest.setRawHeader("apikey", kPublishableKey);
	if (!bearer.isEmpty())
		networkRequest.setRawHeader("Authorization", "Bearer " + bearer.toUtf8());
	QNetworkReply *reply = manager.post(networkRequest, QJsonDocument(body).toJson(QJsonDocument::Compact));
	QEventLoop loop;
	QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	QTimer timeout;
	timeout.setSingleShot(true);
	QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
	timeout.start(25000);
	loop.exec();
	if (!reply->isFinished()) {
		reply->abort();
		if (error) *error = "The ICEWIRE cloud did not respond. Check your internet connection and try again.";
		if (status) *status = 0;
		reply->deleteLater();
		return {};
	}
	const QByteArray bytes = reply->readAll();
	const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	if (status) *status = code;
	if (reply->error() != QNetworkReply::NoError || code < 200 || code >= 300) {
		if (error) *error = readableError(bytes, reply->errorString());
		reply->deleteLater();
		return {};
	}
	reply->deleteLater();
	const auto document = QJsonDocument::fromJson(bytes);
	return document.isObject() ? document.object() : QJsonObject{};
}

bool CloudClient::signIn(const QString &email, const QString &password, const QString &captchaToken,
			 QString *error)
{
	if (email.isEmpty() || password.isEmpty()) {
		if (error) *error = "Enter your email and password.";
		return false;
	}
	if (captchaToken.isEmpty()) {
		if (error) *error = "Complete the security check before signing in.";
		return false;
	}
	int status = 0;
	QString requestError;
	const auto result = request(QString(kSupabaseUrl) + "/auth/v1/token?grant_type=password",
				    {{"email", email},
				     {"password", password},
				     {"gotrue_meta_security", QJsonObject{{"captcha_token", captchaToken}}}},
				    {}, &status, &requestError);
	if (!requestError.isEmpty()) {
		if (error) *error = requestError;
		return false;
	}
	currentSession.accessToken = result.value("access_token").toString();
	currentSession.refreshToken = result.value("refresh_token").toString();
	currentSession.userId = result.value("user").toObject().value("id").toString();
	currentSession.email = result.value("user").toObject().value("email").toString(email);
	currentSession.signedInAt = QDateTime::currentMSecsSinceEpoch();
	if (!currentSession.isValid()) {
		if (error) *error = "ICEWIRE did not return a complete session.";
		return false;
	}
	return saveSession(error);
}

bool CloudClient::refresh(QString *error)
{
	int status = 0;
	QString requestError;
	const auto result = request(QString(kSupabaseUrl) + "/auth/v1/token?grant_type=refresh_token",
				    {{"refresh_token", currentSession.refreshToken}}, {}, &status, &requestError);
	if (!requestError.isEmpty()) {
		if (error) *error = requestError;
		return false;
	}
	currentSession.accessToken = result.value("access_token").toString();
	currentSession.refreshToken = result.value("refresh_token").toString();
	if (!currentSession.isValid())
		return false;
	return saveSession(error);
}

bool CloudClient::saveSession(QString *error) const
{
	const QJsonObject data{{"accessToken", currentSession.accessToken}, {"refreshToken", currentSession.refreshToken},
			       {"userId", currentSession.userId}, {"email", currentSession.email},
			       {"signedInAt", QString::number(currentSession.signedInAt)}};
	const QByteArray encrypted = protectBytes(QJsonDocument(data).toJson(QJsonDocument::Compact));
	if (encrypted.isEmpty()) {
		if (error) *error = "Windows could not securely remember this session.";
		return false;
	}
	QFile file(sessionFile());
	if (!file.open(QIODevice::WriteOnly) || file.write(encrypted) != encrypted.size()) {
		if (error) *error = "IWC could not save the remembered session.";
		return false;
	}
	return true;
}

bool CloudClient::loadSession(QString *error)
{
	QFile file(sessionFile());
	if (!file.exists())
		return false;
	if (!file.open(QIODevice::ReadOnly)) {
		if (error) *error = "IWC could not open the remembered session.";
		return false;
	}
	const auto document = QJsonDocument::fromJson(unprotectBytes(file.readAll()));
	if (!document.isObject())
		return false;
	const auto data = document.object();
	currentSession.accessToken = data.value("accessToken").toString();
	currentSession.refreshToken = data.value("refreshToken").toString();
	currentSession.userId = data.value("userId").toString();
	currentSession.email = data.value("email").toString();
	currentSession.signedInAt = data.value("signedInAt").toString().toLongLong();
	return currentSession.isValid();
}

bool CloudClient::restoreRememberedSession(QString *error)
{
	if (!loadSession(error))
		return false;
	if (currentSession.signedInAt <= 0 || QDateTime::currentMSecsSinceEpoch() - currentSession.signedInAt > kRememberForMs) {
		signOut();
		return false;
	}
	if (!refresh(error)) {
		signOut();
		return false;
	}
	return true;
}

void CloudClient::signOut()
{
	currentSession = {};
	QFile::remove(sessionFile());
}

QJsonObject CloudClient::call(const QJsonObject &body, QString *error)
{
	int status = 0;
	QString requestError;
	auto result = request(QString(kSupabaseUrl) + "/functions/v1/iwc-desktop", body,
			      currentSession.accessToken, &status, &requestError);
	if (status == 401 && refresh(nullptr)) {
		requestError.clear();
		result = request(QString(kSupabaseUrl) + "/functions/v1/iwc-desktop", body,
				 currentSession.accessToken, &status, &requestError);
	}
	if (error) *error = requestError;
	return result;
}

bool CloudClient::uploadSignedFile(const QString &path, const QString &token, const QString &localFile,
				   const QString &mimeType, QString *error)
{
	QFile file(localFile);
	if (!file.open(QIODevice::ReadOnly)) {
		if (error) *error = "IWC could not open the selected media file.";
		return false;
	}
	const QString encodedPath = QString::fromLatin1(QUrl::toPercentEncoding(path, "/"));
	const QString encodedToken = QString::fromLatin1(QUrl::toPercentEncoding(token));
	QNetworkRequest request{QUrl(QString(kSupabaseUrl) + "/storage/v1/object/upload/sign/broadcast-assets/" + encodedPath + "?token=" + encodedToken)};
	request.setHeader(QNetworkRequest::ContentTypeHeader, mimeType);
	request.setRawHeader("apikey", kPublishableKey);
	request.setRawHeader("Authorization", "Bearer " + currentSession.accessToken.toUtf8());
	request.setRawHeader("x-upsert", "false");
	request.setRawHeader("cache-control", "max-age=3600");
	QNetworkAccessManager manager;
	QNetworkReply *reply = manager.put(request, &file);
	QEventLoop loop;
	QTimer timeout;
	timeout.setSingleShot(true);
	QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
	QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	timeout.start(120000);
	loop.exec();
	if (!reply->isFinished()) {
		reply->abort();
		if (error) *error = "The media upload timed out. Check the connection and try again.";
		reply->deleteLater();
		return false;
	}
	const QByteArray response = reply->readAll();
	const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	const bool ok = reply->error() == QNetworkReply::NoError && status >= 200 && status < 300;
	if (!ok && error)
		*error = readableError(response, reply->errorString());
	reply->deleteLater();
	return ok;
}

CloudClient *client()
{
	return runtime().cloud.get();
}

const EventContext &eventContext()
{
	return runtime().event;
}

void ShowCameraSourceDialog(OBSBasic *mainWindow)
{
	if (!mainWindow || runtime().event.id().isEmpty() || !runtime().cloud) {
		QMessageBox::information(mainWindow, "Add IWC Camera", "Open an ICEWIRE event before adding a phone camera source.");
		return;
	}
	CameraSourceDialog dialog(mainWindow);
	dialog.exec();
}

bool RunStartupWorkflow()
{
	auto &state = runtime();
	state.cloud = std::make_unique<CloudClient>();
	QString restoreError;
	if (!state.cloud->restoreRememberedSession(&restoreError)) {
		SignInDialog signIn(state.cloud.get());
		if (signIn.exec() != QDialog::Accepted)
			return false;
	}
	QString checkInError;
	QString releasePlatform = "windows-x64";
#ifdef __APPLE__
	releasePlatform = QSysInfo::currentCpuArchitecture().contains("arm", Qt::CaseInsensitive)
				  ? "macos-arm64"
				  : "macos-x64";
#endif
	state.cloud->call({{"action", "device_checkin"},
			   {"installationId", installationId()},
			   {"appVersion", QString::fromUtf8(IWC_APP_VERSION)},
			   {"channel", "stable"},
			   {"platform", releasePlatform},
			   {"deviceName", QSysInfo::machineHostName()},
			   {"osName", QSysInfo::prettyProductName()},
			   {"architecture", QSysInfo::currentCpuArchitecture()}},
			  &checkInError);
	if (!checkInError.isEmpty())
		blog(LOG_WARNING, "IWC device check-in failed: %s", checkInError.toUtf8().constData());
	while (true) {
		EventsDialog events(state.cloud.get(), &state.event);
		const int result = events.exec();
		if (result == QDialog::Accepted)
			return true;
		if (result != 2)
			return false;
		SignInDialog signIn(state.cloud.get());
		if (signIn.exec() != QDialog::Accepted)
			return false;
	}
}

void ConfigureManagedProfile(OBSBasic *mainWindow)
{
	if (!mainWindow || runtime().event.id().isEmpty())
		return;
	config_t *profile = mainWindow->Config();
	if (!profile)
		return;
	// Operators get a predictable rolling replay without needing access to the
	// underlying OBS Output settings. Configure both modes so a migrated profile
	// cannot accidentally disable the IWC replay workflow.
	config_set_bool(profile, "SimpleOutput", "RecRB", true);
	config_set_int(profile, "SimpleOutput", "RecRBTime", 15);
	config_set_int(profile, "SimpleOutput", "RecRBSize", 512);
	config_set_string(profile, "SimpleOutput", "RecRBPrefix", "IWC-Replay");
	config_set_bool(profile, "AdvOut", "RecRB", true);
	config_set_int(profile, "AdvOut", "RecRBTime", 15);
	config_set_int(profile, "AdvOut", "RecRBSize", 512);
	config_t *user = App()->GetUserConfig();
	if (user) {
		config_set_bool(user, "BasicWindow", "ReplayBufferWhileStreaming", true);
		config_set_bool(user, "BasicWindow", "KeepReplayBufferStreamStops", false);
	}
}

void InstallStudio(OBSBasic *mainWindow)
{
	auto &state = runtime();
	if (!mainWindow || state.event.id().isEmpty())
		return;
	// IWC always uses a real preview/program workflow. Scene edits stay in
	// Preview until the operator presses Cut or Fade to take them live.
	mainWindow->EnableIwcStudioMode();
	mainWindow->setWindowTitle(QString("ICEWIRE CAST · %1").arg(state.event.event.value("title").toString("IWC STUDIO")));

	if (!installAutomaticService(mainWindow, state.event.ingest)) {
		if (auto *button = mainWindow->findChild<QPushButton *>("streamButton")) {
			button->setEnabled(false);
			button->setText("CONNECTING TO ICEWIRE…");
			button->setToolTip(state.event.ingest.value("error").toString("ICEWIRE is preparing the private broadcast destination automatically."));
		}

		auto *timer = new QTimer(mainWindow);
		timer->setInterval(4000);
		state.reconnectTimer = timer;
		QObject::connect(timer, &QTimer::timeout, mainWindow, [mainWindow, timer] {
			auto &current = runtime();
			if (!current.cloud || current.event.id().isEmpty())
				return;

			QString error;
			const QJsonObject provisioned = current.cloud->call({{"action", "provision"},
									     {"eventId", current.event.id()},
									     {"youtubeEnabled", current.event.ingest.value("youtubeEnabled").toBool(false)}},
									    &error);
			const QJsonObject ingest = provisioned.value("ingest").toObject();
			if (!error.isEmpty() || !ingest.value("configured").toBool(false)) {
				if (auto *button = mainWindow->findChild<QPushButton *>("streamButton"))
					button->setToolTip(!error.isEmpty() ? error : ingest.value("error").toString("ICEWIRE is still preparing the broadcast destination."));
				return;
			}

			current.event.bundle = provisioned;
			current.event.event = provisioned.value("event").toObject();
			current.event.ingest = ingest;
			current.event.clockPreferences = provisioned.value("clockPreferences").toObject();
			current.event.youtube = provisioned.value("youtube").toObject();
			if (installAutomaticService(mainWindow, ingest)) {
				timer->stop();
				current.reconnectTimer = nullptr;
				timer->deleteLater();
			}
		});
		timer->start();
	}

	if (state.event.isGame()) {
		auto *score = new ScorebugController(mainWindow, state.event, mainWindow);
		state.scoreDock = score->buildDock();
		mainWindow->addDockWidget(Qt::RightDockWidgetArea, state.scoreDock);
		auto *leagueBoard = new LeagueBoardController(mainWindow, state.event, mainWindow);
		state.leagueBoardDock = leagueBoard->buildDock();
		mainWindow->addDockWidget(Qt::LeftDockWidgetArea, state.leagueBoardDock);
	}
	auto *media = new MediaLibraryController(mainWindow, state.event, mainWindow);
	state.mediaDock = media->buildDock();
	mainWindow->addDockWidget(Qt::LeftDockWidgetArea, state.mediaDock);
	auto *replay = new InstantReplayController(mainWindow, mainWindow);
	state.replayDock = replay->buildDock();
	mainWindow->addDockWidget(Qt::LeftDockWidgetArea, state.replayDock);
	auto *comments = new CommentsController(mainWindow, state.event, mainWindow);
	state.commentsDock = comments->buildDock();
	mainWindow->addDockWidget(Qt::RightDockWidgetArea, state.commentsDock);

	// Use a broadcast-console layout that stays inside the Windows work area.
	// Every production tool lives in a compact tabbed bottom rack; only Score Bug
	// and Live Comments remain in the right-side rack. The studio-mode Cut/Fade
	// controls already live between Preview and Program, so the redundant
	// transitions dock is intentionally hidden.
	auto *scenesDock = mainWindow->findChild<QDockWidget *>("scenesDock");
	auto *sourcesDock = mainWindow->findChild<QDockWidget *>("sourcesDock");
	auto *mixerDock = mainWindow->findChild<QDockWidget *>("mixerDock");
	auto *transitionsDock = mainWindow->findChild<QDockWidget *>("transitionsDock");
	auto *controlsDock = mainWindow->findChild<QDockWidget *>("controlsDock");
	if (transitionsDock)
		transitionsDock->hide();
	if (scenesDock && sourcesDock && mixerDock && controlsDock) {
		for (auto *dock : {scenesDock, sourcesDock, mixerDock, controlsDock})
			mainWindow->removeDockWidget(dock);
		for (auto *dock : {state.mediaDock.data(), state.leagueBoardDock.data(), state.replayDock.data()}) {
			if (dock)
				mainWindow->removeDockWidget(dock);
		}

		// Four visible columns keep the live workspace shallow enough for a
		// laptop display. Secondary tools share clearly labeled tabs below.
		mainWindow->addDockWidget(Qt::BottomDockWidgetArea, scenesDock);
		mainWindow->splitDockWidget(scenesDock, sourcesDock, Qt::Horizontal);
		mainWindow->splitDockWidget(sourcesDock, mixerDock, Qt::Horizontal);
		mainWindow->splitDockWidget(mixerDock, controlsDock, Qt::Horizontal);

		if (state.mediaDock)
			mainWindow->tabifyDockWidget(scenesDock, state.mediaDock);
		if (state.leagueBoardDock)
			mainWindow->tabifyDockWidget(sourcesDock, state.leagueBoardDock);
		if (state.replayDock)
			mainWindow->tabifyDockWidget(sourcesDock, state.replayDock);

		for (auto *dock : {scenesDock, sourcesDock, mixerDock, controlsDock})
			dock->show();
		if (state.mediaDock)
			state.mediaDock->show();
		if (state.leagueBoardDock)
			state.leagueBoardDock->show();
		if (state.replayDock)
			state.replayDock->show();
		scenesDock->raise();
		sourcesDock->raise();
	}
	if (state.scoreDock && state.commentsDock) {
		mainWindow->removeDockWidget(state.commentsDock);
		mainWindow->splitDockWidget(state.scoreDock, state.commentsDock, Qt::Vertical);
		state.scoreDock->show();
		state.commentsDock->show();
	}
	mainWindow->setTabPosition(Qt::BottomDockWidgetArea, QTabWidget::North);
	mainWindow->setTabPosition(Qt::RightDockWidgetArea, QTabWidget::North);

	QTimer::singleShot(0, mainWindow, [mainWindow] {
		QScreen *screen = mainWindow->screen();
		if (!screen)
			screen = QGuiApplication::primaryScreen();
		if (!screen)
			return;

		const QRect usable = screen->availableGeometry();
		mainWindow->setMinimumSize(std::min(900, usable.width()), std::min(600, usable.height()));

		if (auto *scoreDock = runtime().scoreDock.data()) {
			const int rightWidth = std::clamp(usable.width() * 17 / 100, 250, 300);
			if (auto *commentsDock = runtime().commentsDock.data()) {
				mainWindow->resizeDocks({scoreDock, commentsDock}, {rightWidth, rightWidth}, Qt::Horizontal);
				mainWindow->resizeDocks({scoreDock, commentsDock},
							{usable.height() * 68 / 100, usable.height() * 32 / 100}, Qt::Vertical);
			} else {
				mainWindow->resizeDocks({scoreDock}, {rightWidth}, Qt::Horizontal);
			}
		}
		if (auto *bottomScenes = mainWindow->findChild<QDockWidget *>("scenesDock")) {
			auto *bottomSources = mainWindow->findChild<QDockWidget *>("sourcesDock");
			auto *bottomMixer = mainWindow->findChild<QDockWidget *>("mixerDock");
			auto *bottomControls = mainWindow->findChild<QDockWidget *>("controlsDock");
			if (bottomSources && bottomMixer && bottomControls) {
				const int rackWidth = usable.width() - std::clamp(usable.width() * 17 / 100, 250, 300);
				mainWindow->resizeDocks({bottomScenes, bottomSources, bottomMixer, bottomControls},
							{rackWidth * 21 / 100, rackWidth * 31 / 100,
							 rackWidth * 28 / 100, rackWidth * 20 / 100},
							Qt::Horizontal);
				mainWindow->resizeDocks({bottomScenes, bottomSources, bottomMixer, bottomControls},
							{210, 210, 210, 210},
							Qt::Vertical);
			}
		}

		// Maximized is the predictable broadcast layout. Qt then uses the actual
		// Windows work area (including taskbar and DPI scaling) instead of a saved
		// pixel size from another monitor.
		mainWindow->showMaximized();
	});
	applyManagedIwcPolicy(mainWindow);

	QObject::connect(mainWindow, &OBSBasic::StreamingStarted, mainWindow, [](bool) {
		QString error;
		runtime().cloud->call({{"action", "broadcast"}, {"eventId", runtime().event.id()}, {"status", "live"}}, &error);
		if (!error.isEmpty())
			QMessageBox::warning(OBSBasic::Get(), "IWC broadcast status", error);
	});
	QObject::connect(mainWindow, &OBSBasic::StreamingStopped, mainWindow, [](bool) {
		QString error;
		runtime().cloud->call({{"action", "broadcast"}, {"eventId", runtime().event.id()}, {"status", "ended"}}, &error);
		if (!error.isEmpty())
			QMessageBox::warning(OBSBasic::Get(), "IWC broadcast status", error);
		// Cloudflare may need a short processing window before its recording is
		// visible. Retry silently so the replay reaches the fan archive without
		// requiring the operator to open the website or press a sync button.
		for (const int delay : {15000, 45000, 90000}) {
			QTimer::singleShot(delay, OBSBasic::Get(), [] {
				if (!runtime().cloud || runtime().event.id().isEmpty())
					return;
				QString syncError;
				runtime().cloud->call({{"action", "archive_sync"}, {"eventId", runtime().event.id()}}, &syncError);
			});
		}
	});
}

} // namespace Iwc
