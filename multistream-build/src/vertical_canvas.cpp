// OBS MultiStream Hub - Creado por Sintacto (@sintacto12)
#include "vertical_canvas.hpp"

#include <obs-frontend-api.h>
#include <util/base.h>

#include <algorithm>
#include <cmath>

VerticalCanvas::~VerticalCanvas()
{
	shutdown();
}

void VerticalCanvas::init()
{
	if (scene_)
		return;

	// Escena privada: no aparece en la lista de escenas de OBS ni en otros plugins.
	scene_ = obs_scene_create_private("MultiStream Hub Vertical");
	// Vista sin video todavía: el video se añade solo al transmitir.
	view_ = obs_view_create();
	if (!scene_ || !view_) {
		blog(LOG_ERROR, "[MultiStream Hub] No se pudo crear el lienzo vertical.");
		shutdown();
		return;
	}

	layout_ = loadLayout();
	rebuild();
	blog(LOG_INFO, "[MultiStream Hub] Lienzo vertical listo (%dx%d, %d capas).", w_, h_,
	     (int)layout_.layers.size());
}

void VerticalCanvas::shutdown()
{
	stopVideo();
	setPreviewShowing(false);
	clearItems();
	if (view_) {
		obs_view_destroy(view_);
		view_ = nullptr;
	}
	if (scene_) {
		obs_scene_release(scene_);
		scene_ = nullptr;
	}
}

obs_source_t *VerticalCanvas::source() const
{
	return scene_ ? obs_scene_get_source(scene_) : nullptr;
}

// ---------------------------------------------------------------------------
// Video (solo mientras se transmite)
// ---------------------------------------------------------------------------

bool VerticalCanvas::startVideo(std::string &err)
{
	if (video_)
		return true;
	if (!scene_ || !view_) {
		err = "El lienzo vertical no está disponible.";
		return false;
	}

	// Mismas propiedades que el video principal (adaptador, formato, color, fps);
	// solo cambia el tamaño. Base = salida, así que no hay reescalado.
	obs_video_info ovi = {};
	if (!obs_get_video_info(&ovi)) {
		err = "OBS no tiene el video iniciado.";
		return false;
	}
	ovi.base_width = ovi.output_width = (uint32_t)w_;
	ovi.base_height = ovi.output_height = (uint32_t)h_;

	video_ = obs_view_add2(view_, &ovi);
	if (!video_) {
		err = "OBS no pudo crear la salida de video vertical.";
		return false;
	}
	// Al ponerla en el canal 0 las fuentes de la escena pasan a estar activas.
	obs_view_set_source(view_, 0, source());
	blog(LOG_INFO, "[MultiStream Hub] Video vertical iniciado (%dx%d).", w_, h_);
	return true;
}

void VerticalCanvas::stopVideo()
{
	if (!video_)
		return;
	// Primero se desactiva la escena y luego se quita la salida de video:
	// si no, las fuentes seguirían activas gastando recursos sin transmitir.
	obs_view_set_source(view_, 0, nullptr);
	obs_view_remove(view_);
	video_ = nullptr;
	blog(LOG_INFO, "[MultiStream Hub] Video vertical detenido.");
}

void VerticalCanvas::setPreviewShowing(bool on)
{
	obs_source_t *src = source();
	if (!src || on == showing_)
		return;
	if (on)
		obs_source_inc_showing(src);
	else
		obs_source_dec_showing(src);
	showing_ = on;
}

// ---------------------------------------------------------------------------
// Capas
// ---------------------------------------------------------------------------

void VerticalCanvas::place(obs_sceneitem_t *item, obs_source_t *src, const VerticalLayer &layer) const
{
	float sw = (float)obs_source_get_width(src);
	float sh = (float)obs_source_get_height(src);
	if (sw < 1.0f)
		sw = 1920.0f;
	if (sh < 1.0f)
		sh = 1080.0f;

	// Zoom 100 % = la escena llena el alto del lienzo (se ve el centro, recortado a 9:16).
	const float scale = ((float)h_ / sh) * (float)std::clamp(layer.zoomPercent, 10, 500) / 100.0f;

	vec2 s;
	vec2_set(&s, scale, scale);
	vec2 p;
	vec2_set(&p, layer.x * (float)w_, layer.y * (float)h_);

	obs_sceneitem_set_bounds_type(item, OBS_BOUNDS_NONE);
	obs_sceneitem_set_alignment(item, OBS_ALIGN_CENTER);
	obs_sceneitem_set_scale(item, &s);
	obs_sceneitem_set_pos(item, &p);
}

void VerticalCanvas::clearItems()
{
	for (obs_sceneitem_t *it : items_)
		if (it)
			obs_sceneitem_remove(it); // quita el elemento y suelta la referencia a la escena
	items_.clear();
	followSrc_ = nullptr;
}

void VerticalCanvas::rebuild()
{
	if (!scene_)
		return;
	clearItems();

	obs_source_t *self = source();
	for (const auto &layer : layout_.layers) {
		const bool follow = layer.scene == kFollowScene;
		obs_source_t *src = nullptr;
		if (follow)
			src = obs_frontend_get_current_scene();
		else if (!layer.scene.empty())
			src = obs_get_source_by_name(layer.scene.c_str());

		obs_sceneitem_t *item = nullptr;
		if (src && src != self) { // escena o cualquier fuente de OBS
			item = obs_scene_add(scene_, src);
			if (follow)
				followSrc_ = src;
		}
		if (item) {
			obs_sceneitem_set_visible(item, layer.visible);
			place(item, src, layer);
		}
		blog(LOG_INFO, "[MultiStream Hub] Capa '%s': escena %s, elemento %s (%dx%d).", layer.scene.c_str(),
		     src ? "encontrada" : "NO encontrada", item ? "creado" : "NO creado",
		     src ? (int)obs_source_get_width(src) : 0, src ? (int)obs_source_get_height(src) : 0);
		items_.push_back(item);
		if (src)
			obs_source_release(src);
	}
}

void VerticalCanvas::refreshFollow()
{
	bool anyFollow = false;
	for (const auto &l : layout_.layers)
		anyFollow = anyFollow || l.scene == kFollowScene;
	if (!anyFollow || !scene_)
		return;

	obs_source_t *cur = obs_frontend_get_current_scene();
	const void *p = cur;
	if (cur)
		obs_source_release(cur);
	if (p != followSrc_)
		rebuild();
}

void VerticalCanvas::setLayout(const VerticalLayout &layout)
{
	layout_ = layout;
	rebuild();
	saveLayout(layout_);
}

void VerticalCanvas::setLayerTransform(int index, float x, float y, int zoomPercent)
{
	if (index < 0 || index >= (int)layout_.layers.size())
		return;
	VerticalLayer &ly = layout_.layers[(size_t)index];
	ly.x = std::clamp(x, -1.0f, 2.0f);
	ly.y = std::clamp(y, -1.0f, 2.0f);
	ly.zoomPercent = std::clamp(zoomPercent, 10, 500);

	if ((size_t)index < items_.size() && items_[(size_t)index])
		place(items_[(size_t)index], obs_sceneitem_get_source(items_[(size_t)index]), ly);
}

void VerticalCanvas::commit()
{
	saveLayout(layout_);
}

int VerticalCanvas::fullSceneZoom(int index) const
{
	float sw = 1920.0f, sh = 1080.0f;
	if (index >= 0 && (size_t)index < items_.size() && items_[(size_t)index]) {
		obs_source_t *src = obs_sceneitem_get_source(items_[(size_t)index]);
		if (src) {
			float w = (float)obs_source_get_width(src);
			float h = (float)obs_source_get_height(src);
			if (w >= 1.0f && h >= 1.0f) {
				sw = w;
				sh = h;
			}
		}
	}
	return (int)std::lround(100.0f * ((float)w_ * sh) / (sw * (float)h_));
}

bool VerticalCanvas::layerBounds(int index, float &cx, float &cy, float &w, float &h) const
{
	if (index < 0 || (size_t)index >= items_.size() || !items_[(size_t)index])
		return false;
	obs_sceneitem_t *it = items_[(size_t)index];
	obs_source_t *src = obs_sceneitem_get_source(it);
	float sw = src ? (float)obs_source_get_width(src) : 0.0f;
	float sh = src ? (float)obs_source_get_height(src) : 0.0f;
	if (sw < 1.0f)
		sw = 1920.0f;
	if (sh < 1.0f)
		sh = 1080.0f;
	vec2 pos, scale;
	obs_sceneitem_get_pos(it, &pos);
	obs_sceneitem_get_scale(it, &scale);
	cx = pos.x;
	cy = pos.y;
	w = sw * scale.x;
	h = sh * scale.y;
	return true;
}
