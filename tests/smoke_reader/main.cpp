#include "video/FFmpegVideoReader.h"

#include <cstdio>

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("uso: smoke_reader <vídeo>\n");
        return 2;
    }

    FFmpegVideoReader reader;
    if (!reader.open(argv[1])) {
        std::printf("FAIL: no se pudo abrir %s\n", argv[1]);
        return 1;
    }

    std::printf("w=%d h=%d fps=%.3f frames=%lld durUs=%lld\n",
                reader.width(), reader.height(), reader.fps(),
                static_cast<long long>(reader.frameCount()),
                static_cast<long long>(reader.durationUs()));

    Frame f;
    int count = 0;
    int64_t lastPts = -1;
    while (reader.readNext(f) && count < 100) {
        if (f.ptsUs <= lastPts) {
            std::printf("FAIL: pts no monótono en frame %d\n", count);
            return 1;
        }
        if (f.image.empty() || f.image.cols != reader.width() || f.image.rows != reader.height()) {
            std::printf("FAIL: frame %d de tamaño %dx%d (esperado %dx%d)\n",
                        count, f.image.cols, f.image.rows,
                        reader.width(), reader.height());
            return 1;
        }
        lastPts = f.ptsUs;
        ++count;
    }
    if (count == 0) {
        std::printf("FAIL: no se decodificó ningún frame\n");
        return 1;
    }

    if (!reader.seekToUs(2000000)) {
        std::printf("FAIL: seekToUs falló\n");
        return 1;
    }
    if (!reader.readNext(f)) {
        std::printf("FAIL: readNext tras seek falló\n");
        return 1;
    }
    std::printf("OK: %d frames decodificados, seek -> pts=%lld\n",
                count, static_cast<long long>(f.ptsUs));
    return 0;
}