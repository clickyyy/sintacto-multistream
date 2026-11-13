OBS MultiStream Hub (Multistream Sintacto)

Plugin de OBS Studio para transmitir a varias plataformas a la vez (TikTok, Kick, Twitch, YouTube horizontal, YouTube vertical y Starvios) gastando los mínimos recursos posibles.

Creado por Sintacto (@sintacto12).

Versión: 0.3.0
Probado en: OBS Studio 32.2.2 (Windows 10/11)
Estado: en desarrollo
Características
Dos codificadores de video en total. Uno compartido por todos los destinos horizontales y otro por todos los verticales.
Un codificador de audio AAC compartido por todos los destinos.
Una salida RTMP por destino. Solo empaqueta y envía, no codifica, así que es barata.
Lienzo vertical 9:16 propio (1080x1920), independiente del horizontal. Existe siempre para poder editarlo, pero su video solo se crea mientras transmites en vertical.
Vista previa vertical con capas: puedes añadir escenas o fuentes sueltas, moverlas con el ratón y cambiar su zoom.
Claves de transmisión cifradas con Windows DPAPI. Nunca se escriben en el log.
TikTok es vertical por defecto.
Reconexión automática y estadísticas por destino. Un fallo en un destino no afecta a los demás.
Tema visual propio en azul noche con acentos en degradado azul a morado.
Botón de apoyo al creador, con enlaces editables en donaciones.json (solo https).
Cómo compilar

Requisitos: Windows 10/11, Visual Studio 2022 con herramientas de C++, CMake 3.28 o superior y Qt 6 (se descarga con las dependencias de OBS al configurar).

Desde la carpeta raíz del repositorio, en PowerShell:

powershell
cmake -S multistream-build -B multistream-build\build_x64
cmake --build multistream-build\build_x64 --config RelWithDebInfo

El resultado es obs-multistream-hub.dll, en multistream-build\build_x64\RelWithDebInfo\.

Cómo instalar
Cierra OBS por completo.
Copia obs-multistream-hub.dll a C:\Program Files\obs-studio\obs-plugins\64bit\ (hace falta PowerShell como administrador).
Abre OBS. Deben aparecer los docks Multistream Sintacto y Lienzo Vertical.

Si el plugin no carga (Failed to initialize module), hay que recompilarlo contra los encabezados de tu versión de OBS.

Configuración

Los datos se guardan en:

%APPDATA%\obs-studio\plugin_config\obs-multistream-hub\
Archivo	Contenido
destinations.json	Destinos, servidores y claves cifradas
settings.json	Ajustes de emisión (video H, video V, audio)
layout.json	Capas del lienzo vertical
donaciones.json	Enlaces de apoyo al creador

Las actualizaciones del plugin no los sobrescriben. No subas estos archivos a ningún repositorio, porque contienen tus claves de transmisión (cifradas, pero personales).

Estructura del código
Archivo	Función
plugin-main.cpp	Entrada del módulo y eventos de OBS
destination.*	Destinos, perfiles de video, ajustes y guardado
encoder_pool.*	Un codificador por orientación y uno de audio
output_manager.*	Inicio en paralelo, paradas, reconexión y estadísticas
vertical_canvas.*	Escena privada 9:16, capas y video vertical
vertical_panel.*	Panel Lienzo Vertical (vista previa y capas)
dock.*	Panel principal (tarjetas de destinos, iniciar y detener)
theme.hpp	Colores y estilos compartidos
donations.*	Enlaces de donación editables
secure_store.*	Cifrado de claves con DPAPI
obs_raii.hpp	Envoltorios RAII para objetos de libobs
Pendiente
Probar la transmisión real en horizontal y vertical a la vez.
Recompilar contra los encabezados de OBS 32.2.2.
Copiar la posición original de cada fuente al traerla del horizontal.
Crear fuentes y escenas verticales propias dentro del panel.
Revisar la lista de codificadores de hardware (NVENC, AMF, QSV).
Licencia

Ver el archivo LICENSE. OBS Studio se distribuye bajo GPL v2.
