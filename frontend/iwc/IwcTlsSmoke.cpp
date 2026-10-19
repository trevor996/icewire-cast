#include <QCoreApplication>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslSocket>
#include <QTimer>

#include <iostream>

int main(int argc, char *argv[])
{
	QCoreApplication app(argc, argv);
	if (argc < 3) {
		std::cerr << "usage: iwc-tls-smoke <url> <publishable-key>\n";
		return 2;
	}

	QNetworkAccessManager manager;
	QNetworkRequest request{QUrl::fromUserInput(QString::fromLocal8Bit(argv[1]))};
	request.setRawHeader("apikey", QByteArray(argv[2]));
	QNetworkReply *reply = manager.get(request);

	QEventLoop loop;
	QTimer timeout;
	timeout.setSingleShot(true);
	QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
	QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	timeout.start(15000);
	loop.exec();

	const bool timedOut = !reply->isFinished();
	const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	const auto error = reply->error();
	std::cout << "ssl=" << (QSslSocket::supportsSsl() ? "true" : "false")
		  << " backend=" << QSslSocket::activeBackend().toStdString() << " status=" << status
		  << " error=" << static_cast<int>(error) << " timed_out=" << (timedOut ? "true" : "false")
		  << "\n";

	if (timedOut)
		reply->abort();
	reply->deleteLater();
	return !timedOut && QSslSocket::supportsSsl() && status == 200 && error == QNetworkReply::NoError ? 0 : 1;
}
