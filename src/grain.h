// Geração das texturas (sem dependência do Windows, para poder gerar prévias em qualquer sistema).
// Todos os estilos partem da "Seda" (grão de 1 px) e ficam cada vez mais finos: o ruído é
// filtrado para tirar os aglomerados (ruído "azul", que o olho percebe como mais fino) e
// os pontos ficam mais suaves.
#pragma once
#include <stdint.h>
#include <math.h>

typedef struct {
    const wchar_t *name, *desc;
    float hp;               // 0 = ruído comum (Seda); 1 = sem aglomerados, aparência mais fina
    float gain, light, cap; // força, proporção de pontos claros, alpha máximo
} GrainPreset;

// Do mais fino ao mais grosso
static const GrainPreset PRESETS[] = {
    {L"Pó", L"O mais fino de todos, quase imperceptível", 1.00f, 0.070f, 0.95f, 0.22f},
    {L"Névoa", L"Finíssimo e suave", 1.00f, 0.090f, 0.92f, 0.27f},
    {L"Véu", L"Muito fino, sem aglomerados", 0.70f, 0.100f, 0.90f, 0.30f},
    {L"Cetim", L"Fino e uniforme", 0.40f, 0.110f, 0.90f, 0.33f},
    {L"Seda", L"O mais marcado, ótimo para ler por horas", 0.00f, 0.120f, 0.90f, 0.35f},
};
#define NUM_PRESETS ((int)(sizeof PRESETS / sizeof PRESETS[0]))
#define DEFAULT_PRESET 4

static inline uint32_t grain_hash(uint32_t x, uint32_t y, uint32_t s)
{
    uint32_t h = x * 0x8da6b343u ^ y * 0xd8163841u ^ s * 0xcb1ab31fu;
    h ^= h >> 13; h *= 0x5bd1e995u;
    h ^= h >> 15; h *= 0x27d4eb2du;
    h ^= h >> 16;
    return h;
}

// Soma de 4 bytes uniformes: aproximação de uma normal com média 0 e desvio 1.
static inline float grain_gauss(uint32_t h)
{
    return ((float)((h & 255) + ((h >> 8) & 255) + ((h >> 16) & 255) + (h >> 24)) - 510.0f) / 147.8f;
}

// Preenche px (BGRA pré-multiplicado, w*h) com a textura na intensidade máxima.
// Pontos escuros e claros, para aparecer tanto em fundos claros quanto escuros.
// O grão é sempre em pixels físicos, então fica fino em qualquer escala de tela.
static void render_grain(uint32_t *px, int w, int h, float scale, uint32_t seed, const GrainPreset *p)
{
    (void)scale;
    float norm = 1.0f / sqrtf(1.0f + p->hp * p->hp / 4.0f);
    for (int y = 0; y < h; y++) {
        uint32_t *row = px + (size_t)y * w;
        for (int x = 0; x < w; x++) {
            float n = grain_gauss(grain_hash(x, y, seed));
            if (p->hp > 0) {
                float m = grain_gauss(grain_hash(x - 1, y, seed)) + grain_gauss(grain_hash(x + 1, y, seed)) +
                          grain_gauss(grain_hash(x, y - 1, seed)) + grain_gauss(grain_hash(x, y + 1, seed));
                n = (n - p->hp * m * 0.25f) * norm;
            }
            float a = fabsf(n) * p->gain;
            if (n > 0)
                a *= p->light;
            if (a > p->cap)
                a = p->cap;
            uint32_t A = (uint32_t)(a * 255.0f + 0.5f), V = n > 0 ? A : 0;
            row[x] = A << 24 | V << 16 | V << 8 | V;
        }
    }
}
