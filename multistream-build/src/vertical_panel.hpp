#pragma once
// OBS MultiStream Hub - Creado por Sintacto (@sintacto12)
//
// Panel "Lienzo Vertical": vista previa 9:16 en vivo + editor de capas.
// Sirve para ver y preparar lo que saldrá en vertical ANTES de transmitir.
//
// Ahorro de recursos: la vista previa solo se dibuja mientras el panel está
// visible y activado; si lo ocultas o lo desactivas, no gasta GPU.

#include <QCheckBox>
#include <QComboBox>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QWidget>

#include <obs.h>

#include <atomic>
#include <functional>

#include "vertical_canvas.hpp"

// Vista previa 9:16. Arrastrar con el ratón mueve la capa seleccionada;
// la rueda cambia su zoom.
class VerticalPreview : public QWidget {
	Q_OBJECT

public:
	explicit VerticalPreview(VerticalCanvas *canvas, QWidget *parent = nullptr);
	~VerticalPreview() override;

	void setActive(bool active);
	void setSelected(int layerIndex) { selected_ = layerIndex; }
	void destroyDisplay(); // llamar antes de apagar el lienzo
	void setHighlight(float cx, float cy, float w, float h, bool on); // recuadro de la capa seleccionada

	QPaintEngine *paintEngine() const override { return nullptr; }

signals:
	void layerChanged(int index, float x, float y, int zoomPercent);
	void editFinished();
	void layerPicked(int index); // clic sobre otra capa
	void contextRequested(int layerIndex, const QPoint &globalPos); // clic derecho (capa bajo el cursor, o -1)

protected:
	void showEvent(QShowEvent *e) override;
	void hideEvent(QHideEvent *e) override;
	void resizeEvent(QResizeEvent *e) override;
	void paintEvent(QPaintEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void mouseReleaseEvent(QMouseEvent *e) override;
	void wheelEvent(QWheelEvent *e) override;
	void contextMenuEvent(QContextMenuEvent *e) override;

private:
	void createDisplay();
	void syncState();
	double logicalScale() const;
	int layerAt(const QPointF &pos) const; // capa de más arriba bajo ese punto, o -1

	static void drawCallback(void *param, uint32_t cx, uint32_t cy);

	VerticalCanvas *cv_;
	obs_display_t *display_ = nullptr;
	bool active_ = true;
	int selected_ = -1;
	bool dragging_ = false;
	QPointF last_;
	std::atomic<float> hlX_{0.0f}, hlY_{0.0f}, hlW_{0.0f}, hlH_{0.0f};
	std::atomic<bool> hlOn_{false};
};

class VerticalPanel : public QWidget {
	Q_OBJECT

public:
	explicit VerticalPanel(VerticalCanvas *canvas, QWidget *parent = nullptr);
	~VerticalPanel() override;

	void reloadScenes();   // lista de escenas disponibles para añadir
	void refreshLayers();  // repinta la lista de capas
	void shutdown();       // destruye la vista previa (antes de cerrar OBS)

private:
	int currentRow() const;
	void loadSelection();
	void onLayerChangedFromPreview(int index, float x, float y, int zoom);
	void onSpinsChanged();
	void mutate(const std::function<void(VerticalLayout &, int)> &fn);
	void updateHighlight();
	void showContextMenu(int hitLayer, const QPoint &globalPos); // clic derecho: añadir fuentes/escenas
	void addLayerNamed(const std::string &name, bool fitWhole);

	VerticalCanvas *cv_;
	VerticalPreview *preview_;
	QCheckBox *previewOn_;
	QListWidget *list_;
	QComboBox *sceneCombo_;
	QSpinBox *zoom_;
	QSpinBox *x_;
	QSpinBox *y_;
	QTimer saveTimer_;
	QTimer hlTimer_;
	bool loading_ = false;
};
