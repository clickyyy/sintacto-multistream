// OBS MultiStream Hub - Creado por Sintacto (@sintacto12)
#include "dock.hpp"
#include "donations.hpp"
#include "theme.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QScrollArea>
#include <QStyle>
#include <QSpinBox>
#include <QUrl>
#include <QVBoxLayout>

namespace {

const char *kStyle = theme::kStyle;

struct HelpInfo {
	const char *where;
	const char *steps;
	const char *link;
};

HelpInfo helpFor(Platform p)
{
	// TODO: verificar cada texto y enlace contra la ayuda oficial vigente.
	switch (p) {
	case Platform::Twitch:
		return {"Twitch",
			"Panel del creador → Configuración → Transmisión → copia la <b>clave de transmisión</b>. "
			"El servidor por defecto ya viene puesto; pega solo la clave.",
			"https://dashboard.twitch.tv/settings/stream"};
	case Platform::YouTubeH:
	case Platform::YouTubeV:
		return {"YouTube Live",
			"YouTube Studio → Crear → Transmitir en vivo → copia la <b>clave de transmisión</b>. "
			"Para el directo vertical crea una transmisión aparte con su propia clave.",
			"https://studio.youtube.com"};
	case Platform::Kick:
		return {"Kick",
			"Panel de creador → ajustes de transmisión: copia la <b>URL del servidor</b> y la "
			"<b>clave de transmisión</b>, y pégalas aquí.",
			"https://kick.com"};
	case Platform::TikTok:
		return {"TikTok Live",
			"TikTok solo entrega servidor y clave a cuentas con acceso a LIVE. Si tu cuenta lo tiene, "
			"los obtendrás desde las herramientas de LIVE de TikTok. Si no aparecen, no es posible "
			"transmitir desde aquí. TikTok usa el lienzo vertical 9:16.",
			"https://www.tiktok.com/live"};
	case Platform::Starvios:
		return {"Starvios Stream",
			"Pendiente de verificar el sistema de transmisión de Starvios. Si te da una URL RTMP/RTMPS y "
			"una clave, pégalas aquí.",
			""};
	default:
		return {"Servidor personalizado", "Pega la URL RTMP/RTMPS y la clave de tu servicio.", ""};
	}
}

QString stateText(RunState s, bool configured)
{
	switch (s) {
	case RunState::Starting: return QStringLiteral("Iniciando…");
	case RunState::Live: return QStringLiteral("Transmitiendo");
	case RunState::Reconnecting: return QStringLiteral("Reconectando…");
	case RunState::Stopping: return QStringLiteral("Deteniendo…");
	case RunState::Error: return QStringLiteral("Error");
	default: return configured ? QStringLiteral("Listo") : QStringLiteral("Sin configurar");
	}
}

QString stateColor(RunState s, bool configured)
{
	switch (s) {
	case RunState::Live: return QStringLiteral("#22c55e");
	case RunState::Starting:
	case RunState::Reconnecting:
	case RunState::Stopping: return QStringLiteral("#fbbf24");
	case RunState::Error: return QStringLiteral("#f87171");
	default: return configured ? QStringLiteral("#60a5fa") : QStringLiteral("#6b7199");
	}
}

QString formatTime(qint64 secs)
{
	return QStringLiteral("%1:%2:%3")
		.arg(secs / 3600, 2, 10, QLatin1Char('0'))
		.arg((secs / 60) % 60, 2, 10, QLatin1Char('0'))
		.arg(secs % 60, 2, 10, QLatin1Char('0'));
}

void addEncoderItems(QComboBox *c)
{
	c->addItem(QStringLiteral("Automático (hardware si hay)"), "auto");
	c->addItem(QStringLiteral("x264 (CPU)"), "x264");
	c->addItem(QStringLiteral("NVIDIA NVENC"), "nvenc");
	c->addItem(QStringLiteral("AMD AMF"), "amf");
	c->addItem(QStringLiteral("Intel Quick Sync"), "qsv");
}

QSpinBox *makeBitrateBox(int value)
{
	auto *b = new QSpinBox;
	b->setRange(500, 50000);
	b->setSuffix(QStringLiteral(" kbps"));
	b->setValue(value);
	return b;
}

// ---------------------------------------------------------------------------
// Configuración de un destino: solo servidor, clave y formato.
// ---------------------------------------------------------------------------

class ConfigDialog : public QDialog {
public:
	ConfigDialog(const Destination &d, QWidget *parent) : QDialog(parent)
	{
		setWindowTitle(QString::fromStdString(d.name));
		setStyleSheet(kStyle);
		setMinimumWidth(420);

		auto *heading = new QLabel(QStringLiteral("Configurar ") + QString::fromStdString(d.name));
		heading->setObjectName("title");

		server_ = new QLineEdit(QString::fromStdString(d.server));
		server_->setPlaceholderText(QStringLiteral("rtmp://… o rtmps://…"));
		key_ = new QLineEdit(QString::fromStdString(d.streamKey));
		key_->setEchoMode(QLineEdit::Password);
		auto *show = new QCheckBox(QStringLiteral("Mostrar clave"));
		connect(show, &QCheckBox::toggled, this,
			[this](bool on) { key_->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password); });

		orientation_ = new QComboBox;
		orientation_->addItem(QStringLiteral("Horizontal (escena principal de OBS)"));
		orientation_->addItem(QStringLiteral("Vertical (lienzo 9:16 compartido)"));
		orientation_->setCurrentIndex(d.orientation == Orientation::Vertical ? 1 : 0);

		bitrate_ = new QSpinBox;
		bitrate_->setRange(0, 100000);
		bitrate_->setSingleStep(500);
		bitrate_->setSuffix(QStringLiteral(" kbps"));
		bitrate_->setSpecialValueText(QStringLiteral("Igual que Ajustes de emisión"));
		bitrate_->setValue(d.bitrateKbps);
		bitrate_->setToolTip(QStringLiteral("Bitrate solo para este destino. En el mínimo usa el compartido."));

		auto *note = new QLabel(QStringLiteral("La calidad (codificador, resolución y bitrate) se ajusta una sola "
						       "vez en «Ajustes de emisión» y se comparte entre destinos. Si pones un bitrate propio distinto, "
						       "se usa un codificador extra solo para este destino (gasta más GPU/CPU)."));
		note->setWordWrap(true);
		note->setObjectName("hint");

		auto *form = new QFormLayout;
		form->setVerticalSpacing(10);
		form->addRow(QStringLiteral("URL del servidor"), server_);
		form->addRow(QStringLiteral("Clave de transmisión"), key_);
		form->addRow(QString(), show);
		form->addRow(QStringLiteral("Formato"), orientation_);
		form->addRow(QStringLiteral("Bitrate de este destino"), bitrate_);

		auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
		buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Guardar"));
		buttons->button(QDialogButtonBox::Ok)->setObjectName("primary");
		buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Cancelar"));
		connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
		connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

		auto *card = new QFrame;
		card->setObjectName("card");
		auto *cl = new QVBoxLayout(card);
		cl->setContentsMargins(14, 14, 14, 14);
		cl->addLayout(form);

		auto *lay = new QVBoxLayout(this);
		lay->setContentsMargins(18, 18, 18, 18);
		lay->setSpacing(12);
		lay->addWidget(heading);
		lay->addWidget(card);
		lay->addWidget(note);
		lay->addWidget(buttons);
	}

	void apply(Destination &d) const
	{
		d.server = server_->text().trimmed().toStdString();
		d.streamKey = key_->text().trimmed().toStdString();
		d.orientation = orientation_->currentIndex() == 1 ? Orientation::Vertical : Orientation::Horizontal;
		d.bitrateKbps = bitrate_->value();
	}

private:
	QLineEdit *server_;
	QLineEdit *key_;
	QComboBox *orientation_;
	QSpinBox *bitrate_;
};

// ---------------------------------------------------------------------------
// Ajustes globales: un codificador horizontal, uno vertical, uno de audio y
// el encuadre del lienzo vertical.
// ---------------------------------------------------------------------------

class SettingsDialog : public QDialog {
public:
	SettingsDialog(const StreamSettings &s, QWidget *parent) : QDialog(parent)
	{
		setWindowTitle(QStringLiteral("Ajustes de emisión"));
		setStyleSheet(kStyle);
		setMinimumWidth(460);

		encH_ = new QComboBox;
		addEncoderItems(encH_);
		int ei = encH_->findData(QString::fromStdString(s.horizontal.encoder));
		encH_->setCurrentIndex(ei < 0 ? 0 : ei);

		resH_ = new QComboBox;
		resH_->addItem(QStringLiteral("Igual que OBS (recomendado, sin escalar)"), 0);
		resH_->addItem(QStringLiteral("1920 × 1080"), 1080);
		resH_->addItem(QStringLiteral("1280 × 720"), 720);
		resH_->addItem(QStringLiteral("854 × 480"), 480);
		int ri = resH_->findData(s.horizontal.height);
		resH_->setCurrentIndex(ri < 0 ? 0 : ri);

		brH_ = makeBitrateBox(s.horizontal.bitrateKbps);

		encV_ = new QComboBox;
		addEncoderItems(encV_);
		int vi = encV_->findData(QString::fromStdString(s.vertical.encoder));
		encV_->setCurrentIndex(vi < 0 ? 0 : vi);

		brV_ = makeBitrateBox(s.vertical.bitrateKbps);

		audio_ = new QSpinBox;
		audio_->setRange(64, 320);
		audio_->setSuffix(QStringLiteral(" kbps"));
		audio_->setValue(s.audioKbps);

		auto *note = new QLabel(QStringLiteral("Todos los destinos horizontales comparten UNA sola codificación; "
						       "todos los verticales (YouTube vertical, TikTok…), otra sobre un único "
						       "lienzo 9:16 (se edita en el panel «Lienzo Vertical»). El audio se codifica una sola vez."));
		note->setWordWrap(true);
		note->setObjectName("hint");

		auto *heading = new QLabel(QStringLiteral("Ajustes de emisión"));
		heading->setObjectName("title");

		// Cada grupo va en su propia tarjeta con un título pequeño.
		auto addGroup = [&](QVBoxLayout *into, const QString &title, QFormLayout *form) {
			auto *sec = new QLabel(title);
			sec->setObjectName("section");
			auto *card = new QFrame;
			card->setObjectName("card");
			auto *cl = new QVBoxLayout(card);
			cl->setContentsMargins(14, 12, 14, 14);
			cl->setSpacing(8);
			form->setVerticalSpacing(10);
			cl->addWidget(sec);
			cl->addLayout(form);
			into->addWidget(card);
		};
		auto *formH = new QFormLayout;
		formH->addRow(QStringLiteral("Codificador"), encH_);
		formH->addRow(QStringLiteral("Resolución"), resH_);
		formH->addRow(QStringLiteral("Bitrate"), brH_);
		auto *formV = new QFormLayout;
		formV->addRow(QStringLiteral("Codificador"), encV_);
		formV->addRow(QStringLiteral("Bitrate"), brV_);
		auto *formA = new QFormLayout;
		formA->addRow(QStringLiteral("Bitrate"), audio_);

		auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
		buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Guardar"));
		buttons->button(QDialogButtonBox::Ok)->setObjectName("primary");
		buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Cancelar"));
		connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
		connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

		auto *lay = new QVBoxLayout(this);
		lay->setContentsMargins(18, 18, 18, 18);
		lay->setSpacing(10);
		lay->addWidget(heading);
		lay->addWidget(note);
		addGroup(lay, QStringLiteral("VIDEO HORIZONTAL"), formH);
		addGroup(lay, QStringLiteral("VIDEO VERTICAL (9:16)"), formV);
		addGroup(lay, QStringLiteral("AUDIO"), formA);
		lay->addWidget(buttons);
	}

	void apply(StreamSettings &s) const
	{
		s.horizontal.encoder = encH_->currentData().toString().toStdString();
		int h = resH_->currentData().toInt();
		s.horizontal.height = h;
		s.horizontal.width = h == 1080 ? 1920 : h == 720 ? 1280 : h == 480 ? 854 : 0;
		s.horizontal.bitrateKbps = brH_->value();
		s.vertical.encoder = encV_->currentData().toString().toStdString();
		s.vertical.bitrateKbps = brV_->value();
		s.audioKbps = audio_->value();
	}

private:
	QComboBox *encH_;
	QComboBox *resH_;
	QSpinBox *brH_;
	QComboBox *encV_;
	QSpinBox *brV_;
	QSpinBox *audio_;
};

// ---------------------------------------------------------------------------
// Donaciones (voluntarias). Solo se abre cuando el usuario pulsa el botón.
// ---------------------------------------------------------------------------

class DonationDialog : public QDialog {
public:
	DonationDialog(const DonationConfig &cfg, QWidget *parent) : QDialog(parent)
	{
		setWindowTitle(QStringLiteral("Apoya este proyecto"));
		setStyleSheet(kStyle);
		setMinimumWidth(400);

		auto *hdr = new QFrame;
		hdr->setObjectName("header");
		auto *hl = new QVBoxLayout(hdr);
		hl->setContentsMargins(16, 14, 16, 14);
		auto *ht = new QLabel(QStringLiteral("❤  Apoya este proyecto"));
		ht->setObjectName("headerTitle");
		auto *hs = new QLabel(QStringLiteral("Donación 100 % voluntaria"));
		hs->setObjectName("headerSub");
		hl->addWidget(ht);
		hl->addWidget(hs);

		auto *text = new QLabel(QStringLiteral(
			"<b>¿Te está gustando Multistream Sintacto?</b><br><br>"
			"Este proyecto fue creado por Sintacto (@sintacto12).<br>"
			"Si esta herramienta te resulta útil y quieres apoyar su desarrollo, puedes realizar una "
			"donación voluntaria.<br>"
			"Tu apoyo ayuda a mantener el proyecto, mejorar sus funciones y desarrollar nuevas "
			"características.<br><br>"
			"¡Muchas gracias por apoyar este proyecto!"));
		text->setWordWrap(true);
		text->setTextFormat(Qt::RichText);

		auto *lay = new QVBoxLayout(this);
		lay->setContentsMargins(18, 18, 18, 18);
		lay->setSpacing(12);
		lay->addWidget(hdr);
		lay->addWidget(text);

		if (cfg.links.empty()) {
			auto *none = new QLabel(QStringLiteral("Por ahora no hay enlaces de donación configurados."));
			none->setObjectName("muted");
			none->setWordWrap(true);
			lay->addWidget(none);
		}
		for (const auto &l : cfg.links) {
			auto *btn = new QPushButton(QString::fromStdString(l.name));
			btn->setObjectName("primary");
			const QUrl url(QString::fromStdString(l.url));
			// Siempre en el navegador predeterminado; no se rastrea nada.
			connect(btn, &QPushButton::clicked, this, [url] { QDesktopServices::openUrl(url); });
			lay->addWidget(btn);
		}

		if (!cfg.qrPath.empty()) {
			QPixmap qr(QString::fromStdString(cfg.qrPath));
			if (!qr.isNull()) {
				auto *img = new QLabel;
				img->setPixmap(qr.scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation));
				img->setAlignment(Qt::AlignCenter);
				lay->addWidget(img);
			}
		}

		auto *close = new QPushButton(QStringLiteral("Cerrar"));
		connect(close, &QPushButton::clicked, this, &QDialog::accept);
		lay->addWidget(close);
	}
};

} // namespace

// ---------------------------------------------------------------------------
// DestinationCard
// ---------------------------------------------------------------------------

DestinationCard::DestinationCard(int index, const Destination &d, QWidget *parent) : QFrame(parent), index_(index)
{
	setObjectName("card");

	toggle_ = new theme::ToggleSwitch;
	toggle_->setToolTip(QStringLiteral("Incluir este destino al iniciar"));
	name_ = new QLabel;
	name_->setTextFormat(Qt::RichText);
	QFont f = name_->font();
	f.setBold(true);
	name_->setFont(f);
	dot_ = new QLabel(QStringLiteral("●"));
	status_ = new QLabel;
	stats_ = new QLabel;
	stats_->setObjectName("hint");
	stats_->setIndent(52);

	auto *cfg = new QPushButton(QStringLiteral("⚙"));
	cfg->setToolTip(QStringLiteral("Configurar"));
	cfg->setObjectName("icon");
	auto *help = new QPushButton(QStringLiteral("?"));
	help->setToolTip(QStringLiteral("¿Cómo conectar mi cuenta?"));
	help->setObjectName("icon");

	auto *top = new QHBoxLayout;
	top->setSpacing(10);
	top->addWidget(toggle_);
	top->addWidget(name_, 1);
	top->addWidget(cfg);
	top->addWidget(help);

	auto *mid = new QHBoxLayout;
	mid->setContentsMargins(52, 0, 0, 0);
	mid->setSpacing(6);
	mid->addWidget(dot_);
	mid->addWidget(status_);
	mid->addStretch(1);

	auto *lay = new QVBoxLayout(this);
	lay->setContentsMargins(12, 10, 12, 10);
	lay->setSpacing(4);
	lay->addLayout(top);
	lay->addLayout(mid);
	lay->addWidget(stats_);

	connect(toggle_, &QCheckBox::toggled, this, [this](bool on) { emit toggled(index_, on); });
	connect(cfg, &QPushButton::clicked, this, [this] { emit configureClicked(index_); });
	connect(help, &QPushButton::clicked, this, [this] { emit helpClicked(index_); });

	refresh(d);
}

void DestinationCard::refresh(const Destination &d)
{
	configured_ = !d.server.empty() && !d.streamKey.empty();
	name_->setText(QString::fromStdString(d.name).toHtmlEscaped() +
		       (d.orientation == Orientation::Vertical ? QStringLiteral("  <span style=\"color:#a78bfa;\">9:16</span>")
							       : QString()));
	QSignalBlocker b(toggle_);
	toggle_->setChecked(d.enabled);
	setState(state_, QString());
}

void DestinationCard::setState(RunState s, const QString &message)
{
	state_ = s;
	status_->setText(stateText(s, configured_));
	const QString col = stateColor(s, configured_);
	dot_->setStyleSheet(QStringLiteral("color:%1; font-size:10px;").arg(col));
	status_->setStyleSheet(QStringLiteral("color:%1; font-weight:600;").arg(col));
	// El borde de la tarjeta cambia con el estado (ver theme.hpp).
	const char *cs = s == RunState::Live ? "live"
			 : s == RunState::Error ? "error"
			 : (s == RunState::Starting || s == RunState::Reconnecting || s == RunState::Stopping) ? "busy"
			 : "idle";
	setProperty("state", cs);
	style()->unpolish(this);
	style()->polish(this);
	status_->setToolTip(message);
	if (s == RunState::Error)
		stats_->setText(message);
	else if (s == RunState::Reconnecting)
		stats_->setText(message);
	else if (s != RunState::Live)
		stats_->clear();
	toggle_->setEnabled(s == RunState::Idle || s == RunState::Error);
}

void DestinationCard::setStats(qint64 liveSeconds, double kbps, int dropped, int total)
{
	stats_->setText(QStringLiteral("%1  ·  %2 kbps  ·  perdidos %3/%4")
				.arg(formatTime(liveSeconds))
				.arg(qRound(kbps))
				.arg(dropped)
				.arg(total));
}

// ---------------------------------------------------------------------------
// HubDock
// ---------------------------------------------------------------------------

HubDock::HubDock(OutputManager *manager, QWidget *parent) : QWidget(parent), mgr_(manager)
{
	setStyleSheet(kStyle);
	dests_ = loadDestinations();
	settings_ = loadSettings();
	states_.assign(dests_.size(), RunState::Idle);

	// Cabecera con degradado azul -> morado y estado general.
	auto *header = new QFrame;
	header->setObjectName("header");
	auto *hl = new QHBoxLayout(header);
	hl->setContentsMargins(14, 12, 14, 12);
	auto *ht = new QVBoxLayout;
	ht->setSpacing(1);
	auto *htitle = new QLabel(QStringLiteral("Multistream Sintacto"));
	htitle->setObjectName("headerTitle");
	auto *hsub = new QLabel(QStringLiteral("Transmite a todas tus plataformas a la vez"));
	hsub->setObjectName("headerSub");
	ht->addWidget(htitle);
	ht->addWidget(hsub);
	auto *badge = new QLabel(QStringLiteral("EN REPOSO"));
	badge->setObjectName("badge");
	hl->addLayout(ht, 1);
	hl->addWidget(badge, 0, Qt::AlignVCenter);

	auto *list = new QVBoxLayout;
	list->setSpacing(8);
	list->setContentsMargins(0, 0, 4, 0);
	for (size_t i = 0; i < dests_.size(); i++) {
		auto *card = new DestinationCard((int)i, dests_[i]);
		connect(card, &DestinationCard::toggled, this, [this](int i, bool on) { onToggled(i, on); });
		connect(card, &DestinationCard::configureClicked, this, [this](int i) { onConfigure(i); });
		connect(card, &DestinationCard::helpClicked, this, [this](int i) { onHelp(i); });
		cards_.push_back(card);
		list->addWidget(card);
	}
	list->addStretch(1);

	auto *inner = new QWidget;
	inner->setLayout(list);
	auto *scroll = new QScrollArea;
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);
	scroll->setWidget(inner);

	auto *settingsBtn = new QPushButton(QStringLiteral("⚙ Ajustes de emisión (codificadores)"));
	connect(settingsBtn, &QPushButton::clicked, this, &HubDock::onSettings);

	startBtn_ = new QPushButton(QStringLiteral("INICIAR TODAS LAS TRANSMISIONES"));
	startBtn_->setObjectName("start");
	stopBtn_ = new QPushButton(QStringLiteral("DETENER TODAS"));
	stopBtn_->setObjectName("stop");
	stopBtn_->setEnabled(false);
	summary_ = new QLabel;
	summary_->setWordWrap(true);
	summary_->setObjectName("muted");
	summary_->setTextFormat(Qt::RichText);

	// Discreto y siempre visible; no hace nada por sí solo (sin avisos ni ventanas automáticas).
	auto *supportBtn = new QPushButton(QStringLiteral("❤️ APOYAR AL CREADOR"));
	supportBtn->setObjectName("support");
	connect(supportBtn, &QPushButton::clicked, this, &HubDock::onDonate);

	auto *credit = new QLabel(QStringLiteral("Multistream Sintacto — Creado por Sintacto (@sintacto12)"));
	credit->setAlignment(Qt::AlignCenter);
	credit->setObjectName("credit");

	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(10, 10, 10, 10);
	root->setSpacing(8);
	root->addWidget(header);
	root->addWidget(scroll, 1);
	root->addWidget(settingsBtn);
	root->addWidget(summary_);
	root->addWidget(startBtn_);
	root->addWidget(stopBtn_);
	root->addWidget(supportBtn);
	root->addWidget(credit);

	connect(startBtn_, &QPushButton::clicked, this, &HubDock::onStartAll);
	connect(stopBtn_, &QPushButton::clicked, this, &HubDock::onStopAll);
	connect(mgr_, &OutputManager::stateChanged, this, &HubDock::onState);
	connect(mgr_, &OutputManager::statsUpdated, this, &HubDock::onStats);

	updateSummary();
}

int HubDock::indexOf(const QString &id) const
{
	for (size_t i = 0; i < dests_.size(); i++)
		if (QString::fromStdString(dests_[i].id) == id)
			return (int)i;
	return -1;
}

void HubDock::onToggled(int index, bool enabled)
{
	dests_[index].enabled = enabled;
	saveDestinations(dests_);
	updateSummary();
}

void HubDock::onConfigure(int index)
{
	ConfigDialog dlg(dests_[index], this);
	if (dlg.exec() != QDialog::Accepted)
		return;
	dlg.apply(dests_[index]);
	saveDestinations(dests_);
	cards_[index]->refresh(dests_[index]);
	updateSummary();
}

void HubDock::onSettings()
{
	SettingsDialog dlg(settings_, this);
	if (dlg.exec() != QDialog::Accepted)
		return;
	dlg.apply(settings_);
	saveSettings(settings_);
}

void HubDock::onDonate()
{
	DonationDialog dlg(loadDonations(), this);
	dlg.exec();
}

void HubDock::onHelp(int index)
{
	HelpInfo h = helpFor(dests_[index].platform);
	QString text = QStringLiteral("<p><b>%1</b></p><p>%2</p>").arg(QString::fromUtf8(h.where), QString::fromUtf8(h.steps));
	if (*h.link)
		text += QStringLiteral("<p><a href=\"%1\">%1</a></p>").arg(QString::fromUtf8(h.link));
	text += QStringLiteral("<p>1. Entra en la plataforma. 2. Busca el apartado de transmisión. "
			       "3. Copia la URL del servidor (si la pide). 4. Copia la clave de transmisión. "
			       "5. Pégalas en ⚙ de esta tarjeta.</p>"
			       "<p>Nunca se piden contraseñas. La clave se guarda cifrada en este equipo.</p>");

	QMessageBox box(this);
	box.setWindowTitle(QStringLiteral("¿Cómo conectar mi cuenta?"));
	box.setStyleSheet(kStyle);
	box.setTextFormat(Qt::RichText);
	box.setText(text);
	box.exec();
}

void HubDock::onStartAll()
{
	bool any = false;
	for (const auto &d : dests_)
		any = any || d.enabled;
	if (!any) {
		summary_->setText(QStringLiteral("Activa al menos un destino con el interruptor de su tarjeta."));
		return;
	}
	saveDestinations(dests_);
	mgr_->startAll(dests_, settings_);
}

void HubDock::onStopAll()
{
	mgr_->stopAll();
}

void HubDock::onState(const QString &id, int state, const QString &message)
{
	int i = indexOf(id);
	if (i < 0)
		return;
	states_[i] = static_cast<RunState>(state);
	cards_[i]->setState(states_[i], message);
	updateSummary();
}

void HubDock::onStats(const QString &id, qint64 liveSeconds, double kbps, int dropped, int total)
{
	int i = indexOf(id);
	if (i >= 0)
		cards_[i]->setStats(liveSeconds, kbps, dropped, total);
}

void HubDock::updateSummary()
{
	int live = 0, err = 0, enabled = 0;
	bool active = false;
	for (size_t i = 0; i < dests_.size(); i++) {
		if (dests_[i].enabled)
			enabled++;
		if (states_[i] == RunState::Live)
			live++;
		if (states_[i] == RunState::Error)
			err++;
		if (states_[i] == RunState::Starting || states_[i] == RunState::Live ||
		    states_[i] == RunState::Reconnecting || states_[i] == RunState::Stopping)
			active = true;
	}
	startBtn_->setEnabled(!active);
	stopBtn_->setEnabled(active);
	summary_->setText(QStringLiteral("Seleccionados <b style=\"color:#a5b4fc\">%1</b>  ·  "
					 "Transmitiendo <b style=\"color:#22c55e\">%2</b>  ·  "
					 "Con error <b style=\"color:%4\">%3</b>")
				  .arg(enabled)
				  .arg(live)
				  .arg(err)
				  .arg(err ? QStringLiteral("#f87171") : QStringLiteral("#a5b4fc")));

	if (auto *badge = findChild<QLabel *>(QStringLiteral("badge"))) {
		badge->setText(live > 0 ? QStringLiteral("● EN VIVO · %1").arg(live)
					: active ? QStringLiteral("CONECTANDO…") : QStringLiteral("EN REPOSO"));
		badge->setStyleSheet(live > 0 ? QStringLiteral("background:#16a34a;")
					      : active ? QStringLiteral("background:#d97706;") : QString());
	}
}
