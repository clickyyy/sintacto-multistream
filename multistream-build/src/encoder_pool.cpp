// OBS MultiStream Hub - Creado por Sintacto (@sintacto12)
#include "encoder_pool.hpp"

#include <obs.h>
#include <util/base.h>
#include <vector>

static bool encoderRegistered(const char *wanted)
{
	const char *id = nullptr;
	for (size_t i = 0; obs_enum_encoder_types(i, &id); i++) {
		if (id && std::string(id) == wanted)
			return true;
	}
	return false;
}

std::string EncoderPool::resolveCodecId(const std::string &pref)
{
	// Los ids cambian entre versiones de OBS, por eso se prueban varios y solo
	// se usa el que esta instalación tiene registrado.
	// TODO: confirmar la lista contra OBS 32.2.2.
	static const std::vector<const char *> nvenc = {"obs_nvenc_h264_tex", "jim_nvenc", "ffmpeg_nvenc"};
	static const std::vector<const char *> amf = {"h264_texture_amf", "amd_amf_h264"};
	static const std::vector<const char *> qsv = {"obs_qsv11_v2", "obs_qsv11", "obs_qsv11_soft_v2"};
	static const std::vector<const char *> x264 = {"obs_x264"};

	std::vector<const std::vector<const char *> *> order;
	if (pref == "nvenc")
		order = {&nvenc};
	else if (pref == "amf")
		order = {&amf};
	else if (pref == "qsv")
		order = {&qsv};
	else if (pref == "x264")
		order = {&x264};
	else
		order = {&nvenc, &amf, &qsv, &x264}; // auto: hardware primero

	for (auto *group : order)
		for (const char *id : *group)
			if (encoderRegistered(id))
				return id;

	if (pref != "auto" && pref != "x264")
		blog(LOG_WARNING, "[MultiStream Hub] Codificador '%s' no disponible; se usa x264.", pref.c_str());
	return "obs_x264";
}

EncoderPtr EncoderPool::video(Orientation o, const VideoProfile &p, video_t *source)
{
	if (!source)
		return nullptr;

	// La clave es orientación + bitrate: los destinos con el mismo bitrate comparten
	// codificador; uno con bitrate propio distinto recibe uno aparte.
	const std::string key = std::string((o == Orientation::Vertical) ? "V" : "H") + "-" + std::to_string(p.bitrateKbps);
	auto it = video_.find(key);
	if (it != video_.end())
		return it->second;

	std::string codec = resolveCodecId(p.encoder);

	obs_data_t *s = obs_data_create();
	obs_data_set_string(s, "rate_control", "CBR");
	obs_data_set_int(s, "bitrate", p.bitrateKbps);
	obs_data_set_int(s, "keyint_sec", p.keyintSec);
	if (codec == "obs_x264")
		obs_data_set_string(s, "preset", "veryfast");

	std::string name = "mshub-venc-" + key;
	EncoderPtr enc = wrapEncoder(obs_video_encoder_create(codec.c_str(), name.c_str(), s, nullptr));
	obs_data_release(s);
	if (!enc) {
		blog(LOG_ERROR, "[MultiStream Hub] No se pudo crear el codificador de video '%s'.", codec.c_str());
		return nullptr;
	}

	obs_encoder_set_video(enc.get(), source);
	// Solo se escala si la resolución pedida difiere de la de la fuente (el escalado cuesta GPU).
	const uint32_t srcW = video_output_get_width(source);
	const uint32_t srcH = video_output_get_height(source);
	if (p.width > 0 && p.height > 0 && ((uint32_t)p.width != srcW || (uint32_t)p.height != srcH))
		obs_encoder_set_scaled_size(enc.get(), (uint32_t)p.width, (uint32_t)p.height);

	blog(LOG_INFO, "[MultiStream Hub] Codificador de video %s: %s (%d kbps)", key.c_str(), codec.c_str(),
	     p.bitrateKbps);
	video_[key] = enc;
	return enc;
}

EncoderPtr EncoderPool::audio(int bitrateKbps)
{
	const std::string key = "A"; // uno solo para todos
	auto it = audio_.find(key);
	if (it != audio_.end())
		return it->second;

	obs_data_t *s = obs_data_create();
	obs_data_set_int(s, "bitrate", bitrateKbps);
	// Mezcla 0 = pista de audio 1 de OBS.
	EncoderPtr enc = wrapEncoder(obs_audio_encoder_create("ffmpeg_aac", "mshub-aenc", s, 0, nullptr));
	obs_data_release(s);
	if (!enc) {
		blog(LOG_ERROR, "[MultiStream Hub] No se pudo crear el codificador de audio.");
		return nullptr;
	}
	obs_encoder_set_audio(enc.get(), obs_get_audio());
	audio_[key] = enc;
	return enc;
}

void EncoderPool::dropVideo(Orientation o)
{
	const char prefix = (o == Orientation::Vertical) ? 'V' : 'H';
	for (auto it = video_.begin(); it != video_.end();) {
		if (!it->first.empty() && it->first[0] == prefix)
			it = video_.erase(it);
		else
			++it;
	}
}

void EncoderPool::pruneUnused()
{
	// use_count() == 1: solo el pool lo tiene, ninguna salida lo usa.
	for (auto it = video_.begin(); it != video_.end();) {
		if (it->second.use_count() == 1)
			it = video_.erase(it);
		else
			++it;
	}
}

void EncoderPool::clear()
{
	video_.clear();
	audio_.clear();
}
