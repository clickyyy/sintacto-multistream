#pragma once
// OBS MultiStream Hub - Creado por Sintacto (@sintacto12)

#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QWidget>

#include <vector>

#include "destination.hpp"
#include "output_manager.hpp"

class QCheckBox;

// Tarjeta de una plataforma: interruptor, estado, estadísticas, configurar y ayuda.
class DestinationCard : public QFrame {
	Q_OBJECT

public:
	DestinationCard(int index, const Destination &d, QWidget *parent = nullptr);

	void refresh(const Destination &d);
	void setState(RunState s, const QString &message);
	void setStats(qint64 liveSeconds, double kbps, int dropped, int total);

signals:
	void toggled(int index, bool enabled);
	void configureClicked(int index);
	void helpClicked(int index);

private:
	int index_;
	bool configured_ = false;
	RunState state_ = RunState::Idle;
	QCheckBox *toggle_;
	QLabel *name_;
	QLabel *dot_;
	QLabel *status_;
	QLabel *stats_;
};

class HubDock : public QWidget {
	Q_OBJECT

public:
	HubDock(OutputManager *manager, QWidget *parent = nullptr);

private slots:
	void onStartAll();
	void onStopAll();
	void onSettings();
	void onDonate();
	void onState(const QString &id, int state, const QString &message);
	void onStats(const QString &id, qint64 liveSeconds, double kbps, int dropped, int total);

private:
	int indexOf(const QString &id) const;
	void onToggled(int index, bool enabled);
	void onConfigure(int index);
	void onHelp(int index);
	void updateSummary();

	OutputManager *mgr_;
	std::vector<Destination> dests_;
	StreamSettings settings_;
	std::vector<DestinationCard *> cards_;
	std::vector<RunState> states_;
	QPushButton *startBtn_;
	QPushButton *stopBtn_;
	QLabel *summary_;
};
