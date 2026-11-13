#pragma once
// OBS MultiStream Hub - Creado por Sintacto (@sintacto12)

#include <string>
#include <vector>

enum class Platform { TikTok, Kick, Twitch, YouTubeH, YouTubeV, Starvios, Custom };
enum class Orientation { Horizontal, Vertical };

// Perfil de video COMPARTIDO por todos los destinos de una misma orientación.
struct VideoProfile {
	std::string encoder = "auto"; // auto | x264 | nvenc | amf | qsv
	int width = 0;                // 0 = misma resolución que OBS (sin escalar)
	int height = 0;
	int bitrateKbps = 6000;
	int keyintSec = 2;
};

// Ajustes globales de emisión: 1 codificador horizontal + 1 vertical + 1 audio.
struct StreamSettings {
	VideoProfile horizontal;
	VideoProfile vertical;
	int audioKbps = 160;

	StreamSettings()
	{
		vertical.width = 1080;
		vertical.height = 1920;
	}
};

// ---------------------------------------------------------------------------
// Lienzo vertical: una pila de capas, cada una es una escena de OBS.
// ---------------------------------------------------------------------------

// Nombre especial: la capa muestra la escena que esté activa en OBS en ese momento.
constexpr const char *kFollowScene = "@actual";

struct VerticalLayer {
	std::string scene;   // nombre de una escena de OBS, o kFollowScene
	int zoomPercent = 100; // 100 = la escena llena el alto del lienzo vertical
	float x = 0.5f;      // centro de la capa dentro del lienzo (0..1)
	float y = 0.5f;
	bool visible = true;
};

struct VerticalLayout {
	std::vector<VerticalLayer> layers; // la primera es la de abajo
};

struct Destination {
	std::string id;
	Platform platform = Platform::Custom;
	std::string name;
	bool enabled = false;
	Orientation orientation = Orientation::Horizontal;
	std::string server;    // rtmp:// o rtmps://
	std::string streamKey; // solo en memoria; en disco va cifrada (DPAPI)
	// Bitrate propio de este destino (kbps). 0 = usa el compartido de «Ajustes de emisión».
	// Si es distinto al compartido, se crea un codificador extra solo para este destino.
	int bitrateKbps = 0;
};

std::vector<Destination> defaultDestinations();

// Carga la configuración guardada y la mezcla sobre los valores por defecto.
std::vector<Destination> loadDestinations();
void saveDestinations(const std::vector<Destination> &dests);

// Ajustes globales de codificación (settings.json).
StreamSettings loadSettings();
void saveSettings(const StreamSettings &s);

// Capas del lienzo vertical (layout.json).
VerticalLayout loadLayout();
void saveLayout(const VerticalLayout &layout);

// Comprueba que el destino se puede iniciar de verdad. Si no, explica por qué.
bool validateDestination(const Destination &d, std::string &why);
