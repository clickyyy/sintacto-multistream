// OBS MultiStream Hub - Creado por Sintacto (@sintacto12)
#include "vertical_panel.hpp"
#include "theme.hpp"

#include <obs-frontend-api.h>
#include <graphics/graphics.h>
#include <graphics/vec4.h>
#include <util/base.h>

#include <QAction>
#include <QContextMenuEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QMenu>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <QTimer>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <functional>
#include <set>
#include <string>
#include <vector>

namespace {

const char *kPanelStyle = theme::kStyle;

// obs_display_create tiene dos formas según la versión de libobs:
//   (gs_init_data*, uint32_t color_de_fondo)  y  (gs_init_data*).
// Esta pareja de plantillas elige la que exista, para compilar con cualquiera.
template<class F> auto createDisplayImpl(F f, const gs_init_data *i, uint32_t bg, int) -> decltype(f(i, bg))
{
	return f(i, bg);
}
template<class F> auto createDisplayImpl(F f, const gs_init_data *i, uint32_t, long) -> decltype(f(i))
{
	return f(i);
}

} // namespace

// ---------------------------------------------------------------------------
// Traer fuentes del lienzo horizontal
// ---------------------------------------------------------------------------

static bool enumSceneItem(obs_scene_t *, obs_sceneitem_t *item, void *param)
{
	auto *v = static_cast<std::vector<std::string> *>(param);
	obs_source_t *s = obs_sceneitem_get_source(item);
	const char *n = s ? obs_source_get_name(s) : nullptr;
	if (n && *n)
		v->push_back(n);
	return true;
}

// Muestra las fuentes de una escena del horizontal y devuelve las marcadas,
// en orden de abajo hacia arriba (así quedan en el mismo orden en el vertical).
static std::vector<std::string> pickHorizontalSources(QWidget *parent)
{
	QDialog dlg(parent);
	dlg.setWindowTitle(QStringLiteral("Traer fuentes del horizontal"));
	dlg.setStyleSheet(QString::fromUtf8(kPanelStyle));
	dlg.setMinimumWidth(360);

	auto *intro = new QLabel(QStringLiteral("Marca las fuentes de tu escena normal que quieres copiar al vertical. "
						"No se duplican: son las mismas fuentes."));
	intro->setWordWrap(true);
	intro->setObjectName(QStringLiteral("hint"));

	auto *sceneBox = new QComboBox;
	auto *list = new QListWidget;
	list->setMinimumHeight(180);

	QString current;
	if (obs_source_t *cur = obs_frontend_get_current_scene()) {
		const char *cn = obs_source_get_name(cur);
		current = cn ? QString::fromUtf8(cn) : QString();
		obs_source_release(cur);
	}
	obs_frontend_source_list scenes = {};
	obs_frontend_get_scenes(&scenes);
	for (size_t i = 0; i < scenes.sources.num; i++) {
		const char *nm = obs_source_get_name(scenes.sources.array[i]);
		if (nm)
			sceneBox->addItem(QString::fromUtf8(nm));
	}
	obs_frontend_source_list_free(&scenes);
	const int ci = sceneBox->findText(current);
	if (ci >= 0)
		sceneBox->setCurrentIndex(ci);

	// Se muestran de arriba hacia abajo, como en OBS.
	auto fill = [&] {
		list->clear();
		std::vector<std::string> names;
		obs_source_t *sc = obs_get_source_by_name(sceneBox->currentText().toUtf8().constData());
		if (sc) {
			if (obs_scene_t *scene = obs_scene_from_source(sc))
				obs_scene_enum_items(scene, &enumSceneItem, &names);
			obs_source_release(sc);
		}
		for (auto it = names.rbegin(); it != names.rend(); ++it) {
			auto *li = new QListWidgetItem(QString::fromStdString(*it));
			li->setFlags(li->flags() | Qt::ItemIsUserCheckable);
			li->setCheckState(Qt::Checked);
			list->addItem(li);
		}
	};
	QObject::connect(sceneBox, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg, [&](int) { fill(); });
	fill();

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Traer al vertical"));
	buttons->button(QDialogButtonBox::Ok)->setObjectName(QStringLiteral("primary"));
	buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Cancelar"));
	QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
	QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

	auto *lay = new QVBoxLayout(&dlg);
	lay->setContentsMargins(18, 18, 18, 18);
	lay->setSpacing(10);
	lay->addWidget(intro);
	lay->addWidget(sceneBox);
	lay->addWidget(list, 1);
	lay->addWidget(buttons);

	std::vector<std::string> out;
	if (dlg.exec() != QDialog::Accepted)
		return out;
	for (int i = list->count() - 1; i >= 0; i--)
		if (list->item(i)->checkState() == Qt::Checked)
			out.push_back(list->item(i)->text().toStdString());
	return out;
}

// ---------------------------------------------------------------------------
// VerticalPreview
// ---------------------------------------------------------------------------

VerticalPreview::VerticalPreview(VerticalCanvas *canvas, QWidget *parent) : QWidget(parent), cv_(canvas)
{
	// El dibujo lo hace libobs directamente sobre la ventana nativa.
	setAttribute(Qt::WA_PaintOnScreen);
	setAttribute(Qt::WA_NativeWindow);
	setAttribute(Qt::WA_NoSystemBackground);
	setAttribute(Qt::WA_OpaquePaintEvent);
	setMinimumSize(180, 320);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	setCursor(Qt::OpenHandCursor);
}

VerticalPreview::~VerticalPreview()
{
	destroyDisplay();
}

void VerticalPreview::createDisplay()
{
	if (display_ || !isVisible())
		return;

	const double dpr = devicePixelRatioF();
	gs_init_data info = {};
	info.cx = (uint32_t)std::max(1.0, width() * dpr);
	info.cy = (uint32_t)std::max(1.0, height() * dpr);
	info.format = GS_BGRA;
	info.zsformat = GS_ZS_NONE;
#ifdef _WIN32
	info.window.hwnd = (void *)winId();
#endif
	display_ = createDisplayImpl(&obs_display_create, &info, 0xFF1A0C0Bu, 0);
	if (!display_) {
		blog(LOG_ERROR, "[MultiStream Hub] No se pudo crear la vista previa vertical.");
		return;
	}
	obs_display_add_draw_callback(display_, &VerticalPreview::drawCallback, this);
	blog(LOG_INFO, "[MultiStream Hub] Vista previa creada (%ux%u).", info.cx, info.cy);
}

void VerticalPreview::destroyDisplay()
{
	if (cv_)
		cv_->setPreviewShowing(false);
	if (!display_)
		return;
	obs_display_remove_draw_callback(display_, &VerticalPreview::drawCallback, this);
	obs_display_destroy(display_);
	display_ = nullptr;
}

void VerticalPreview::setHighlight(float cx, float cy, float w, float h, bool on)
{
	hlX_ = cx;
	hlY_ = cy;
	hlW_ = w;
	hlH_ = h;
	hlOn_ = on;
}

void VerticalPreview::setActive(bool active)
{
	active_ = active;
	syncState();
}

// La vista previa solo trabaja si el panel es visible Y está activada.
void VerticalPreview::syncState()
{
	const bool on = active_ && isVisible();
	if (display_)
		obs_display_set_enabled(display_, on);
	if (cv_)
		cv_->setPreviewShowing(on);
}

void VerticalPreview::showEvent(QShowEvent *e)
{
	QWidget::showEvent(e);
	// Se difiere un ciclo: la ventana nativa ya tiene tamaño y está expuesta.
	QTimer::singleShot(0, this, [this] {
		createDisplay();
		syncState();
	});
}

void VerticalPreview::hideEvent(QHideEvent *e)
{
	QWidget::hideEvent(e);
	syncState();
}

void VerticalPreview::resizeEvent(QResizeEvent *e)
{
	QWidget::resizeEvent(e);
	if (display_) {
		const double dpr = devicePixelRatioF();
		obs_display_resize(display_, (uint32_t)std::max(1.0, width() * dpr),
				   (uint32_t)std::max(1.0, height() * dpr));
	}
}

void VerticalPreview::paintEvent(QPaintEvent *)
{
	// Intencionadamente vacío: dibuja libobs.
}

// Se ejecuta en el hilo de gráficos de OBS.
void VerticalPreview::drawCallback(void *param, uint32_t cx, uint32_t cy)
{
	auto *self = static_cast<VerticalPreview *>(param);
	if (!self->cv_)
		return;
	obs_source_t *src = self->cv_->source();
	if (!src)
		return;

	static std::atomic<bool> logged{false};
	if (!logged.exchange(true))
		blog(LOG_INFO, "[MultiStream Hub] Primer dibujo de la vista previa: %ux%u, escena %dx%d.", cx, cy,
		     (int)obs_source_get_width(src), (int)obs_source_get_height(src));

	const float W = (float)self->cv_->width();
	const float H = (float)self->cv_->height();
	const float scale = std::min((float)cx / W, (float)cy / H);
	const int vw = (int)(W * scale);
	const int vh = (int)(H * scale);
	const int vx = ((int)cx - vw) / 2;
	const int vy = ((int)cy - vh) / 2;

	gs_viewport_push();
	gs_projection_push();
	gs_blend_state_push();
	gs_reset_blend_state();
	gs_ortho(0.0f, W, 0.0f, H, -100.0f, 100.0f);
	gs_set_viewport(vx, vy, vw, vh);

	// Lo que cae fuera del lienzo 9:16 no se ve (igual que en la transmisión).
	gs_rect clip = {vx, vy, vw, vh};
	gs_set_scissor_rect(&clip);

	// Diagnóstico: fondo gris 9:16. Si se ve el gris, la ventana y el viewport funcionan.
	if (gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID)) {
		if (gs_eparam_t *col = gs_effect_get_param_by_name(solid, "color")) {
			vec4 c;
			vec4_set(&c, 0.09f, 0.10f, 0.19f, 1.0f);
			gs_effect_set_vec4(col, &c);
			while (gs_effect_loop(solid, "Solid"))
				gs_draw_sprite(nullptr, 0, (uint32_t)W, (uint32_t)H);
		}
	}
	obs_source_video_render(src);

	// Recuadro violeta de la capa seleccionada (para ver dónde está aunque esté vacía).
	if (self->hlOn_.load()) {
		if (gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID)) {
			if (gs_eparam_t *col = gs_effect_get_param_by_name(solid, "color")) {
				vec4 c;
				vec4_set(&c, 0.65f, 0.55f, 0.98f, 1.0f);
				gs_effect_set_vec4(col, &c);
				const float hw = self->hlW_.load(), hh = self->hlH_.load();
				const float x0 = self->hlX_.load() - hw / 2.0f, y0 = self->hlY_.load() - hh / 2.0f;
				const float t = std::max(1.0f, 2.0f / std::max(scale, 0.01f));
				auto edge = [&](float ex, float ey, float ew, float eh) {
					gs_matrix_push();
					gs_matrix_translate3f(ex, ey, 0.0f);
					gs_draw_sprite(nullptr, 0, (uint32_t)std::max(1.0f, ew), (uint32_t)std::max(1.0f, eh));
					gs_matrix_pop();
				};
				while (gs_effect_loop(solid, "Solid")) {
					edge(x0, y0, hw, t);
					edge(x0, y0 + hh - t, hw, t);
					edge(x0, y0, t, hh);
					edge(x0 + hw - t, y0, t, hh);
				}
			}
		}
	}
	gs_set_scissor_rect(nullptr);

	gs_blend_state_pop();
	gs_projection_pop();
	gs_viewport_pop();
}

// Píxeles lógicos del widget por píxel del lienzo.
double VerticalPreview::logicalScale() const
{
	return std::min((double)width() / cv_->width(), (double)height() / cv_->height());
}

int VerticalPreview::layerAt(const QPointF &pos) const
{
	const double s = logicalScale();
	if (s <= 0.0)
		return -1;
	const double ox = (width() - cv_->width() * s) / 2.0;
	const double oy = (height() - cv_->height() * s) / 2.0;
	const float px = (float)((pos.x() - ox) / s);
	const float py = (float)((pos.y() - oy) / s);
	for (int i = (int)cv_->layout().layers.size() - 1; i >= 0; i--) {
		float cx, cy, w, h;
		if (cv_->layerBounds(i, cx, cy, w, h) && std::fabs(px - cx) <= w / 2 && std::fabs(py - cy) <= h / 2)
			return i;
	}
	return -1;
}

void VerticalPreview::contextMenuEvent(QContextMenuEvent *e)
{
	emit contextRequested(layerAt(e->pos()), e->globalPos());
}

void VerticalPreview::mousePressEvent(QMouseEvent *e)
{
	if (e->button() == Qt::LeftButton) {
		// Punto del clic en píxeles del lienzo.
		const double s = logicalScale();
		const double ox = (width() - cv_->width() * s) / 2.0;
		const double oy = (height() - cv_->height() * s) / 2.0;
		const float px = (float)((e->position().x() - ox) / s);
		const float py = (float)((e->position().y() - oy) / s);
		auto hit = [&](int i) {
			float cx, cy, w, h;
			return cv_->layerBounds(i, cx, cy, w, h) && std::fabs(px - cx) <= w / 2 && std::fabs(py - cy) <= h / 2;
		};
		// Si el clic no cae en la capa seleccionada, se elige la de más arriba que lo contenga.
		if (selected_ < 0 || !hit(selected_)) {
			for (int i = (int)cv_->layout().layers.size() - 1; i >= 0; i--) {
				if (hit(i)) {
					selected_ = i;
					emit layerPicked(i);
					break;
				}
			}
		}
	}
	if (e->button() == Qt::LeftButton && selected_ >= 0) {
		dragging_ = true;
		last_ = e->position();
		setCursor(Qt::ClosedHandCursor);
	}
}

void VerticalPreview::mouseMoveEvent(QMouseEvent *e)
{
	if (!dragging_ || selected_ < 0 || selected_ >= (int)cv_->layout().layers.size())
		return;
	const QPointF d = e->position() - last_;
	last_ = e->position();

	const double s = logicalScale();
	if (s <= 0.0)
		return;
	const VerticalLayer &ly = cv_->layout().layers[(size_t)selected_];
	const float nx = ly.x + (float)(d.x() / (cv_->width() * s));
	const float ny = ly.y + (float)(d.y() / (cv_->height() * s));
	cv_->setLayerTransform(selected_, nx, ny, ly.zoomPercent);

	const VerticalLayer &now = cv_->layout().layers[(size_t)selected_];
	emit layerChanged(selected_, now.x, now.y, now.zoomPercent);
}

void VerticalPreview::mouseReleaseEvent(QMouseEvent *e)
{
	if (e->button() == Qt::LeftButton && dragging_) {
		dragging_ = false;
		setCursor(Qt::OpenHandCursor);
		emit editFinished();
	}
}

void VerticalPreview::wheelEvent(QWheelEvent *e)
{
	if (selected_ < 0 || selected_ >= (int)cv_->layout().layers.size())
		return;
	const int steps = e->angleDelta().y() / 120;
	if (steps == 0)
		return;
	const VerticalLayer &ly = cv_->layout().layers[(size_t)selected_];
	cv_->setLayerTransform(selected_, ly.x, ly.y, ly.zoomPercent + steps * 5);

	const VerticalLayer &now = cv_->layout().layers[(size_t)selected_];
	emit layerChanged(selected_, now.x, now.y, now.zoomPercent);
	emit editFinished();
}

// ---------------------------------------------------------------------------
// VerticalPanel
// ---------------------------------------------------------------------------

VerticalPanel::VerticalPanel(VerticalCanvas *canvas, QWidget *parent) : QWidget(parent), cv_(canvas)
{
	setStyleSheet(QString::fromUtf8(kPanelStyle));

	preview_ = new VerticalPreview(cv_);

	auto makeCard = [] {
		auto *f = new QFrame;
		f->setObjectName(QStringLiteral("card"));
		return f;
	};
	auto makeSection = [](const QString &t) {
		auto *l = new QLabel(t);
		l->setObjectName(QStringLiteral("section"));
		return l;
	};

	auto *title = new QLabel(QStringLiteral("Lienzo vertical 9:16"));
	title->setObjectName(QStringLiteral("headerTitle"));
	auto *subtitle = new QLabel(QStringLiteral("Vista previa y capas de tu directo vertical"));
	subtitle->setObjectName(QStringLiteral("headerSub"));

	previewOn_ = new QCheckBox(QStringLiteral("Vista previa"));
	previewOn_->setChecked(true);
	previewOn_->setToolTip(QStringLiteral("Desactívala para no gastar GPU mientras no la necesites."));

	list_ = new QListWidget;
	list_->setMinimumHeight(100);
	list_->setMaximumHeight(160);

	sceneCombo_ = new QComboBox;
	auto *addBtn = new QPushButton(QStringLiteral("＋ Añadir"));
	addBtn->setObjectName(QStringLiteral("primary"));
	addBtn->setToolTip(QStringLiteral("Añade la escena o fuente elegida como una capa nueva."));
	auto *refreshBtn = new QPushButton(QStringLiteral("↻"));
	refreshBtn->setObjectName(QStringLiteral("icon"));
	refreshBtn->setToolTip(QStringLiteral("Actualizar la lista de escenas y fuentes."));
	connect(refreshBtn, &QPushButton::clicked, this, [this] { reloadScenes(); });

	auto *upBtn = new QPushButton(QStringLiteral("▲ Subir"));
	upBtn->setToolTip(QStringLiteral("Subir la capa (queda por encima de las demás)."));
	auto *downBtn = new QPushButton(QStringLiteral("▼ Bajar"));
	downBtn->setToolTip(QStringLiteral("Bajar la capa (queda por debajo de las demás)."));
	auto *importBtn = new QPushButton(QStringLiteral("⇩ Traer fuentes del horizontal…"));
	importBtn->setToolTip(QStringLiteral("Copia al vertical las fuentes de tu escena normal. No las duplica."));
	auto *delBtn = new QPushButton(QStringLiteral("Quitar"));
	delBtn->setObjectName(QStringLiteral("danger"));
	delBtn->setToolTip(QStringLiteral("Quita la capa del vertical. No borra la fuente de OBS."));

	zoom_ = new QSpinBox;
	zoom_->setRange(10, 500);
	zoom_->setSuffix(QStringLiteral(" %"));
	x_ = new QSpinBox;
	x_->setRange(-100, 200);
	x_->setSuffix(QStringLiteral(" %"));
	y_ = new QSpinBox;
	y_->setRange(-100, 200);
	y_->setSuffix(QStringLiteral(" %"));

	auto *fillBtn = new QPushButton(QStringLiteral("Llenar"));
	fillBtn->setToolTip(QStringLiteral("La capa ocupa todo el alto del lienzo."));
	auto *fullBtn = new QPushButton(QStringLiteral("Ver completa"));
	fullBtn->setToolTip(QStringLiteral("Se ve la capa entera, con bandas arriba y abajo."));
	auto *centerBtn = new QPushButton(QStringLiteral("Centrar"));
	centerBtn->setToolTip(QStringLiteral("Vuelve la capa al centro del lienzo."));

	auto *hint = new QLabel(QStringLiteral("Clic en la vista previa para elegir una capa · arrastra para moverla · "
					       "rueda del ratón para cambiar su tamaño · clic derecho para añadir fuentes."));
	hint->setObjectName(QStringLiteral("hint"));
	hint->setWordWrap(true);

	// Tarjeta: vista previa
	auto *previewCard = makeCard();
	auto *pl = new QVBoxLayout(previewCard);
	pl->setContentsMargins(8, 8, 8, 8);
	pl->addWidget(preview_);

	// Tarjeta: capas
	auto *layersCard = makeCard();
	auto *ll = new QVBoxLayout(layersCard);
	ll->setContentsMargins(12, 12, 12, 12);
	ll->setSpacing(8);
	ll->addWidget(makeSection(QStringLiteral("CAPAS")));
	ll->addWidget(list_);
	auto *listBtns = new QHBoxLayout;
	listBtns->addWidget(upBtn);
	listBtns->addWidget(downBtn);
	listBtns->addStretch(1);
	listBtns->addWidget(delBtn);
	ll->addLayout(listBtns);
	ll->addWidget(importBtn);
	auto *addRow = new QHBoxLayout;
	addRow->addWidget(sceneCombo_, 1);
	addRow->addWidget(refreshBtn);
	addRow->addWidget(addBtn);
	ll->addLayout(addRow);

	// Tarjeta: posición y tamaño
	auto *posCard = makeCard();
	auto *pc = new QVBoxLayout(posCard);
	pc->setContentsMargins(12, 12, 12, 12);
	pc->setSpacing(8);
	pc->addWidget(makeSection(QStringLiteral("TAMAÑO Y POSICIÓN DE LA CAPA")));
	auto *form = new QFormLayout;
	form->setVerticalSpacing(8);
	form->addRow(QStringLiteral("Tamaño"), zoom_);
	form->addRow(QStringLiteral("Horizontal"), x_);
	form->addRow(QStringLiteral("Vertical"), y_);
	pc->addLayout(form);
	auto *quick = new QHBoxLayout;
	quick->addWidget(fillBtn);
	quick->addWidget(fullBtn);
	quick->addWidget(centerBtn);
	pc->addLayout(quick);

	// Cabecera
	auto *head = new QFrame;
	head->setObjectName(QStringLiteral("header"));
	auto *hl = new QHBoxLayout(head);
	hl->setContentsMargins(14, 12, 14, 12);
	auto *ht = new QVBoxLayout;
	ht->setSpacing(1);
	ht->addWidget(title);
	ht->addWidget(subtitle);
	hl->addLayout(ht, 1);
	hl->addWidget(previewOn_, 0, Qt::AlignVCenter);

	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(10, 10, 10, 10);
	root->setSpacing(10);
	root->addWidget(head);
	root->addWidget(previewCard, 1);
	root->addWidget(layersCard);
	root->addWidget(posCard);
	root->addWidget(hint);

	// Guardado diferido: no se escribe el archivo en cada movimiento del ratón.
	saveTimer_.setSingleShot(true);
	saveTimer_.setInterval(400);
	connect(&saveTimer_, &QTimer::timeout, this, [this] { cv_->commit(); });

	connect(previewOn_, &QCheckBox::toggled, this, [this](bool on) { preview_->setActive(on); });

	connect(list_, &QListWidget::currentRowChanged, this, [this](int) { loadSelection(); });
	connect(list_, &QListWidget::itemChanged, this, [this](QListWidgetItem *it) {
		if (loading_)
			return;
		const int row = list_->row(it);
		const bool vis = it->checkState() == Qt::Checked;
		mutate([&](VerticalLayout &l, int) {
			if (row >= 0 && row < (int)l.layers.size())
				l.layers[(size_t)row].visible = vis;
		});
	});

	connect(addBtn, &QPushButton::clicked, this, [this] {
		const std::string scene = sceneCombo_->currentData().toString().toStdString();
		if (scene.empty())
			return;
		VerticalLayout l = cv_->layout();
		VerticalLayer ly;
		ly.scene = scene;
		l.layers.push_back(ly);
		cv_->setLayout(l);
		refreshLayers();
		list_->setCurrentRow((int)l.layers.size() - 1);
	});
	connect(delBtn, &QPushButton::clicked, this, [this] {
		const int row = currentRow();
		VerticalLayout l = cv_->layout();
		if (row < 0 || row >= (int)l.layers.size())
			return;
		l.layers.erase(l.layers.begin() + row);
		cv_->setLayout(l);
		refreshLayers();
		list_->setCurrentRow(std::min(row, (int)l.layers.size() - 1));
	});
	connect(upBtn, &QPushButton::clicked, this, [this] {
		const int row = currentRow();
		VerticalLayout l = cv_->layout();
		if (row < 0 || row + 1 >= (int)l.layers.size())
			return;
		std::swap(l.layers[(size_t)row], l.layers[(size_t)row + 1]);
		cv_->setLayout(l);
		refreshLayers();
		list_->setCurrentRow(row + 1);
	});
	connect(downBtn, &QPushButton::clicked, this, [this] {
		const int row = currentRow();
		VerticalLayout l = cv_->layout();
		if (row <= 0 || row >= (int)l.layers.size())
			return;
		std::swap(l.layers[(size_t)row], l.layers[(size_t)row - 1]);
		cv_->setLayout(l);
		refreshLayers();
		list_->setCurrentRow(row - 1);
	});

	connect(zoom_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) { onSpinsChanged(); });
	connect(x_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) { onSpinsChanged(); });
	connect(y_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) { onSpinsChanged(); });

	connect(fillBtn, &QPushButton::clicked, this, [this] { zoom_->setValue(100); });
	connect(fullBtn, &QPushButton::clicked, this, [this] {
		const int row = currentRow();
		if (row >= 0)
			zoom_->setValue(cv_->fullSceneZoom(row));
	});
	connect(centerBtn, &QPushButton::clicked, this, [this] {
		QSignalBlocker bx(x_), by(y_);
		x_->setValue(50);
		y_->setValue(50);
		onSpinsChanged();
	});

	connect(preview_, &VerticalPreview::layerChanged, this,
		[this](int i, float x, float y, int z) { onLayerChangedFromPreview(i, x, y, z); });
	connect(preview_, &VerticalPreview::editFinished, this, [this] { saveTimer_.start(); });
	connect(importBtn, &QPushButton::clicked, this, [this] {
		const std::vector<std::string> names = pickHorizontalSources(this);
		if (names.empty())
			return;
		VerticalLayout l = cv_->layout();
		const int first = (int)l.layers.size();
		for (const auto &nm : names) {
			VerticalLayer ly;
			ly.scene = nm;
			l.layers.push_back(ly);
		}
		cv_->setLayout(l); // reconstruye las capas
		// Cada fuente nueva entra entera y centrada; luego se ajusta con el ratón.
		for (int i = first; i < (int)l.layers.size(); i++)
			cv_->setLayerTransform(i, 0.5f, 0.5f, cv_->fullSceneZoom(i));
		cv_->commit();
		refreshLayers();
		list_->setCurrentRow((int)l.layers.size() - 1);
	});

	connect(preview_, &VerticalPreview::layerPicked, this, [this](int i) { list_->setCurrentRow(i); });

	// Clic derecho (en la vista previa o en la lista de capas): añadir fuentes y escenas.
	list_->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(list_, &QWidget::customContextMenuRequested, this, [this](const QPoint &p) {
		const QModelIndex idx = list_->indexAt(p);
		showContextMenu(idx.isValid() ? idx.row() : -1, list_->viewport()->mapToGlobal(p));
	});
	connect(preview_, &VerticalPreview::contextRequested, this,
		[this](int i, const QPoint &g) { showContextMenu(i, g); });

	hlTimer_.setInterval(100);
	connect(&hlTimer_, &QTimer::timeout, this, [this] { updateHighlight(); });
	hlTimer_.start();

	reloadScenes();
	refreshLayers();
}

VerticalPanel::~VerticalPanel()
{
	shutdown();
}

void VerticalPanel::shutdown()
{
	preview_->destroyDisplay();
}

int VerticalPanel::currentRow() const
{
	return list_->currentRow();
}

static bool collectInputs(void *param, obs_source_t *s)
{
	auto *names = static_cast<std::vector<std::string> *>(param);
	const char *nm = obs_source_get_name(s);
	if (nm && *nm && std::string(nm).rfind("MultiStream Hub", 0) != 0)
		names->push_back(nm);
	return true;
}

void VerticalPanel::reloadScenes()
{
	const QString keep = sceneCombo_->currentData().toString();
	sceneCombo_->clear();
	sceneCombo_->addItem(QStringLiteral("★ Escena activa de OBS"), QString::fromUtf8(kFollowScene));

	// Escenas del horizontal.
	obs_frontend_source_list scenes = {};
	obs_frontend_get_scenes(&scenes);
	for (size_t i = 0; i < scenes.sources.num; i++) {
		const char *nm = obs_source_get_name(scenes.sources.array[i]);
		if (nm)
			sceneCombo_->addItem(QStringLiteral("Escena: ") + QString::fromUtf8(nm), QString::fromUtf8(nm));
	}
	obs_frontend_source_list_free(&scenes);

	// Fuentes sueltas (captura de ventana, cámara, navegador, medios...).
	std::vector<std::string> inputs;
	obs_enum_sources(&collectInputs, &inputs);
	std::sort(inputs.begin(), inputs.end());
	for (const auto &nm : inputs)
		sceneCombo_->addItem(QStringLiteral("Fuente: ") + QString::fromStdString(nm), QString::fromStdString(nm));

	const int idx = sceneCombo_->findData(keep);
	if (idx >= 0)
		sceneCombo_->setCurrentIndex(idx);
}

void VerticalPanel::refreshLayers()
{
	loading_ = true;
	const int keep = list_->currentRow();
	list_->clear();
	for (const auto &ly : cv_->layout().layers) {
		const QString name = ly.scene == kFollowScene ? QStringLiteral("★ Escena activa de OBS")
							      : QString::fromStdString(ly.scene);
		auto *it = new QListWidgetItem(name);
		it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
		it->setCheckState(ly.visible ? Qt::Checked : Qt::Unchecked);
		list_->addItem(it);
	}
	if (keep >= 0 && keep < list_->count())
		list_->setCurrentRow(keep);
	else if (list_->count() > 0)
		list_->setCurrentRow(0);
	loading_ = false;
	loadSelection();
}

void VerticalPanel::loadSelection()
{
	const int row = currentRow();
	preview_->setSelected(row);
	if (row < 0 || row >= (int)cv_->layout().layers.size())
		return;

	const VerticalLayer &ly = cv_->layout().layers[(size_t)row];
	loading_ = true;
	QSignalBlocker bz(zoom_), bx(x_), by(y_);
	zoom_->setValue(ly.zoomPercent);
	x_->setValue((int)std::lround(ly.x * 100.0f));
	y_->setValue((int)std::lround(ly.y * 100.0f));
	loading_ = false;
}

void VerticalPanel::onLayerChangedFromPreview(int index, float x, float y, int zoom)
{
	if (index != currentRow())
		return;
	QSignalBlocker bz(zoom_), bx(x_), by(y_);
	zoom_->setValue(zoom);
	x_->setValue((int)std::lround(x * 100.0f));
	y_->setValue((int)std::lround(y * 100.0f));
}

void VerticalPanel::onSpinsChanged()
{
	if (loading_)
		return;
	const int row = currentRow();
	if (row < 0)
		return;
	cv_->setLayerTransform(row, x_->value() / 100.0f, y_->value() / 100.0f, zoom_->value());
	saveTimer_.start();
}

// Aplica un cambio a una copia del layout y lo reconstruye.
void VerticalPanel::mutate(const std::function<void(VerticalLayout &, int)> &fn)
{
	VerticalLayout l = cv_->layout();
	fn(l, currentRow());
	cv_->setLayout(l);
}

void VerticalPanel::updateHighlight()
{
	if (!preview_->isVisible())
		return;
	float cx = 0, cy = 0, w = 0, h = 0;
	const bool ok = cv_->layerBounds(currentRow(), cx, cy, w, h);
	preview_->setHighlight(cx, cy, w, h, ok);
}

// ---------------------------------------------------------------------------
// Menú de clic derecho: añadir a mano cualquier fuente o escena del horizontal
// ---------------------------------------------------------------------------

// Fuentes de una escena (entrando también en los grupos), sin las del propio plugin.
static bool collectSceneItems(obs_scene_t *, obs_sceneitem_t *item, void *param)
{
	auto *v = static_cast<std::vector<std::string> *>(param);
	if (obs_sceneitem_is_group(item)) {
		obs_sceneitem_group_enum_items(item, &collectSceneItems, param);
		return true;
	}
	obs_source_t *s = obs_sceneitem_get_source(item);
	const char *n = s ? obs_source_get_name(s) : nullptr;
	if (n && *n && std::string(n).rfind("MultiStream Hub", 0) != 0)
		v->push_back(n);
	return true;
}

void VerticalPanel::addLayerNamed(const std::string &name, bool fitWhole)
{
	if (name.empty())
		return;
	VerticalLayout l = cv_->layout();
	VerticalLayer ly;
	ly.scene = name;
	l.layers.push_back(ly);
	const int idx = (int)l.layers.size() - 1;
	cv_->setLayout(l); // reconstruye las capas
	// Las fuentes sueltas entran enteras y centradas; luego se ajustan con el ratón.
	if (fitWhole)
		cv_->setLayerTransform(idx, 0.5f, 0.5f, cv_->fullSceneZoom(idx));
	cv_->commit();
	refreshLayers();
	list_->setCurrentRow(idx);
}

void VerticalPanel::showContextMenu(int hitLayer, const QPoint &globalPos)
{
	const int nLayers = (int)cv_->layout().layers.size();
	if (hitLayer >= nLayers)
		hitLayer = -1;
	if (hitLayer >= 0 && hitLayer != currentRow())
		list_->setCurrentRow(hitLayer);

	// Se copian los nombres y se suelta la lista de OBS enseguida.
	struct SceneInfo {
		std::string name;
		std::vector<std::string> sources;
	};
	std::vector<SceneInfo> scenes;
	obs_frontend_source_list fl = {};
	obs_frontend_get_scenes(&fl);
	for (size_t i = 0; i < fl.sources.num; i++) {
		obs_source_t *sc = fl.sources.array[i];
		const char *nm = obs_source_get_name(sc);
		if (!nm)
			continue;
		SceneInfo info;
		info.name = nm;
		std::vector<std::string> names;
		if (obs_scene_t *scene = obs_scene_from_source(sc))
			obs_scene_enum_items(scene, &collectSceneItems, &names);
		std::set<std::string> seen;
		for (auto it = names.rbegin(); it != names.rend(); ++it) // de arriba hacia abajo, como en OBS
			if (seen.insert(*it).second)
				info.sources.push_back(*it);
		scenes.push_back(std::move(info));
	}
	obs_frontend_source_list_free(&fl);

	std::set<std::string> used; // las que ya son capas llevan una marca ✓
	for (const auto &ly : cv_->layout().layers)
		used.insert(ly.scene);
	auto label = [&](const std::string &n) {
		return (used.count(n) ? QStringLiteral("✓  ") : QString()) + QString::fromStdString(n);
	};

	QMenu menu(this);

	// 1) Fuentes, agrupadas por escena del horizontal.
	QMenu *srcMenu = menu.addMenu(QStringLiteral("＋ Añadir fuente"));
	if (scenes.empty())
		srcMenu->addAction(QStringLiteral("(no hay escenas)"))->setEnabled(false);
	for (const auto &sc : scenes) {
		QMenu *sub = srcMenu->addMenu(QString::fromStdString(sc.name));
		if (sc.sources.empty()) {
			sub->addAction(QStringLiteral("(sin fuentes)"))->setEnabled(false);
			continue;
		}
		for (const auto &n : sc.sources) {
			QAction *a = sub->addAction(label(n));
			connect(a, &QAction::triggered, this, [this, n] { addLayerNamed(n, true); });
		}
	}

	// 2) Cualquier fuente de OBS, por orden alfabético.
	QMenu *allMenu = menu.addMenu(QStringLiteral("＋ Añadir cualquier fuente (A–Z)"));
	std::vector<std::string> inputs;
	obs_enum_sources(&collectInputs, &inputs);
	std::sort(inputs.begin(), inputs.end());
	if (inputs.empty())
		allMenu->addAction(QStringLiteral("(no hay fuentes)"))->setEnabled(false);
	for (const auto &n : inputs) {
		QAction *a = allMenu->addAction(label(n));
		connect(a, &QAction::triggered, this, [this, n] { addLayerNamed(n, true); });
	}

	// 3) Una escena completa.
	QMenu *sceneMenu = menu.addMenu(QStringLiteral("＋ Añadir escena completa"));
	QAction *follow = sceneMenu->addAction(QStringLiteral("★ Escena activa de OBS"));
	connect(follow, &QAction::triggered, this, [this] { addLayerNamed(kFollowScene, false); });
	sceneMenu->addSeparator();
	for (const auto &sc : scenes) {
		QAction *a = sceneMenu->addAction(label(sc.name));
		const std::string n = sc.name;
		connect(a, &QAction::triggered, this, [this, n] { addLayerNamed(n, false); });
	}

	// 4) Si se hizo clic sobre una capa: poder quitarla.
	if (hitLayer >= 0) {
		const std::string &sn = cv_->layout().layers[(size_t)hitLayer].scene;
		const QString nm = sn == kFollowScene ? QStringLiteral("Escena activa de OBS") : QString::fromStdString(sn);
		menu.addSeparator();
		QAction *del = menu.addAction(QStringLiteral("Quitar capa «%1»").arg(nm));
		connect(del, &QAction::triggered, this, [this, hitLayer] {
			VerticalLayout l = cv_->layout();
			if (hitLayer < 0 || hitLayer >= (int)l.layers.size())
				return;
			l.layers.erase(l.layers.begin() + hitLayer);
			cv_->setLayout(l);
			refreshLayers();
			list_->setCurrentRow(std::min(hitLayer, (int)l.layers.size() - 1));
		});
	}

	menu.exec(globalPos);
}
