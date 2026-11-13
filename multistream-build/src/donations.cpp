// OBS MultiStream Hub - Creado por Sintacto (@sintacto12)
#include "donations.hpp"

#include <obs-module.h>
#include <util/platform.h>
#include <util/bmem.h>

static const char *kPayPalName = "PayPal";
static const char *kPayPalUrl = "https://www.paypal.com/donate/?hosted_button_id=VESLUDWPQ93FE";

bool isSafeDonationUrl(const std::string &url)
{
	// Solo https, sin espacios, y sin marcadores de posición sin rellenar.
	if (url.rfind("https://", 0) != 0 || url.size() <= 8)
		return false;
	if (url.find(' ') != std::string::npos)
		return false;
	if (url.find("PON-AQUI") != std::string::npos)
		return false;
	return true;
}

static std::string donationsPath()
{
	char *dir = obs_module_config_path("");
	if (dir) {
		os_mkdirs(dir);
		bfree(dir);
	}
	char *p = obs_module_config_path("donaciones.json");
	std::string path = p ? p : "";
	bfree(p);
	return path;
}

static void writeDefault(const std::string &path)
{
	obs_data_t *root = obs_data_create();
	obs_data_array_t *arr = obs_data_array_create();

	obs_data_t *pp = obs_data_create();
	obs_data_set_string(pp, "name", kPayPalName);
	obs_data_set_string(pp, "url", kPayPalUrl);
	obs_data_array_push_back(arr, pp);
	obs_data_release(pp);

	obs_data_set_array(root, "links", arr);
	obs_data_array_release(arr);
	// Ruta opcional a una imagen con tu código QR (png/jpg). Vacío = sin QR.
	obs_data_set_string(root, "qr", "");
	obs_data_save_json_safe(root, path.c_str(), "tmp", "bak");
	obs_data_release(root);
}

DonationConfig loadDonations()
{
	DonationConfig cfg;
	std::string path = donationsPath();
	if (path.empty())
		return cfg;

	if (!os_file_exists(path.c_str()))
		writeDefault(path);

	obs_data_t *root = obs_data_create_from_json_file(path.c_str());
	if (!root)
		return cfg; // archivo ilegible: se muestra el mensaje neutro

	obs_data_array_t *arr = obs_data_get_array(root, "links");
	size_t n = arr ? obs_data_array_count(arr) : 0;
	for (size_t i = 0; i < n; i++) {
		obs_data_t *o = obs_data_array_item(arr, i);
		DonationLink l;
		l.name = obs_data_get_string(o, "name");
		l.url = obs_data_get_string(o, "url");
		obs_data_release(o);
		if (l.name.empty() || !isSafeDonationUrl(l.url))
			continue;
		cfg.links.push_back(l);
	}
	if (arr)
		obs_data_array_release(arr);
	cfg.qrPath = obs_data_get_string(root, "qr");
	obs_data_release(root);
	return cfg;
}
