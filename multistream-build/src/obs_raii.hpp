#pragma once
// OBS MultiStream Hub - Creado por Sintacto (@sintacto12)
//
// Pequeños envoltorios RAII para los objetos de libobs, para no depender de
// los detalles de conteo de referencias de obs.hpp.

#include <obs.h>
#include <memory>

struct OutputDeleter {
	void operator()(obs_output_t *o) const
	{
		if (o)
			obs_output_release(o);
	}
};
struct ServiceDeleter {
	void operator()(obs_service_t *s) const
	{
		if (s)
			obs_service_release(s);
	}
};

using OutputPtr = std::unique_ptr<obs_output_t, OutputDeleter>;
using ServicePtr = std::unique_ptr<obs_service_t, ServiceDeleter>;
using EncoderPtr = std::shared_ptr<obs_encoder_t>; // compartible entre salidas

inline EncoderPtr wrapEncoder(obs_encoder_t *e)
{
	if (!e)
		return nullptr;
	return EncoderPtr(e, [](obs_encoder_t *x) { obs_encoder_release(x); });
}

// Conexión a una señal de libobs que se desconecta sola al destruirse.
struct SignalConn {
	signal_handler_t *handler = nullptr;
	const char *name = nullptr;
	signal_callback_t cb = nullptr;
	void *param = nullptr;

	SignalConn(signal_handler_t *h, const char *n, signal_callback_t c, void *p) : handler(h), name(n), cb(c), param(p)
	{
		signal_handler_connect(handler, name, cb, param);
	}
	SignalConn(SignalConn &&o) noexcept : handler(o.handler), name(o.name), cb(o.cb), param(o.param)
	{
		o.handler = nullptr;
	}
	SignalConn(const SignalConn &) = delete;
	SignalConn &operator=(const SignalConn &) = delete;
	~SignalConn()
	{
		if (handler)
			signal_handler_disconnect(handler, name, cb, param);
	}
};
