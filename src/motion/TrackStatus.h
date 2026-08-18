#pragma once

// Estado del seguimiento según la fiabilidad de la última medición.
enum class TrackStatus {
    VALID,     // la medición del tracker confirmó la posición
    UNCERTAIN, // sin medición reciente (1..lostAfter misses), solo predicción
    LOST       // tracking perdido; se sigue prediciendo pero no se confía
};