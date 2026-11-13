#pragma once
// OBS MultiStream Hub - Creado por Sintacto (@sintacto12)
//
// Núcleo del multistream. Cada destino tiene su propia salida RTMP(S) de OBS,
// pero los codificadores se COMPARTEN: uno para todos los horizontales, uno
// para todos los verticales (sobre un único lienzo 9:16) y uno de audio.
// Todos se lanzan en el mismo clic; un fallo en uno no toca a los demás.
// Todos los métodos públicos se llaman desde el hilo de la interfaz (Qt).

#include <QObject>
#include <QString>
#include <QTimer>

#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "destination.hpp"
#include "encoder_pool.hpp"
#include "obs_raii.hpp"
#include "vertical_canvas.hpp"

enum class RunState { Idle = 0, Starting, Live, Reconnecting, Stopping, Error };

class OutputManager : public QObject {
	Q_OBJECT

public:
	explicit OutputManager(QObject *parent = nullptr);
	~OutputManager() override;

	// Inicia en paralelo todos los destinos activados y válidos.
	void startAll(const std::vector<Destination> &dests, const StreamSettings &settings);
	void stopAll();

	// Parada inmediata y liberación total (al cerrar OBS).
	void shutdown();

	bool anyActive() const;

	// Lienzo vertical (siempre existe; su video solo al transmitir en vertical).
	VerticalCanvas &canvas() { return vcanvas_; }

signals:
	void stateChanged(const QString &id, int state, const QString &message);
	void statsUpdated(const QString &id, qint64 liveSeconds, double kbps, int dropped, int total);

private:
	using Clock = std::chrono::steady_clock;

	struct Ctx {
		OutputManager *mgr;
		std::string id;
	};

	struct Runtime {
		Destination dest;
		ServicePtr service;
		OutputPtr output;
		EncoderPtr venc;
		EncoderPtr aenc;
		std::unique_ptr<Ctx> ctx;
		std::vector<SignalConn> conns; // se declara después de output: se destruye antes
		RunState state = RunState::Idle;
		Clock::time_point liveSince;
		Clock::time_point lastTick;
		uint64_t lastBytes = 0;
	};

	void launch(const Destination &d, const StreamSettings &settings);
	void fail(const std::string &id, const QString &message);
	void set(const std::string &id, RunState state, const QString &message = QString());
	void releaseIfIdle();
	static bool isActive(RunState s);

	// Se ejecutan en el hilo de la interfaz (encoladas desde las señales de OBS).
	void handleStart(const std::string &id);
	void handleStop(const std::string &id, int code);
	void handleReconnect(const std::string &id);
	void handleReconnectSuccess(const std::string &id);
	void tick();

	// Callbacks de libobs (hilos propios de OBS: solo encolan).
	static void onStart(void *param, calldata_t *cd);
	static void onStop(void *param, calldata_t *cd);
	static void onReconnect(void *param, calldata_t *cd);
	static void onReconnectSuccess(void *param, calldata_t *cd);

	std::map<std::string, std::unique_ptr<Runtime>> rt_;
	EncoderPool pool_;
	VerticalCanvas vcanvas_;
	QTimer timer_;
};
