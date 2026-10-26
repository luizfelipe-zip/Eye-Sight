// Gera uma prévia dos estilos de textura em PPM, sem precisar do Windows.
// Coluna da esquerda: fundo claro; direita: fundo escuro. Uma linha por estilo.
//
//   gcc -O2 -Isrc tools/preview.c -o preview -lm
//   ./preview [escala] [intensidade 0..1] > preview.ppm
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include "grain.h"

int main(int argc, char **argv)
{
    float scale = argc > 1 ? atof(argv[1]) : 1.0f;
    float t = argc > 2 ? atof(argv[2]) : 1.0f;
    int tw = 320, th = 110, W = tw * 2, H = th * NUM_PRESETS;
    unsigned char *img = malloc((size_t)W * H * 3);
    uint32_t *g = malloc((size_t)tw * th * 4);
    for (int p = 0; p < NUM_PRESETS; p++) {
        render_grain(g, tw, th, scale, 0x1234567u, &PRESETS[p]);
        for (int y = 0; y < th; y++)
            for (int x = 0; x < W; x++) {
                float bg = x < tw ? 245 : 30;
                uint32_t px = g[y * tw + x % tw];
                float a = (px >> 24) / 255.f * t, v = (px & 255) / 255.f * t * 255;
                float o = bg * (1 - a) + v;
                unsigned char *q = &img[((size_t)(p * th + y) * W + x) * 3];
                q[0] = q[1] = q[2] = (unsigned char)(o > 255 ? 255 : o);
            }
    }
    printf("P6 %d %d 255\n", W, H);
    fwrite(img, 1, (size_t)W * H * 3, stdout);
    return 0;
}
