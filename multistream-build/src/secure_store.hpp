#pragma once
// OBS MultiStream Hub - Creado por Sintacto (@sintacto12)
//
// Cifrado de claves de transmisión con Windows DPAPI: solo el mismo usuario
// de Windows, en el mismo equipo, puede descifrarlas. Las claves nunca se
// escriben en el registro de OBS ni se envían a ningún servidor.

#include <string>

// Devuelve el texto cifrado en base64, o "" si falla (o si plain está vacío).
std::string protectSecret(const std::string &plain);

// Devuelve el texto original, o "" si falla.
std::string unprotectSecret(const std::string &b64);
