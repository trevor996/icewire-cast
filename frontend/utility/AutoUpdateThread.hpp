#pragma once

#include <QThread>

class AutoUpdateThread : public QThread {
	Q_OBJECT

	bool manualUpdate;
	bool repairMode;

	virtual void run() override;

	void info(const QString &title, const QString &text);
	bool queryUpdate(const QString &version, const QString &notes, bool required);

private slots:
	void infoMsg(const QString &title, const QString &text);
	bool queryUpdateSlot(const QString &version, const QString &notes, bool required);

public:
	AutoUpdateThread(bool manualUpdate_, bool repairMode_ = false)
		: manualUpdate(manualUpdate_),
		  repairMode(repairMode_)
	{
	}
};
