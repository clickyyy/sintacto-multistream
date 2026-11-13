// OBS MultiStream Hub - Creado por Sintacto (@sintacto12)
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/base.h>

#include <QWidget>

#include "dock.hpp"
#include "output_manager.hpp"
#include "vertical_panel.hpp"

OBS_DECLARE_MODULE()

MODULE_EXPORT const char *obs_module_description(void)
{
	return "Multistream Sintacto - Creado por Sintacto (@sintacto12)";
}

MODULE_EXPORT const char *obs_module_name(void)
{
	return "Multistream Sintacto";
}

static OutputManager *g_manager = nullptr;
static VerticalPanel *g_panel = nullptr;

// Orden de cierre: primero la vista previa (usa la escena), luego los
// codificadores y salidas, y al final el lienzo.
static void shutdownAll()
{
	if (g_panel)
		g_panel->shutdown();
	if (g_manager)
		g_manager->shutdown();
}

static void onFrontendEvent(enum obs_frontend_event event, void *)
{
	if (!g_manager)
		return;

	switch (event) {
	case OBS_FRONTEND_EVENT_EXIT:
		shutdownAll();
		break;
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING:
		// Las escenas de la colección actual van a desaparecer: se sueltan.
		g_manager->canvas().clearItems();
		break;
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
	case OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED:
		g_manager->canvas().rebuild();
		if (g_panel) {
			g_panel->reloadScenes();
			g_panel->refreshLayers();
		}
		break;
	case OBS_FRONTEND_EVENT_SCENE_CHANGED:
		// Las capas que siguen "la escena activa" se actualizan solas.
		g_manager->canvas().refreshFollow();
		break;
	default:
		break;
	}
}

bool obs_module_load(void)
{
	blog(LOG_INFO, "[MultiStream Hub] Cargando v0.3.0 (Sintacto @sintacto12)");

	auto *mainWindow = static_cast<QWidget *>(obs_frontend_get_main_window());
	if (!mainWindow)
		return false;

	g_manager = new OutputManager(mainWindow);
	auto *dock = new HubDock(g_manager, mainWindow);
	g_panel = new VerticalPanel(&g_manager->canvas(), mainWindow);

	// OBS toma la propiedad de los widgets de los docks.
	obs_frontend_add_dock_by_id("obs-multistream-hub", "Multistream Sintacto", dock);
	obs_frontend_add_dock_by_id("obs-multistream-hub-vertical", "Lienzo Vertical", g_panel);
	obs_frontend_add_event_callback(onFrontendEvent, nullptr);
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(onFrontendEvent, nullptr);
	shutdownAll();
	g_panel = nullptr; // su dueño Qt (el dock) lo destruye
	g_manager = nullptr;
}
