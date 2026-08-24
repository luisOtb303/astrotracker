#pragma once

// Parámetros del seguimiento del disco (Sol/Luna) por perfil radial.
// Header propio para evitar ciclos de includes entre el motor y sus
// componentes (scorer, detectores).
struct DiscTrackerParams {
    int rays = 72;                 // número de rayos desde el centro predicho
    float radiusTolerance = 4.f;   // píxeles de tolerancia del limbo alrededor de R
    float bandScale = 0.25f;       // banda radial de búsqueda: R*(1±bandScale)
    float contrastThreshold = 10.f; // salto de intensidad mínimo del limbo (8-bit)
    float validRatio = 0.40f;      // inliers/rayos para considerar VALID
    float acceptRatio = 0.15f;     // mínimo para aceptar la medición (UNCERTAIN)
    int lostAfterMisses = 3;
    // Ventana de búsqueda base para re-adquirir el disco: R*searchMarginScale,
    // con un mínimo de searchMarginMinPx píxeles. Si el objeto salta entre
    // fotos más de lo que abarca esta ventana, crece searchGrowthPerMiss por
    // cada foto fallida hasta maxSearchFactor*base.
    float searchMarginScale = 1.2f;
    float searchMarginMinPx = 40.f;
    float searchGrowthPerMiss = 0.8f;
    float maxSearchFactor = 8.f;
    // Distancia máxima aceptada de la plantilla al buscarla con matchTemplate
    // (TM_SQDIFF_NORMED; 0 = idéntico). Las regiones planas dan valores altos,
    // así que el mínimo del disco real queda discriminado.
    float templateSqMax = 0.5f;
};
