#pragma once
// OBS MultiStream Hub - Creado por Sintacto (@sintacto12)
//
// Lienzo vertical (9:16) propio del plugin, compartido por todos los destinos
// verticales (YouTube vertical, TikTok...).
//
// Cómo ahorra recursos:
//  - Existe SIEMPRE (para poder editarlo y verlo antes de transmitir), pero en
//    reposo es solo una escena privada: no tiene salida de video, no renderiza
//    y no activa fuentes.
//  - Su salida de video (la que alimenta al codificador) se crea únicamente
//    mientras hay un destino vertical transmitiendo.
//  - La vista previa solo se dibuja mientras el panel está visible.
//
// Contenido: una pila de capas; cada capa es una escena del lienzo horizontal
// de OBS, anidada, escalada y colocada dentro de 1080x1920.
//
// Todos los métodos se llaman desde el hilo de la interfaz (Qt).

#include <obs.h>

#include <string>
#include <vector>

#include "destination.hpp"

class VerticalCanvas {
public:
	VerticalCanvas() = default;
	~VerticalCanvas();

	VerticalCanvas(const VerticalCanvas &) = delete;
	VerticalCanvas &operator=(const VerticalCanvas &) = delete;

	// Crea la escena privada y la vista (sin video). Idempotente.
	void init();
	// Libera todo. Antes hay que haber destruido la vista previa y los codificadores.
	void shutdown();

	int width() const { return w_; }
	int height() const { return h_; }

	// Escena vertical (sin incrementar referencia). nullptr si no está iniciado.
	obs_source_t *source() const;

	// Salida de video para el codificador vertical: solo existe al transmitir.
	bool startVideo(std::string &err);
	void stopVideo();
	bool videoActive() const { return video_ != nullptr; }
	video_t *video() const { return video_; }

	// Capas.
	const VerticalLayout &layout() const { return layout_; }
	void setLayout(const VerticalLayout &layout); // reconstruye y guarda
	void setLayerTransform(int index, float x, float y, int zoomPercent); // en vivo, sin guardar
	void commit();                                                        // guarda layout.json
	int fullSceneZoom(int index) const; // zoom para ver la escena completa (con bandas)
	// Centro y tamaño de una capa en píxeles del lienzo (para dibujar su recuadro).
	bool layerBounds(int index, float &cx, float &cy, float &w, float &h) const;

	// Eventos de OBS.
	void rebuild();       // vuelve a resolver las escenas por nombre
	void clearItems();    // suelta las escenas (cambio de colección)
	void refreshFollow(); // la escena activa de OBS cambió

	// La vista previa pide que las fuentes de la escena estén "mostrándose".
	void setPreviewShowing(bool on);

private:
	void place(obs_sceneitem_t *item, obs_source_t *src, const VerticalLayer &layer) const;

	obs_scene_t *scene_ = nullptr;
	obs_view_t *view_ = nullptr;
	video_t *video_ = nullptr;
	std::vector<obs_sceneitem_t *> items_; // paralelo a layout_.layers (nullptr si falta la escena)
	VerticalLayout layout_;
	const void *followSrc_ = nullptr; // solo para comparar, nunca se usa como puntero
	bool showing_ = false;
	int w_ = 1080;
	int h_ = 1920;
};
