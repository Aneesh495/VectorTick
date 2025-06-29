#include "vectortick/storage/segment_reader.hpp"
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <unistd.h>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0 || size > 1024 * 1024) {
        return 0;
    }

    char tmp_path[] = "/tmp/vt_fuzz_segment_XXXXXX";
    int fd = mkstemp(tmp_path);
    if (fd < 0) {
        return 0;
    }
    
    ssize_t written = write(fd, data, size);
    close(fd);
    if (written < 0 || static_cast<size_t>(written) != size) {
        unlink(tmp_path);
        return 0;
    }

    vectortick::SegmentReader reader;
    auto status = reader.open(tmp_path);
    if (status.ok()) {
        (void)reader.validate();
        if (reader.row_count() > 0 && reader.row_count() < 1000) {
            vectortick::CanonicalEvent ev{};
            (void)reader.read_event(0, ev);
        }
        reader.close();
    }
    unlink(tmp_path);
    return 0;
}
