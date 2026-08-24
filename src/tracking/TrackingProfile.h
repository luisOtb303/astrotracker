#pragma once

#include "tracking/DiscDetection.h"

#include <vector>

// Perfiles de objeto: cada uno define la cadena de prioridad de métodos de
// localización que el motor prueba en cada foto. Los métodos aún no
// implementados se ignoran al construir los detectores.
enum class ObjectProfile {
    Auto,
    Sun,
    Moon,
    Planet,
    SolarEclipse,
    LunarEclipse
};

struct TrackingProfile {
    ObjectProfile profile = ObjectProfile::Auto;
    // Orden de prueba de las fuentes de candidatos. El orden importa: las
    // fuentes previas bloquean la búsqueda a frame completo de las siguientes
    // (comportamiento histórico del motor).
    std::vector<DiscMethod> priority;
};

// Cadena de prioridad predefinida de cada perfil. El perfil AUTO reproduce el
// comportamiento histórico exacto del motor.
inline TrackingProfile trackingProfileFor(ObjectProfile profile)
{
    TrackingProfile tp;
    tp.profile = profile;
    switch (profile) {
    case ObjectProfile::Sun:
    case ObjectProfile::SolarEclipse:
        tp.priority = {DiscMethod::Template, DiscMethod::ArcBlob};
        break;
    case ObjectProfile::Moon:
    case ObjectProfile::LunarEclipse:
        tp.priority = {DiscMethod::Template, DiscMethod::ArcBlob};
        break;
    case ObjectProfile::Planet:
        tp.priority = {DiscMethod::ArcBlob, DiscMethod::Template};
        break;
    case ObjectProfile::Auto:
        break;
    }
    if (tp.priority.empty())
        tp.priority = {DiscMethod::Template, DiscMethod::ArcBlob};
    return tp;
}

// Nombre para la interfaz (español).
inline const char* objectProfileName(ObjectProfile profile)
{
    switch (profile) {
    case ObjectProfile::Sun: return "Sol";
    case ObjectProfile::Moon: return "Luna";
    case ObjectProfile::Planet: return "Planeta";
    case ObjectProfile::SolarEclipse: return "Eclipse solar";
    case ObjectProfile::LunarEclipse: return "Eclipse lunar";
    case ObjectProfile::Auto: break;
    }
    return "Auto";
}

// Claves estables ASCII para JSON y QSettings.
inline const char* objectProfileKey(ObjectProfile profile)
{
    switch (profile) {
    case ObjectProfile::Sun: return "sol";
    case ObjectProfile::Moon: return "luna";
    case ObjectProfile::Planet: return "planeta";
    case ObjectProfile::SolarEclipse: return "eclipse_solar";
    case ObjectProfile::LunarEclipse: return "eclipse_lunar";
    case ObjectProfile::Auto: break;
    }
    return "auto";
}

inline bool keyEquals(const char* a, const char* b)
{
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb)
            return false;
        ++a;
        ++b;
    }
    return *a == *b;
}

inline bool objectProfileFromKey(const char* key, ObjectProfile& out)
{
    if (!key)
        return false;
    if (keyEquals(key, "sol")) { out = ObjectProfile::Sun; return true; }
    if (keyEquals(key, "luna")) { out = ObjectProfile::Moon; return true; }
    if (keyEquals(key, "planeta")) { out = ObjectProfile::Planet; return true; }
    if (keyEquals(key, "eclipse_solar")) { out = ObjectProfile::SolarEclipse; return true; }
    if (keyEquals(key, "eclipse_lunar")) { out = ObjectProfile::LunarEclipse; return true; }
    if (keyEquals(key, "auto")) { out = ObjectProfile::Auto; return true; }
    return false;
}

inline const char* discMethodKey(DiscMethod method)
{
    switch (method) {
    case DiscMethod::Template: return "plantilla";
    case DiscMethod::ArcBlob: return "arco";
    case DiscMethod::KnownRadius: return "radio_conocido";
    case DiscMethod::PhaseCorrelation: return "correlacion_fase";
    case DiscMethod::Ecc: return "ecc";
    case DiscMethod::Features: return "features";
    case DiscMethod::Centroid: return "centroide";
    case DiscMethod::Prediction: break;
    }
    return "prediccion";
}

inline bool discMethodFromKey(const char* key, DiscMethod& out)
{
    if (!key)
        return false;
    if (keyEquals(key, "plantilla")) { out = DiscMethod::Template; return true; }
    if (keyEquals(key, "arco")) { out = DiscMethod::ArcBlob; return true; }
    if (keyEquals(key, "radio_conocido")) { out = DiscMethod::KnownRadius; return true; }
    if (keyEquals(key, "correlacion_fase")) { out = DiscMethod::PhaseCorrelation; return true; }
    if (keyEquals(key, "ecc")) { out = DiscMethod::Ecc; return true; }
    if (keyEquals(key, "features")) { out = DiscMethod::Features; return true; }
    if (keyEquals(key, "centroide")) { out = DiscMethod::Centroid; return true; }
    if (keyEquals(key, "prediccion")) { out = DiscMethod::Prediction; return true; }
    return false;
}
