#include "../app/src/main/cpp/dsp/ring_buffer.h"
#include "test_framework.h"

using onc::dsp::RingBuffer;

ONC_TEST(RingBuffer_WriteThenReadRoundTrips) {
    RingBuffer rb(8);
    float in[5] = {1, 2, 3, 4, 5};
    const size_t written = rb.write(in, 5);
    ONC_CHECK(written == 5);
    ONC_CHECK(rb.availableToRead() == 5);

    float out[5] = {};
    const size_t readCount = rb.read(out, 5);
    ONC_CHECK(readCount == 5);
    for (int i = 0; i < 5; ++i) {
        ONC_CHECK_NEAR(out[i], in[i], 1e-9);
    }
    ONC_CHECK(rb.availableToRead() == 0);
}

ONC_TEST(RingBuffer_WriteRejectsWhenFull) {
    RingBuffer rb(4);
    float in[6] = {1, 2, 3, 4, 5, 6};
    const size_t written = rb.write(in, 6);
    ONC_CHECK(written == 4); // capacity caps the write, no overflow/crash
}

ONC_TEST(RingBuffer_ReadReturnsZeroWhenEmpty) {
    RingBuffer rb(4);
    float out[4] = {9, 9, 9, 9};
    const size_t readCount = rb.read(out, 4);
    ONC_CHECK(readCount == 0);
}

ONC_TEST(RingBuffer_WrapsAroundCorrectly) {
    RingBuffer rb(4);
    float a[3] = {1, 2, 3};
    rb.write(a, 3);
    float tmp[2] = {};
    rb.read(tmp, 2); // consume 1, 2 -> tail advances past wrap point

    float b[3] = {4, 5, 6};
    const size_t written = rb.write(b, 3);
    ONC_CHECK(written == 3);

    float out[4] = {};
    const size_t readCount = rb.read(out, 4);
    ONC_CHECK(readCount == 4);
    ONC_CHECK_NEAR(out[0], 3.0, 1e-9);
    ONC_CHECK_NEAR(out[1], 4.0, 1e-9);
    ONC_CHECK_NEAR(out[2], 5.0, 1e-9);
    ONC_CHECK_NEAR(out[3], 6.0, 1e-9);
}

ONC_TEST(RingBuffer_ResetClearsState) {
    RingBuffer rb(4);
    float a[3] = {1, 2, 3};
    rb.write(a, 3);
    rb.reset();
    ONC_CHECK(rb.availableToRead() == 0);
}
