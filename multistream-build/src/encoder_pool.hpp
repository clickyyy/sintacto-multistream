#pragma once
// OBS MultiStream Hub - Creado por Sintacto (@sintacto12)
//
// Un solo codificador de video por orientación (H y V) y uno de audio.
// Todos los destinos de la misma orientación comparten el mismo codificador:
// se codifica UNA vez y se reparte a todas las salidas RTMP.

#include <map>
#include <string>

#include "destination.hpp"
#include "obs_raii.hpp"

class EncoderPool {
public:
	EncoderPool() = default;

	EncoderPool(const EncoderPool &) = delete;
	EncoderPool &operator=(const EncoderPool &) = delete;

	// Devuelve nullptr si no se pudo crear.
	EncoderPtr video(Orientation o, const VideoProfile &p, video_t *source);
	EncoderPtr audio(int bitrateKbps);

	// Libera el codificador de una orientación cuando ya ningún destino lo usa.
	void dropVideo(Orientation o);

	// Libera los codificadores de video que ya no usa ninguna salida (p. ej. el de un
	// bitrate propio cuando ese destino se detiene y los demás siguen).
	void pruneUnused();

	void clear();

	// Elige el id de codificador de OBS según la preferencia y lo que haya
	// registrado esta instalación. Siempre cae a x264 si no hay nada mejor.
	static std::string resolveCodecId(const std::string &preference);

private:
	std::map<std::string, EncoderPtr> video_;
	std::map<std::string, EncoderPtr> audio_;
};
