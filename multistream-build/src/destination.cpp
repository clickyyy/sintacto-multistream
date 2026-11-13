// OBS MultiStream Hub - Creado por Sintacto (@sintacto12)
#include "destination.hpp"
#include "secure_store.hpp"

#include <obs-module.h>
#include <util/platform.h>
#include <util/bmem.h>

#include <algorithm>

// Versión del formato de destinations.json. La 2 pasa TikTok a vertical.
static const int kDestinationsSchema = 2;

static Destination make(const char *id, Platform p, const char *name, const char *server, Orientation o)
{
	Destination d;
	d.id = id;
	d.platform = p;
	d.name = name;
	d.server = server;
	d.orientation = o;
	return d;
}

std::vector<Destination> defaultDestinations()
{
	// Solo se rellenan servidores públicos y estables. TikTok, Kick y Starvios
	// entregan la URL de ingesta desde su propio panel: el usuario la pega.
	// TODO: verificar estos valores por defecto contra la documentación oficial.
	return {
		make("tiktok", Platform::TikTok, "TikTok Live", "", Orientation::Vertical),
		make("kick", Platform::Kick, "Kick", "", Orientation::Horizontal),
		make("twitch", Platform::Twitch, "Twitch", "rtmp://live.twitch.tv/app", Orientation::Horizontal),
		make("youtube-h", Platform::YouTubeH, "YouTube Live (horizontal)", "rtmp://a.rtmp.youtube.com/live2",
		     Orientation::Horizontal),
		make("youtube-v", Platform::YouTubeV, "YouTube Live (vertical)", "rtmp://a.rtmp.youtube.com/live2",
		     Orientation::Vertical),
		make("starvios", Platform::Starvios, "Starvios Stream", "", Orientation::Horizontal),
	};
}

static std::string configPathFor(const char *file)
{
	char *dir = obs_module_config_path("");
	if (dir) {
		os_mkdirs(dir);
		bfree(dir);
	}
	char *p = obs_module_config_path(file);
	std::string path = p ? p : "";
	bfree(p);
	return path;
}

std::vector<Destination> loadDestinations()
{
	std::vector<Destination> dests = defaultDestinations();

	std::string path = configPathFor("destinations.json");
	if (path.empty())
		return dests;

	obs_data_t *root = obs_data_create_from_json_file(path.c_str());
	if (!root)
		return dests; // primera ejecución o archivo ausente

	const int schema = (int)obs_data_get_int(root, "schema");

	obs_data_array_t *arr = obs_data_get_array(root, "destinations");
	size_t n = arr ? obs_data_array_count(arr) : 0;
	for (size_t i = 0; i < n; i++) {
		obs_data_t *o = obs_data_array_item(arr, i);
		std::string id = obs_data_get_string(o, "id");
		for (auto &d : dests) {
			if (d.id != id)
				continue;
			d.enabled = obs_data_get_bool(o, "enabled");
			d.server = obs_data_get_string(o, "server");
			d.streamKey = unprotectSecret(obs_data_get_string(o, "key_enc"));
			d.bitrateKbps = std::clamp((int)obs_data_get_int(o, "bitrate"), 0, 100000);
			d.orientation = obs_data_get_int(o, "orientation") == 1 ? Orientation::Vertical
										: Orientation::Horizontal;
			// Migración: antes TikTok se guardaba como horizontal. Ahora comparte
			// el lienzo vertical con YouTube vertical.
			if (schema < kDestinationsSchema && d.platform == Platform::TikTok)
				d.orientation = Orientation::Vertical;
		}
		obs_data_release(o);
	}
	if (arr)
		obs_data_array_release(arr);
	obs_data_release(root);
	return dests;
}

void saveDestinations(const std::vector<Destination> &dests)
{
	std::string path = configPathFor("destinations.json");
	if (path.empty())
		return;

	obs_data_t *root = obs_data_create();
	obs_data_set_int(root, "schema", kDestinationsSchema);
	obs_data_array_t *arr = obs_data_array_create();
	for (const auto &d : dests) {
		obs_data_t *o = obs_data_create();
		obs_data_set_string(o, "id", d.id.c_str());
		obs_data_set_bool(o, "enabled", d.enabled);
		obs_data_set_string(o, "server", d.server.c_str());
		obs_data_set_string(o, "key_enc", protectSecret(d.streamKey).c_str());
		obs_data_set_int(o, "bitrate", d.bitrateKbps);
		obs_data_set_int(o, "orientation", d.orientation == Orientation::Vertical ? 1 : 0);
		obs_data_array_push_back(arr, o);
		obs_data_release(o);
	}
	obs_data_set_array(root, "destinations", arr);
	obs_data_array_release(arr);
	obs_data_save_json_safe(root, path.c_str(), "tmp", "bak");
	obs_data_release(root);
}

static void readProfile(obs_data_t *root, const char *key, VideoProfile &p)
{
	obs_data_t *o = obs_data_get_obj(root, key);
	if (!o)
		return;
	std::string enc = obs_data_get_string(o, "encoder");
	if (!enc.empty())
		p.encoder = enc;
	p.width = (int)obs_data_get_int(o, "width");
	p.height = (int)obs_data_get_int(o, "height");
	int br = (int)obs_data_get_int(o, "bitrate");
	if (br > 0)
		p.bitrateKbps = br;
	int ki = (int)obs_data_get_int(o, "keyint");
	if (ki > 0)
		p.keyintSec = ki;
	obs_data_release(o);
}

static void writeProfile(obs_data_t *root, const char *key, const VideoProfile &p)
{
	obs_data_t *o = obs_data_create();
	obs_data_set_string(o, "encoder", p.encoder.c_str());
	obs_data_set_int(o, "width", p.width);
	obs_data_set_int(o, "height", p.height);
	obs_data_set_int(o, "bitrate", p.bitrateKbps);
	obs_data_set_int(o, "keyint", p.keyintSec);
	obs_data_set_obj(root, key, o);
	obs_data_release(o);
}

StreamSettings loadSettings()
{
	StreamSettings s;
	std::string path = configPathFor("settings.json");
	if (path.empty())
		return s;
	obs_data_t *root = obs_data_create_from_json_file(path.c_str());
	if (!root)
		return s;
	readProfile(root, "horizontal", s.horizontal);
	readProfile(root, "vertical", s.vertical);
	int a = (int)obs_data_get_int(root, "audio_kbps");
	if (a > 0)
		s.audioKbps = a;
	obs_data_release(root);
	return s;
}

void saveSettings(const StreamSettings &s)
{
	std::string path = configPathFor("settings.json");
	if (path.empty())
		return;
	obs_data_t *root = obs_data_create();
	writeProfile(root, "horizontal", s.horizontal);
	writeProfile(root, "vertical", s.vertical);
	obs_data_set_int(root, "audio_kbps", s.audioKbps);
	obs_data_save_json_safe(root, path.c_str(), "tmp", "bak");
	obs_data_release(root);
}

// ---------------------------------------------------------------------------
// Capas del lienzo vertical
// ---------------------------------------------------------------------------

static VerticalLayout defaultLayout()
{
	VerticalLayout l;
	VerticalLayer base; // una capa que sigue la escena activa de OBS
	base.scene = kFollowScene;
	l.layers.push_back(base);
	return l;
}

VerticalLayout loadLayout()
{
	std::string path = configPathFor("layout.json");
	if (path.empty())
		return defaultLayout();

	obs_data_t *root = obs_data_create_from_json_file(path.c_str());
	if (!root)
		return defaultLayout();

	VerticalLayout l;
	obs_data_array_t *arr = obs_data_get_array(root, "layers");
	size_t n = arr ? obs_data_array_count(arr) : 0;
	for (size_t i = 0; i < n; i++) {
		obs_data_t *o = obs_data_array_item(arr, i);
		VerticalLayer ly;
		ly.scene = obs_data_get_string(o, "scene");
		ly.zoomPercent = std::clamp((int)obs_data_get_int(o, "zoom"), 10, 500);
		ly.x = std::clamp((float)obs_data_get_double(o, "x"), -1.0f, 2.0f);
		ly.y = std::clamp((float)obs_data_get_double(o, "y"), -1.0f, 2.0f);
		ly.visible = obs_data_get_bool(o, "visible");
		obs_data_release(o);
		if (!ly.scene.empty())
			l.layers.push_back(ly);
	}
	if (arr)
		obs_data_array_release(arr);
	obs_data_release(root);

	// Archivo sin capas válidas (o con valores vacíos): se parte de la capa base.
	if (l.layers.empty())
		return defaultLayout();
	return l;
}

void saveLayout(const VerticalLayout &layout)
{
	std::string path = configPathFor("layout.json");
	if (path.empty())
		return;

	obs_data_t *root = obs_data_create();
	obs_data_array_t *arr = obs_data_array_create();
	for (const auto &ly : layout.layers) {
		obs_data_t *o = obs_data_create();
		obs_data_set_string(o, "scene", ly.scene.c_str());
		obs_data_set_int(o, "zoom", ly.zoomPercent);
		obs_data_set_double(o, "x", ly.x);
		obs_data_set_double(o, "y", ly.y);
		obs_data_set_bool(o, "visible", ly.visible);
		obs_data_array_push_back(arr, o);
		obs_data_release(o);
	}
	obs_data_set_array(root, "layers", arr);
	obs_data_array_release(arr);
	obs_data_save_json_safe(root, path.c_str(), "tmp", "bak");
	obs_data_release(root);
}

bool validateDestination(const Destination &d, std::string &why)
{
	// La disponibilidad del lienzo vertical se comprueba al iniciar (OutputManager),
	// porque depende de que OBS pueda crear su salida de video en ese momento.
	if (d.server.empty()) {
		why = "Falta la URL del servidor.";
		return false;
	}
	if (d.server.rfind("rtmp://", 0) != 0 && d.server.rfind("rtmps://", 0) != 0) {
		why = "La URL debe empezar por rtmp:// o rtmps://";
		return false;
	}
	if (d.streamKey.empty()) {
		why = "Falta la clave de transmisión.";
		return false;
	}
	return true;
}
