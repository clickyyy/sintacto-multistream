// OBS MultiStream Hub - Creado por Sintacto (@sintacto12)
#include "output_manager.hpp"

#include <obs.h>
#include <util/base.h>

#include <QMetaObject>

namespace {
const char *kService = "rtmp_custom";
const char *kOutput = "rtmp_output";
const int kReconnectRetries = 25;
const int kReconnectDelaySec = 10;

QString describeStopCode(int code, const QString &lastError)
{
	QString base;
	switch (code) {
	case OBS_OUTPUT_BAD_PATH:
		base = QStringLiteral("La URL del servidor no es válida.");
		break;
	case OBS_OUTPUT_CONNECT_FAILED:
		base = QStringLiteral("No se pudo conectar con el servidor.");
		break;
	case OBS_OUTPUT_INVALID_STREAM:
		base = QStringLiteral("Servidor o clave de transmisión rechazados.");
		break;
	case OBS_OUTPUT_DISCONNECTED:
		base = QStringLiteral("Se perdió la conexión y no se pudo recuperar.");
		break;
	case OBS_OUTPUT_UNSUPPORTED:
		base = QStringLiteral("El destino no admite este formato de video/audio.");
		break;
	case OBS_OUTPUT_ENCODE_ERROR:
		base = QStringLiteral("Error del codificador.");
		break;
	default:
		base = QStringLiteral("La transmisión terminó con un error.");
		break;
	}
	if (!lastError.isEmpty())
		base += QStringLiteral(" (") + lastError + QStringLiteral(")");
	return base;
}
} // namespace

OutputManager::OutputManager(QObject *parent) : QObject(parent)
{
	connect(&timer_, &QTimer::timeout, this, &OutputManager::tick);
	timer_.start(1000);
	vcanvas_.init(); // escena privada y vista, sin video: no gasta nada en reposo
}

OutputManager::~OutputManager()
{
	shutdown();
}

bool OutputManager::isActive(RunState s)
{
	return s == RunState::Starting || s == RunState::Live || s == RunState::Reconnecting ||
	       s == RunState::Stopping;
}

bool OutputManager::anyActive() const
{
	for (const auto &kv : rt_)
		if (isActive(kv.second->state))
			return true;
	return false;
}

void OutputManager::set(const std::string &id, RunState state, const QString &message)
{
	auto it = rt_.find(id);
	if (it != rt_.end())
		it->second->state = state;
	emit stateChanged(QString::fromStdString(id), static_cast<int>(state), message);
}

void OutputManager::fail(const std::string &id, const QString &message)
{
	blog(LOG_WARNING, "[MultiStream Hub] %s: %s", id.c_str(), message.toUtf8().constData());
	rt_.erase(id);
	emit stateChanged(QString::fromStdString(id), static_cast<int>(RunState::Error), message);
	releaseIfIdle();
}

// Libera lo que ya nadie usa. Orden importante: primero los codificadores y
// después el lienzo, porque el codificador vertical lee del video del lienzo.
void OutputManager::releaseIfIdle()
{
	pool_.pruneUnused(); // codificadores de bitrate propio que ya nadie usa
	bool hasH = false, hasV = false;
	for (const auto &kv : rt_) {
		if (kv.second->dest.orientation == Orientation::Vertical)
			hasV = true;
		else
			hasH = true;
	}

	if (!hasH)
		pool_.dropVideo(Orientation::Horizontal);
	if (!hasV) {
		pool_.dropVideo(Orientation::Vertical);
		vcanvas_.stopVideo(); // el lienzo sigue existiendo para editarlo; solo se apaga su video
	}
	if (rt_.empty())
		pool_.clear(); // incluye el codificador de audio
}

// ---------------------------------------------------------------------------
// Inicio en paralelo
// ---------------------------------------------------------------------------

void OutputManager::startAll(const std::vector<Destination> &dests, const StreamSettings &settings)
{
	for (const auto &d : dests) {
		if (!d.enabled)
			continue;

		// Nunca iniciar dos veces el mismo destino.
		auto it = rt_.find(d.id);
		if (it != rt_.end() && isActive(it->second->state))
			continue;

		std::string why;
		if (!validateDestination(d, why)) {
			emit stateChanged(QString::fromStdString(d.id), static_cast<int>(RunState::Error),
					  QString::fromStdString(why));
			continue; // un destino inválido no bloquea a los demás
		}
		launch(d, settings);
	}
}

void OutputManager::launch(const Destination &d, const StreamSettings &settings)
{
	auto rt = std::make_unique<Runtime>();
	rt->dest = d;
	rt->ctx = std::make_unique<Ctx>(Ctx{this, d.id});

	// Se suelta el Runtime ANTES de llamar a fail(): así los codificadores dejan de
	// estar referenciados y releaseIfIdle() puede cerrar el lienzo con seguridad.
	auto abort = [&](const QString &msg) {
		rt.reset();
		fail(d.id, msg);
	};

	const bool vertical = d.orientation == Orientation::Vertical;
	VideoProfile profile = vertical ? settings.vertical : settings.horizontal;
	if (d.bitrateKbps > 0)
		profile.bitrateKbps = d.bitrateKbps; // bitrate propio de este destino

	video_t *src = nullptr;
	if (vertical) {
		// Un solo lienzo vertical para todos los destinos verticales.
		std::string err;
		if (!vcanvas_.startVideo(err)) {
			abort(QString::fromStdString(err));
			return;
		}
		src = vcanvas_.video();
	} else {
		src = obs_get_video();
	}

	// Todos los destinos de la misma orientación reciben el MISMO codificador.
	rt->venc = pool_.video(d.orientation, profile, src);
	rt->aenc = pool_.audio(settings.audioKbps);
	if (!rt->venc || !rt->aenc) {
		abort(QStringLiteral("No se pudo crear el codificador."));
		return;
	}

	obs_data_t *ss = obs_data_create();
	obs_data_set_string(ss, "server", d.server.c_str());
	obs_data_set_string(ss, "key", d.streamKey.c_str()); // nunca se registra en el log
	obs_data_set_bool(ss, "use_auth", false);
	std::string svcName = "mshub-svc-" + d.id;
	rt->service.reset(obs_service_create(kService, svcName.c_str(), ss, nullptr));
	obs_data_release(ss);

	std::string outName = "mshub-out-" + d.id;
	rt->output.reset(obs_output_create(kOutput, outName.c_str(), nullptr, nullptr));
	if (!rt->service || !rt->output) {
		abort(QStringLiteral("No se pudo crear la salida RTMP."));
		return;
	}

	obs_output_set_service(rt->output.get(), rt->service.get());
	obs_output_set_video_encoder(rt->output.get(), rt->venc.get());
	obs_output_set_audio_encoder(rt->output.get(), rt->aenc.get(), 0);
	obs_output_set_reconnect_settings(rt->output.get(), kReconnectRetries, kReconnectDelaySec);

	signal_handler_t *sh = obs_output_get_signal_handler(rt->output.get());
	void *p = rt->ctx.get();
	rt->conns.emplace_back(sh, "start", &OutputManager::onStart, p);
	rt->conns.emplace_back(sh, "stop", &OutputManager::onStop, p);
	rt->conns.emplace_back(sh, "reconnect", &OutputManager::onReconnect, p);
	rt->conns.emplace_back(sh, "reconnect_success", &OutputManager::onReconnectSuccess, p);

	rt->state = RunState::Starting;
	obs_output_t *out = rt->output.get();
	rt_[d.id] = std::move(rt);
	emit stateChanged(QString::fromStdString(d.id), static_cast<int>(RunState::Starting), QString());

	// obs_output_start es asíncrono: vuelve enseguida y el resultado llega por
	// las señales "start" / "stop". Por eso todos los destinos arrancan a la vez.
	if (!obs_output_start(out)) {
		const char *err = obs_output_get_last_error(out);
		QString msg = err && *err ? QString::fromUtf8(err) : QStringLiteral("No se pudo iniciar la salida.");
		fail(d.id, msg);
	}
}

// ---------------------------------------------------------------------------
// Parada
// ---------------------------------------------------------------------------

void OutputManager::stopAll()
{
	for (auto &kv : rt_) {
		Runtime &r = *kv.second;
		if (r.state == RunState::Starting || r.state == RunState::Live || r.state == RunState::Reconnecting) {
			r.state = RunState::Stopping;
			emit stateChanged(QString::fromStdString(kv.first), static_cast<int>(RunState::Stopping),
					  QString());
			obs_output_stop(r.output.get());
		}
	}
}

void OutputManager::shutdown()
{
	timer_.stop();
	for (auto &kv : rt_) {
		kv.second->conns.clear(); // ya no queremos callbacks
		if (kv.second->output)
			obs_output_force_stop(kv.second->output.get());
	}
	rt_.clear();
	pool_.clear();
	vcanvas_.shutdown(); // siempre después de soltar los codificadores
}

// ---------------------------------------------------------------------------
// Señales de OBS -> hilo de la interfaz
// ---------------------------------------------------------------------------

void OutputManager::onStart(void *param, calldata_t *)
{
	auto *c = static_cast<Ctx *>(param);
	OutputManager *mgr = c->mgr;
	std::string id = c->id;
	QMetaObject::invokeMethod(mgr, [mgr, id] { mgr->handleStart(id); }, Qt::QueuedConnection);
}

void OutputManager::onStop(void *param, calldata_t *cd)
{
	auto *c = static_cast<Ctx *>(param);
	OutputManager *mgr = c->mgr;
	std::string id = c->id;
	int code = static_cast<int>(calldata_int(cd, "code"));
	QMetaObject::invokeMethod(mgr, [mgr, id, code] { mgr->handleStop(id, code); }, Qt::QueuedConnection);
}

void OutputManager::onReconnect(void *param, calldata_t *)
{
	auto *c = static_cast<Ctx *>(param);
	OutputManager *mgr = c->mgr;
	std::string id = c->id;
	QMetaObject::invokeMethod(mgr, [mgr, id] { mgr->handleReconnect(id); }, Qt::QueuedConnection);
}

void OutputManager::onReconnectSuccess(void *param, calldata_t *)
{
	auto *c = static_cast<Ctx *>(param);
	OutputManager *mgr = c->mgr;
	std::string id = c->id;
	QMetaObject::invokeMethod(mgr, [mgr, id] { mgr->handleReconnectSuccess(id); }, Qt::QueuedConnection);
}

void OutputManager::handleStart(const std::string &id)
{
	auto it = rt_.find(id);
	if (it == rt_.end())
		return;
	Runtime &r = *it->second;
	if (r.state == RunState::Stopping)
		return;
	r.liveSince = Clock::now();
	r.lastTick = r.liveSince;
	r.lastBytes = 0;
	blog(LOG_INFO, "[MultiStream Hub] %s transmitiendo.", id.c_str());
	set(id, RunState::Live);
}

void OutputManager::handleReconnect(const std::string &id)
{
	auto it = rt_.find(id);
	if (it == rt_.end() || it->second->state == RunState::Stopping)
		return;
	set(id, RunState::Reconnecting, QStringLiteral("Conexión perdida; reintentando…"));
}

void OutputManager::handleReconnectSuccess(const std::string &id)
{
	auto it = rt_.find(id);
	if (it == rt_.end() || it->second->state == RunState::Stopping)
		return;
	it->second->lastBytes = obs_output_get_total_bytes(it->second->output.get());
	it->second->lastTick = Clock::now();
	set(id, RunState::Live);
}

void OutputManager::handleStop(const std::string &id, int code)
{
	auto it = rt_.find(id);
	if (it == rt_.end())
		return;

	Runtime &r = *it->second;
	const char *le = obs_output_get_last_error(r.output.get());
	QString lastError = le ? QString::fromUtf8(le) : QString();
	bool requested = (r.state == RunState::Stopping);

	r.conns.clear(); // desconectar antes de liberar la salida
	rt_.erase(it);   // libera salida, servicio y referencias a codificadores

	if (code == OBS_OUTPUT_SUCCESS || requested) {
		emit stateChanged(QString::fromStdString(id), static_cast<int>(RunState::Idle), QString());
	} else {
		QString msg = describeStopCode(code, lastError);
		blog(LOG_WARNING, "[MultiStream Hub] %s se detuvo: %s", id.c_str(), msg.toUtf8().constData());
		emit stateChanged(QString::fromStdString(id), static_cast<int>(RunState::Error), msg);
	}
	releaseIfIdle();
}

// ---------------------------------------------------------------------------
// Estadísticas (1 Hz)
// ---------------------------------------------------------------------------

void OutputManager::tick()
{
	auto now = Clock::now();
	for (auto &kv : rt_) {
		Runtime &r = *kv.second;
		if (r.state != RunState::Live || !r.output)
			continue;

		uint64_t bytes = obs_output_get_total_bytes(r.output.get());
		double dt = std::chrono::duration<double>(now - r.lastTick).count();
		double kbps = dt > 0.0 && bytes >= r.lastBytes ? (double)(bytes - r.lastBytes) * 8.0 / 1000.0 / dt : 0.0;
		r.lastBytes = bytes;
		r.lastTick = now;

		qint64 secs = std::chrono::duration_cast<std::chrono::seconds>(now - r.liveSince).count();
		emit statsUpdated(QString::fromStdString(kv.first), secs, kbps,
				  obs_output_get_frames_dropped(r.output.get()),
				  obs_output_get_total_frames(r.output.get()));
	}
}
