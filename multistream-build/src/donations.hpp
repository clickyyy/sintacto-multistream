#pragma once
// OBS MultiStream Hub - Creado por Sintacto (@sintacto12)
//
// Enlaces de donación (voluntarios). Se leen de donaciones.json en la carpeta
// de configuración del plugin, así se pueden cambiar sin recompilar y las
// actualizaciones no lo sobrescriben. Solo se aceptan enlaces https://.

#include <string>
#include <vector>

struct DonationLink {
	std::string name;
	std::string url;
};

struct DonationConfig {
	std::vector<DonationLink> links;
	std::string qrPath; // imagen opcional con el QR de donación
};

// Si donaciones.json no existe, lo crea con el enlace de PayPal del creador.
// Si existe, respeta lo que haya (incluso vacío).
DonationConfig loadDonations();

bool isSafeDonationUrl(const std::string &url);
