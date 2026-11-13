#pragma once
// OBS MultiStream Hub - Creado por Sintacto (@sintacto12)
//
// Tema visual compartido por todos los paneles y ventanas: fondo azul noche,
// tarjetas suaves y acentos en degradado azul -> morado.
// Solo cabeceras: no hace falta añadirlo a CMake.

#include <QCheckBox>
#include <QLinearGradient>
#include <QPainter>

namespace theme {

// Paleta (por si hace falta en código):
//   fondo #0d0f1f · tarjeta #151833 · borde #2b3066 · texto #e8eaff
//   acento azul #3b82f6 · acento morado #8b5cf6
inline const char *const kStyle = R"(
HubDock, VerticalPanel, QDialog, QMessageBox { background: #0d0f1f; color: #e8eaff; }
QLabel { color: #e8eaff; background: transparent; }
QLabel#title { font-size: 15px; font-weight: 700; color: #ffffff; }
QLabel#section { color: #a5b4fc; font-size: 10px; font-weight: 700; }
QLabel#hint { color: #8f95c4; font-size: 11px; }
QLabel#muted { color: #8f95c4; }
QLabel#credit { color: #5d6390; font-size: 10px; }

QFrame#header {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #1d4ed8, stop:0.55 #5b3fd9, stop:1 #8b5cf6);
    border: none; border-radius: 12px;
}
QFrame#header QLabel { background: transparent; color: #ffffff; }
QLabel#headerTitle { font-size: 15px; font-weight: 700; color: #ffffff; }
QLabel#headerSub { font-size: 11px; color: rgba(255,255,255,190); }
QLabel#badge { background: rgba(255,255,255,40); color: #ffffff; border-radius: 10px; padding: 3px 10px; font-size: 11px; font-weight: 600; }

QFrame#card { background: #151833; border: 1px solid #2b3066; border-radius: 12px; }
QFrame#card:hover { border: 1px solid #4c55a8; }
QFrame#card[state="live"] { border: 1px solid #22c55e; background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #151833, stop:1 #10283a); }
QFrame#card[state="busy"] { border: 1px solid #f59e0b; }
QFrame#card[state="error"] { border: 1px solid #ef4444; }

QPushButton { background: #1c2040; color: #e8eaff; border: 1px solid #2b3066; border-radius: 8px; padding: 7px 12px; }
QPushButton:hover { background: #262b5a; border: 1px solid #4c55a8; }
QPushButton:pressed { background: #30367a; }
QPushButton:disabled { color: #5d6390; background: #141730; border: 1px solid #20244a; }

QPushButton#primary { border: none; color: #ffffff; font-weight: 600;
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #3b82f6, stop:1 #8b5cf6); }
QPushButton#primary:hover { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #5b9bff, stop:1 #a78bfa); }
QPushButton#primary:pressed { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #2f6fe0, stop:1 #7c4be0); }
QPushButton#primary:disabled { background: #1c2040; color: #5d6390; }

QPushButton#start { border: none; color: #ffffff; font-weight: 700; font-size: 14px; padding: 14px; border-radius: 10px;
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #2563eb, stop:0.5 #5b3fd9, stop:1 #8b5cf6); }
QPushButton#start:hover { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #3b82f6, stop:0.5 #6d4ff0, stop:1 #a78bfa); }
QPushButton#start:pressed { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #1d4ed8, stop:0.5 #4c32c0, stop:1 #7c4be0); }
QPushButton#start:disabled { background: #1c2040; color: #5d6390; }

QPushButton#stop { background: transparent; color: #ff8a9a; border: 1px solid #ef4444; font-weight: 700; padding: 10px; border-radius: 10px; }
QPushButton#stop:hover { background: #3a1b2e; }
QPushButton#stop:disabled { color: #5d6390; border: 1px solid #2b3066; }

QPushButton#danger { color: #ff9aa8; }
QPushButton#danger:hover { background: #3a1b2e; border: 1px solid #7f2f45; }

QPushButton#icon { padding: 4px; min-width: 30px; max-width: 30px; min-height: 26px; border-radius: 8px; }

QPushButton#support { background: transparent; color: #e9b8ff; border: 1px solid #6b3a8f; padding: 7px 10px; font-weight: 600; }
QPushButton#support:hover { background: #2a1a45; border: 1px solid #a855f7; }

QLineEdit, QComboBox, QSpinBox { background: #0a0c1a; color: #e8eaff; border: 1px solid #2b3066; border-radius: 8px; padding: 6px 8px; selection-background-color: #6d5ce8; }
QLineEdit:focus, QComboBox:focus, QSpinBox:focus { border: 1px solid #8b5cf6; }
QComboBox::drop-down { border: none; width: 22px; }
QComboBox QAbstractItemView { background: #0a0c1a; color: #e8eaff; border: 1px solid #2b3066; selection-background-color: #4c3fd1; outline: 0; }
QSpinBox::up-button, QSpinBox::down-button { width: 0px; border: none; }

QCheckBox { color: #e8eaff; spacing: 8px; background: transparent; }
QCheckBox::indicator { width: 16px; height: 16px; border-radius: 5px; border: 1px solid #4c55a8; background: #0a0c1a; }
QCheckBox::indicator:checked { border: 1px solid #8b5cf6; background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #3b82f6, stop:1 #8b5cf6); }

QListWidget { background: #0a0c1a; color: #e8eaff; border: 1px solid #2b3066; border-radius: 10px; padding: 3px; outline: 0; }
QListWidget::item { padding: 8px 8px; border-radius: 8px; margin: 1px; }
QListWidget::item:hover { background: #1c2040; }
QListWidget::item:selected { color: #ffffff; background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #2b4fb8, stop:1 #5b3fd1); }
QListWidget::indicator { width: 16px; height: 16px; border-radius: 5px; border: 1px solid #4c55a8; background: #0a0c1a; }
QListWidget::indicator:checked { border: 1px solid #8b5cf6; background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #3b82f6, stop:1 #8b5cf6); }

QScrollArea { background: transparent; border: none; }
QScrollArea > QWidget > QWidget { background: transparent; }
QScrollBar:vertical { background: transparent; width: 8px; margin: 0; }
QScrollBar::handle:vertical { background: #2b3066; border-radius: 4px; min-height: 24px; }
QScrollBar::handle:vertical:hover { background: #4c55a8; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }

QMenu { background: #151833; color: #e8eaff; border: 1px solid #3a4080; padding: 6px; }
QMenu::item { padding: 8px 26px 8px 14px; border-radius: 6px; }
QMenu::item:selected { color: #ffffff; background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #2b4fb8, stop:1 #5b3fd1); }
QMenu::item:disabled { color: #5d6390; }
QMenu::separator { height: 1px; background: #2b3066; margin: 5px 8px; }

QToolTip { background: #1c2040; color: #e8eaff; border: 1px solid #4c55a8; border-radius: 6px; padding: 4px 6px; }
QDialogButtonBox QPushButton { min-width: 90px; }
)";

// Interruptor tipo "píldora" con degradado azul -> morado. Es un QCheckBox
// (misma API: toggled, setChecked...), solo cambia cómo se dibuja.
class ToggleSwitch : public QCheckBox {
public:
	explicit ToggleSwitch(QWidget *parent = nullptr) : QCheckBox(parent)
	{
		setFixedSize(42, 24);
		setCursor(Qt::PointingHandCursor);
	}

	QSize sizeHint() const override { return QSize(42, 24); }

protected:
	bool hitButton(const QPoint &pos) const override { return rect().contains(pos); }

	void paintEvent(QPaintEvent *) override
	{
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing);
		p.setOpacity(isEnabled() ? 1.0 : 0.45);
		p.setPen(Qt::NoPen);

		const QRectF r = QRectF(rect()).adjusted(1, 1, -1, -1);
		if (isChecked()) {
			QLinearGradient g(r.topLeft(), r.topRight());
			g.setColorAt(0.0, QColor(0x3b, 0x82, 0xf6));
			g.setColorAt(1.0, QColor(0x8b, 0x5c, 0xf6));
			p.setBrush(g);
		} else {
			p.setBrush(QColor(0x2b, 0x30, 0x66));
		}
		p.drawRoundedRect(r, r.height() / 2.0, r.height() / 2.0);

		const qreal d = r.height() - 6.0;
		const qreal x = isChecked() ? r.right() - 3.0 - d : r.left() + 3.0;
		p.setBrush(QColor(255, 255, 255));
		p.drawEllipse(QRectF(x, r.top() + 3.0, d, d));
	}
};

} // namespace theme
